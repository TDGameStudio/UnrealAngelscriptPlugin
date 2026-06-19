#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
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

TEST_CLASS_WITH_FLAGS(FAngelscriptFileSystemRenameTest,
	"Angelscript.TestModule.FileSystem.RenameUpdatesModuleLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InPlaceRenameRemapsFilenameWithoutDiscard)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemRenameTests_Private;
		CleanFileSystemRenameTestRoot();

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
		ASSERT_THAT(IsTrue(WriteFileSystemRenameTestFile(TEXT("Game/AI/OldPatrol.as"), OldScript, OldAbsolutePath), TEXT("Write original patrol file should succeed")));

		ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), OldAbsolutePath, OldScript), TEXT("Compile original patrol module should succeed")));

	int32 ResultBeforeRename = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, OldAbsolutePath, TEXT("Game.AI.Patrol"), TEXT("int PatrolEntry()"), ResultBeforeRename), TEXT("Original patrol module should execute before rename")));
		ASSERT_THAT(AreEqual(7, ResultBeforeRename, TEXT("Original patrol module should return the initial value")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleByOldFilename = Engine.GetModuleByFilename(OldAbsolutePath);
		ASSERT_THAT(IsTrue(ModuleByOldFilename.IsValid(), TEXT("Original filename lookup should resolve before rename")));

		const FString NewAbsolutePath = FPaths::Combine(GetFileSystemRenameTestRoot(), TEXT("Game/AI/NewPatrol.as"));
		ASSERT_THAT(IsTrue(IFileManager::Get().Move(*NewAbsolutePath, *OldAbsolutePath, true, true), TEXT("Move original patrol file to renamed path should succeed")));

		FString RewrittenAbsolutePath;
		ASSERT_THAT(IsTrue(WriteFileSystemRenameTestFile(TEXT("Game/AI/NewPatrol.as"), RenamedScript, RewrittenAbsolutePath), TEXT("Rewrite renamed patrol file should succeed")));

		ASSERT_THAT(AreEqual(NewAbsolutePath, RewrittenAbsolutePath, TEXT("Rewrite helper should target the renamed absolute path")));

		ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), NewAbsolutePath, RenamedScript), TEXT("Compile renamed patrol module without discard should succeed")));

	TSharedPtr<FAngelscriptModuleDesc> ModuleByNewFilename = Engine.GetModuleByFilename(NewAbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(NewAbsolutePath, TEXT("Game.AI.Patrol"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleByName = Engine.GetModule(TEXT("Game.AI.Patrol"));

		ASSERT_THAT(IsTrue(ModuleByNewFilename.IsValid(), TEXT("Renamed filename lookup should resolve the active patrol module")));
		ASSERT_THAT(IsTrue(ModuleByEither.IsValid(), TEXT("Filename-or-module lookup should resolve the active patrol module after rename")));
		ASSERT_THAT(IsTrue(ModuleByName.IsValid(), TEXT("Module-name lookup should keep the patrol module alive after rename")));
		ASSERT_THAT(IsTrue(!Engine.GetModuleByFilename(OldAbsolutePath).IsValid(), TEXT("Old filename lookup should stop resolving after in-place rename remap")));

		ASSERT_THAT(IsTrue(ModuleByNewFilename == ModuleByEither, TEXT("Renamed filename lookup and filename-or-module lookup should resolve the same module")));
		ASSERT_THAT(IsTrue(ModuleByNewFilename == ModuleByName, TEXT("Renamed filename lookup and module-name lookup should resolve the same module")));
		ASSERT_THAT(AreEqual(FString(TEXT("Game.AI.Patrol")), ModuleByNewFilename->ModuleName, TEXT("Renamed module should preserve the requested module name")));
		ASSERT_THAT(IsTrue(ModuleByNewFilename->Code.Num() > 0, TEXT("Renamed module should keep at least one code section")));
		ASSERT_THAT(AreEqual(NewAbsolutePath, ModuleByNewFilename->Code[0].AbsoluteFilename, TEXT("Renamed module should remap its first code section to the new absolute filename")));

		int32 ResultAfterRename = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, NewAbsolutePath, TEXT("Game.AI.Patrol"), TEXT("int PatrolEntry()"), ResultAfterRename), TEXT("Renamed patrol module should execute after in-place remap")));

		ASSERT_THAT(AreEqual(13, ResultAfterRename, TEXT("Renamed patrol module should execute the updated source after the filename remap")));
		}
	}
};

#endif
