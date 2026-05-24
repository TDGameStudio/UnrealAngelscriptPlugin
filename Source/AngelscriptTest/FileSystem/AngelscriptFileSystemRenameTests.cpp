#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_FileSystem_AngelscriptFileSystemRenameTests_Private
{
	FString GetFileSystemRenameTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("FileSystemRename"));
	}

	void CleanFileSystemRenameTestRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetFileSystemRenameTestRoot(), false, true);
	}

	bool WriteFileSystemRenameTestFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = FPaths::Combine(GetFileSystemRenameTestRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFileSystemRenameWithoutDiscardTest,
	"Angelscript.TestModule.FileSystem.RenameUpdatesModuleLookup.InPlaceRenameRemapsFilenameWithoutDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFileSystemRenameWithoutDiscardTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemRenameTests_Private;
	CleanFileSystemRenameTestRoot();

	bool bResult = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("Game.AI.Patrol"));
		ASTEST_RESET_ENGINE(Engine);
		CleanFileSystemRenameTestRoot();
	};

	const FString OldScript = TEXT(R"AS(
int PatrolEntry()
{
	return 7;
}
)AS");
	const FString RenamedScript = TEXT(R"AS(
int PatrolEntry()
{
	return 13;
}
)AS");

	FString OldAbsolutePath;
	if (!TestTrue(
		TEXT("Write original patrol file should succeed"),
		WriteFileSystemRenameTestFile(TEXT("Game/AI/OldPatrol.as"), OldScript, OldAbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Compile original patrol module should succeed"),
		CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), OldAbsolutePath, OldScript)))
	{
		return false;
	}

	int32 ResultBeforeRename = 0;
	if (!TestTrue(
		TEXT("Original patrol module should execute before rename"),
		ExecuteIntFunction(&Engine, OldAbsolutePath, TEXT("Game.AI.Patrol"), TEXT("int PatrolEntry()"), ResultBeforeRename)))
	{
		return false;
	}
	TestEqual(TEXT("Original patrol module should return the initial value"), ResultBeforeRename, 7);

	TSharedPtr<FAngelscriptModuleDesc> ModuleByOldFilename = Engine.GetModuleByFilename(OldAbsolutePath);
	if (!TestTrue(
		TEXT("Original filename lookup should resolve before rename"),
		ModuleByOldFilename.IsValid()))
	{
		return false;
	}

	const FString NewAbsolutePath = FPaths::Combine(GetFileSystemRenameTestRoot(), TEXT("Game/AI/NewPatrol.as"));
	if (!TestTrue(
		TEXT("Move original patrol file to renamed path should succeed"),
		IFileManager::Get().Move(*NewAbsolutePath, *OldAbsolutePath, true, true)))
	{
		return false;
	}

	FString RewrittenAbsolutePath;
	if (!TestTrue(
		TEXT("Rewrite renamed patrol file should succeed"),
		WriteFileSystemRenameTestFile(TEXT("Game/AI/NewPatrol.as"), RenamedScript, RewrittenAbsolutePath)))
	{
		return false;
	}

	if (!TestEqual(
		TEXT("Rewrite helper should target the renamed absolute path"),
		RewrittenAbsolutePath,
		NewAbsolutePath))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Compile renamed patrol module without discard should succeed"),
		CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), NewAbsolutePath, RenamedScript)))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleByNewFilename = Engine.GetModuleByFilename(NewAbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(NewAbsolutePath, TEXT("Game.AI.Patrol"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleByName = Engine.GetModule(TEXT("Game.AI.Patrol"));

	bResult = TestTrue(
		TEXT("Renamed filename lookup should resolve the active patrol module"),
		ModuleByNewFilename.IsValid());
	bResult &= TestTrue(
		TEXT("Filename-or-module lookup should resolve the active patrol module after rename"),
		ModuleByEither.IsValid());
	bResult &= TestTrue(
		TEXT("Module-name lookup should keep the patrol module alive after rename"),
		ModuleByName.IsValid());
	bResult &= TestTrue(
		TEXT("Old filename lookup should stop resolving after in-place rename remap"),
		!Engine.GetModuleByFilename(OldAbsolutePath).IsValid());

	if (!ModuleByNewFilename.IsValid() || !ModuleByEither.IsValid() || !ModuleByName.IsValid())
	{
		return false;
	}

	bResult &= TestTrue(
		TEXT("Renamed filename lookup and filename-or-module lookup should resolve the same module"),
		ModuleByNewFilename == ModuleByEither);
	bResult &= TestTrue(
		TEXT("Renamed filename lookup and module-name lookup should resolve the same module"),
		ModuleByNewFilename == ModuleByName);
	bResult &= TestEqual(
		TEXT("Renamed module should preserve the requested module name"),
		ModuleByNewFilename->ModuleName,
		FString(TEXT("Game.AI.Patrol")));
	bResult &= TestTrue(
		TEXT("Renamed module should keep at least one code section"),
		ModuleByNewFilename->Code.Num() > 0);
	if (ModuleByNewFilename->Code.Num() > 0)
	{
		bResult &= TestEqual(
			TEXT("Renamed module should remap its first code section to the new absolute filename"),
			ModuleByNewFilename->Code[0].AbsoluteFilename,
			NewAbsolutePath);
	}

	int32 ResultAfterRename = 0;
	if (!TestTrue(
		TEXT("Renamed patrol module should execute after in-place remap"),
		ExecuteIntFunction(&Engine, NewAbsolutePath, TEXT("Game.AI.Patrol"), TEXT("int PatrolEntry()"), ResultAfterRename)))
	{
		return false;
	}

	bResult &= TestEqual(
		TEXT("Renamed patrol module should execute the updated source after the filename remap"),
		ResultAfterRename,
		13);
	}
	return bResult;
}

#endif
