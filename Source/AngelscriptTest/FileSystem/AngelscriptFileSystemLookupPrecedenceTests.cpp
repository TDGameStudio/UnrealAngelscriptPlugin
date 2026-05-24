#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptModuleLookupFilenamePrecedenceTest,
	"Angelscript.TestModule.FileSystem.ModuleLookupByFilenameOrModuleName.PrefersFilenameOverMismatchedModuleName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptModuleLookupFilenamePrecedenceTest::RunTest(const FString& Parameters)
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
	if (!TestTrue(TEXT("Write lookup-precedence script A should succeed"), WriteFileSystemLookupPrecedenceTestFile(TEXT("Lookup/A.as"), ScriptA, AbsolutePathA))
		|| !TestTrue(TEXT("Write lookup-precedence script B should succeed"), WriteFileSystemLookupPrecedenceTestFile(TEXT("Lookup/B.as"), ScriptB, AbsolutePathB)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Compile lookup-precedence module A should succeed"), CompileModuleFromMemory(&Engine, ModuleNameA, AbsolutePathA, ScriptA))
		|| !TestTrue(TEXT("Compile lookup-precedence module B should succeed"), CompileModuleFromMemory(&Engine, ModuleNameB, AbsolutePathB, ScriptB)))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleFromFilenameA = Engine.GetModuleByFilenameOrModuleName(AbsolutePathA, ModuleNameB.ToString());
	TSharedPtr<FAngelscriptModuleDesc> ModuleFromFilenameB = Engine.GetModuleByFilenameOrModuleName(AbsolutePathB, ModuleNameA.ToString());
	const FString MissingAbsolutePath = FPaths::Combine(GetFileSystemLookupPrecedenceTestRoot(), TEXT("Lookup"), TEXT("Missing.as"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleFromFallback = Engine.GetModuleByFilenameOrModuleName(MissingAbsolutePath, ModuleNameB.ToString());

	if (!TestTrue(TEXT("Conflicting lookup for path A should resolve a module"), ModuleFromFilenameA.IsValid())
		|| !TestTrue(TEXT("Conflicting lookup for path B should resolve a module"), ModuleFromFilenameB.IsValid())
		|| !TestFalse(TEXT("Missing filename should not resolve through direct filename lookup"), Engine.GetModuleByFilename(MissingAbsolutePath).IsValid())
		|| !TestTrue(TEXT("Missing filename should still fall back to module-name lookup"), ModuleFromFallback.IsValid()))
	{
		return false;
	}

	TestEqual(TEXT("Path A should win over mismatched module-name fallback"), ModuleFromFilenameA->ModuleName, ModuleNameA.ToString());
	TestEqual(TEXT("Path B should win over mismatched module-name fallback"), ModuleFromFilenameB->ModuleName, ModuleNameB.ToString());
	TestEqual(TEXT("Missing filename should fall back to module-name lookup"), ModuleFromFallback->ModuleName, ModuleNameB.ToString());

	if (!TestTrue(TEXT("Resolved module A should expose at least one code section"), ModuleFromFilenameA->Code.Num() > 0)
		|| !TestTrue(TEXT("Resolved module B should expose at least one code section"), ModuleFromFilenameB->Code.Num() > 0)
		|| !TestTrue(TEXT("Fallback module should expose at least one code section"), ModuleFromFallback->Code.Num() > 0))
	{
		return false;
	}

	TestEqual(TEXT("Path A lookup should preserve the exact absolute filename"), ModuleFromFilenameA->Code[0].AbsoluteFilename, AbsolutePathA);
	TestEqual(TEXT("Path B lookup should preserve the exact absolute filename"), ModuleFromFilenameB->Code[0].AbsoluteFilename, AbsolutePathB);
	TestEqual(TEXT("Fallback lookup should still resolve module B's compiled filename"), ModuleFromFallback->Code[0].AbsoluteFilename, AbsolutePathB);

	}

	return true;
}

#endif
