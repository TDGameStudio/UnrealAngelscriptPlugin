#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_FileSystem_AngelscriptFileSystemLookupPrecedenceTests_Private
{
	FString GetFileSystemLookupPrecedenceTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("FileSystem") / TEXT("LookupPrecedence"));
	}

	void CleanFileSystemLookupPrecedenceTestRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetFileSystemLookupPrecedenceTestRoot(), false, true);
	}

	bool WriteFileSystemLookupPrecedenceTestFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = FPaths::Combine(GetFileSystemLookupPrecedenceTestRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(
			Content,
			*OutAbsolutePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleLookupFilenamePrecedenceTest,
	"Angelscript.TestModule.FileSystem.ModuleLookupByFilenameOrModuleName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PrefersFilenameOverMismatchedModuleName)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemLookupPrecedenceTests_Private;
		CleanFileSystemLookupPrecedenceTestRoot();

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleNameA(TEXT("Game.Lookup.A"));
		static const FName ModuleNameB(TEXT("Game.Lookup.B"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleNameA.ToString());
			Engine.DiscardModule(*ModuleNameB.ToString());
			CleanFileSystemLookupPrecedenceTestRoot();
		};

	const FString ScriptA = TEXT(R"AS(
int EntryA()
{
	return 1;
}
)AS");
	const FString ScriptB = TEXT(R"AS(
int EntryB()
{
	return 2;
}
)AS");

	FString AbsolutePathA;
	FString AbsolutePathB;
		ASSERT_THAT(IsTrue(WriteFileSystemLookupPrecedenceTestFile(TEXT("Lookup/A.as"), ScriptA, AbsolutePathA), TEXT("Write lookup-precedence script A should succeed")));
		ASSERT_THAT(IsTrue(WriteFileSystemLookupPrecedenceTestFile(TEXT("Lookup/B.as"), ScriptB, AbsolutePathB), TEXT("Write lookup-precedence script B should succeed")));

		ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, ModuleNameA, AbsolutePathA, ScriptA), TEXT("Compile lookup-precedence module A should succeed")));
		ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, ModuleNameB, AbsolutePathB, ScriptB), TEXT("Compile lookup-precedence module B should succeed")));

	TSharedPtr<FAngelscriptModuleDesc> ModuleFromFilenameA = Engine.GetModuleByFilenameOrModuleName(AbsolutePathA, ModuleNameB.ToString());
	TSharedPtr<FAngelscriptModuleDesc> ModuleFromFilenameB = Engine.GetModuleByFilenameOrModuleName(AbsolutePathB, ModuleNameA.ToString());
	const FString MissingAbsolutePath = FPaths::Combine(GetFileSystemLookupPrecedenceTestRoot(), TEXT("Lookup"), TEXT("Missing.as"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleFromFallback = Engine.GetModuleByFilenameOrModuleName(MissingAbsolutePath, ModuleNameB.ToString());

		ASSERT_THAT(IsTrue(ModuleFromFilenameA.IsValid(), TEXT("Conflicting lookup for path A should resolve a module")));
		ASSERT_THAT(IsTrue(ModuleFromFilenameB.IsValid(), TEXT("Conflicting lookup for path B should resolve a module")));
		ASSERT_THAT(IsFalse(Engine.GetModuleByFilename(MissingAbsolutePath).IsValid(), TEXT("Missing filename should not resolve through direct filename lookup")));
		ASSERT_THAT(IsTrue(ModuleFromFallback.IsValid(), TEXT("Missing filename should still fall back to module-name lookup")));

		ASSERT_THAT(AreEqual(ModuleNameA.ToString(), ModuleFromFilenameA->ModuleName, TEXT("Path A should win over mismatched module-name fallback")));
		ASSERT_THAT(AreEqual(ModuleNameB.ToString(), ModuleFromFilenameB->ModuleName, TEXT("Path B should win over mismatched module-name fallback")));
		ASSERT_THAT(AreEqual(ModuleNameB.ToString(), ModuleFromFallback->ModuleName, TEXT("Missing filename should fall back to module-name lookup")));

		ASSERT_THAT(IsTrue(ModuleFromFilenameA->Code.Num() > 0, TEXT("Resolved module A should expose at least one code section")));
		ASSERT_THAT(IsTrue(ModuleFromFilenameB->Code.Num() > 0, TEXT("Resolved module B should expose at least one code section")));
		ASSERT_THAT(IsTrue(ModuleFromFallback->Code.Num() > 0, TEXT("Fallback module should expose at least one code section")));

		ASSERT_THAT(AreEqual(AbsolutePathA, ModuleFromFilenameA->Code[0].AbsoluteFilename, TEXT("Path A lookup should preserve the exact absolute filename")));
		ASSERT_THAT(AreEqual(AbsolutePathB, ModuleFromFilenameB->Code[0].AbsoluteFilename, TEXT("Path B lookup should preserve the exact absolute filename")));
		ASSERT_THAT(AreEqual(AbsolutePathB, ModuleFromFallback->Code[0].AbsoluteFilename, TEXT("Fallback lookup should still resolve module B's compiled filename")));

		}
	}
};

#endif
