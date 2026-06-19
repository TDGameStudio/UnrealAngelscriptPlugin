#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"

#include "AngelscriptEngine.h"

#include "HAL/FileManager.h"
#include "IDirectoryWatcher.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleDirectoryWatcherTests_Private
{
	class FMockDirectoryWatcher final : public IDirectoryWatcher
	{
	public:
		struct FRegisterCall
		{
			FString Directory;
			FDelegateHandle Handle;
			uint32 Flags = 0;
			FDirectoryChanged Delegate;
		};

		bool RegisterDirectoryChangedCallback_Handle(const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, uint32 Flags) override
		{
			OutHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
			RegisterCalls.Add({ Directory, OutHandle, Flags, InDelegate });
			return true;
		}

		bool UnregisterDirectoryChangedCallback_Handle(const FString& Directory, FDelegateHandle InHandle) override
		{
			UnregisterCalls.Add(TPair<FString, FDelegateHandle>(Directory, InHandle));
			return true;
		}

		bool BroadcastFirst(const TArray<FFileChangeData>& Changes)
		{
			if (RegisterCalls.IsEmpty() || !RegisterCalls[0].Delegate.IsBound())
			{
				return false;
			}

			RegisterCalls[0].Delegate.Execute(Changes);
			return true;
		}

		TArray<FRegisterCall> RegisterCalls;
		TArray<TPair<FString, FDelegateHandle>> UnregisterCalls;
	};

	FString MakeTempEditorModuleRoot(const TCHAR* Prefix)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("EditorModuleTests") / FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	FFileChangeData MakeFileChange(const FString& Filename, FFileChangeData::EFileChangeAction Action)
	{
		return FFileChangeData(Filename, Action);
	}

	TUniquePtr<FAngelscriptEngine> MakeEditorModuleTestEngine(const FString& RootPath)
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (Engine.IsValid())
		{
			Engine->AllRootPaths = { FPaths::ConvertRelativePathToFull(RootPath) };
		}
		return Engine;
	}

	TSharedRef<FAngelscriptModuleDesc> MakeEditorModuleDesc(const FString& ModuleName)
	{
		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
		ModuleDesc->ModuleName = ModuleName;
		return ModuleDesc;
	}

	void AddEditorModuleCodeSection(
		const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc,
		const FString& RelativeFilename,
		const FString& AbsoluteFilename,
		const TCHAR* Code)
	{
		FAngelscriptModuleDesc::FCodeSection& Section = ModuleDesc->Code.AddDefaulted_GetRef();
		Section.RelativeFilename = RelativeFilename;
		Section.AbsoluteFilename = AbsoluteFilename;
		Section.Code = Code;
		Section.CodeHash = static_cast<int64>(FCrc::StrCrc32(*Section.Code));
		ModuleDesc->CodeHash ^= Section.CodeHash;
	}

	bool CompileEditorModuleModules(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& ModulesToCompile)
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::SoftReloadOnly, ModulesToCompile, CompiledModules);
		const bool bCompileHandled =
			CompileResult == ECompileResult::FullyHandled ||
			CompileResult == ECompileResult::PartiallyHandled;
		if (!Test.TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should compile the synthetic loaded modules"), bCompileHandled))
		{
			return false;
		}

		return Test.TestEqual(
			TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should track each synthetic module"),
			CompiledModules.Num(),
			ModulesToCompile.Num());
	}

	bool ContainsFilenamePair(const TArray<FAngelscriptEngine::FFilenamePair>& Files, const FString& AbsolutePath, const FString& RelativePath)
	{
		for (const FAngelscriptEngine::FFilenamePair& File : Files)
		{
			if (File.AbsolutePath == AbsolutePath && File.RelativePath == RelativePath)
			{
				return true;
			}
		}

		return false;
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunOnScriptFileChangesGuardsEngineInitAndQueuesRootScripts(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempEditorModuleRoot(TEXT("OnScriptFileChanges"));
	const FString ScriptsFolder = RootPath / TEXT("Scripts");
	const FString RemovedFolder = RootPath / TEXT("RemovedFolder");
	const FString AddedScriptAbsolutePath = FPaths::ConvertRelativePathToFull(ScriptsFolder / TEXT("Inside.as"));
	const FString RemovedScriptAbsolutePath = FPaths::ConvertRelativePathToFull(RemovedFolder / TEXT("RemovedA.as"));
	const FString NestedRemovedScriptAbsolutePath = FPaths::ConvertRelativePathToFull(RemovedFolder / TEXT("Nested/RemovedB.as"));

	FMockDirectoryWatcher DirectoryWatcher;
	FAngelscriptEditorModule Module;
	bool bModuleStarted = false;
	TUniquePtr<FAngelscriptEngine> Engine = MakeEditorModuleTestEngine(RootPath);
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();

	FAngelscriptEditorModuleTestAccess::SetDirectoryWatcherResolver([&DirectoryWatcher]()
	{
		return &DirectoryWatcher;
	});

	ON_SCOPE_EXIT
	{
		EngineScope.Reset();
		if (bModuleStarted)
		{
			Module.ShutdownModule();
		}
		FAngelscriptEditorModuleTestAccess::ResetDirectoryWatcherResolver();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*ScriptsFolder, true);
	FileManager.MakeDirectory(*(RemovedFolder / TEXT("Nested")), true);

	if (!TestNotNull(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	if (!TestFalse(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should begin from an uninitialized scoped-engine state"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	Module.StartupModule();
	bModuleStarted = true;

	if (!TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should capture the directory watcher callback"), DirectoryWatcher.RegisterCalls.Num() > 0))
	{
		return false;
	}

	Engine->LastFileChangeDetectedTime = 1234.0;
	const TArray<FFileChangeData> AddedScriptChanges = { MakeFileChange(AddedScriptAbsolutePath, FFileChangeData::FCA_Added) };
	if (!TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should replay the registered directory watcher callback"), DirectoryWatcher.BroadcastFirst(AddedScriptChanges)))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should ignore file changes before the engine is initialized"), Engine->FileChangesDetectedForReload.Num(), 0)
		|| !TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should not queue deletions before the engine is initialized"), Engine->FileDeletionsDetectedForReload.Num(), 0)
		|| !TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should preserve the timestamp before initialization"), Engine->LastFileChangeDetectedTime, 1234.0))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->FileChangesDetectedForReload.Reset();
	Engine->FileDeletionsDetectedForReload.Reset();
	Engine->LastFileChangeDetectedTime = 4321.0;

	if (!TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should replay the file-change callback after initialization"), DirectoryWatcher.BroadcastFirst(AddedScriptChanges)))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should queue one root script change after initialization"), Engine->FileChangesDetectedForReload.Num(), 1)
		|| !TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should keep deletion queue empty for file adds"), Engine->FileDeletionsDetectedForReload.Num(), 0)
		|| !TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should advance the timestamp after initialization"), Engine->LastFileChangeDetectedTime > 4321.0)
		|| !TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should normalize the queued script change against the current root path"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, AddedScriptAbsolutePath, TEXT("Scripts/Inside.as"))))
	{
		return false;
	}

	TSharedRef<FAngelscriptModuleDesc> RemovedFolderModule = MakeEditorModuleDesc(TEXT("Editor.Module.OnScriptFileChanges.RemovedFolder"));
	AddEditorModuleCodeSection(RemovedFolderModule, TEXT("RemovedFolder/RemovedA.as"), RemovedScriptAbsolutePath, TEXT("int RemovedFolderPrimary() { return 1; }"));
	AddEditorModuleCodeSection(RemovedFolderModule, TEXT("RemovedFolder/Nested/RemovedB.as"), NestedRemovedScriptAbsolutePath, TEXT("int RemovedFolderSecondary() { return 2; }"));

	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile;
	ModulesToCompile.Add(RemovedFolderModule);
	if (!CompileEditorModuleModules(Test, *Engine, ModulesToCompile))
	{
		return false;
	}

	Engine->FileChangesDetectedForReload.Reset();
	Engine->FileDeletionsDetectedForReload.Reset();
	Engine->LastFileChangeDetectedTime = 6789.0;
	const TArray<FFileChangeData> RemovedFolderChanges = { MakeFileChange(RemovedFolder, FFileChangeData::FCA_Removed) };
	if (!TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should replay the folder-remove callback after initialization"), DirectoryWatcher.BroadcastFirst(RemovedFolderChanges)))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should queue loaded scripts when a root folder is removed"), Engine->FileDeletionsDetectedForReload.Num(), 2)
		|| !TestEqual(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should not queue additions for folder removals"), Engine->FileChangesDetectedForReload.Num(), 0)
		|| !TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should advance the timestamp for folder removals"), Engine->LastFileChangeDetectedTime > 6789.0)
		|| !TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should use GatherLoadedScriptsForFolder for the direct removed script"), ContainsFilenamePair(Engine->FileDeletionsDetectedForReload, RemovedScriptAbsolutePath, TEXT("RemovedFolder/RemovedA.as")))
		|| !TestTrue(TEXT("Editor.Module.OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts should use GatherLoadedScriptsForFolder for nested removed scripts"), ContainsFilenamePair(Engine->FileDeletionsDetectedForReload, NestedRemovedScriptAbsolutePath, TEXT("RemovedFolder/Nested/RemovedB.as"))))
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
	FAngelscriptEditorModuleDirectoryWatcherTests,
	"Angelscript.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OnScriptFileChangesGuardsEngineInitAndQueuesRootScripts)
	{
		ASSERT_THAT(IsTrue(RunOnScriptFileChangesGuardsEngineInitAndQueuesRootScripts(*TestRunner)));
	}
};

#endif
