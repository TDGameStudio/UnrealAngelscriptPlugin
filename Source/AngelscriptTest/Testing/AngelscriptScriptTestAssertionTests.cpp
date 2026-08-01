#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptScriptTestTestHelpers.h"

#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestAssertionTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.Assertions",
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

	TEST_METHOD(PassingAssertionFamiliesAndOverloads)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UPassingAssertionScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void AllFamiliesPass()
				{
					AssertTrue(true);
					AssertFalse(false);
					AssertNull(nullptr);
					AssertNotNull(this);
					AssertSame(this, this);
					AssertNotSame(this, nullptr);

					AssertEquals(7, 7);
					AssertNotEquals(7, 8);
					AssertEquals(int64(9), int64(9));
					AssertNotEquals(int64(9), int64(10));
					AssertEquals(float32(1.25), float32(1.25));
					AssertNotEquals(float32(1.25), float32(2.25));
					AssertEquals(float64(2.5), float64(2.5));
					AssertNotEquals(float64(2.5), float64(3.5));
					AssertEquals(true, true);
					AssertNotEquals(true, false);
					AssertEquals(n"Alpha", n"Alpha");
					AssertNotEquals(n"Alpha", n"Beta");
					AssertEquals("Alpha", "Alpha");
					AssertNotEquals("Alpha", "Beta");

					const FVector VectorA(1.0, 2.0, 3.0);
					const FVector VectorB(1.0, 2.0, 3.0);
					const FRotator RotatorA(1.0, 2.0, 3.0);
					const FRotator RotatorB(1.0, 2.0, 3.0);
					const FQuat QuatA = FQuat::Identity;
					const FQuat QuatB = FQuat::Identity;
					const FTransform TransformA(QuatA, VectorA, FVector::OneVector);
					const FTransform TransformB(QuatB, VectorB, FVector::OneVector);
					AssertEquals(VectorA, VectorB);
					AssertEquals(RotatorA, RotatorB);
					AssertEquals(QuatA, QuatB);
					AssertEquals(TransformA, TransformB);
					AssertNotEquals(VectorA, FVector::ZeroVector);
					AssertNotEquals(RotatorA, FRotator::ZeroRotator);
					AssertNotEquals(QuatA, FQuat(1.0, 0.0, 0.0, 0.0));
					AssertNotEquals(
						TransformA,
						FTransform(QuatA, FVector::ZeroVector, FVector::OneVector));

					AssertNear(float32(1.0), float32(1.00001));
					AssertNear(float64(1.0), float64(1.00001));
					AssertNear(VectorA, FVector(1.00001, 2.0, 3.0));
					AssertNear(RotatorA, FRotator(1.00001, 2.0, 3.0));
					AssertNear(QuatA, QuatB);
					AssertNear(TransformA, TransformB);

					AssertLessThan(1, 2);
					AssertLessThanOrEqual(2, 2);
					AssertGreaterThan(2, 1);
					AssertGreaterThanOrEqual(2, 2);
					AssertLessThan(int64(1), int64(2));
					AssertLessThan(float32(1.0), float32(2.0));
					AssertLessThan(float64(1.0), float64(2.0));
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAssertions_Pass"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
	}

	TEST_METHOD(ExpectedErrorsUseLiteralContainsRegexAndCounts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UExpectedErrorScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void MatchesExpectedErrors()
				{
					ExpectError("literal [brackets]", 1);
					ExpectErrorRegex("regex-value-[0-9]+", 2);
					Error("prefix literal [brackets] suffix");
					Error("regex-value-12");
					Error("regex-value-34");
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAssertions_Expected"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
	}

	TEST_METHOD(DetachedRunnerCapturesOrdinaryExceptionsInEveryLeafPhase)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UBeforeEachExceptionScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
				void BeforeEach()
				{
					throw("detached BeforeEach exception");
				}

				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UMethodExceptionScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
					throw("detached method exception");
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UAfterEachExceptionScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
				}

				UFUNCTION(BlueprintOverride)
				void AfterEach()
				{
					throw("detached AfterEach exception");
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAssertions_DetachedExceptions"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(3, Build.Snapshot->Tests.Num()));

		struct FExceptionCase
		{
			const TCHAR* Suite;
			const TCHAR* Exception;
		};
		const FExceptionCase Cases[] = {
			{
				TEXT("UBeforeEachExceptionScriptTests"),
				TEXT("detached BeforeEach exception"),
			},
			{
				TEXT("UMethodExceptionScriptTests"),
				TEXT("detached method exception"),
			},
			{
				TEXT("UAfterEachExceptionScriptTests"),
				TEXT("detached AfterEach exception"),
			},
		};

		for (const FExceptionCase& Case : Cases)
		{
			const FAngelscriptScriptTestDescriptor* Descriptor =
				Build.Snapshot->Tests.FindByPredicate(
					[&Case](
						const FAngelscriptScriptTestDescriptor& Candidate)
					{
						return Candidate.Id.SuiteName == Case.Suite;
					});
			ASSERT_THAT(IsNotNull(Descriptor));

			FAngelscriptScriptTestProbe Probe(
				FString::Printf(
					TEXT("FAngelscriptScriptTestDetachedException_%s_%p"),
					Case.Suite,
					this));
			TSharedPtr<FAngelscriptScriptTestExecutionContext> Context;
			{
				// Keep the intentional inner exception out of this enclosing
				// CQTest. The detached result must still receive its explicit
				// source-located exception diagnostic.
				TGuardValue<bool> SuppressOuterLogErrors(
					FAutomationTestBase::bSuppressLogErrors,
					true);
				Context = FAngelscriptScriptTestRunner::Start(
					Descriptor->Id,
					Probe,
					false);
			}
			ASSERT_THAT(IsTrue(Context.IsValid()));
			ASSERT_THAT(IsTrue(Context->IsComplete()));
			ASSERT_THAT(IsTrue(Context->HasFailed()));

			const TArray<FAutomationExecutionEntry> Entries =
				Probe.GetExecutionEntries();
			ASSERT_THAT(AreEqual(1, Entries.Num()));
			ASSERT_THAT(IsTrue(
				Entries[0].Event.Message.Contains(Case.Exception)));
			ASSERT_THAT(IsTrue(
				Entries[0].Event.Message.Contains(
					TEXT("ASTesting_ScriptTestAssertions_DetachedExceptions"))));
		}
	}

	TEST_METHOD(DetachedRunnerReportsOrdinaryCleanupExceptionsAfterPriorFailure)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UAfterEachSecondaryExceptionScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
					Fail("primary body failure");
				}

				UFUNCTION(BlueprintOverride)
				void AfterEach()
				{
					throw("secondary AfterEach exception");
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UTeardownSecondaryExceptionScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
					FAngelscriptTest::Commands()
						.OnCleanup(n"ThrowingCleanup");
					Fail("primary teardown-body failure");
				}

				void ThrowingCleanup()
				{
					throw("secondary teardown exception");
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAssertions_SecondaryExceptions"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(2, Build.Snapshot->Tests.Num()));

		struct FSecondaryExceptionCase
		{
			const TCHAR* Suite;
			const TCHAR* Primary;
			const TCHAR* Secondary;
		};
		const FSecondaryExceptionCase Cases[] = {
			{
				TEXT("UAfterEachSecondaryExceptionScriptTests"),
				TEXT("primary body failure"),
				TEXT("secondary AfterEach exception"),
			},
			{
				TEXT("UTeardownSecondaryExceptionScriptTests"),
				TEXT("primary teardown-body failure"),
				TEXT("secondary teardown exception"),
			},
		};

		for (const FSecondaryExceptionCase& Case : Cases)
		{
			const FAngelscriptScriptTestDescriptor* Descriptor =
				Build.Snapshot->Tests.FindByPredicate(
					[&Case](
						const FAngelscriptScriptTestDescriptor& Candidate)
					{
						return Candidate.Id.SuiteName == Case.Suite;
					});
			ASSERT_THAT(IsNotNull(Descriptor));

			FAngelscriptScriptTestProbe Probe(
				FString::Printf(
					TEXT("FAngelscriptSecondaryException_%s_%p"),
					Case.Suite,
					this));
			const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
				FAngelscriptScriptTestRunner::Start(
					Descriptor->Id,
					Probe,
					false);
			ASSERT_THAT(IsTrue(Context.IsValid()));
			ASSERT_THAT(IsTrue(Context->IsComplete()));
			ASSERT_THAT(IsTrue(Context->HasFailed()));

			const TArray<FAutomationExecutionEntry> Entries =
				Probe.GetExecutionEntries();
			ASSERT_THAT(IsNotNull(
				Entries.FindByPredicate(
					[&Case](const FAutomationExecutionEntry& Entry)
					{
						return Entry.Event.Message.Contains(
							Case.Primary);
					})));
			ASSERT_THAT(IsNotNull(
				Entries.FindByPredicate(
					[&Case](const FAutomationExecutionEntry& Entry)
					{
						return Entry.Event.Message.Contains(
							Case.Secondary);
					})));
		}
	}

	TEST_METHOD(DetachedRunnerCapturesAndFinalizesExpectedErrors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UDetachedExpectedErrorScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void MatchingExpectedError()
				{
					ExpectError("detached expected log", 1);
					Error("detached expected log");
				}

				UFUNCTION(meta=(AngelscriptTest))
				void MissingExpectedError()
				{
					ExpectError("detached missing log", 1);
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAssertions_DetachedExpected"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(2, Build.Snapshot->Tests.Num()));

		const FAngelscriptScriptTestDescriptor* MatchingDescriptor =
			Build.Snapshot->Tests.FindByPredicate(
				[](const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.MethodName
						== TEXT("MatchingExpectedError");
				});
		ASSERT_THAT(IsNotNull(MatchingDescriptor));
		TestRunner->AddExpectedError(
			TEXT("detached expected log"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		FAngelscriptScriptTestProbe MatchingProbe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestDetachedExpectedMatch_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext>
			MatchingContext =
				FAngelscriptScriptTestRunner::Start(
					MatchingDescriptor->Id,
					MatchingProbe,
					false);
		ASSERT_THAT(IsTrue(MatchingContext.IsValid()));
		ASSERT_THAT(IsTrue(MatchingContext->IsComplete()));
		ASSERT_THAT(IsFalse(MatchingContext->HasFailed()));
		ASSERT_THAT(IsTrue(MatchingProbe.HasMetExpectedErrors()));
		ASSERT_THAT(AreEqual(
			0,
			MatchingProbe.GetExecutionEntries().Num()));

		const FAngelscriptScriptTestDescriptor* MissingDescriptor =
			Build.Snapshot->Tests.FindByPredicate(
				[](const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.MethodName
						== TEXT("MissingExpectedError");
				});
		ASSERT_THAT(IsNotNull(MissingDescriptor));
		FAngelscriptScriptTestProbe MissingProbe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestDetachedExpectedMissing_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext>
			MissingContext =
				FAngelscriptScriptTestRunner::Start(
					MissingDescriptor->Id,
					MissingProbe,
					false);
		ASSERT_THAT(IsTrue(MissingContext.IsValid()));
		ASSERT_THAT(IsTrue(MissingContext->IsComplete()));
		ASSERT_THAT(IsTrue(MissingContext->HasFailed()));
		const TArray<FAutomationExecutionEntry> MissingEntries =
			MissingProbe.GetExecutionEntries();
		ASSERT_THAT(AreEqual(1, MissingEntries.Num()));
		ASSERT_THAT(IsTrue(
			MissingEntries[0].Event.Message.Contains(
				TEXT("detached missing log"))));
	}

	TEST_METHOD(FailingAssertionHasOneSourceLocatedCustomDiagnostic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UFailingAssertionScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void FailsAtTheCallSite()
				{
					AssertEquals(1, 2, "custom assertion message");
					Fail("must not execute after fail-fast assertion");
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAssertions_Fail"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestAssertionProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->HasFailed()));

		const TArray<FAutomationExecutionEntry> Entries =
			Probe.GetExecutionEntries();
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("custom assertion message"))));
		ASSERT_THAT(IsFalse(
			Entries[0].Event.Message.Contains(
				TEXT("must not execute"))));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("ASTesting_ScriptTestAssertions_Fail"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
