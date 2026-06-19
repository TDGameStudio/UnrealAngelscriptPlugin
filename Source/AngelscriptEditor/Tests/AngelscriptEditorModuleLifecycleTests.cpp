#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"

#include "ContentBrowser/AngelscriptContentBrowserDataSource.h"
#include "AngelscriptEngine.h"

#include "IContentBrowserDataModule.h"
#include "ContentBrowserDataSubsystem.h"
#include "Delegates/IDelegateInstance.h"
#include "IDirectoryWatcher.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestNull(...) Test.TestNull(__VA_ARGS__)

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLifecycleTests_Private
{
	class FMockDirectoryWatcher final : public IDirectoryWatcher
	{
	public:
		struct FRegisterCall
		{
			FString Directory;
			FDelegateHandle Handle;
			uint32 Flags = 0;
		};

		struct FUnregisterCall
		{
			FString Directory;
			FDelegateHandle Handle;
		};

		bool RegisterDirectoryChangedCallback_Handle(const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, uint32 Flags) override
		{
			OutHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
			RegisterCalls.Add({ Directory, OutHandle, Flags });
			ActiveHandles.FindOrAdd(Directory).Add(OutHandle);
			return true;
		}

		bool UnregisterDirectoryChangedCallback_Handle(const FString& Directory, FDelegateHandle InHandle) override
		{
			UnregisterCalls.Add({ Directory, InHandle });

			if (TArray<FDelegateHandle>* Handles = ActiveHandles.Find(Directory))
			{
				const int32 RemovedCount = Handles->RemoveSingle(InHandle);
				if (Handles->Num() == 0)
				{
					ActiveHandles.Remove(Directory);
				}
				return RemovedCount > 0;
			}

			return false;
		}

		int32 GetActiveHandleCount() const
		{
			int32 Count = 0;
			for (const TPair<FString, TArray<FDelegateHandle>>& Pair : ActiveHandles)
			{
				Count += Pair.Value.Num();
			}
			return Count;
		}

		TArray<FRegisterCall> RegisterCalls;
		TArray<FUnregisterCall> UnregisterCalls;
		TMap<FString, TArray<FDelegateHandle>> ActiveHandles;
	};

	TArray<UAngelscriptContentBrowserDataSource*> CollectTransientAngelscriptDataSources()
	{
		TArray<UAngelscriptContentBrowserDataSource*> Sources;
		for (TObjectIterator<UAngelscriptContentBrowserDataSource> It; It; ++It)
		{
			if (It->GetOuter() == GetTransientPackage() && It->GetName().StartsWith(TEXT("AngelscriptData")))
			{
				Sources.Add(*It);
			}
		}
		return Sources;
	}

	int32 CountDataSourceName(const TArray<UAngelscriptContentBrowserDataSource*>& Sources, const FName ExpectedName)
	{
		int32 Count = 0;
		for (const UAngelscriptContentBrowserDataSource* Source : Sources)
		{
			if (Source != nullptr && Source->GetFName() == ExpectedName)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 CountActiveDataSourceName(const TArray<FName>& ActiveNames, const FName ExpectedName)
	{
		int32 Count = 0;
		for (const FName ActiveName : ActiveNames)
		{
			if (ActiveName == ExpectedName)
			{
				++Count;
			}
		}
		return Count;
	}

	void CleanupTransientAngelscriptDataSources(UContentBrowserDataSubsystem& ContentBrowserDataSubsystem)
	{
		TArray<UAngelscriptContentBrowserDataSource*> Sources = CollectTransientAngelscriptDataSources();
		for (UAngelscriptContentBrowserDataSource* Source : Sources)
		{
			if (Source == nullptr)
			{
				continue;
			}

			ContentBrowserDataSubsystem.DeactivateDataSource(Source->GetFName());
			if (Source->IsRooted())
			{
				Source->RemoveFromRoot();
			}
			Source->MarkAsGarbage();
		}

		CollectGarbage(RF_NoFlags, true);
	}
}


static bool RunStartupShutdownRegistersAndCleansHooks(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLifecycleTests_Private;
	FMockDirectoryWatcher DirectoryWatcher;
	FAngelscriptEditorModule Module;
	bool bModuleStarted = false;

	FAngelscriptEditorModuleTestAccess::SetDirectoryWatcherResolver([&DirectoryWatcher]()
	{
		return &DirectoryWatcher;
	});

	ON_SCOPE_EXIT
	{
		if (bModuleStarted)
		{
			Module.ShutdownModule();
		}
		FAngelscriptEditorModuleTestAccess::ResetDirectoryWatcherResolver();
	};

	const TArray<FString> ExpectedRootPaths = FAngelscriptEngine::MakeAllScriptRoots();
	if (!TestTrue(TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should have at least one script root to watch"), ExpectedRootPaths.Num() > 0))
	{
		return false;
	}

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
	if (!TestNotNull(TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should resolve the Tools menu"), ToolsMenu))
	{
		return false;
	}

	Module.StartupModule();
	bModuleStarted = true;

	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should register one directory watcher per script root"),
		DirectoryWatcher.RegisterCalls.Num(),
		ExpectedRootPaths.Num());
	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should keep one active watcher handle per script root after startup"),
		DirectoryWatcher.GetActiveHandleCount(),
		ExpectedRootPaths.Num());
	TestTrue(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should register the literal asset pre-save hook during startup"),
		FAngelscriptEditorModuleTestAccess::IsLiteralAssetPreSaveRegistered());
	TestTrue(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should keep a valid state dump extension handle during startup"),
		FAngelscriptEditorModuleTestAccess::HasStateDumpExtensionHandle(Module));

	FAngelscriptEditorModuleTestAccess::RegisterToolsMenuEntries(Module);
	FToolMenuSection* ProgrammingSection = ToolsMenu->FindSection("Programming");
	if (!TestNotNull(TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should create the Programming menu section"), ProgrammingSection))
	{
		return false;
	}
	TestNotNull(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should register the ASOpenCode tool menu entry"),
		ProgrammingSection->FindEntry("ASOpenCode"));

	Module.ShutdownModule();
	bModuleStarted = false;

	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should unregister one directory watcher per script root during shutdown"),
		DirectoryWatcher.UnregisterCalls.Num(),
		ExpectedRootPaths.Num());
	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should clear all active watcher handles during shutdown"),
		DirectoryWatcher.GetActiveHandleCount(),
		0);
	TestFalse(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should remove the literal asset pre-save hook during shutdown"),
		FAngelscriptEditorModuleTestAccess::IsLiteralAssetPreSaveRegistered());
	TestFalse(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should reset the state dump extension handle during shutdown"),
		FAngelscriptEditorModuleTestAccess::HasStateDumpExtensionHandle(Module));
	ProgrammingSection = ToolsMenu->FindSection("Programming");
	if (ProgrammingSection != nullptr)
	{
		TestNull(
			TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should remove the ASOpenCode tool menu entry during shutdown"),
			ProgrammingSection->FindEntry("ASOpenCode"));
	}

	const int32 UnregisterCountAfterFirstShutdown = DirectoryWatcher.UnregisterCalls.Num();
	Module.ShutdownModule();
	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should keep shutdown idempotent"),
		DirectoryWatcher.UnregisterCalls.Num(),
		UnregisterCountAfterFirstShutdown);

	Module.StartupModule();
	bModuleStarted = true;

	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should add exactly one watcher per root after restart"),
		DirectoryWatcher.RegisterCalls.Num(),
		ExpectedRootPaths.Num() * 2);
	TestEqual(
		TEXT("Editor.Module.StartupShutdownRegistersAndCleansHooks should not retain stale watcher handles after restart"),
		DirectoryWatcher.GetActiveHandleCount(),
		ExpectedRootPaths.Num());

	return true;
}

static bool RunOnEngineInitDoneActivatesAngelscriptDataSourceOnce(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLifecycleTests_Private;
	UContentBrowserDataSubsystem* ContentBrowserDataSubsystem = IContentBrowserDataModule::Get().GetSubsystem();
	if (!TestNotNull(TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should resolve the content browser data subsystem"), ContentBrowserDataSubsystem))
	{
		return false;
	}

	CleanupTransientAngelscriptDataSources(*ContentBrowserDataSubsystem);

	ON_SCOPE_EXIT
	{
		CleanupTransientAngelscriptDataSources(*ContentBrowserDataSubsystem);
	};

	const FName DataSourceName(TEXT("AngelscriptData"));
	if (!TestEqual(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should start from zero transient Angelscript data sources"),
			CountDataSourceName(CollectTransientAngelscriptDataSources(), DataSourceName),
			0)
		|| !TestEqual(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should start with the data source deactivated"),
			CountActiveDataSourceName(ContentBrowserDataSubsystem->GetActiveDataSources(), DataSourceName),
			0))
	{
		return false;
	}

	FAngelscriptEditorModuleTestAccess::InvokeOnEngineInitDone();

	TArray<UAngelscriptContentBrowserDataSource*> SourcesAfterFirstInit = CollectTransientAngelscriptDataSources();
	UAngelscriptContentBrowserDataSource* const* CreatedDataSourcePtr = SourcesAfterFirstInit.FindByPredicate([](const UAngelscriptContentBrowserDataSource* Source)
	{
		return Source != nullptr && Source->GetFName() == FName(TEXT("AngelscriptData"));
	});
	UAngelscriptContentBrowserDataSource* CreatedDataSource = CreatedDataSourcePtr != nullptr ? *CreatedDataSourcePtr : nullptr;
	if (!TestEqual(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should create exactly one transient AngelscriptData source on first init"),
			CountDataSourceName(SourcesAfterFirstInit, DataSourceName),
			1)
		|| !TestNotNull(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should create the named transient data source"),
			CreatedDataSource))
	{
		return false;
	}

	if (!TestTrue(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should initialize the created data source"),
			CreatedDataSource->IsInitialized())
		|| !TestTrue(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should root the created data source"),
			CreatedDataSource->IsRooted())
		|| !TestTrue(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should keep the created data source transient"),
			CreatedDataSource->HasAnyFlags(RF_Transient)))
	{
		return false;
	}

	if (!TestEqual(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should activate the data source after first init"),
			CountActiveDataSourceName(ContentBrowserDataSubsystem->GetActiveDataSources(), DataSourceName),
			1))
	{
		return false;
	}

	FAngelscriptEditorModuleTestAccess::InvokeOnEngineInitDone();

	const TArray<UAngelscriptContentBrowserDataSource*> SourcesAfterSecondInit = CollectTransientAngelscriptDataSources();
	if (!TestEqual(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should stay idempotent across repeated init triggers"),
			CountDataSourceName(SourcesAfterSecondInit, DataSourceName),
			1)
		|| !TestEqual(
			TEXT("Editor.Module.OnEngineInitDoneActivatesAngelscriptDataSourceOnce should keep exactly one active AngelscriptData source after repeated init triggers"),
			CountActiveDataSourceName(ContentBrowserDataSubsystem->GetActiveDataSources(), DataSourceName),
			1))
	{
		return false;
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull
#undef TestNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleLifecycleTests,
	"Angelscript.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StartupShutdownRegistersAndCleansHooks)
	{
		ASSERT_THAT(IsTrue(RunStartupShutdownRegistersAndCleansHooks(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleLifecycleTestModuleTests,
	"Angelscript.TestModule.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OnEngineInitDoneActivatesAngelscriptDataSourceOnce)
	{
		ASSERT_THAT(IsTrue(RunOnEngineInitDoneActivatesAngelscriptDataSourceOnce(*TestRunner)));
	}
};

#endif
