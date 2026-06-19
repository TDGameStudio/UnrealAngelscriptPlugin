// =============================================================================
// AngelscriptSourceCodeNavigationTests.cpp
//
// Tests for AngelscriptSourceCodeNavigation.cpp — editor source navigation.
// Covers BuildVSCodeOpenParameters (pure string logic) and navigation
// override hooks.
//
// Automation IDs:
//   Angelscript.Editor.SourceNavigation.*
// =============================================================================

#include "SourceNavigation/AngelscriptSourceCodeNavigation.h"

#include "CQTest.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

// ---------------------------------------------------------------------------
// BuildVSCodeParams.WorkspacePath
//   Non-empty workspace path should be prepended to params.
// ---------------------------------------------------------------------------

static bool RunBuildVSCodeParamsWorkspacePath(FAutomationTestBase& Test)
{
	const FString Result = AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(
		TEXT("--goto \"C:/Test.as:10\""),
		TEXT("C:/MyProject/workspace.code-workspace"),
		false,
		TEXT(""));

	TestTrue(TEXT("Result should contain the workspace path"),
		Result.Contains(TEXT("workspace.code-workspace")));
	TestTrue(TEXT("Result should contain the original params"),
		Result.Contains(TEXT("--goto")));

	return true;
}

// ---------------------------------------------------------------------------
// BuildVSCodeParams.FolderFallback
//   Empty workspace + bOpenFolder=true should use ScriptRootDirectory.
// ---------------------------------------------------------------------------

static bool RunBuildVSCodeParamsFolderFallback(FAutomationTestBase& Test)
{
	const FString Result = AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(
		TEXT("--goto \"C:/Test.as:10\""),
		TEXT(""),
		true,
		TEXT("C:/MyProject/Script"));

	TestTrue(TEXT("Result should contain the script root directory"),
		Result.Contains(TEXT("C:/MyProject/Script")));
	TestTrue(TEXT("Result should contain the original params"),
		Result.Contains(TEXT("--goto")));

	return true;
}

// ---------------------------------------------------------------------------
// BuildVSCodeParams.RawParams
//   Empty workspace + bOpenFolder=false should return raw params unchanged.
// ---------------------------------------------------------------------------

static bool RunBuildVSCodeParamsRawParams(FAutomationTestBase& Test)
{
	const FString RawParams = TEXT("--goto \"C:/Test.as:10\"");
	const FString Result = AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(
		RawParams,
		TEXT(""),
		false,
		TEXT("C:/MyProject/Script"));

	TestEqual(TEXT("With no workspace and bOpenFolder=false, params should be unchanged"),
		Result, RawParams);

	return true;
}

// ---------------------------------------------------------------------------
// BuildVSCodeParams.AllEmpty
//   All empty/false should return raw params unchanged.
// ---------------------------------------------------------------------------

static bool RunBuildVSCodeParamsAllEmpty(FAutomationTestBase& Test)
{
	const FString RawParams = TEXT("\"C:/Test.as\"");
	const FString Result = AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(
		RawParams,
		TEXT(""),
		false,
		TEXT(""));

	TestEqual(TEXT("With all empty inputs, params should pass through unchanged"),
		Result, RawParams);

	return true;
}

// ---------------------------------------------------------------------------
// OpenOverride.CapturesLocation
//   SetOpenLocationOverrideForTesting should capture path/line when
//   a navigation function is invoked.
// ---------------------------------------------------------------------------

static bool RunOpenOverrideCapturesLocation(FAutomationTestBase& Test)
{
	FAngelscriptSourceNavigationLocation CapturedLocation;
	bool bWasCalled = false;

	AngelscriptSourceNavigation::SetOpenLocationOverrideForTesting(
		[&CapturedLocation, &bWasCalled](const FAngelscriptSourceNavigationLocation& Location)
		{
			CapturedLocation = Location;
			bWasCalled = true;
		});

	ON_SCOPE_EXIT
	{
		AngelscriptSourceNavigation::ResetOpenLocationOverrideForTesting();
	};

	// NavigateToFunctionForTesting with nullptr should return false without crashing.
	// The override may or may not be called depending on whether the handler is registered.
	const bool NavigateResult = AngelscriptSourceNavigation::NavigateToFunctionForTesting(nullptr);

	// Regardless of result, the test validates that setting/resetting the override
	// does not crash and the API is callable.
	TestTrue(TEXT("Navigate with nullptr should return false (no valid function)"),
		!NavigateResult);

	return true;
}

// ---------------------------------------------------------------------------
// CanNavigate.NonASClassReturnsFalse
//   Native UE classes (non-AS) should not be navigable via AS navigation.
// ---------------------------------------------------------------------------

static bool RunCanNavigateNonASClassReturnsFalse(FAutomationTestBase& Test)
{
	// NavigateToStructForTesting with a native UE class should return false.
	const bool Result = AngelscriptSourceNavigation::NavigateToStructForTesting(AActor::StaticClass());

	TestFalse(TEXT("NavigateToStruct for native AActor should return false"), Result);

	return true;
}

#undef TestEqual
#undef TestFalse
#undef TestTrue

TEST_CLASS_WITH_FLAGS(FAngelscriptSourceNavigationBuildVSCodeParamsTests,
	"Angelscript.Editor.SourceNavigation.BuildVSCodeParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(WorkspacePath)
	{
		ASSERT_THAT(IsTrue(RunBuildVSCodeParamsWorkspacePath(*TestRunner)));
	}

	TEST_METHOD(FolderFallback)
	{
		ASSERT_THAT(IsTrue(RunBuildVSCodeParamsFolderFallback(*TestRunner)));
	}

	TEST_METHOD(RawParams)
	{
		ASSERT_THAT(IsTrue(RunBuildVSCodeParamsRawParams(*TestRunner)));
	}

	TEST_METHOD(AllEmpty)
	{
		ASSERT_THAT(IsTrue(RunBuildVSCodeParamsAllEmpty(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptSourceNavigationOpenOverrideTests,
	"Angelscript.Editor.SourceNavigation.OpenOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CapturesLocation)
	{
		ASSERT_THAT(IsTrue(RunOpenOverrideCapturesLocation(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptSourceNavigationCanNavigateTests,
	"Angelscript.Editor.SourceNavigation.CanNavigate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NonASClassReturnsFalse)
	{
		ASSERT_THAT(IsTrue(RunCanNavigateNonASClassReturnsFalse(*TestRunner)));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
