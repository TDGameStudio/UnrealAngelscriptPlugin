// ============================================================================
// AngelscriptSyntaxMathModuleTests.cpp
//
// Syntax coverage tests for Math module functions, vector/rotator math,
// and numeric literals — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.MathModule.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxMathModuleTest,
	"Angelscript.TestModule.Syntax.MathModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Math Functions — Positive
	// ====================================================================

	TEST_METHOD(Functions_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathAbs"),
			TEXT(R"(
void Test() { float X = Math::Abs(-5.0f); int Y = Math::Abs(-3); }
)"),
			TEXT("Math::Abs"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathMin"),
			TEXT(R"(
void Test() { int X = Math::Min(3, 5); float Y = Math::Min(1.0f, 2.0f); }
)"),
			TEXT("Math::Min"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathMax"),
			TEXT(R"(
void Test() { int X = Math::Max(3, 5); float Y = Math::Max(1.0f, 2.0f); }
)"),
			TEXT("Math::Max"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathClamp"),
			TEXT(R"(
void Test() { float X = Math::Clamp(1.5f, 0.0f, 1.0f); int Y = Math::Clamp(15, 0, 10); }
)"),
			TEXT("Math::Clamp"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathSqrt"),
			TEXT(R"(
void Test() { float X = Math::Sqrt(4.0f); }
)"),
			TEXT("Math::Sqrt"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathPow"),
			TEXT(R"(
void Test() { float X = Math::Pow(2.0f, 3.0f); }
)"),
			TEXT("Math::Pow"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathSinCosTan"),
			TEXT(R"(
void Test()
{
	float S = Math::Sin(1.0f);
	float C = Math::Cos(1.0f);
	float T = Math::Tan(1.0f);
}
)"),
			TEXT("Math::Sin/Cos/Tan"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathFloorCeil"),
			TEXT(R"(
void Test() { int F = Math::FloorToInt(3.7f); int C = Math::CeilToInt(3.2f); }
)"),
			TEXT("Math::FloorToInt/CeilToInt"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLerp"),
			TEXT(R"(
void Test() { float X = Math::Lerp(0.0f, 10.0f, 0.5f); }
)"),
			TEXT("Math::Lerp"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathRandRange"),
			TEXT(R"(
void Test() { int X = Math::RandRange(0, 100); float Y = Math::RandRange(0.0f, 1.0f); }
)"),
			TEXT("Math::RandRange"));
	}

	// ====================================================================
	// Math Functions — Negative
	// ====================================================================

	TEST_METHOD(Functions_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathBadFunc"),
			TEXT(R"(
void Test() { float X = Math::NonExistentFunc(1.0f); }
)"),
			TEXT("Non-existent Math function should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathBadArgType"),
			TEXT(R"(
void Test() { float X = Math::Sqrt("hello"); }
)"),
			TEXT("Math function with wrong arg type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathBadArgCount"),
			TEXT(R"(
void Test() { float X = Math::Min(1.0f); }
)"),
			TEXT("Math function with wrong arg count should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathBadReturn"),
			TEXT(R"(
void Test() { FString X = Math::Abs(-5.0f); }
)"),
			TEXT("Assigning math result to incompatible type should fail"));
	}

	// ====================================================================
	// Vector Math — Positive
	// ====================================================================

	TEST_METHOD(Vector_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathVecConstruct"),
			TEXT(R"(
void Test() { FVector V = FVector(1.0f, 2.0f, 3.0f); }
)"),
			TEXT("FVector construction"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathVecOps"),
			TEXT(R"(
void Test()
{
	FVector A = FVector(1, 0, 0);
	FVector B = FVector(0, 1, 0);
	FVector C = A + B;
	FVector D = A - B;
	FVector E = A * 2.0f;
}
)"),
			TEXT("FVector arithmetic operations"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathVecMembers"),
			TEXT(R"(
void Test()
{
	FVector V = FVector(1, 2, 3);
	float X = V.X;
	float Y = V.Y;
	float Z = V.Z;
}
)"),
			TEXT("FVector member access"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathVecMethods"),
			TEXT(R"(
void Test()
{
	FVector V = FVector(3, 4, 0);
	float Len = V.Size();
	FVector N = V.GetSafeNormal();
}
)"),
			TEXT("FVector methods"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathVecDotCross"),
			TEXT(R"(
void Test()
{
	FVector A = FVector(1, 0, 0);
	FVector B = FVector(0, 1, 0);
	float Dot = A.DotProduct(B);
	FVector Cross = A.CrossProduct(B);
}
)"),
			TEXT("FVector dot and cross product"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathVecConstants"),
			TEXT(R"(
void Test()
{
	FVector Zero = FVector::ZeroVector;
	FVector One = FVector::OneVector;
	FVector Up = FVector::UpVector;
}
)"),
			TEXT("FVector static constants"));
	}

	// ====================================================================
	// Vector Math — Negative
	// ====================================================================

	TEST_METHOD(Vector_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathVecAddRot"),
			TEXT(R"(
void Test()
{
	FVector V = FVector(1, 0, 0);
	FRotator R = FRotator(45, 0, 0);
	FVector X = V + R;
}
)"),
			TEXT("FVector + FRotator should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathVecBadConstruct"),
			TEXT(R"(
void Test() { FVector V = FVector(1, 2); }
)"),
			TEXT("FVector with 2 args should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathVecBadMember"),
			TEXT(R"(
void Test() { FVector V = FVector(1, 0, 0); float W = V.W; }
)"),
			TEXT("Accessing non-existent FVector member should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathVecMulVec"),
			TEXT(R"(
void Test()
{
	FVector A = FVector(1, 0, 0);
	FVector B = FVector(0, 1, 0);
	float X = A * B;
}
)"),
			TEXT("FVector * FVector to float should fail"));
	}

	// ====================================================================
	// Numeric Literals — Positive
	// ====================================================================

	TEST_METHOD(Literals_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitDec"),
			TEXT(R"(
void Test() { int X = 42; }
)"),
			TEXT("Decimal integer literal"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitHex"),
			TEXT(R"(
void Test() { int X = 0xFF; }
)"),
			TEXT("Hexadecimal integer literal"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitBin"),
			TEXT(R"(
void Test() { int X = 0b1010; }
)"),
			TEXT("Binary integer literal"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitOct"),
			TEXT(R"(
void Test() { int X = 0o77; }
)"),
			TEXT("Octal integer literal"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitFloat"),
			TEXT(R"(
void Test() { float X = 3.14f; float Y = .5f; float Z = 1.0e5f; }
)"),
			TEXT("Float literal formats"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitNeg"),
			TEXT(R"(
void Test() { int X = -42; float Y = -3.14f; }
)"),
			TEXT("Negative literals"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMathLitLarge"),
			TEXT(R"(
void Test() { int64 X = 9223372036854775807; }
)"),
			TEXT("Large int64 literal"));
	}

	// ====================================================================
	// Numeric Literals — Negative
	// ====================================================================

	TEST_METHOD(Literals_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathLitBadHex"),
			TEXT(R"(
void Test() { int X = 0xGG; }
)"),
			TEXT("Invalid hex literal should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathLitBadBin"),
			TEXT(R"(
void Test() { int X = 0b123; }
)"),
			TEXT("Invalid binary literal should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMathLitBadFloat"),
			TEXT(R"(
void Test() { float X = 3.14.15f; }
)"),
			TEXT("Multiple decimal points should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
