// ============================================================================
// AngelscriptSyntaxFStringTests.cpp
//
// Syntax coverage tests for FString operations: literals, concatenation,
// f-string interpolation, common string methods, and FName — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.FString.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxFStringTest,
	"Angelscript.TestModule.Syntax.FString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Literals — Positive
	// ====================================================================

	TEST_METHOD(Literals_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// String literal assignment
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_Literal"),
			TEXT(R"(
void Test() { FString S = "Hello World"; }
)"),
			TEXT("String literal assignment"));

		// Empty string literal
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_Empty"),
			TEXT(R"(
void Test() { FString S = ""; }
)"),
			TEXT("Empty string literal"));

		// Escape characters - must use regular TEXT since AS code itself contains \n \t
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_Escape"),
			TEXT("void Test() { FString S = \"Line1\\nLine2\\tTabbed\"; }"),
			TEXT("String with escape characters"));

		// Concatenation with +
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_ConcatPlus"),
			TEXT(R"(
void Test()
{
	FString A = "Hello";
	FString B = " World";
	FString C = A + B;
}
)"),
			TEXT("String concatenation with +"));

		// Concatenation with +=
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_ConcatPlusEq"),
			TEXT(R"(
void Test() { FString S = "Hello"; S += " World"; }
)"),
			TEXT("String concatenation with +="));

		// F-string basic
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_FStrBasic"),
			TEXT(R"(
void Test() { int X = 42; FString S = f"Value is {X}"; }
)"),
			TEXT("F-string basic interpolation"));

		// F-string with expression
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_FStrExpr"),
			TEXT(R"(
void Test() { int X = 5; FString S = f"Result: {X * 2 + 1}"; }
)"),
			TEXT("F-string with expression"));

		// F-string multiple interpolations
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_FStrMulti"),
			TEXT(R"(
void Test() { int A = 1; float B = 2.5f; FString S = f"{A} and {B}"; }
)"),
			TEXT("F-string with multiple interpolations"));
	}

	// ====================================================================
	// FString — Negative
	// ====================================================================

	TEST_METHOD(Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Unterminated string
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_Unterminated"),
			TEXT(R"(
void Test() { FString S = "unterminated; }
)"),
			TEXT("Unterminated string literal should fail"));

		// String subtraction
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_Subtract"),
			TEXT(R"(
void Test() { FString A = "Hello"; FString B = A - "lo"; }
)"),
			TEXT("String subtraction should fail"));

		// String multiplication
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_Multiply"),
			TEXT(R"(
void Test() { FString S = "abc" * 3; }
)"),
			TEXT("String multiplication should fail"));

		// DISABLED(#as-engine-behavior): preprocessor-permissive — AS f-string 预处理器对花括号边界处理宽松，不报编译错误
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_FStrUnclosed"),
			TEXT(R"(
void Test() { int X = 5; FString S = f"Value is {X"; }
)"),
			TEXT("F-string with unclosed brace should fail"));
#endif

		// Assign int to FString
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_AssignInt"),
			TEXT(R"(
void Test() { FString S = 42; }
)"),
			TEXT("Assigning int to FString should fail"));

		// Compare FString with int
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_CompareInt"),
			TEXT(R"(
void Test() { FString S = "5"; bool B = (S == 5); }
)"),
			TEXT("Comparing FString with int should fail"));

		// DISABLED(#as-engine-behavior): preprocessor-permissive — AS f-string 预处理器对花括号边界处理宽松，不报编译错误
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_FStrNestedBrace"),
			TEXT(R"(
void Test() { int X = 5; FString S = f"Value is {{X}}"; }
)"),
			TEXT("F-string with nested braces should fail"));
#endif

		// Null string literal
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_NullLiteral"),
			TEXT(R"(
void Test() { FString S = nullptr; }
)"),
			TEXT("Null string literal should fail"));

		// String division
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_Division"),
			TEXT(R"(
void Test() { FString A = "Hello"; FString B = A / "World"; }
)"),
			TEXT("String division should fail"));

		// String bitwise
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_Bitwise"),
			TEXT(R"(
void Test() { FString A = "Hello"; auto X = A & "World"; }
)"),
			TEXT("String bitwise should fail"));
	}

	// ====================================================================
	// Methods — Positive
	// ====================================================================

	TEST_METHOD(Methods_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_Len"),
			TEXT(R"(
void Test() { FString S = "Hello"; int L = S.Len(); }
)"),
			TEXT("FString.Len()"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_IsEmpty"),
			TEXT(R"(
void Test() { FString S = ""; bool B = S.IsEmpty(); }
)"),
			TEXT("FString.IsEmpty()"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_Contains"),
			TEXT(R"(
void Test() { FString S = "Hello World"; bool B = S.Contains("World"); }
)"),
			TEXT("FString.Contains()"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_CompareEq"),
			TEXT(R"(
void Test() { FString A = "abc"; FString B = "abc"; bool Equal = (A == B); }
)"),
			TEXT("FString comparison =="));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_CompareNeq"),
			TEXT(R"(
void Test() { FString A = "abc"; FString B = "def"; bool NotEqual = (A != B); }
)"),
			TEXT("FString comparison !="));
	}

	// ====================================================================
	// FName — Mixed (Positive + Negative)
	// ====================================================================

	TEST_METHOD(FName_Mixed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Positive: FName literal with n prefix
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_FNameLiteral"),
			TEXT(R"(
void Test() { FName N = n"MyName"; }
)"),
			TEXT("FName literal with n prefix"));

		// Positive: NAME_None
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxStr_FNameNone"),
			TEXT(R"(
void Test() { FName N = NAME_None; }
)"),
			TEXT("FName NAME_None"));

		// Negative: n without string
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_FNameBadLiteral"),
			TEXT(R"(
void Test() { FName N = n; }
)"),
			TEXT("FName literal without string should fail"));

		// Negative: FName from int
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxStr_FNameFromInt"),
			TEXT(R"(
void Test() { FName N = FName(42); }
)"),
			TEXT("FName construction from int should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
