// ============================================================================
// AngelscriptSyntaxOperatorOverloadTests.cpp
//
// Syntax coverage tests for operator overloading declarations and usage
// — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.OperatorOverload.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxOperatorOverloadTest,
	"Angelscript.TestModule.Syntax.OperatorOverload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Operator Overload — Positive
	// ====================================================================

	TEST_METHOD(Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOAdd"),
			TEXT(R"(
struct FVecAdd
{
	int X = 0;
	int Y = 0;

	FVecAdd opAdd(const FVecAdd& Other) const
	{
		FVecAdd Result;
		Result.X = X + Other.X;
		Result.Y = Y + Other.Y;
		return Result;
	}
}
)"),
			TEXT("Operator overload: opAdd"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOSub"),
			TEXT(R"(
struct FVecSub
{
	int X = 0;
	int Y = 0;

	FVecSub opSub(const FVecSub& Other) const
	{
		FVecSub Result;
		Result.X = X - Other.X;
		Result.Y = Y - Other.Y;
		return Result;
	}
}
)"),
			TEXT("Operator overload: opSub"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOMul"),
			TEXT(R"(
struct FVecMul
{
	int X = 0;
	int Y = 0;

	FVecMul opMul(int Scalar) const
	{
		FVecMul Result;
		Result.X = X * Scalar;
		Result.Y = Y * Scalar;
		return Result;
	}
}
)"),
			TEXT("Operator overload: opMul"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOEquals"),
			TEXT(R"(
struct FVecEquals
{
	int X = 0;
	int Y = 0;

	bool opEquals(const FVecEquals& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}
}
)"),
			TEXT("Operator overload: opEquals"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOCmp"),
			TEXT(R"(
struct FValCmp
{
	int Value = 0;

	int opCmp(const FValCmp& Other) const
	{
		return Value - Other.Value;
	}
}
)"),
			TEXT("Operator overload: opCmp"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOIndex"),
			TEXT(R"(
struct FMyContainer
{
	TArray<int> Data;

	int opIndex(int Index) const
	{
		return Data[Index];
	}
}
)"),
			TEXT("Operator overload: opIndex"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOONeg"),
			TEXT(R"(
struct FVecNeg
{
	int X = 0;
	int Y = 0;

	FVecNeg opNeg() const
	{
		FVecNeg Result;
		Result.X = -X;
		Result.Y = -Y;
		return Result;
	}
}
)"),
			TEXT("Operator overload: opNeg (unary minus)"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOAddAssign"),
			TEXT(R"(
struct FVecAddAssign
{
	int X = 0;
	int Y = 0;

	FVecAddAssign& opAddAssign(const FVecAddAssign& Other)
	{
		X += Other.X;
		Y += Other.Y;
		return this;
	}
}
)"),
			TEXT("Operator overload: opAddAssign"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxOOUsage"),
			TEXT(R"(
struct FVecUsage
{
	int X = 0;
	int Y = 0;

	FVecUsage opAdd(const FVecUsage& Other) const
	{
		FVecUsage R;
		R.X = X + Other.X;
		R.Y = Y + Other.Y;
		return R;
	}

	bool opEquals(const FVecUsage& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}
}

void Test()
{
	FVecUsage A;
	FVecUsage B;
	A.X = 1;
	B.X = 2;
	FVecUsage C = A + B;
	bool Eq = (A == B);
}
)"),
			TEXT("Using overloaded operators"));
	}

	// ====================================================================
	// Operator Overload — Negative
	// ====================================================================

	TEST_METHOD(Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 operator overload 函数命名规范
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOInvalid"),
			TEXT(R"(
struct FVecInvalid
{
	int X = 0;

	FVecInvalid opInvalid(const FVecInvalid& Other) const
	{
		return FVecInvalid();
	}
}
)"),
			TEXT("Invalid operator overload name should fail"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 opEquals 返回类型必须为 bool
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOEqualsWrongReturn"),
			TEXT(R"(
struct FVecEqWrongRet
{
	int X = 0;

	int opEquals(const FVecEqWrongRet& Other) const { return 0; }
}
)"),
			TEXT("opEquals with non-bool return should fail"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 opCmp 返回类型必须为 int
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOCmpWrongReturn"),
			TEXT(R"(
struct FValCmpWrongRet
{
	int Value = 0;

	float opCmp(const FValCmpWrongRet& Other) const { return 0.0f; }
}
)"),
			TEXT("opCmp with non-int return should fail"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 允许全局作用域 operator overload
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOGlobal"),
			TEXT(R"(
int opAdd(int A, int B) { return A + B; }
)"),
			TEXT("Operator overload at global scope should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOONoOp"),
			TEXT(R"(
struct FMyType
{
	int X = 0;
}

void Test()
{
	FMyType A;
	FMyType B;
	FMyType C = A + B;
}
)"),
			TEXT("Using + without opAdd overload should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 opAdd 必须有参数
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOBadParams"),
			TEXT(R"(
struct FVecBadParams
{
	int X = 0;

	FVecBadParams opAdd() const { return FVecBadParams(); }
}
)"),
			TEXT("opAdd without parameter should fail"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 opAdd 不能返回 void
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOAddVoid"),
			TEXT(R"(
struct FVecAddVoid
{
	int X = 0;

	void opAdd(const FVecAddVoid& Other) const { }
}
)"),
			TEXT("opAdd returning void should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOODuplicateAdd"),
			TEXT(R"(
struct FVecDupAdd
{
	int X = 0;

	FVecDupAdd opAdd(const FVecDupAdd& Other) const { return FVecDupAdd(); }
	FVecDupAdd opAdd(const FVecDupAdd& Other) const { return FVecDupAdd(); }
}
)"),
			TEXT("Duplicate opAdd overload should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 opIndex 不能返回 void
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOOIndexBadReturn"),
			TEXT(R"(
struct FContainerBadRet
{
	TArray<int> Data;

	void opIndex(int Index) const { }
}
)"),
			TEXT("opIndex returning void should fail"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 opNeg 不能有参数
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxOONegWithParam"),
			TEXT(R"(
struct FVecNegParam
{
	int X = 0;

	FVecNegParam opNeg(int Dummy) const { return FVecNegParam(); }
}
)"),
			TEXT("opNeg with parameter should fail"));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
