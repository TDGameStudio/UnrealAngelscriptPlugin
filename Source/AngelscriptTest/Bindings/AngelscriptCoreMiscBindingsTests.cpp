// ============================================================================
// AngelscriptCoreMiscBindingsTests.cpp
//
// Core misc binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.CoreMisc.FAngelscriptCoreMiscBindingsTest.*
//
// Sections:
//   GuidCompat               — FGuid construction, parse, compare, invalidate
//   PathsCompat              — FPaths basic API round-trip
//   PathsExactCompat         — FPaths deterministic results vs native baselines
//   NumberFormattingOptions   — FNumberFormattingOptions builder and identity
//
// CQTest adaptation notes:
//   Four IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   GuidCompat, PathsCompat, NumberFormattingOptions: bitmask int Entry() split
//   into per-aspect functions. PathsExactCompat: token-replacement pattern
//   retained with single Entry() invoked via ExpectGlobalInt.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptCoreMiscBindingsTest,
	"Angelscript.TestModule.Bindings.CoreMisc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: GuidCompat
	// ====================================================================

	TEST_METHOD(GuidCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCoreMisc_GuidCompat"), TEXT(R"(
int ExplicitGuidIsValid()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	return ExplicitGuid.IsValid() ? 1 : 0;
}

int GuidToStringNotEmpty()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	FString GuidString = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
	return (!GuidString.IsEmpty()) ? 1 : 0;
}

int GuidParseRoundTrip()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	FString GuidString = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
	FGuid ParsedGuid;
	if (!FGuid::Parse(GuidString, ParsedGuid))
		return 0;
	return (ParsedGuid == ExplicitGuid) ? 1 : 0;
}

int GuidOpCmpEqual()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	FString GuidString = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
	FGuid ParsedGuid;
	FGuid::Parse(GuidString, ParsedGuid);
	return (ParsedGuid.opCmp(ExplicitGuid) == 0) ? 1 : 0;
}

int GuidParseExactRoundTrip()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	FString GuidString = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
	FGuid ParsedExactGuid;
	if (!FGuid::ParseExact(GuidString, EGuidFormats::DigitsWithHyphens, ParsedExactGuid))
		return 0;
	return (ParsedExactGuid == ExplicitGuid) ? 1 : 0;
}

int GuidCopyEquality()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	FGuid Copy = ExplicitGuid;
	return (Copy == ExplicitGuid) ? 1 : 0;
}

int GuidInvalidateWorks()
{
	FGuid ExplicitGuid(1, 2, 3, 4);
	FGuid Copy = ExplicitGuid;
	Copy.Invalidate();
	return (!Copy.IsValid()) ? 1 : 0;
}

int GuidNewGuidIsValid()
{
	FGuid NewGuid = FGuid::NewGuid();
	return NewGuid.IsValid() ? 1 : 0;
}

int GuidGetTypeHashNonZero()
{
	FGuid NewGuid = FGuid::NewGuid();
	return (NewGuid.GetTypeHash() != 0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ExplicitGuidIsValid()"), TEXT("Explicit FGuid should be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidToStringNotEmpty()"), TEXT("FGuid ToString should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidParseRoundTrip()"), TEXT("FGuid Parse round-trip should match"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidOpCmpEqual()"), TEXT("FGuid opCmp should return 0 for equal guids"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidParseExactRoundTrip()"), TEXT("FGuid ParseExact round-trip should match"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidCopyEquality()"), TEXT("FGuid copy should equal original"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidInvalidateWorks()"), TEXT("Invalidated FGuid should not be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidNewGuidIsValid()"), TEXT("NewGuid should be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GuidGetTypeHashNonZero()"), TEXT("NewGuid type hash should be non-zero"), 1);
	}

	// ====================================================================
	// Section: PathsCompat
	// ====================================================================

	TEST_METHOD(PathsCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCoreMisc_PathsCompat"), TEXT(R"(
int ProjectDirNotEmpty()
{
	return (!FPaths::ProjectDir().IsEmpty()) ? 1 : 0;
}

int CombinePathsNotEmpty()
{
	FString Combined = FPaths::CombinePaths(FPaths::ProjectDir(), "Script/Test.as");
	return (!Combined.IsEmpty()) ? 1 : 0;
}

int IsRelativeWorks()
{
	return FPaths::IsRelative("Script/Test.as") ? 1 : 0;
}

int ConvertRelativeToFullNotEmpty()
{
	return (!FPaths::ConvertRelativePathToFull("Script/Test.as").IsEmpty()) ? 1 : 0;
}

int ConvertRelativeToFullFromBaseNotEmpty()
{
	return (!FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), "Script/Test.as").IsEmpty()) ? 1 : 0;
}

int GetExtensionCorrect()
{
	FString Combined = FPaths::CombinePaths(FPaths::ProjectDir(), "Script/Test.as");
	return (FPaths::GetExtension(Combined, true) == ".as") ? 1 : 0;
}

int GetCleanFilenameCorrect()
{
	FString Combined = FPaths::CombinePaths(FPaths::ProjectDir(), "Script/Test.as");
	return (FPaths::GetCleanFilename(Combined) == "Test.as") ? 1 : 0;
}

int GetBaseFilenameCorrect()
{
	FString Combined = FPaths::CombinePaths(FPaths::ProjectDir(), "Script/Test.as");
	return (FPaths::GetBaseFilename(Combined, true) == "Test") ? 1 : 0;
}

int GetPathNotEmpty()
{
	FString Combined = FPaths::CombinePaths(FPaths::ProjectDir(), "Script/Test.as");
	return (!FPaths::GetPath(Combined).IsEmpty()) ? 1 : 0;
}

int DirectoryExistsForProjectDir()
{
	return FPaths::DirectoryExists(FPaths::ProjectDir()) ? 1 : 0;
}

int FileExistsForProjectDirIsFalse()
{
	return (!FPaths::FileExists(FPaths::ProjectDir())) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ProjectDirNotEmpty()"), TEXT("ProjectDir should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CombinePathsNotEmpty()"), TEXT("CombinePaths should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IsRelativeWorks()"), TEXT("Relative path should be detected"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ConvertRelativeToFullNotEmpty()"), TEXT("ConvertRelativePathToFull should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ConvertRelativeToFullFromBaseNotEmpty()"), TEXT("ConvertRelativePathToFull from base should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetExtensionCorrect()"), TEXT("GetExtension should return .as"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetCleanFilenameCorrect()"), TEXT("GetCleanFilename should return Test.as"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetBaseFilenameCorrect()"), TEXT("GetBaseFilename should return Test"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetPathNotEmpty()"), TEXT("GetPath should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DirectoryExistsForProjectDir()"), TEXT("DirectoryExists for ProjectDir should be true"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FileExistsForProjectDirIsFalse()"), TEXT("FileExists for ProjectDir should be false"), 1);
	}

	// ====================================================================
	// Section: PathsExactCompat
	// ====================================================================

	TEST_METHOD(PathsExactCompat)
	{
		auto EscapeScriptString = [](const FString& Value) -> FString
		{
			FString Escaped = Value;
			Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
			Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
			Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
			Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			return Escaped;
		};

		const FString ProjectDir = FPaths::ProjectDir();
		const FString Relative = TEXT("Script/Test.as");
		const FString ExpectedCombined = FPaths::Combine(ProjectDir, Relative);
		const FString ExpectedFullFromRelative = FPaths::ConvertRelativePathToFull(Relative);
		const FString ExpectedFullFromBase = FPaths::ConvertRelativePathToFull(ProjectDir, Relative);
		const FString ExpectedPathOnly = FPaths::GetPath(ExpectedCombined);
		const FString ExpectedExtension = FPaths::GetExtension(ExpectedCombined, true);
		const FString ExpectedBase = FPaths::GetBaseFilename(ExpectedCombined, true);
		const FString CaseVariantCombined = ExpectedCombined.ToUpper();
		const FString PrefixCollisionPath = FPaths::Combine(ProjectDir, TEXT("ScriptBackup/Test.as"));
		const bool bExpectedSamePathIgnoresCase = FPaths::IsSamePath(ExpectedCombined, CaseVariantCombined);
		const bool bExpectedCombinedIsUnderScriptDir = FPaths::IsUnderDirectory(ExpectedCombined, ExpectedPathOnly);
		const bool bExpectedPrefixCollisionIsUnderScriptDir = FPaths::IsUnderDirectory(PrefixCollisionPath, ExpectedPathOnly);
		const bool bExpectedProjectDirExists = FPaths::DirectoryExists(ProjectDir);
		const bool bExpectedProjectDirIsFile = FPaths::FileExists(ProjectDir);

		FString Script = TEXT(R"(
int Entry()
{
	const FString Relative = "__RELATIVE__";
	const FString ExpectedCombined = "__EXPECTED_COMBINED__";
	const FString ExpectedFullFromRelative = "__EXPECTED_FULL_FROM_RELATIVE__";
	const FString ExpectedFullFromBase = "__EXPECTED_FULL_FROM_BASE__";
	const FString ExpectedPathOnly = "__EXPECTED_PATH_ONLY__";
	const FString ExpectedExtension = "__EXPECTED_EXTENSION__";
	const FString ExpectedBase = "__EXPECTED_BASE__";
	const FString CaseVariantCombined = "__CASE_VARIANT_COMBINED__";
	const FString PrefixCollisionPath = "__PREFIX_COLLISION_PATH__";
	const bool bExpectedSamePathIgnoresCase = __EXPECTED_SAME_PATH_IGNORES_CASE__;
	const bool bExpectedCombinedIsUnderScriptDir = __EXPECTED_COMBINED_UNDER_SCRIPT_DIR__;
	const bool bExpectedPrefixCollisionIsUnderScriptDir = __EXPECTED_PREFIX_COLLISION_UNDER_SCRIPT_DIR__;
	const bool bExpectedProjectDirExists = __EXPECTED_PROJECT_DIR_EXISTS__;
	const bool bExpectedProjectDirIsFile = __EXPECTED_PROJECT_DIR_IS_FILE__;

	FString ProjectDir = FPaths::ProjectDir();
	if (!(ProjectDir == "__EXPECTED_PROJECT_DIR__"))
		return 10;

	FString Combined = FPaths::CombinePaths(ProjectDir, Relative);
	if (!(Combined == ExpectedCombined))
		return 20;

	if (!(FPaths::ConvertRelativePathToFull(Relative) == ExpectedFullFromRelative))
		return 30;
	if (!(FPaths::ConvertRelativePathToFull(ProjectDir, Relative) == ExpectedFullFromBase))
		return 40;
	if (!(FPaths::GetPath(Combined) == ExpectedPathOnly))
		return 50;
	if (!(FPaths::GetExtension(Combined, true) == ExpectedExtension))
		return 60;
	if (!(FPaths::GetBaseFilename(Combined, true) == ExpectedBase))
		return 70;
	if (FPaths::DirectoryExists(ProjectDir) != bExpectedProjectDirExists)
		return 80;
	if (FPaths::FileExists(ProjectDir) != bExpectedProjectDirIsFile)
		return 90;
	if (FPaths::IsSamePath(Combined, CaseVariantCombined) != bExpectedSamePathIgnoresCase)
		return 100;
	if (FPaths::IsUnderDirectory(Combined, ExpectedPathOnly) != bExpectedCombinedIsUnderScriptDir)
		return 110;
	if (FPaths::IsUnderDirectory(PrefixCollisionPath, ExpectedPathOnly) != bExpectedPrefixCollisionIsUnderScriptDir)
		return 120;

	return 1;
}
)");

		Script.ReplaceInline(TEXT("__RELATIVE__"), *EscapeScriptString(Relative), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_PROJECT_DIR__"), *EscapeScriptString(ProjectDir), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_COMBINED__"), *EscapeScriptString(ExpectedCombined), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_FULL_FROM_RELATIVE__"), *EscapeScriptString(ExpectedFullFromRelative), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_FULL_FROM_BASE__"), *EscapeScriptString(ExpectedFullFromBase), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_PATH_ONLY__"), *EscapeScriptString(ExpectedPathOnly), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_EXTENSION__"), *EscapeScriptString(ExpectedExtension), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_BASE__"), *EscapeScriptString(ExpectedBase), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__CASE_VARIANT_COMBINED__"), *EscapeScriptString(CaseVariantCombined), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__PREFIX_COLLISION_PATH__"), *EscapeScriptString(PrefixCollisionPath), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_SAME_PATH_IGNORES_CASE__"), bExpectedSamePathIgnoresCase ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_COMBINED_UNDER_SCRIPT_DIR__"), bExpectedCombinedIsUnderScriptDir ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_PREFIX_COLLISION_UNDER_SCRIPT_DIR__"), bExpectedPrefixCollisionIsUnderScriptDir ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_PROJECT_DIR_EXISTS__"), bExpectedProjectDirExists ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_PROJECT_DIR_IS_FILE__"), bExpectedProjectDirIsFile ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCoreMisc_PathsExactCompat"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Entry()"), TEXT("Paths exact compat should match native FPaths results"), 1);
	}

	// ====================================================================
	// Section: NumberFormattingOptions
	// ====================================================================

	TEST_METHOD(NumberFormattingOptions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCoreMisc_NumberFormattingOptions"), TEXT(R"(
int OptionsIdenticalAfterCopy()
{
	FNumberFormattingOptions Options;
	Options.SetAlwaysSign(true)
		.SetUseGrouping(false)
		.SetMinimumIntegralDigits(2)
		.SetMaximumIntegralDigits(4)
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(3);
	FNumberFormattingOptions Copy = Options;
	return Options.IsIdentical(Copy) ? 1 : 0;
}

int OptionsHashMatchAfterCopy()
{
	FNumberFormattingOptions Options;
	Options.SetAlwaysSign(true)
		.SetUseGrouping(false)
		.SetMinimumIntegralDigits(2)
		.SetMaximumIntegralDigits(4)
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(3);
	FNumberFormattingOptions Copy = Options;
	return (Options.GetTypeHash() == Copy.GetTypeHash()) ? 1 : 0;
}

int DefaultGroupedNotIdenticalToUngrouped()
{
	FNumberFormattingOptions DefaultGrouped = FNumberFormattingOptions::DefaultWithGrouping();
	FNumberFormattingOptions DefaultUngrouped = FNumberFormattingOptions::DefaultNoGrouping();
	return (!DefaultGrouped.IsIdentical(DefaultUngrouped)) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int OptionsIdenticalAfterCopy()"), TEXT("Copied options should be identical"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int OptionsHashMatchAfterCopy()"), TEXT("Copied options type hash should match"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DefaultGroupedNotIdenticalToUngrouped()"), TEXT("DefaultWithGrouping should differ from DefaultNoGrouping"), 1);
	}
};

#endif
