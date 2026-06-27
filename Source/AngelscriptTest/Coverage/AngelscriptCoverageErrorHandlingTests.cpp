#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

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

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageErrorHandlingTest,
	"Angelscript.TestModule.Coverage.ErrorHandling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		if (Module == nullptr)
		{
			TestRunner->AddError(FString::Printf(TEXT("%s: backing module failed to build"), Message));
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		const T Result = Invoker.CallAndReturn<T>();
		TestRunner->TestEqual(Message, Result, Expected);
	}

	TEST_METHOD(ReturnPatternsAndOutResults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageErrorHandling_ReturnPatterns", ASTEST_AS(R"AS(
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

	OutResult = Numerator / Denominator;
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
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int BoolReturnPattern()"), -1, TEXT("bool return pattern should report failure"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OutResultSuccessPattern()"), 4, TEXT("out result pattern should return computed value"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ErrorCodePattern()"), 12, TEXT("error code enum should distinguish not-found and invalid-param"));
	}

	TEST_METHOD(NullBoundsEarlyReturnAndRetry)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageErrorHandling_DefensivePatterns", ASTEST_AS(R"AS(
int SafeActorNameLength(AActor Actor)
{
	if (Actor == nullptr)
	{
		PrintWarning("SafeActorNameLength: null actor");
		return 0;
	}

	return Actor.GetName().ToString().Len();
}

int SafeArrayRead(const TArray<int>&in Values, int Index, int DefaultValue)
{
	if (!Values.IsValidIndex(Index))
	{
		PrintWarning("SafeArrayRead: invalid index");
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

	PrintError("RetryPattern: failed");
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
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int SafeActorNameLength(AActor)"), 0, TEXT("null check should return fallback"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int EarlyReturnPattern(int)"), -10, TEXT("early return should reject invalid state"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int RetryPattern()"), 3, TEXT("retry loop should stop on first success"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int DefensivePatternSummary()"), 9, TEXT("defensive patterns should combine deterministic fallbacks"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
