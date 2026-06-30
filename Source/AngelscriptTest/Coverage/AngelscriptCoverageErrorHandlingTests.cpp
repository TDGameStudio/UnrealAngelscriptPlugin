#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageErrorHandlingTests
// -----------------------------------------------------------------------------
// Coverage landing file for script-side error handling patterns: bool return,
// error code enum, out-result pattern, null/range checks, early returns, retry,
// and safe fallback values. Crash-style check/ensure behavior is intentionally
// left to lower-level engine tests; these cases stay deterministic in headless
// automation.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageErrorHandlingTest,
	"Angelscript.TestModule.Coverage.ErrorHandling",
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

	TEST_METHOD(ReturnPatternsAndOutResults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageErrorHandling_ReturnPatterns"), ASTEST_AS(R"AS(
			enum ECoverageErrorCode
			{
				Success = 0,
				NotFound = 1,
				InvalidParam = 2
			}

			bool TryDivide(int Numerator, int Denominator, int&out OutResult)
			{
				if (Denominator == 0)
				{
					OutResult = 0;
					return false;
				}

				OutResult = Math::IntegerDivisionTrunc(Numerator, Denominator);
				return true;
			}

			ECoverageErrorCode ValidateIndex(const TArray<int>&in Values, int Index)
			{
				if (Index < 0)
				{
					return ECoverageErrorCode::InvalidParam;
				}
				if (!Values.IsValidIndex(Index))
				{
					return ECoverageErrorCode::NotFound;
				}
				return ECoverageErrorCode::Success;
			}

			int BoolReturnPattern()
			{
				int Value = 0;
				if (!TryDivide(10, 0, Value))
				{
					return -1;
				}
				return Value;
			}

			int OutResultSuccessPattern()
			{
				int Value = 0;
				return TryDivide(12, 3, Value) ? Value : -1;
			}

			int ErrorCodePattern()
			{
				TArray<int> Values;
				Values.Add(10);
				Values.Add(20);
				return int(ValidateIndex(Values, 5)) * 10 + int(ValidateIndex(Values, -1));
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("return/out-result module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int BoolReturnPattern()"),
			TEXT("bool return pattern should report failure"), -1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int OutResultSuccessPattern()"),
			TEXT("out result pattern should return computed value"), 4)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ErrorCodePattern()"),
			TEXT("error code enum should distinguish not-found and invalid-param"), 12)));
	}

	TEST_METHOD(NullBoundsEarlyReturnAndRetry)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageErrorHandling_DefensivePatterns"), ASTEST_AS(R"AS(
			int SafeActorNameLength(AActor Actor)
			{
				if (Actor == nullptr)
				{
					return 0;
				}

				return Actor.GetName().ToString().Len();
			}

			int SafeNullActorNameLength()
			{
				return SafeActorNameLength(nullptr);
			}

			int SafeArrayRead(const TArray<int>&in Values, int Index, int DefaultValue)
			{
				if (!Values.IsValidIndex(Index))
				{
					return DefaultValue;
				}

				return Values[Index];
			}

			bool IsReady(int State)
			{
				return State > 0;
			}

			int EarlyReturnPattern(int State)
			{
				if (!IsReady(State))
				{
					return -10;
				}

				if (State > 10)
				{
					return 10;
				}

				return State;
			}

			int EarlyReturnRejectsInvalidState()
			{
				return EarlyReturnPattern(0);
			}

			bool TryOperation(int Attempt)
			{
				return Attempt >= 3;
			}

			int RetryPattern()
			{
				for (int Attempt = 1; Attempt <= 5; ++Attempt)
				{
					if (TryOperation(Attempt))
					{
						return Attempt;
					}
				}

				return -1;
			}

			int DefensivePatternSummary()
			{
				TArray<int> Values;
				Values.Add(3);
				Values.Add(9);
				return SafeActorNameLength(nullptr)
					+ SafeArrayRead(Values, 1, -1)
					+ SafeArrayRead(Values, 4, 7)
					+ EarlyReturnPattern(0)
					+ RetryPattern();
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("defensive pattern module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int SafeNullActorNameLength()"),
			TEXT("null check should return fallback"), 0)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int EarlyReturnRejectsInvalidState()"),
			TEXT("early return should reject invalid state"), -10)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int RetryPattern()"),
			TEXT("retry loop should stop on first success"), 3)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int DefensivePatternSummary()"),
			TEXT("defensive patterns should combine deterministic fallbacks"), 9)));
	}

	TEST_METHOD(ThrowIfReportsScriptException)
	{
		TestRunner->AddExpectedError(TEXT("CoverageThrowIfTriggered"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("ASCoverageErrorHandling_ThrowIf"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("void TriggerThrowIf()"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageErrorHandling_ThrowIf"), ASTEST_AS(R"AS(
			int ThrowIfFalseContinues()
			{
				ThrowIf(false, "CoverageThrowIfSkipped");
				return 11;
			}

			void TriggerThrowIf()
			{
				ThrowIf(true, "CoverageThrowIfTriggered");
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("ThrowIf module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ThrowIfFalseContinues()"),
			TEXT("ThrowIf(false) should not interrupt execution"), 11)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("void TriggerThrowIf()"),
			TEXT("ThrowIf(true) should raise a script exception"), TEXT("CoverageThrowIfTriggered"))));
	}

	TEST_METHOD(NegativeRuntimeAndCompileBoundaries)
	{
		TestRunner->AddExpectedError(TEXT("Array index out of bounds."), EAutomationExpectedErrorFlags::Exact, 1, false);
		TestRunner->AddExpectedError(TEXT("Array index out of bounds. Need to insert between 0 and ArraySize"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Invalid negative Num"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Cannot move assign an array into itself."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Division by zero"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("ASCoverageErrorHandling_NegativeBoundaries"), EAutomationExpectedErrorFlags::Contains, 6);
		TestRunner->AddExpectedError(TEXT("TriggerArrayOutOfBounds"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("TriggerNullObjectAccess"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("TriggerDivideByZero"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("TriggerInsertOutOfBounds"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("TriggerNegativeArraySize"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("TriggerMoveAssignSelf"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			void TriggerArrayOutOfBounds()
			{
				TArray<int> Values;
				Values.Add(10);
				Values[1] = 20;
			}

			FVector TriggerNullObjectAccess()
			{
				AActor Actor;
				return Actor.GetActorLocation();
			}

			void TriggerDivideByZero()
			{
				int Numerator = 12;
				int Denominator = 0;
				int Result = Math::IntegerDivisionTrunc(Numerator, Denominator);
				ThrowIf(Result != 0, "CoverageUnexpectedDivideResult");
			}

			void TriggerInsertOutOfBounds()
			{
				TArray<int> Values;
				Values.Insert(10, 1);
			}

			void TriggerNegativeArraySize()
			{
				TArray<int> Values;
				Values.SetNum(-1);
			}

			void TriggerMoveAssignSelf()
			{
				TArray<int> Values;
				Values.Add(1);
				Values.MoveAssignFrom(Values);
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageErrorHandling_NegativeBoundaries"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("negative-boundary module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("void TriggerArrayOutOfBounds()"),
			TEXT("array write past the valid range should raise a script exception"), TEXT("Array index out of bounds."))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("FVector TriggerNullObjectAccess()"),
			TEXT("null UObject method access should raise a script exception"), TEXT("Null pointer access"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("void TriggerDivideByZero()"),
			TEXT("integer divide by zero should raise a script exception"), TEXT("Division by zero"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("void TriggerInsertOutOfBounds()"),
			TEXT("array insert past Num should raise a stable script exception"), TEXT("Array index out of bounds. Need to insert between 0 and ArraySize"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("void TriggerNegativeArraySize()"),
			TEXT("negative array SetNum should raise a stable script exception"), TEXT("Invalid negative Num"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(*TestRunner, Engine, ScriptModule, TEXT("void TriggerMoveAssignSelf()"),
			TEXT("array self move assignment should raise a stable script exception"), TEXT("Cannot move assign an array into itself."))));
	}

	TEST_METHOD(GuardedNegativeBoundariesPreserveFallbacks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int GuardedMapMissingKey()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 10);

				int FoundValue = 77;
				if (Values.Find(FName("Missing"), FoundValue))
				{
					return FoundValue;
				}

				return FoundValue;
			}

			int GuardedMapHit()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 10);

				int FoundValue = 77;
				if (!Values.Find(FName("Alpha"), FoundValue))
				{
					return -1;
				}

				return FoundValue;
			}

			int GuardedArrayInvalidIndex()
			{
				TArray<int> Values;
				Values.Add(4);
				Values.Add(8);

				if (!Values.IsValidIndex(-1))
				{
					return 31;
				}

				return Values[-1];
			}

			int GuardedArrayValidIndex()
			{
				TArray<int> Values;
				Values.Add(4);
				Values.Add(8);

				if (!Values.IsValidIndex(1))
				{
					return -1;
				}

				return Values[1];
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageErrorHandling_GuardedNegativeBoundaries"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("guarded negative-boundary module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GuardedMapMissingKey()"),
			TEXT("missing map key should preserve caller fallback output"), 77)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GuardedMapHit()"),
			TEXT("present map key should overwrite fallback output"), 10)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GuardedArrayInvalidIndex()"),
			TEXT("invalid array index should be guarded before access"), 31)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GuardedArrayValidIndex()"),
			TEXT("valid array index should still read normally after guard"), 8)));
	}

	TEST_METHOD(NegativeCompileBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString MissingSymbolSource = ASTEST_AS(R"AS(
			int TriggerMissingSymbolCompileFailure()
			{
				return MissingCoverageBoundarySymbol();
			}
			)AS");
		const TArray<FString> MissingSymbolDiagnostics = { TEXT("MissingCoverageBoundarySymbol") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageErrorHandling_MissingSymbolBoundary"),
			MissingSymbolSource,
			TEXT("missing symbol should remain a deterministic compile-failure boundary"),
			MakeArrayView(MissingSymbolDiagnostics))));

		const FString SyntaxFailureSource = ASTEST_AS(R"AS(
			int TriggerSyntaxCompileFailure()
			{
				int Value = 12
				return Value;
			}
			)AS");
		const TArray<FString> SyntaxDiagnostics =
		{
			TEXT("Expected ',' or ';'"),
			TEXT("Instead found")
		};
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageErrorHandling_SyntaxBoundary"),
			SyntaxFailureSource,
			TEXT("syntax error should remain a deterministic compile-failure boundary"),
			MakeArrayView(SyntaxDiagnostics))));

		const FString UnsupportedErrorPayloadSource = ASTEST_AS(R"AS(
			class FCoverageErrorPayload
			{
				TArray<TMap<int, FString>> Details;
			}
			)AS");
		const TArray<FString> UnsupportedErrorPayloadDiagnostics =
		{
			TEXT("Containers cannot be nested in other containers")
		};
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageErrorHandling_UnsupportedErrorPayloadBoundary"),
			UnsupportedErrorPayloadSource,
			TEXT("unsupported nested container error payload should remain a deterministic compile-failure boundary"),
			MakeArrayView(UnsupportedErrorPayloadDiagnostics))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
