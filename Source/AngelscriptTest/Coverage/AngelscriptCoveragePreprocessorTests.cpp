#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

// -----------------------------------------------------------------------------
// AngelscriptCoveragePreprocessorTests
// -----------------------------------------------------------------------------
// Coverage landing file for the preprocessor matrix. The detailed suite lives in
// AngelscriptTest/Preprocessor; this file captures the stable user-facing subset
// expected by Documents/Coverage/Coverage_Preprocessor.md.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoveragePreprocessorTest,
	"Angelscript.TestModule.Coverage.Preprocessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ImportDependencyAndConditionalBranches)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			TPair<FString, FString> FixtureData[] = {
				{
					TEXT("Tests/Coverage/Preprocessor/Shared.as"),
					TEXT(R"(
int SharedValue()
{
	return 40;
}
)"),
				},
				{
					TEXT("Tests/Coverage/Preprocessor/Consumer.as"),
					TEXT(R"(
#ifdef USE_SHARED
import Tests.Coverage.Preprocessor.Shared;
#endif

int Entry()
{
#ifdef USE_SHARED
	return SharedValue() + 2;
#else
	return -1;
#endif
}
)"),
				},
			};

			TArray<FFixtureFile> Files = WriteFixtures(MakeArrayView(FixtureData));
			FPreprocessResult Result = RunPreprocess(Engine, Files, {{TEXT("USE_SHARED"), true}});

			AssertPreprocessSucceeded(*TestRunner, Result);
			AssertModuleCount(*TestRunner, Result, 2);
			AssertErrorCount(*TestRunner, Result, 0);
			AssertNoDiagnostics(*TestRunner, Result);

			ASSERT_THAT(AreEqual(
				FString(TEXT("Tests.Coverage.Preprocessor.Shared -> Tests.Coverage.Preprocessor.Consumer")),
				Result.ModuleOrder(),
				TEXT("import dependency should order provider before consumer")));

			const FAngelscriptModuleDesc* Consumer = AssertModuleExists(
				*TestRunner,
				Result,
				TEXT("Tests.Coverage.Preprocessor.Consumer"));
			if (Consumer != nullptr)
			{
				AssertImportCount(*TestRunner, *Consumer, 1);
				AssertModuleImports(*TestRunner, *Consumer, TEXT("Tests.Coverage.Preprocessor.Shared"));
				AssertModuleCodeContains(*TestRunner, Result, *Consumer, TEXT("return SharedValue() + 2;"));
				AssertModuleCodeNotContains(*TestRunner, Result, *Consumer, TEXT("return -1;"));
			}
		}
	}

	TEST_METHOD(DisabledImportBranchIsIgnored)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			TPair<FString, FString> FixtureData[] = {
				{
					TEXT("Tests/Coverage/Preprocessor/UnusedShared.as"),
					TEXT(R"(
int SharedValue()
{
	return 40;
}
)"),
				},
				{
					TEXT("Tests/Coverage/Preprocessor/DisabledConsumer.as"),
					TEXT(R"(
#ifdef USE_SHARED
import Tests.Coverage.Preprocessor.UnusedShared;
#endif

int Entry()
{
#ifdef USE_SHARED
	return SharedValue();
#else
	return 7;
#endif
}
)"),
				},
			};

			TArray<FFixtureFile> Files = WriteFixtures(MakeArrayView(FixtureData));
			FPreprocessResult Result = RunPreprocess(Engine, Files, {{TEXT("USE_SHARED"), false}});

			AssertPreprocessSucceeded(*TestRunner, Result);
			AssertErrorCount(*TestRunner, Result, 0);
			AssertNoDiagnostics(*TestRunner, Result);

			const FAngelscriptModuleDesc* Consumer = AssertModuleExists(
				*TestRunner,
				Result,
				TEXT("Tests.Coverage.Preprocessor.DisabledConsumer"));
			if (Consumer != nullptr)
			{
				AssertImportCount(*TestRunner, *Consumer, 0);
				AssertModuleCodeContains(*TestRunner, Result, *Consumer, TEXT("return 7;"));
				AssertModuleCodeNotContains(*TestRunner, Result, *Consumer, TEXT("SharedValue"));
			}
		}
	}

	TEST_METHOD(IncludeDirectiveReportsUnsupportedDiagnostic)
	{
		using namespace PreprocessorTestHelpers;

		static const FString ExpectedDiagnostic(TEXT("Unsupported preprocessor directive '#include'. Use import or automatic imports instead."));
		TestRunner->AddExpectedError(*ExpectedDiagnostic, EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/UnsupportedInclude.as"), TEXT(R"(
#include "Shared.as"
int Entry()
{
	return 1;
}
)"));

			FPreprocessResult Result = RunPreprocess(Engine, File);

			AssertPreprocessFailed(*TestRunner, Result);
			AssertErrorCount(*TestRunner, Result, 1);
			AssertDiagnosticContains(*TestRunner, Result, ExpectedDiagnostic);
			AssertDiagnosticAt(*TestRunner, Result, ExpectedDiagnostic, 1);
		}
	}

	TEST_METHOD(SummaryReportsCoverageFixtureShape)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/SummaryCarrier.as"), TEXT(R"(
UENUM()
enum ECoveragePreprocessorState
{
	Idle,
	Active
}

UCLASS()
class UCoveragePreprocessorSummaryCarrier : UObject
{
	UPROPERTY()
	int Value;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}
}
)"));

			FPreprocessSession Session = RunPreprocessSession(Engine, File);

			AssertPreprocessSucceeded(*TestRunner, Session.Result);
			AssertErrorCount(*TestRunner, Session.Result, 0);
			AssertNoDiagnostics(*TestRunner, Session.Result);

			const FAngelscriptPreprocessorSummary Summary = Session.Preprocessor.GetSummary();
			ASSERT_THAT(IsTrue(Summary.bSucceeded, TEXT("summary should report success")));
			ASSERT_THAT(AreEqual(1, Summary.ModuleCount, TEXT("summary should report one module")));
			ASSERT_THAT(AreEqual(1, Summary.ClassCount, TEXT("summary should report one class")));
			ASSERT_THAT(AreEqual(1, Summary.FunctionCount, TEXT("summary should report one function")));
			ASSERT_THAT(AreEqual(1, Summary.PropertyCount, TEXT("summary should report one property")));
			ASSERT_THAT(AreEqual(1, Summary.EnumCount, TEXT("summary should report one enum")));
			ASSERT_THAT(IsTrue(Summary.ProcessedCodeCharacterCount > 0, TEXT("summary should report processed code")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
