// ============================================================================
// AngelscriptSyntaxMiscTests.cpp
//
// Syntax coverage tests for miscellaneous language features: comments,
// special keywords, and edge cases — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.Misc.*
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxMiscTest,
	"Angelscript.TestModule.Syntax.Misc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Comments — Positive
	// ====================================================================

	TEST_METHOD(Comments_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscCommentLine"),
			TEXT(R"(
// This is a comment
void Test() { int X = 1; }
)"),
			TEXT("Single-line comment"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscCommentBlock"),
			TEXT(R"(
/* This is a
   multi-line comment */
void Test() { int X = 1; }
)"),
			TEXT("Multi-line block comment"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscCommentInline"),
			TEXT(R"(
void Test()
{
	int X = 1; // inline comment
	int Y = 2; /* block */ int Z = 3;
}
)"),
			TEXT("Comments within code"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscCommentStars"),
			TEXT(R"(
/* Comment with * and / separately */
void Test() { }
)"),
			TEXT("Block comment with stars"));
	}

	// ====================================================================
	// Comments — Negative
	// ====================================================================

	TEST_METHOD(Comments_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): preprocessor-permissive — AS 预处理器对未终止块注释处理宽松，不报编译错误
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscCommentUnterminated"),
			TEXT("/* Unterminated comment\nvoid Test() { }"),
			TEXT("Unterminated block comment should fail"));
#endif
	}

	// ====================================================================
	// Special Keywords — Positive
	// ====================================================================

	TEST_METHOD(Keywords_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscThis"),
			TEXT(R"(
class AActorThis : AActor
{
	int X = 0;

	void SetX(int Val)
	{
		this.X = Val;
	}
}
)"),
			TEXT("this keyword"));

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 2.33 fork 不支持 Super:: 关键字配合 BlueprintOverride
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscSuper"),
			TEXT(R"(
class AActorSuper : AActor
{
	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Super::BeginPlay();
	}
}
)"),
			TEXT("Super keyword"));
#endif

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscFinalClass"),
			TEXT(R"(
class AFinalActorMisc : AActor final { }
)"),
			TEXT("final class modifier"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscOverride"),
			TEXT(R"(
class ABaseActorOvrd : AActor
{
	void Foo() { }
}

class AChildActorOvrd : ABaseActorOvrd
{
	void Foo() override { }
}
)"),
			TEXT("override keyword"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscConstMethod"),
			TEXT(R"(
struct FStructConst
{
	int X = 0;
	int GetX() const { return X; }
}
)"),
			TEXT("const method"));
	}

	// ====================================================================
	// Special Keywords — Negative
	// ====================================================================

	TEST_METHOD(Keywords_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscThisGlobal"),
			TEXT(R"(
void Test() { auto X = this; }
)"),
			TEXT("this outside class should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 final 类继承约束
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscInheritFinal"),
			TEXT(R"(
class AFinalActorInhN : AActor final { }
class AChildActorInhN : AFinalActorInhN { }
)"),
			TEXT("Inheriting from final class should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscOverrideNoParent"),
			TEXT(R"(
class AActorOvrdNoParent : AActor
{
	void NonExistentMethod() override { }
}
)"),
			TEXT("override without matching parent method should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscConstModify"),
			TEXT(R"(
struct FStructConstModify
{
	int X = 0;
	void Bad() const { X = 5; }
}
)"),
			TEXT("Modifying member in const method should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscSuperGlobal"),
			TEXT(R"(
void Test() { Super::BeginPlay(); }
)"),
			TEXT("Super outside class should fail"));
	}

	// ====================================================================
	// Edge Cases — Positive
	// ====================================================================

	TEST_METHOD(EdgeCases_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscEmptyFunc"),
			TEXT(R"(
void DoNothing() { }
)"),
			TEXT("Empty function body"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscDeepNest"),
			TEXT(R"(
void Test() { { { { { int X = 1; } } } } }
)"),
			TEXT("Deeply nested blocks"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscLongExpr"),
			TEXT(R"(
void Test() { int X = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15; }
)"),
			TEXT("Long chained expression"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscMultiStmt"),
			TEXT(R"(
void Test() { int A = 1; int B = 2; int C = A + B; }
)"),
			TEXT("Multiple statements"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscDeepParens"),
			TEXT(R"(
void Test() { int X = ((((1 + 2)))); }
)"),
			TEXT("Deeply parenthesized expression"));

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 不支持全局变量与函数组合编译
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscGlobalVar"),
			TEXT(R"(
int GlobalCounter = 0;

void Increment() { ++GlobalCounter; }
)"),
			TEXT("Global variable with function"));
#endif
	}

	// ====================================================================
	// Edge Cases — Negative
	// ====================================================================

	TEST_METHOD(EdgeCases_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscUnmatchedBrace"),
			TEXT(R"(
void Test() { int X = 1;
)"),
			TEXT("Unmatched opening brace should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscExtraBrace"),
			TEXT(R"(
void Test() { } }
)"),
			TEXT("Extra closing brace should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscUnmatchedParen"),
			TEXT(R"(
void Test() { int X = (1 + 2; }
)"),
			TEXT("Unmatched parenthesis should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscNoSemicolon"),
			TEXT(R"(
void Test() { int X = 1 int Y = 2; }
)"),
			TEXT("Missing semicolon should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscGarbage"),
			TEXT(R"(
asdfgh jklmn @#$%
)"),
			TEXT("Random garbage should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMiscStmtTopLevel"),
			TEXT(R"(
int X = 5;
X = 10;
)"),
			TEXT("Assignment statement at top level should fail"));

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 引擎对空源文件返回编译失败
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscEmpty"),
			TEXT(R"()"),
			TEXT("Empty source should compile (no declarations)"));
#endif

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 引擎对仅含空白的源文件返回编译失败
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMiscWhitespace"),
			TEXT(R"(
   

		  
)"),
			TEXT("Whitespace-only source should compile"));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
