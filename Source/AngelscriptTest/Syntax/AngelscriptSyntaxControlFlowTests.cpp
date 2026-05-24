// ============================================================================
// AngelscriptSyntaxControlFlowTests.cpp
//
// Syntax coverage tests for AngelScript control flow — CQTest refactor.
// Tests if/else, for, while, do-while, switch/case, break, continue,
// foreach, fallthrough, and return statements.
//
// Automation prefix: Angelscript.TestModule.Syntax.ControlFlow.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxControlFlowTest,
	"Angelscript.TestModule.Syntax.ControlFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// If/Else — Positive
	// ====================================================================

	TEST_METHOD(IfElse_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASSyntax_ControlFlow_IfPos"), TEXT(R"(
int BasicIf()      { if (true) { return 1; } return 0; }
int IfElse()       { if (false) { return 1; } else { return 2; } }
int IfElseIf()     { int X = 5; if (X > 10) { return 1; } else if (X > 3) { return 2; } else { return 3; } }
int Nested()       { if (true) { if (true) { return 1; } } return 0; }
int NoBrace()      { int X = 0; if (true) X = 1; else X = 2; return X; }
int Complex()      { int A = 1; int B = 2; if (A > 0 && B > 0) { return A + B; } return 0; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BasicIf()"),  TEXT("if(true) => 1"),     1 },
			{ TEXT("int IfElse()"),   TEXT("if(false) else => 2"), 2 },
			{ TEXT("int IfElseIf()"), TEXT("else if chain"),     2 },
			{ TEXT("int Nested()"),   TEXT("nested if"),         1 },
			{ TEXT("int NoBrace()"),  TEXT("if without braces"), 1 },
			{ TEXT("int Complex()"),  TEXT("complex condition"), 3 },
		};
		AngelscriptTestBindings::ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// If/Else — Negative
	// ====================================================================

	TEST_METHOD(IfElse_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_NonBool"),
			TEXT(R"(
void Test() { if (5) { } }
)"),
			TEXT("Non-bool condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_NoParen"),
			TEXT(R"(
void Test() { if true { } }
)"),
			TEXT("Missing parentheses"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_Empty"),
			TEXT(R"(
void Test() { if () { } }
)"),
			TEXT("Empty condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_ElseNoIf"),
			TEXT(R"(
void Test() { else { } }
)"),
			TEXT("Else without if"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_IntCond"),
			TEXT(R"(
void Test() { int X = 0; if (X) { } }
)"),
			TEXT("Integer as condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_FloatCond"),
			TEXT(R"(
void Test() { if (1.0f) { } }
)"),
			TEXT("Float as condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("IfN_StringCond"),
			TEXT(R"(
void Test() { if ("hello") { } }
)"),
			TEXT("String as condition"));
	}

	// ====================================================================
	// For Loop — Positive
	// ====================================================================

	TEST_METHOD(For_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASSyntax_ControlFlow_ForPos"), TEXT(R"(
int BasicFor()     { int S = 0; for (int I = 0; I < 5; ++I) { S += I; } return S; }
int Decrement()    { int S = 0; for (int I = 3; I > 0; --I) { S += I; } return S; }
int Empty()        { int I = 0; for (;;) { if (I >= 3) break; ++I; } return I; }
int Nested()       { int S = 0; for (int I = 0; I < 3; ++I) { for (int J = 0; J < 2; ++J) { ++S; } } return S; }
int CompoundStep() { int S = 0; for (int I = 0; I < 100; I += 25) { ++S; } return S; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BasicFor()"),     TEXT("sum 0..4 = 10"),  10 },
			{ TEXT("int Decrement()"),    TEXT("sum 3+2+1 = 6"),   6 },
			{ TEXT("int Empty()"),        TEXT("for(;;) + break"), 3 },
			{ TEXT("int Nested()"),       TEXT("3x2 = 6 iters"),   6 },
			{ TEXT("int CompoundStep()"), TEXT("step by 25"),       4 },
		};
		AngelscriptTestBindings::ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// For Loop — Negative
	// ====================================================================

	TEST_METHOD(For_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForN_NoSemicolon"),
			TEXT(R"(
void Test() { for (int I = 0 I < 10 ++I) { } }
)"),
			TEXT("For without semicolons"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForN_NonBoolCond"),
			TEXT(R"(
void Test() { for (int I = 0; I; ++I) { } }
)"),
			TEXT("Non-bool condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForN_NoParen"),
			TEXT(R"(
void Test() { for int I = 0; I < 10; ++I { } }
)"),
			TEXT("For without parentheses"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForN_TooFew"),
			TEXT(R"(
void Test() { for (int I = 0; I < 10) { } }
)"),
			TEXT("For with only two clauses"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForN_VarAfterLoop"),
			TEXT(R"(
void Test() { for (int I = 0; I < 5; ++I) { } int X = I; }
)"),
			TEXT("Access loop var after loop"));
	}

	// ====================================================================
	// While / Do-While — Positive
	// ====================================================================

	TEST_METHOD(While_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASSyntax_ControlFlow_WhilePos"), TEXT(R"(
int BasicWhile()  { int I = 0; while (I < 5) { ++I; } return I; }
int WhileBreak()  { int I = 0; while (true) { if (I >= 3) break; ++I; } return I; }
int DoWhile()     { int I = 0; do { ++I; } while (I < 5); return I; }
int DoWhileOnce() { int X = 0; do { X = 42; } while (false); return X; }
int Nested()      { int S = 0; int I = 0; while (I < 3) { int J = 0; while (J < 2) { ++S; ++J; } ++I; } return S; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BasicWhile()"),  TEXT("while I<5"),       5 },
			{ TEXT("int WhileBreak()"),  TEXT("while+break at 3"), 3 },
			{ TEXT("int DoWhile()"),     TEXT("do-while I<5"),    5 },
			{ TEXT("int DoWhileOnce()"), TEXT("do-while(false)"), 42 },
			{ TEXT("int Nested()"),      TEXT("nested while 3x2"), 6 },
		};
		AngelscriptTestBindings::ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// While / Do-While — Negative
	// ====================================================================

	TEST_METHOD(While_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("WhileN_NonBool"),
			TEXT(R"(
void Test() { while (5) { } }
)"),
			TEXT("Non-bool while condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("WhileN_NoParen"),
			TEXT(R"(
void Test() { while true { } }
)"),
			TEXT("While without parentheses"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("WhileN_Empty"),
			TEXT(R"(
void Test() { while () { } }
)"),
			TEXT("While with empty condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("WhileN_DoNoSemi"),
			TEXT(R"(
void Test() { do { } while (true) }
)"),
			TEXT("Do-while missing semicolon"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("WhileN_IntCond"),
			TEXT(R"(
void Test() { int X = 1; while (X) { break; } }
)"),
			TEXT("Integer as while condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("WhileN_DoIntCond"),
			TEXT(R"(
void Test() { do { } while (1); }
)"),
			TEXT("Integer as do-while condition"));
	}

	// ====================================================================
	// Switch/Case — Positive
	// ====================================================================

	TEST_METHOD(Switch_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASSyntax_ControlFlow_SwitchPos"), TEXT(R"(
int BasicSwitch()  { int X = 1; switch(X) { case 0: return 0; case 1: return 1; default: return -1; } }
int Fallthrough()  { int X = 0; int Y = 0; switch(X) { case 0: fallthrough; case 1: Y = 10; break; default: Y = 20; break; } return Y; }
int MultiCase()    { int X = 1; switch(X) { case 0: case 1: case 2: return 99; default: return 0; } }
int DefaultOnly()  { int X = 42; switch(X) { case 0: return 0; default: return 100; } }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BasicSwitch()"), TEXT("switch case 1"),      1 },
			{ TEXT("int Fallthrough()"), TEXT("fallthrough 0->1"),  10 },
			{ TEXT("int MultiCase()"),   TEXT("multi-case"),        99 },
			{ TEXT("int DefaultOnly()"), TEXT("default only"),     100 },
		};
		AngelscriptTestBindings::ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Switch/Case — Negative
	// ====================================================================

	TEST_METHOD(Switch_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_DupCase"),
			TEXT(R"(
void Test() { int X = 1; switch(X) { case 1: break; case 1: break; } }
)"),
			TEXT("Duplicate case labels"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_NonConst"),
			TEXT(R"(
void Test() { int X = 1; int Y = 2; switch(X) { case Y: break; } }
)"),
			TEXT("Non-constant case expression"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_MultiDefault"),
			TEXT(R"(
void Test() { int X = 1; switch(X) { default: break; default: break; } }
)"),
			TEXT("Multiple default labels"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_CaseOutside"),
			TEXT(R"(
void Test() { case 1: int X = 0; }
)"),
			TEXT("Case label outside switch"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_NoBrace"),
			TEXT(R"(
void Test() { int X = 1; switch(X) case 0: break; }
)"),
			TEXT("Switch without braces"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_FloatCase"),
			TEXT(R"(
void Test() { int X = 1; switch(X) { case 1.5f: break; } }
)"),
			TEXT("Float as case value"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("SwitchN_StrCase"),
			TEXT(R"(
void Test() { int X = 1; switch(X) { case "hello": break; } }
)"),
			TEXT("String as case value for int switch"));
	}

	// ====================================================================
	// Break / Continue — Negative (outside loops)
	// ====================================================================

	TEST_METHOD(BreakContinue_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine,
			TEXT("BCN_BreakOutside"),
			TEXT(R"(
void Test() { break; }
)"),
			TEXT("Invalid 'break'"),
			TEXT("Break outside loop"));

		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine,
			TEXT("BCN_ContinueOutside"),
			TEXT(R"(
void Test() { continue; }
)"),
			TEXT("Invalid 'continue'"),
			TEXT("Continue outside loop"));

		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine,
			TEXT("BCN_BreakInIf"),
			TEXT(R"(
void Test() { if (true) { break; } }
)"),
			TEXT("Invalid 'break'"),
			TEXT("Break in if but not loop"));

		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine,
			TEXT("BCN_ContinueInIf"),
			TEXT(R"(
void Test() { if (true) { continue; } }
)"),
			TEXT("Invalid 'continue'"),
			TEXT("Continue in if but not loop"));

		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine,
			TEXT("BCN_BreakInFunc"),
			TEXT(R"(
void Foo() { break; }

void Test()
{
	for(int I=0;I<5;++I) { Foo(); }
}
)"),
			TEXT("Invalid 'break'"),
			TEXT("Break inside function called from loop"));
	}

	// ====================================================================
	// Foreach — Positive
	// ====================================================================

	TEST_METHOD(Foreach_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASSyntax_ControlFlow_ForeachPos"), TEXT(R"(
int BasicForeach()
{
	TArray<int> Arr;
	Arr.Add(1); Arr.Add(2); Arr.Add(3);
	int Sum = 0;
	for (int Val : Arr) { Sum += Val; }
	return Sum;
}

int ForeachBreak()
{
	TArray<int> Arr;
	Arr.Add(1); Arr.Add(2); Arr.Add(3); Arr.Add(4);
	int Sum = 0;
	for (int Val : Arr) { if (Val > 2) break; Sum += Val; }
	return Sum;
}

int ForeachContinue()
{
	TArray<int> Arr;
	Arr.Add(1); Arr.Add(2); Arr.Add(3);
	int Sum = 0;
	for (int Val : Arr) { if (Val == 2) continue; Sum += Val; }
	return Sum;
}
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BasicForeach()"),    TEXT("sum 1+2+3 = 6"),  6 },
			{ TEXT("int ForeachBreak()"),    TEXT("break at >2"),    3 },
			{ TEXT("int ForeachContinue()"), TEXT("skip 2 => 1+3"), 4 },
		};
		AngelscriptTestBindings::ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Foreach — Negative
	// ====================================================================

	TEST_METHOD(Foreach_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForeachN_NonIter"),
			TEXT(R"(
void Test() { int X = 5; for (int Val : X) { } }
)"),
			TEXT("Foreach over non-iterable"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForeachN_TypeMismatch"),
			TEXT(R"(
void Test() { TArray<int> Arr; for (FString Val : Arr) { } }
)"),
			TEXT("Foreach element type mismatch"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForeachN_NoColon"),
			TEXT(R"(
void Test() { TArray<int> Arr; for (int Val Arr) { } }
)"),
			TEXT("Foreach missing colon"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForeachN_OverInt"),
			TEXT(R"(
void Test() { for (int Val : 42) { } }
)"),
			TEXT("Foreach over literal int"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ForeachN_OverStr"),
			TEXT(R"(
void Test() { for (int Val : "hello") { } }
)"),
			TEXT("Foreach over string literal"));
	}

	// ====================================================================
	// Return — Positive and Negative
	// ====================================================================

	TEST_METHOD(Return_Mixed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Positive
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("RetP_Void"),
			TEXT(R"(
void Test() { return; }
)"),
			TEXT("Return void"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("RetP_Int"),
			TEXT(R"(
int Test() { return 42; }
)"),
			TEXT("Return int"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("RetP_Expr"),
			TEXT(R"(
int Test() { int X = 5; return X * 2 + 1; }
)"),
			TEXT("Return expression"));

		// Negative
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("RetN_TypeMismatch"),
			TEXT(R"(
int Test() { return "hello"; }
)"),
			TEXT("Return type mismatch"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("RetN_ValueInVoid"),
			TEXT(R"(
void Test() { return 5; }
)"),
			TEXT("Return value in void function"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("RetN_MissingValue"),
			TEXT(R"(
int Test() { return; }
)"),
			TEXT("Missing return value in non-void"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 float 作为 int 函数返回值（隐式缩窄）
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("RetN_FloatAsInt"),
			TEXT(R"(
int Test() { return 3.14f; }
)"),
			TEXT("Return float for int function"));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
