#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

// -----------------------------------------------------------------------------
// AngelscriptCoveragePreprocessorTests
// -----------------------------------------------------------------------------
// Coverage landing file for the preprocessor matrix. The detailed suite lives in
// AngelscriptTest/Preprocessor; this file captures the stable user-facing subset
// expected by OpenSpec: test-coverage/coverage-matrix.md.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoveragePreprocessorTest,
	"Angelscript.TestModule.Coverage.Preprocessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ImportDependencyAndConditionalBranches)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			TPair<FString, FString> FixtureData[] = {
				{
					TEXT("Tests/Coverage/Preprocessor/Shared.as"),
					ASTEST_AS(R"AS(
					int SharedValue()
					{
						return 40;
					}
					)AS"),
				},
				{
					TEXT("Tests/Coverage/Preprocessor/Consumer.as"),
					ASTEST_AS(R"AS(
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
					)AS"),
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

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			TPair<FString, FString> FixtureData[] = {
				{
					TEXT("Tests/Coverage/Preprocessor/UnusedShared.as"),
					ASTEST_AS(R"AS(
					int SharedValue()
					{
						return 40;
					}
					)AS"),
				},
				{
					TEXT("Tests/Coverage/Preprocessor/DisabledConsumer.as"),
					ASTEST_AS(R"AS(
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
					)AS"),
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

	TEST_METHOD(IfElifElseEndifBranches)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/IfElifElse.as"), ASTEST_AS(R"AS(
			int Entry()
			{
			#if FIRST_BRANCH
				return 1;
			#elif SECOND_BRANCH
				return 2;
			#else
				return 3;
			#endif
			}
			)AS"));

			FPreprocessResult ElifResult = RunPreprocess(Engine, File, {{TEXT("FIRST_BRANCH"), false}, {TEXT("SECOND_BRANCH"), true}});
			AssertPreprocessSucceeded(*TestRunner, ElifResult);
			AssertErrorCount(*TestRunner, ElifResult, 0);
			AssertNoDiagnostics(*TestRunner, ElifResult);

			const FAngelscriptModuleDesc* ElifModule = AssertModuleExists(
				*TestRunner,
				ElifResult,
				TEXT("Tests.Coverage.Preprocessor.IfElifElse"));
			if (ElifModule != nullptr)
			{
				AssertModuleCodeContains(*TestRunner, ElifResult, *ElifModule, TEXT("return 2;"));
				AssertModuleCodeNotContains(*TestRunner, ElifResult, *ElifModule, TEXT("return 1;"));
				AssertModuleCodeNotContains(*TestRunner, ElifResult, *ElifModule, TEXT("return 3;"));
			}

			FPreprocessResult ElseResult = RunPreprocess(Engine, File, {{TEXT("FIRST_BRANCH"), false}, {TEXT("SECOND_BRANCH"), false}});
			AssertPreprocessSucceeded(*TestRunner, ElseResult);
			AssertErrorCount(*TestRunner, ElseResult, 0);
			AssertNoDiagnostics(*TestRunner, ElseResult);

			const FAngelscriptModuleDesc* ElseModule = AssertModuleExists(
				*TestRunner,
				ElseResult,
				TEXT("Tests.Coverage.Preprocessor.IfElifElse"));
			if (ElseModule != nullptr)
			{
				AssertModuleCodeContains(*TestRunner, ElseResult, *ElseModule, TEXT("return 3;"));
				AssertModuleCodeNotContains(*TestRunner, ElseResult, *ElseModule, TEXT("return 1;"));
				AssertModuleCodeNotContains(*TestRunner, ElseResult, *ElseModule, TEXT("return 2;"));
			}
		}
	}

	TEST_METHOD(EditorConfigurationFlagBranch)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/EditorFlag.as"), ASTEST_AS(R"AS(
			int Entry()
			{
			#if EDITOR
				return 11;
			#else
				return -11;
			#endif
			}
			)AS"));

			FPreprocessResult Result = RunPreprocess(Engine, File, {{TEXT("EDITOR"), true}});

			AssertPreprocessSucceeded(*TestRunner, Result);
			AssertErrorCount(*TestRunner, Result, 0);
			AssertNoDiagnostics(*TestRunner, Result);

			const FAngelscriptModuleDesc* Module = AssertModuleExists(
				*TestRunner,
				Result,
				TEXT("Tests.Coverage.Preprocessor.EditorFlag"));
			if (Module != nullptr)
			{
				AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 11;"));
				AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return -11;"));
			}
		}
	}

	TEST_METHOD(UnregisteredLegacyMacroNamesReportDiagnostics)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			{
				static const FString ExpectedDiagnostic(TEXT("Invalid preprocessor condition: PLATFORM_WINDOWS"));
				TestRunner->AddExpectedError(*ExpectedDiagnostic, EAutomationExpectedErrorFlags::Contains, 1);

				FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/PlatformWindowsUnsupported.as"), ASTEST_AS(R"AS(
				#if PLATFORM_WINDOWS
				int Entry()
				{
					return 1;
				}
				#endif
				)AS"));

				FPreprocessResult Result = RunPreprocess(Engine, File);
				AssertPreprocessFailed(*TestRunner, Result);
				AssertErrorCount(*TestRunner, Result, 1);
				AssertDiagnosticContains(*TestRunner, Result, ExpectedDiagnostic);
			}

			{
				static const FString ExpectedDiagnostic(TEXT("Invalid preprocessor condition: WITH_EDITOR"));
				TestRunner->AddExpectedError(*ExpectedDiagnostic, EAutomationExpectedErrorFlags::Contains, 1);

				FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/WithEditorUnsupported.as"), ASTEST_AS(R"AS(
				#if WITH_EDITOR
				int Entry()
				{
					return 1;
				}
				#endif
				)AS"));

				FPreprocessResult Result = RunPreprocess(Engine, File);
				AssertPreprocessFailed(*TestRunner, Result);
				AssertErrorCount(*TestRunner, Result, 1);
				AssertDiagnosticContains(*TestRunner, Result, ExpectedDiagnostic);
			}
		}
	}

	TEST_METHOD(IncludeDirectiveReportsUnsupportedDiagnostic)
	{
		using namespace PreprocessorTestHelpers;

		static const FString ExpectedDiagnostic(TEXT("Unsupported preprocessor directive '#include'. Use import or automatic imports instead."));
		TestRunner->AddExpectedError(*ExpectedDiagnostic, EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/UnsupportedInclude.as"), ASTEST_AS(R"AS(
			#include "Shared.as"
			int Entry()
			{
				return 1;
			}
			)AS"));

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

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(TEXT("Tests/Coverage/Preprocessor/SummaryCarrier.as"), ASTEST_AS(R"AS(
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
			)AS"));

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
