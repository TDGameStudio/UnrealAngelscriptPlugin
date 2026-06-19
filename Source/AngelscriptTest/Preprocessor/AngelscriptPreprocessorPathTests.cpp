// ============================================================================
// AngelscriptPreprocessorPathTests.cpp
//
// Preprocessor tests for path normalization: backslash separators in relative
// paths normalize to dotted module names, and FilenameToModuleName only strips
// the terminal '.as' extension (intermediate '.as' folder segments survive).
//
// Refactored from IMPLEMENT_SIMPLE_AUTOMATION_TEST -> TEST_CLASS_WITH_FLAGS,
// reusing the shared PreprocessorTestHelpers (FFixtureFile / FPreprocessResult /
// RunPreprocess) for the file-based scenarios. The file-local namespace
// helpers from the previous revision were retired in favor of those shared
// utilities.
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Paths.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorPathTest,
	"Angelscript.TestModule.Preprocessor.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// BackslashRelativePathNormalizesModuleName — Windows-style backslash
	// relative paths still produce dotted module names; manual import via the
	// dotted form resolves to the matching provider module.
	// ========================================================================
	TEST_METHOD(BackslashRelativePathNormalizesModuleName)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile SharedFile(TEXT("Tests\\Preprocessor\\PathNormalization\\WinShared.as"), TEXT(R"(
int SharedValue()
{
    return 11;
}
)"));

		FFixtureFile ImportingFile(TEXT("Tests\\Preprocessor\\PathNormalization\\WinUse.as"), TEXT(R"(
import Tests.Preprocessor.PathNormalization.WinShared;
int UseShared()
{
    return SharedValue();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(SharedFile));
		Files.Emplace(MoveTemp(ImportingFile));

		auto Result = RunPreprocess(Engine, Files);

		const FString ModuleNames = FString::JoinBy(
			Result.Modules,
			TEXT(" | "),
			[](const TSharedRef<FAngelscriptModuleDesc>& Module)
			{
				return Module->ModuleName;
			});

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 2);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* SharedModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.PathNormalization.WinShared"));
		const FAngelscriptModuleDesc* ImportingModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.PathNormalization.WinUse"));

		ASSERT_THAT(IsFalse(
			ModuleNames.Contains(TEXT("\\")),
			TEXT("Normalized module names should not preserve raw backslashes")));

		if (SharedModule != nullptr)
		{
			ASSERT_THAT(AreEqual(
				0,
				SharedModule->ImportedModules.Num(),
				TEXT("Normalized provider module should not record any imports")));
		}

		if (ImportingModule != nullptr)
		{
			ASSERT_THAT(AreEqual(
				1,
				ImportingModule->ImportedModules.Num(),
				TEXT("Normalized importer module should record exactly one imported module")));
			ASSERT_THAT(IsTrue(
				ImportingModule->ImportedModules.Contains(TEXT("Tests.Preprocessor.PathNormalization.WinShared")),
				TEXT("Normalized importer module should reference the dotted provider module name")));
			ASSERT_THAT(IsFalse(
				ImportingModule->ImportedModules.Contains(TEXT("Tests\\Preprocessor\\PathNormalization\\WinShared")),
				TEXT("Normalized importer module should not record backslash-based import names")));
		}

		}
	}

	// ========================================================================
	// FilenameToModuleNameOnlyStripsTerminalExtension — FilenameToModuleName
	// only strips the trailing '.as' suffix; intermediate '.as' folder
	// segments survive verbatim, and asset-like '.asset.as' filenames keep
	// the '.asset' part.
	// ========================================================================
	TEST_METHOD(FilenameToModuleNameOnlyStripsTerminalExtension)
	{
		FAngelscriptPreprocessor Preprocessor;

		const FString FolderAsModuleName    = Preprocessor.FilenameToModuleName(TEXT("Tests/Foo.as/Bar.as"));
		const FString RegularModuleName     = Preprocessor.FilenameToModuleName(TEXT("Tests/Foo/Bar.as"));
		const FString AssetSuffixModuleName = Preprocessor.FilenameToModuleName(TEXT("Tests/Foo.as/Baz.asset.as"));

		ASSERT_THAT(AreEqual(
			FString(TEXT("Tests.Foo.as.Bar")),
			FolderAsModuleName,
			TEXT("FilenameToModuleName should preserve '.as' when it appears in an intermediate path segment")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Tests.Foo.Bar")),
			RegularModuleName,
			TEXT("FilenameToModuleName should continue normalizing a standard script filename")));
		ASSERT_THAT(IsTrue(
			FolderAsModuleName != RegularModuleName,
			TEXT("FilenameToModuleName should keep intermediate '.as' segments distinct from plain folders")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Tests.Foo.as.Baz.asset")),
			AssetSuffixModuleName,
			TEXT("FilenameToModuleName should strip only the terminal extension from asset-like script filenames")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
