#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageOperatorOverloadTests
// -----------------------------------------------------------------------------
// Coverage landing file for custom operator overload runtime behavior. Syntax
// declaration coverage remains in Syntax/AngelscriptSyntaxOperatorOverloadTests.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageOperatorOverloadTest,
	"Angelscript.TestModule.Coverage.OperatorOverload",
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

	TEST_METHOD(ArithmeticComparisonAndAssignmentOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageOperatorOverload_ArithmeticComparison", ASTEST_AS(R"AS(
struct FScoreValue
{
	int Value = 0;

	FScoreValue opAdd(const FScoreValue& Other) const
	{
		FScoreValue Result;
		Result.Value = Value + Other.Value;
		return Result;
	}

	FScoreValue opSub(const FScoreValue& Other) const
	{
		FScoreValue Result;
		Result.Value = Value - Other.Value;
		return Result;
	}

	FScoreValue opMul(int Scale) const
	{
		FScoreValue Result;
		Result.Value = Value * Scale;
		return Result;
	}

	FScoreValue& opAddAssign(const FScoreValue& Other)
	{
		Value += Other.Value;
		return this;
	}

	bool opEquals(const FScoreValue& Other) const
	{
		return Value == Other.Value;
	}

	int opCmp(const FScoreValue& Other) const
	{
		if (Value < Other.Value) return -1;
		if (Value > Other.Value) return 1;
		return 0;
	}
}

FScoreValue MakeScore(int Value)
{
	FScoreValue Result;
	Result.Value = Value;
	return Result;
}

int ArithmeticOperators()
{
	FScoreValue A = MakeScore(10);
	FScoreValue B = MakeScore(3);
	FScoreValue Sum = A + B;
	FScoreValue Difference = A - B;
	FScoreValue Product = B * 4;
	return Sum.Value * 100 + Difference.Value * 10 + Product.Value;
}

int CompoundAssignmentOperator()
{
	FScoreValue A = MakeScore(5);
	FScoreValue B = MakeScore(8);
	A += B;
	return A.Value;
}

bool EqualityOperator()
{
	return MakeScore(9) == MakeScore(9);
}

bool ComparisonOperators()
{
	FScoreValue Low = MakeScore(1);
	FScoreValue High = MakeScore(4);
	return Low < High && High > Low && Low <= MakeScore(1) && High >= MakeScore(4);
}
)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ArithmeticOperators()"), 1382, TEXT("custom arithmetic operators should execute"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int CompoundAssignmentOperator()"), 13, TEXT("custom += should mutate lhs"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool EqualityOperator()"), true, TEXT("custom opEquals should drive =="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool ComparisonOperators()"), true, TEXT("custom opCmp should drive ordering"));
	}

	TEST_METHOD(UnaryIndexAndConversionOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageOperatorOverload_IndexConversion", ASTEST_AS(R"AS(
struct FIndexedScores
{
	TArray<int> Values;

	int opIndex(int Index) const
	{
		return Values[Index];
	}
}

struct FUnaryScore
{
	int Value = 0;

	FUnaryScore opNeg() const
	{
		FUnaryScore Result;
		Result.Value = -Value;
		return Result;
	}

	int opImplConv() const
	{
		return Value;
	}
}

int IndexOperator()
{
	FIndexedScores Scores;
	Scores.Values.Add(2);
	Scores.Values.Add(4);
	Scores.Values.Add(6);
	return Scores[0] + Scores[1] + Scores[2];
}

int UnaryOperator()
{
	FUnaryScore Score;
	Score.Value = 14;
	FUnaryScore Negated = -Score;
	return Negated.Value;
}

int ExplicitConversionOperator()
{
	FUnaryScore Score;
	Score.Value = 19;
	return int(Score);
}
)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int IndexOperator()"), 12, TEXT("custom opIndex should read indexed values"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int UnaryOperator()"), -14, TEXT("custom opNeg should drive unary minus"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ExplicitConversionOperator()"), 19, TEXT("custom opImplConv should drive explicit conversion"));
	}

	TEST_METHOD(OperatorNegativeCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASCoverageOperatorOverload_NoPlus"),
			TEXT(R"(
struct FNoPlus
{
	int Value = 0;
}

void Test()
{
	FNoPlus A;
	FNoPlus B;
	FNoPlus C = A + B;
}
)"),
			TEXT("using + without opAdd should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASCoverageOperatorOverload_DuplicateOpAdd"),
			TEXT(R"(
struct FDuplicateOp
{
	FDuplicateOp opAdd(const FDuplicateOp& Other) const { return FDuplicateOp(); }
	FDuplicateOp opAdd(const FDuplicateOp& Other) const { return FDuplicateOp(); }
}
)"),
			TEXT("duplicate operator overload declaration should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
