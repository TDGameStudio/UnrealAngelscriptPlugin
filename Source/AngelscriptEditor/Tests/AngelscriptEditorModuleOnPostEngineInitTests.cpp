#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"

#include "ContentBrowser/AngelscriptContentBrowserDataSource.h"

#include "IContentBrowserDataModule.h"
#include "ContentBrowserDataSubsystem.h"
#include "IDirectoryWatcher.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleOnPostEngineInitTests_Private
{
	class FMockDirectoryWatcher final : public IDirectoryWatcher
	{
	public:
		bool RegisterDirectoryChangedCallback_Handle(const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, uint32 Flags) override
		{
			OutHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
			return true;
		}

		bool UnregisterDirectoryChangedCallback_Handle(const FString& Directory, FDelegateHandle InHandle) override
		{
			return true;
		}
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


static bool RunOnPostEngineInitDoesNotDuplicateAcrossRestart(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleOnPostEngineInitTests_Private;
	UContentBrowserDataSubsystem* ContentBrowserDataSubsystem = IContentBrowserDataModule::Get().GetSubsystem();
	if (!TestNotNull(TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should resolve the content browser data subsystem"), ContentBrowserDataSubsystem))
	{
		return false;
	}

	const FName DataSourceName(TEXT("AngelscriptData"));
	FMockDirectoryWatcher DirectoryWatcher;
	FAngelscriptEditorModule Module;
	bool bModuleStarted = false;
	int32 InitInvocationCount = 0;

	CleanupTransientAngelscriptDataSources(*ContentBrowserDataSubsystem);

	FAngelscriptEditorModuleTestAccess::SetDirectoryWatcherResolver([&DirectoryWatcher]()
	{
		return &DirectoryWatcher;
	});
	FAngelscriptEditorModuleTestAccess::SetOnEngineInitDoneOverride([&InitInvocationCount]()
	{
		++InitInvocationCount;
	});

	ON_SCOPE_EXIT
	{
		if (bModuleStarted)
		{
			Module.ShutdownModule();
		}

		FAngelscriptEditorModuleTestAccess::ResetOnEngineInitDoneOverride();
		FAngelscriptEditorModuleTestAccess::ResetDirectoryWatcherResolver();
		CleanupTransientAngelscriptDataSources(*ContentBrowserDataSubsystem);
	};

	if (!TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should start from zero transient data sources"),
			CountDataSourceName(CollectTransientAngelscriptDataSources(), DataSourceName),
			0)
		|| !TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should start with the data source inactive"),
			CountActiveDataSourceName(ContentBrowserDataSubsystem->GetActiveDataSources(), DataSourceName),
			0))
	{
		return false;
	}

	Module.StartupModule();
	bModuleStarted = true;

	const int32 InvocationCountBeforeFirstBroadcast = InitInvocationCount;
	FAngelscriptEditorModuleTestAccess::BroadcastRegisteredOnPostEngineInit();
	if (!TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should invoke OnEngineInitDone exactly once after the initial startup"),
			InitInvocationCount - InvocationCountBeforeFirstBroadcast,
			1)
		|| !TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should create exactly one transient data source on the first broadcast"),
			CountDataSourceName(CollectTransientAngelscriptDataSources(), DataSourceName),
			1)
		|| !TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should keep exactly one active data source on the first broadcast"),
			CountActiveDataSourceName(ContentBrowserDataSubsystem->GetActiveDataSources(), DataSourceName),
			1))
	{
		return false;
	}

	Module.ShutdownModule();
	bModuleStarted = false;
	CleanupTransientAngelscriptDataSources(*ContentBrowserDataSubsystem);

	const int32 InvocationCountBeforeShutdownBroadcast = InitInvocationCount;
	FAngelscriptEditorModuleTestAccess::BroadcastRegisteredOnPostEngineInit();
	if (!TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should leave no OnPostEngineInit callback behind after shutdown"),
			InitInvocationCount - InvocationCountBeforeShutdownBroadcast,
			0)
		|| !TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should not recreate the data source after shutdown"),
			CountDataSourceName(CollectTransientAngelscriptDataSources(), DataSourceName),
			0))
	{
		return false;
	}

	Module.StartupModule();
	bModuleStarted = true;

	const int32 InvocationCountBeforeRestartBroadcast = InitInvocationCount;
	FAngelscriptEditorModuleTestAccess::BroadcastRegisteredOnPostEngineInit();
	if (!TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should still invoke OnEngineInitDone only once after restart"),
			InitInvocationCount - InvocationCountBeforeRestartBroadcast,
			1)
		|| !TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should recreate only one transient data source after restart"),
			CountDataSourceName(CollectTransientAngelscriptDataSources(), DataSourceName),
			1)
		|| !TestEqual(
			TEXT("Editor.Module.OnPostEngineInitDoesNotDuplicateAcrossRestart should keep exactly one active data source after restart"),
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

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleOnPostEngineInitTests,
	"Angelscript.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OnPostEngineInitDoesNotDuplicateAcrossRestart)
	{
		ASSERT_THAT(IsTrue(RunOnPostEngineInitDoesNotDuplicateAcrossRestart(*TestRunner)));
	}
};

#endif
