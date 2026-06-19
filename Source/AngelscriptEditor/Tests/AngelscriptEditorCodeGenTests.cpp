// =============================================================================
// AngelscriptEditorCodeGenTests.cpp
//
// Tests for AngelscriptEditorCodeGen.cpp — GetIncludeForModule() path
// stripping and GenerateBuildFile() output structure.
//
// Automation IDs:
//   Angelscript.Editor.CodeGen.*
// =============================================================================

#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)

// ---------------------------------------------------------------------------
// GetInclude.PublicPath
//   Header path containing "Public/" should strip everything up to and
//   including "Public/" and produce a proper #include directive.
// ---------------------------------------------------------------------------

static bool RunGetIncludePublicPath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("C:/UnrealEngine/Engine/Source/Runtime/Engine/Public/GameFramework/Actor.h");
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should start with #include"),
		Result.StartsWith(TEXT("#include")));
	TestTrue(TEXT("Result should contain the relative path after Public/"),
		Result.Contains(TEXT("GameFramework/Actor.h")));
	TestFalse(TEXT("Result should not contain the full absolute path prefix"),
		Result.Contains(TEXT("C:/UnrealEngine")));

	return true;
}

// ---------------------------------------------------------------------------
// GetInclude.PrivatePath
//   Header path containing "Private/" should strip the Private/ prefix.
// ---------------------------------------------------------------------------

static bool RunGetIncludePrivatePath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("C:/Project/Plugins/MyPlugin/Source/MyPlugin/Private/Internal/Helper.h");
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should contain #include"), Result.Contains(TEXT("#include")));
	TestTrue(TEXT("Result should contain relative path after Private/"),
		Result.Contains(TEXT("Internal/Helper.h")));

	return true;
}

// ---------------------------------------------------------------------------
// GetInclude.ClassesPath
//   Header path containing "Classes/" should strip the Classes/ prefix.
// ---------------------------------------------------------------------------

static bool RunGetIncludeClassesPath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("C:/Engine/Source/Runtime/CoreUObject/Classes/Object.h");
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should contain #include"), Result.Contains(TEXT("#include")));
	TestTrue(TEXT("Result should contain relative path after Classes/"),
		Result.Contains(TEXT("Object.h")));

	return true;
}

// ---------------------------------------------------------------------------
// GetInclude.EmptyPath
//   Empty header path should not crash and should produce a minimal include.
// ---------------------------------------------------------------------------

static bool RunGetIncludeEmptyPath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("");
	// GetIncludeForModule with null UField and empty path — should not crash.
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should still contain #include directive"),
		Result.Contains(TEXT("#include")));

	return true;
}

// ---------------------------------------------------------------------------
// GenerateBuildFile.OutputStructure
//   Generated build file lines should contain expected C# structure.
// ---------------------------------------------------------------------------

static bool RunGenerateBuildFileOutputStructure(FAutomationTestBase& Test)
{
	TArray<FString> PublicDeps = { TEXT("Core"), TEXT("CoreUObject"), TEXT("Engine") };
	TArray<FString> PrivateDeps = { TEXT("AngelscriptRuntime") };
	TArray<FString> OutBuildFile;

	FAngelscriptEditorModule::GenerateBuildFile(
		TEXT("TestModule"), PublicDeps, PrivateDeps, OutBuildFile, false);

	// Join all lines for easier search
	FString Joined;
	for (const FString& Line : OutBuildFile)
	{
		Joined += Line + TEXT("\n");
	}

	TestTrue(TEXT("Build file should contain 'using UnrealBuildTool'"),
		Joined.Contains(TEXT("using UnrealBuildTool")));
	TestTrue(TEXT("Build file should contain the module class name"),
		Joined.Contains(TEXT("TestModule")));
	TestTrue(TEXT("Build file should contain 'ModuleRules'"),
		Joined.Contains(TEXT("ModuleRules")));
	TestTrue(TEXT("Build file should contain 'Core' in public dependencies"),
		Joined.Contains(TEXT("\"Core\"")));
	TestTrue(TEXT("Build file should contain 'Engine' in public dependencies"),
		Joined.Contains(TEXT("\"Engine\"")));
	TestTrue(TEXT("Build file should contain 'AngelscriptRuntime' in private dependencies"),
		Joined.Contains(TEXT("AngelscriptRuntime")));
	TestTrue(TEXT("Build file should contain PCHUsage"),
		Joined.Contains(TEXT("PCHUsage")));

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorCodeGenGetIncludeTests,
	"Angelscript.Editor.CodeGen.GetInclude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PublicPath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludePublicPath(*TestRunner)));
	}

	TEST_METHOD(PrivatePath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludePrivatePath(*TestRunner)));
	}

	TEST_METHOD(ClassesPath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludeClassesPath(*TestRunner)));
	}

	TEST_METHOD(EmptyPath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludeEmptyPath(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorCodeGenGenerateBuildFileTests,
	"Angelscript.Editor.CodeGen.GenerateBuildFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OutputStructure)
	{
		ASSERT_THAT(IsTrue(RunGenerateBuildFileOutputStructure(*TestRunner)));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
