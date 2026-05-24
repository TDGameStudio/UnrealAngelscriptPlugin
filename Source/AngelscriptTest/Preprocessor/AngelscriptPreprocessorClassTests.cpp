// ============================================================================
// AngelscriptPreprocessorClassTests.cpp
//
// Preprocessor tests for class declarations: unknown super type errors and
// duplicate class name conflict detection across hot-reload batches.
//
// Migrated from:
//   - AngelscriptPreprocessorClassErrorTests.cpp (UnknownSuperType)
//   - AngelscriptPreprocessorClassHierarchyTests.cpp (DuplicateClassName)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Classes.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorClassTest,
	"Angelscript.TestModule.Preprocessor.Classes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// UnknownSuperTypeReportsDiagnostic — UCLASS() extending a non-existent
	// type fails with a stable diagnostic at the class declaration line
	// ========================================================================
	TEST_METHOD(UnknownSuperTypeReportsDiagnostic)
	{
		TestRunner->AddExpectedErrorPlain(
			TEXT("Class UUnknownSuperCarrier has an unknown super type UMissingBaseType."),
			EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		Engine.ResetDiagnostics();
		Engine.LastEmittedDiagnostics.Empty();

		FFixtureFile File(TEXT("Tests/Preprocessor/Classes/UnknownSuperType.as"), TEXT(R"(
UCLASS()
class UUnknownSuperCarrier : UMissingBaseType
{
}
)"));

		auto Result = RunPreprocess(Engine, File);

		AssertPreprocessFailed(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result,
			TEXT("Class UUnknownSuperCarrier has an unknown super type UMissingBaseType."));
		AssertDiagnosticAt(*TestRunner, Result,
			TEXT("unknown super type"), 2, 1);
		AssertNoCompilableCode(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = Result.FindModule(
			TEXT("Tests.Preprocessor.Classes.UnknownSuperType"));
		if (TestRunner->TestNotNull(TEXT("Module should exist for inspection"), Module))
		{
			TestRunner->TestEqual(TEXT("Should have no code sections"), Module->Code.Num(), 0);
			TestRunner->TestEqual(TEXT("Should have no class descriptors"), Module->Classes.Num(), 0);
		}

		}
	}

	// ========================================================================
	// DuplicateClassNameAcrossHotReloadBatchReportsConflict — after seeding a
	// module with UDuplicateCarrier, a batch with two files declaring the same
	// class fails with a conflict diagnostic on the second file
	// ========================================================================
	TEST_METHOD(DuplicateClassNameAcrossHotReloadBatchReportsConflict)
	{
		// This test requires an isolated engine to seed a module first
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!TestRunner->TestTrue(TEXT("Should acquire isolated engine"), Fixture.IsValid()))
		{
			return;
		}

		FAngelscriptEngine& Engine = Fixture.GetEngine();

		// Seed first module
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary SeedSummary;
		const bool bSeedCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::FullReload,
			FName(TEXT("Tests.Preprocessor.First")),
			TEXT("Tests/Preprocessor/First.as"),
			TEXT(R"(
UCLASS()
class UDuplicateCarrier : UObject
{
    UFUNCTION()
    int GetSeedValue()
    {
        return 1;
    }
}
)"),
			true, SeedSummary);

		if (!TestRunner->TestTrue(TEXT("Seed should compile"), bSeedCompiled))
		{
			return;
		}
		TestRunner->TestNotNull(TEXT("Seeded class should be published"),
			FindGeneratedClass(&Engine, TEXT("UDuplicateCarrier")));

		// Now preprocess a batch where both files declare UDuplicateCarrier
		Engine.ResetDiagnostics();
		Engine.LastEmittedDiagnostics.Empty();

		TestRunner->AddExpectedErrorPlain(
			TEXT("Cannot declare class UDuplicateCarrier in module Tests.Preprocessor.Second. A class with this name already exists in module Tests.Preprocessor.First."),
			EAutomationExpectedErrorFlags::Contains, 1);

		FFixtureFile FirstFile(TEXT("Tests/Preprocessor/First.as"), TEXT(R"(
UCLASS()
class UDuplicateCarrier : UObject
{
    UFUNCTION()
    int GetHotReloadValue()
    {
        return 2;
    }
}
)"));

		FFixtureFile SecondFile(TEXT("Tests/Preprocessor/Second.as"), TEXT(R"(
UCLASS()
class UDuplicateCarrier : UObject
{
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(FirstFile));
		Files.Emplace(MoveTemp(SecondFile));

		auto Result = RunPreprocess(Engine, Files);

		AssertPreprocessFailed(*TestRunner, Result);
		TestRunner->TestEqual(TEXT("Should keep both module descriptors"), Result.Modules.Num(), 2);
		AssertDiagnosticContains(*TestRunner, Result,
			TEXT("Cannot declare class UDuplicateCarrier in module Tests.Preprocessor.Second"));
		AssertDiagnosticAt(*TestRunner, Result,
			TEXT("Cannot declare class UDuplicateCarrier"), 2, 1);

		// Second module should not have the duplicate class
		FAngelscriptModuleDesc* SecondModule = Result.FindModule(TEXT("Tests.Preprocessor.Second"));
		if (SecondModule != nullptr)
		{
			TestRunner->TestFalse(TEXT("Second module should not have duplicate class"),
				SecondModule->GetClass(TEXT("UDuplicateCarrier")).IsValid());
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
