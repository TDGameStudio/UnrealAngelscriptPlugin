#include "HotReload/AngelscriptDirectoryWatcherInternal.h"

#include "AngelscriptEngine.h"

#include "HAL/FileManager.h"
#include "IDirectoryWatcher.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherScriptQueueTest,
	"Angelscript.Editor.DirectoryWatcher.Queue.ScriptAddAndRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherNonScriptIgnoreTest,
	"Angelscript.Editor.DirectoryWatcher.Queue.IgnoresNonScriptFiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherModifiedScriptQueueTest,
	"Angelscript.TestModule.Editor.DirectoryWatcher.Queue.ModifiedScriptQueuesReloadWithoutDeletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherFolderAddTest,
	"Angelscript.Editor.DirectoryWatcher.Queue.FolderAddScansContainedScripts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherFolderRemoveTest,
	"Angelscript.Editor.DirectoryWatcher.Queue.FolderRemoveUsesLoadedScriptEnumerator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherRenameWindowTest,
	"Angelscript.Editor.DirectoryWatcher.Queue.RenameWindowTracksRemoveAndAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherOutsideRootTimestampTest,
	"Angelscript.Editor.DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDirectoryWatcherGatherLoadedScriptsForFolderTest,
	"Angelscript.Editor.DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private
{
	struct FResolvedDirectoryWatcherCompileEngine
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		TUniquePtr<FAngelscriptEngineScope> Scope;
		FAngelscriptEngine* Engine = nullptr;

		FAngelscriptEngine& Get() const
		{
			check(Engine != nullptr);
			return *Engine;
		}
	};

	FString MakeTempWatcherRoot(const TCHAR* Prefix)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("DirectoryWatcherTests") / FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	FFileChangeData MakeFileChange(const FString& Filename, FFileChangeData::EFileChangeAction Action)
	{
		return FFileChangeData(Filename, Action);
	}

	TUniquePtr<FAngelscriptEngine> MakeTestEngineWithRoot(const FString& RootPath)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = MakeUnique<FAngelscriptEngine>(Config, Dependencies);
		Engine->AllRootPaths = { FPaths::ConvertRelativePathToFull(RootPath) };
		return Engine;
	}

	bool AcquireDirectoryWatcherCompileEngine(FAutomationTestBase& Test, FResolvedDirectoryWatcherCompileEngine& OutResolved)
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		OutResolved.OwnedEngine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should acquire an Angelscript engine"), OutResolved.OwnedEngine.Get()))
		{
			return false;
		}

		OutResolved.Engine = OutResolved.OwnedEngine.Get();
		OutResolved.Scope = MakeUnique<FAngelscriptEngineScope>(*OutResolved.Engine);
		return true;
	}

	TSharedRef<FAngelscriptModuleDesc> MakeDirectoryWatcherModuleDesc(const FString& ModuleName)
	{
		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
		ModuleDesc->ModuleName = ModuleName;
		return ModuleDesc;
	}

	void AddDirectoryWatcherCodeSection(
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

	bool CompileDirectoryWatcherModules(
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
		if (!Test.TestTrue(TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should compile the synthetic loaded modules"), bCompileHandled))
		{
			return false;
		}

		return Test.TestEqual(
			TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should track each synthetic module"),
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


bool FAngelscriptDirectoryWatcherScriptQueueTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("ScriptQueue"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*(RootPath / TEXT("Scripts")), true);
	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);

	const FString AddedAbsolutePath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("Scripts/Added.as"));
	const FString RemovedAbsolutePath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("Scripts/Removed.as"));
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(AddedAbsolutePath, FFileChangeData::FCA_Added),
		MakeFileChange(RemovedAbsolutePath, FFileChangeData::FCA_Removed)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [](const FString&)
	{
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	TestEqual(TEXT("DirectoryWatcher.Queue.ScriptAddAndRemove should queue one added script"), Engine->FileChangesDetectedForReload.Num(), 1);
	TestEqual(TEXT("DirectoryWatcher.Queue.ScriptAddAndRemove should queue one removed script"), Engine->FileDeletionsDetectedForReload.Num(), 1);
	TestTrue(TEXT("DirectoryWatcher.Queue.ScriptAddAndRemove should store the added script pair"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, AddedAbsolutePath, TEXT("Scripts/Added.as")));
	return TestTrue(TEXT("DirectoryWatcher.Queue.ScriptAddAndRemove should store the removed script pair"), ContainsFilenamePair(Engine->FileDeletionsDetectedForReload, RemovedAbsolutePath, TEXT("Scripts/Removed.as")));
}

bool FAngelscriptDirectoryWatcherNonScriptIgnoreTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("NonScriptIgnore"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*(RootPath / TEXT("Misc")), true);
	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);

	const FString TextFilePath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("Misc/Notes.txt"));
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(TextFilePath, FFileChangeData::FCA_Modified)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [](const FString&)
	{
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresNonScriptFiles should not queue added scripts"), Engine->FileChangesDetectedForReload.Num(), 0);
	return TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresNonScriptFiles should not queue removed scripts"), Engine->FileDeletionsDetectedForReload.Num(), 0);
}

bool FAngelscriptDirectoryWatcherModifiedScriptQueueTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("ModifiedScriptQueue"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	const FString ScriptsPath = RootPath / TEXT("Scripts");
	FileManager.MakeDirectory(*ScriptsPath, true);
	const FString ModifiedAbsolutePath = FPaths::ConvertRelativePathToFull(ScriptsPath / TEXT("Modified.as"));
	FFileHelper::SaveStringToFile(TEXT("// Modified"), *ModifiedAbsolutePath);

	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);
	Engine->LastFileChangeDetectedTime = 1234.0;

	int32 EnumerateCallCount = 0;
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(ModifiedAbsolutePath, FFileChangeData::FCA_Modified)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [&](const FString&)
	{
		++EnumerateCallCount;
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	if (!TestEqual(TEXT("DirectoryWatcher.Queue.ModifiedScriptQueuesReloadWithoutDeletion should queue exactly one modified script"), Engine->FileChangesDetectedForReload.Num(), 1)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.ModifiedScriptQueuesReloadWithoutDeletion should not queue deletions for a modified script"), Engine->FileDeletionsDetectedForReload.Num(), 0)
		|| !TestTrue(TEXT("DirectoryWatcher.Queue.ModifiedScriptQueuesReloadWithoutDeletion should advance the timestamp for modified script files"), Engine->LastFileChangeDetectedTime > 1234.0)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.ModifiedScriptQueuesReloadWithoutDeletion should not enumerate loaded scripts for file-level modified events"), EnumerateCallCount, 0))
	{
		return false;
	}

	return TestTrue(TEXT("DirectoryWatcher.Queue.ModifiedScriptQueuesReloadWithoutDeletion should store the modified script pair"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, ModifiedAbsolutePath, TEXT("Scripts/Modified.as")));
}

bool FAngelscriptDirectoryWatcherFolderAddTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("FolderAdd"));
	const FString AddedFolderPath = RootPath / TEXT("AddedFolder");
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*(AddedFolderPath / TEXT("Nested")), true);
	FFileHelper::SaveStringToFile(TEXT("// A"), *(AddedFolderPath / TEXT("A.as")));
	FFileHelper::SaveStringToFile(TEXT("ignore"), *(AddedFolderPath / TEXT("B.txt")));
	FFileHelper::SaveStringToFile(TEXT("// C"), *(AddedFolderPath / TEXT("Nested/C.as")));

	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(FPaths::ConvertRelativePathToFull(AddedFolderPath), FFileChangeData::FCA_Added)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [](const FString&)
	{
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	TestEqual(TEXT("DirectoryWatcher.Queue.FolderAddScansContainedScripts should queue the two script files in the new folder"), Engine->FileChangesDetectedForReload.Num(), 2);
	TestTrue(TEXT("DirectoryWatcher.Queue.FolderAddScansContainedScripts should queue AddedFolder/A.as"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, FPaths::ConvertRelativePathToFull(AddedFolderPath / TEXT("A.as")), TEXT("AddedFolder/A.as")));
	return TestTrue(TEXT("DirectoryWatcher.Queue.FolderAddScansContainedScripts should queue AddedFolder/Nested/C.as"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, FPaths::ConvertRelativePathToFull(AddedFolderPath / TEXT("Nested/C.as")), TEXT("AddedFolder/Nested/C.as")));
}

bool FAngelscriptDirectoryWatcherFolderRemoveTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("FolderRemove"));
	const FString RemovedFolderPath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("RemovedFolder"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*RootPath, true);
	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);

	const FString RemovedScriptAbsolutePath = RemovedFolderPath / TEXT("RemovedA.as");
	const FString NestedRemovedScriptAbsolutePath = RemovedFolderPath / TEXT("Nested/RemovedB.as");
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(RemovedFolderPath, FFileChangeData::FCA_Removed)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [&](const FString& AbsoluteFolderPath)
	{
		TestEqual(TEXT("DirectoryWatcher.Queue.FolderRemoveUsesLoadedScriptEnumerator should request the removed folder path with a trailing separator"), AbsoluteFolderPath, RemovedFolderPath / TEXT(""));
		return TArray<FAngelscriptEngine::FFilenamePair>{
			{ RemovedScriptAbsolutePath, TEXT("RemovedFolder/RemovedA.as") },
			{ NestedRemovedScriptAbsolutePath, TEXT("RemovedFolder/Nested/RemovedB.as") },
		};
	});

	TestEqual(TEXT("DirectoryWatcher.Queue.FolderRemoveUsesLoadedScriptEnumerator should queue two removed scripts from the enumerator"), Engine->FileDeletionsDetectedForReload.Num(), 2);
	TestTrue(TEXT("DirectoryWatcher.Queue.FolderRemoveUsesLoadedScriptEnumerator should queue the direct removed script"), ContainsFilenamePair(Engine->FileDeletionsDetectedForReload, RemovedScriptAbsolutePath, TEXT("RemovedFolder/RemovedA.as")));
	return TestTrue(TEXT("DirectoryWatcher.Queue.FolderRemoveUsesLoadedScriptEnumerator should queue the nested removed script"), ContainsFilenamePair(Engine->FileDeletionsDetectedForReload, NestedRemovedScriptAbsolutePath, TEXT("RemovedFolder/Nested/RemovedB.as")));
}

bool FAngelscriptDirectoryWatcherRenameWindowTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("RenameWindow"));
	const FString RenameFolderPath = RootPath / TEXT("Rename");
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*RenameFolderPath, true);
	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);

	const FString OldAbsolutePath = FPaths::ConvertRelativePathToFull(RenameFolderPath / TEXT("OldName.as"));
	const FString NewAbsolutePath = FPaths::ConvertRelativePathToFull(RenameFolderPath / TEXT("NewName.as"));
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(OldAbsolutePath, FFileChangeData::FCA_Removed),
		MakeFileChange(NewAbsolutePath, FFileChangeData::FCA_Added)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [](const FString&)
	{
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	TestEqual(TEXT("DirectoryWatcher.Queue.RenameWindowTracksRemoveAndAdd should queue one removed script"), Engine->FileDeletionsDetectedForReload.Num(), 1);
	TestEqual(TEXT("DirectoryWatcher.Queue.RenameWindowTracksRemoveAndAdd should queue one added script"), Engine->FileChangesDetectedForReload.Num(), 1);
	TestTrue(TEXT("DirectoryWatcher.Queue.RenameWindowTracksRemoveAndAdd should retain the old filename in the deletion queue"), ContainsFilenamePair(Engine->FileDeletionsDetectedForReload, OldAbsolutePath, TEXT("Rename/OldName.as")));
	return TestTrue(TEXT("DirectoryWatcher.Queue.RenameWindowTracksRemoveAndAdd should retain the new filename in the addition queue"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, NewAbsolutePath, TEXT("Rename/NewName.as")));
}

bool FAngelscriptDirectoryWatcherOutsideRootTimestampTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("OutsideRootTimestamp"));
	const FString OutsideRootPath = RootPath + TEXT("_Outside");
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
		FileManager.DeleteDirectory(*OutsideRootPath, false, true);
	};

	FileManager.MakeDirectory(*(RootPath / TEXT("Scripts")), true);
	FileManager.MakeDirectory(*(OutsideRootPath / TEXT("Scripts")), true);

	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoot(RootPath);
	Engine->LastFileChangeDetectedTime = 1234.0;

	int32 EnumerateCallCount = 0;
	const FString OutsideScriptAbsolutePath = FPaths::ConvertRelativePathToFull(OutsideRootPath / TEXT("Scripts/Sneaky.as"));
	const TArray<FFileChangeData> OutsideChanges = {
		MakeFileChange(OutsideScriptAbsolutePath, FFileChangeData::FCA_Modified)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(OutsideChanges, Engine->AllRootPaths, *Engine, FileManager, [&](const FString&)
	{
		++EnumerateCallCount;
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	if (!TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should ignore sibling paths that only share the root prefix"), Engine->FileChangesDetectedForReload.Num(), 0)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should not queue deletions for sibling paths that only share the root prefix"), Engine->FileDeletionsDetectedForReload.Num(), 0)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should preserve the timestamp for outside-root changes"), Engine->LastFileChangeDetectedTime, 1234.0)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should not enumerate loaded scripts for outside-root file changes"), EnumerateCallCount, 0))
	{
		return false;
	}

	const double TimestampBeforeInsideChange = Engine->LastFileChangeDetectedTime;
	const FString InsideScriptAbsolutePath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("Scripts/Inside.as"));
	const TArray<FFileChangeData> InsideChanges = {
		MakeFileChange(InsideScriptAbsolutePath, FFileChangeData::FCA_Modified)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(InsideChanges, Engine->AllRootPaths, *Engine, FileManager, [&](const FString&)
	{
		++EnumerateCallCount;
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	if (!TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should queue exactly one in-root script change"), Engine->FileChangesDetectedForReload.Num(), 1)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should still have no deletions after the in-root file change"), Engine->FileDeletionsDetectedForReload.Num(), 0)
		|| !TestTrue(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should advance the timestamp for in-root file changes"), Engine->LastFileChangeDetectedTime > TimestampBeforeInsideChange)
		|| !TestEqual(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should still not enumerate loaded scripts for file-level changes"), EnumerateCallCount, 0))
	{
		return false;
	}

	return TestTrue(TEXT("DirectoryWatcher.Queue.IgnoresOutsideRootsWithoutTimestampMutation should queue the in-root script with the normalized relative path"), ContainsFilenamePair(Engine->FileChangesDetectedForReload, InsideScriptAbsolutePath, TEXT("Scripts/Inside.as")));
}

bool FAngelscriptDirectoryWatcherGatherLoadedScriptsForFolderTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString RootPath = MakeTempWatcherRoot(TEXT("GatherLoadedScriptsForFolder"));
	const FString RemovedFolderPath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("RemovedFolder"));
	const FString RemovedSiblingFolderPath = FPaths::ConvertRelativePathToFull(RootPath / TEXT("RemovedFolderSibling"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*RootPath, false, true);
	};

	FileManager.MakeDirectory(*(RemovedFolderPath / TEXT("Nested")), true);
	FileManager.MakeDirectory(*RemovedSiblingFolderPath, true);

	FResolvedDirectoryWatcherCompileEngine ResolvedEngine;
	if (!AcquireDirectoryWatcherCompileEngine(*this, ResolvedEngine))
	{
		return false;
	}

	TSharedRef<FAngelscriptModuleDesc> RemovedFolderModule = MakeDirectoryWatcherModuleDesc(TEXT("DirectoryWatcher.RemovedFolder"));
	const FString PrimaryAbsolutePath = RemovedFolderPath / TEXT("Primary.as");
	const FString NestedAbsolutePath = RemovedFolderPath / TEXT("Nested/Secondary.as");
	AddDirectoryWatcherCodeSection(RemovedFolderModule, TEXT("RemovedFolder/Primary.as"), PrimaryAbsolutePath, TEXT("int RemovedFolderPrimary() { return 1; }"));
	AddDirectoryWatcherCodeSection(RemovedFolderModule, TEXT("RemovedFolder/Nested/Secondary.as"), NestedAbsolutePath, TEXT("int RemovedFolderSecondary() { return 2; }"));
	AddDirectoryWatcherCodeSection(RemovedFolderModule, TEXT("RemovedFolder/Primary.as"), PrimaryAbsolutePath, TEXT("// Duplicate code section metadata entry for the same script path."));

	TSharedRef<FAngelscriptModuleDesc> RemovedSiblingModule = MakeDirectoryWatcherModuleDesc(TEXT("DirectoryWatcher.RemovedFolderSibling"));
	const FString SiblingAbsolutePath = RemovedSiblingFolderPath / TEXT("Leak.as");
	AddDirectoryWatcherCodeSection(RemovedSiblingModule, TEXT("RemovedFolderSibling/Leak.as"), SiblingAbsolutePath, TEXT("int RemovedFolderSiblingLeak() { return 3; }"));

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = {
		RemovedFolderModule,
		RemovedSiblingModule
	};
	if (!CompileDirectoryWatcherModules(*this, ResolvedEngine.Get(), ModulesToCompile))
	{
		return false;
	}

	const TArray<FAngelscriptEngine::FFilenamePair> LoadedScripts = AngelscriptEditor::Private::GatherLoadedScriptsForFolder(ResolvedEngine.Get(), RemovedFolderPath / TEXT(""));
	if (!TestEqual(
		TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should return only the two unique scripts under the removed folder"),
		LoadedScripts.Num(),
		2))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should include the direct script under the removed folder exactly once"),
		ContainsFilenamePair(LoadedScripts, PrimaryAbsolutePath, TEXT("RemovedFolder/Primary.as"))))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should include nested scripts under the removed folder"),
		ContainsFilenamePair(LoadedScripts, NestedAbsolutePath, TEXT("RemovedFolder/Nested/Secondary.as"))))
	{
		return false;
	}

	return TestFalse(
		TEXT("DirectoryWatcher.GatherLoadedScriptsForFolder.DeduplicatesAndRejectsPrefixCollisions should reject sibling folders that only share the prefix"),
		ContainsFilenamePair(LoadedScripts, SiblingAbsolutePath, TEXT("RemovedFolderSibling/Leak.as")));
}

#endif
