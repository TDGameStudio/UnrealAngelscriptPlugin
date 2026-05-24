// ============================================================================
// AngelscriptSyntaxOperatorsTests.cpp
//
// Syntax coverage tests for AngelScript operators — CQTest refactor.
// Validates arithmetic, bitwise, logical, comparison, assignment, and ternary
// operators compile correctly in valid contexts, and produce proper diagnostics
// in invalid contexts.
//
// Automation prefix: Angelscript.TestModule.Syntax.Operators.*
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxOperatorsTest,
	"Angelscript.TestModule.Syntax.Operators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Arithmetic Operators — Positive
	// ====================================================================

	TEST_METHOD(Arithmetic_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_ArithPos"), TEXT(R"(
int AddInt()       { return 1 + 2; }
int SubInt()       { return 5 - 3; }
int MulInt()       { return 2 * 3; }
int DivInt()       { return 10 / 2; }
int ModInt()       { return 10 % 3; }
int AddFloat()     { float X = 1.0f + 2.5f; return int(X * 10); }
int UnaryNeg()     { int X = 5; return -X; }
int PreInc()       { int X = 0; ++X; return X; }
int PostInc()      { int X = 0; X++; return X; }
int PreDec()       { int X = 5; --X; return X; }
int PostDec()      { int X = 5; X--; return X; }
int CompoundExpr() { return (1 + 2) * 3 - 4 / 2 + 7 % 3; }
int MixedTypes()   { float X = 1 + 2.0f; return int(X * 10); }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int AddInt()"),       TEXT("1 + 2 = 3"),               3 },
			{ TEXT("int SubInt()"),       TEXT("5 - 3 = 2"),               2 },
			{ TEXT("int MulInt()"),       TEXT("2 * 3 = 6"),               6 },
			{ TEXT("int DivInt()"),       TEXT("10 / 2 = 5"),              5 },
			{ TEXT("int ModInt()"),       TEXT("10 % 3 = 1"),              1 },
			{ TEXT("int AddFloat()"),     TEXT("1.0 + 2.5 = 3.5 (*10)"),  35 },
			{ TEXT("int UnaryNeg()"),     TEXT("-5"),                      -5 },
			{ TEXT("int PreInc()"),       TEXT("++X => 1"),                1 },
			{ TEXT("int PostInc()"),      TEXT("X++ => 1"),                1 },
			{ TEXT("int PreDec()"),       TEXT("--X => 4"),                4 },
			{ TEXT("int PostDec()"),      TEXT("X-- => 4"),                4 },
			{ TEXT("int CompoundExpr()"), TEXT("(1+2)*3-4/2+7%3 = 8"),    8 },
			{ TEXT("int MixedTypes()"),   TEXT("int+float = 3.0 (*10)"),  30 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Arithmetic Operators — Negative
	// ====================================================================

	TEST_METHOD(Arithmetic_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Type mismatch: string + int
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_StrPlusInt"),
			TEXT(R"(
void Test() { int X = "hello" + 1; }
)"),
			TEXT("String + int type mismatch"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 bool 参与算术运算
#if 0
		// Bool arithmetic
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_BoolAdd"),
			TEXT(R"(
void Test() { bool A = true; bool B = false; int X = A + B; }
)"),
			TEXT("Bool addition"));
#endif

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 float 类型取模运算
#if 0
		// Float modulo
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_FloatMod"),
			TEXT(R"(
void Test() { float X = 10.0f % 3.0f; }
)"),
			TEXT("Float modulo"));
#endif

		// Missing operand
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_MissingOp"),
			TEXT(R"(
void Test() { int X = 1 + ; }
)"),
			TEXT("Missing right operand"));

		// Increment on literal
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_IncLiteral"),
			TEXT(R"(
void Test() { ++5; }
)"),
			TEXT("Increment on literal"));

		// Increment on const
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_IncConst"),
			TEXT(R"(
void Test() { const int X = 5; ++X; }
)"),
			TEXT("Increment on const"));

		// Double operator
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_DoubleOp"),
			TEXT(R"(
void Test() { int X = 1 ++ 2; }
)"),
			TEXT("Double operator in expression"));

		// Assign to expression result
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_AssignExpr"),
			TEXT(R"(
void Test() { int A = 1; int B = 2; (A + B) = 5; }
)"),
			TEXT("Assign to expression result"));

		// Unary plus on string
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_UnaryPlusStr"),
			TEXT(R"(
void Test() { FString S = +"hello"; }
)"),
			TEXT("Unary plus on string"));

		// Multiply string by int
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ArithN_StrMulInt"),
			TEXT(R"(
void Test() { auto S = "abc" * 3; }
)"),
			TEXT("String * int"));
	}

	// ====================================================================
	// Bitwise Operators — Positive
	// ====================================================================

	TEST_METHOD(Bitwise_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_BitPos"), TEXT(R"(
int BitAnd()      { return 0xFF & 0x0F; }
int BitOr()       { return 0xF0 | 0x0F; }
int BitXor()      { return 0xFF ^ 0x0F; }
int BitNot()      { return ~0 & 0xFF; }
int ShiftLeft()   { return 1 << 4; }
int ShiftRight()  { return 16 >> 2; }
int Compound()    { return (0xFF & 0x0F) | (0xF0 ^ 0x0F); }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BitAnd()"),     TEXT("0xFF & 0x0F = 15"),   15 },
			{ TEXT("int BitOr()"),      TEXT("0xF0 | 0x0F = 255"), 255 },
			{ TEXT("int BitXor()"),     TEXT("0xFF ^ 0x0F = 240"), 240 },
			{ TEXT("int BitNot()"),     TEXT("~0 & 0xFF = 255"),   255 },
			{ TEXT("int ShiftLeft()"),  TEXT("1 << 4 = 16"),        16 },
			{ TEXT("int ShiftRight()"), TEXT("16 >> 2 = 4"),         4 },
			{ TEXT("int Compound()"),   TEXT("compound bitwise"),  255 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Bitwise Operators — Negative
	// ====================================================================

	TEST_METHOD(Bitwise_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_AndFloat"),
			TEXT(R"(
void Test() { float X = 1.0f & 2.0f; }
)"),
			TEXT("Bitwise AND on float"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_OrFloat"),
			TEXT(R"(
void Test() { float X = 1.0f | 2.0f; }
)"),
			TEXT("Bitwise OR on float"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_ShiftFloat"),
			TEXT(R"(
void Test() { float X = 1.0f << 2; }
)"),
			TEXT("Shift on float"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_NotStr"),
			TEXT(R"(
void Test() { auto S = ~"hello"; }
)"),
			TEXT("Bitwise NOT on string"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_MissingOp"),
			TEXT(R"(
void Test() { int X = 0xFF & ; }
)"),
			TEXT("Missing operand in bitwise AND"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 bool 类型位运算
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_XorBool"),
			TEXT(R"(
void Test() { bool A = true; bool B = false; int X = A ^ B; }
)"),
			TEXT("Bitwise XOR on bool"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("BitN_ShiftStr"),
			TEXT(R"(
void Test() { auto X = "abc" >> 2; }
)"),
			TEXT("Shift on string"));
	}

	// ====================================================================
	// Logical Operators — Positive
	// ====================================================================

	TEST_METHOD(Logical_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_LogicPos"), TEXT(R"(
int LogicAnd()      { return (true && true) ? 1 : 0; }
int LogicOr()       { return (false || true) ? 1 : 0; }
int LogicNot()      { return (!false) ? 1 : 0; }
int LogicCompound() { return ((true && !false) || (false && true)) ? 1 : 0; }
int ShortCircuit()  { bool A = false; int Z = 0; return (A && (1/Z > 0)) ? 1 : 0; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int LogicAnd()"),      TEXT("true && true"),      1 },
			{ TEXT("int LogicOr()"),       TEXT("false || true"),     1 },
			{ TEXT("int LogicNot()"),      TEXT("!false"),            1 },
			{ TEXT("int LogicCompound()"), TEXT("compound logic"),    1 },
			{ TEXT("int ShortCircuit()"),  TEXT("short-circuit &&"),  0 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Logical Operators — Negative
	// ====================================================================

	TEST_METHOD(Logical_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("LogicN_AndInt"),
			TEXT(R"(
void Test() { int X = 1 && 2; }
)"),
			TEXT("Logical AND on integers"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("LogicN_NotInt"),
			TEXT(R"(
void Test() { int X = !5; }
)"),
			TEXT("Logical NOT on integer"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("LogicN_Missing"),
			TEXT(R"(
void Test() { bool X = true && ; }
)"),
			TEXT("Missing right operand"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("LogicN_TripleAnd"),
			TEXT(R"(
void Test() { bool X = true &&& false; }
)"),
			TEXT("Triple & is invalid"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("LogicN_OrStr"),
			TEXT(R"(
void Test() { auto X = "a" || "b"; }
)"),
			TEXT("Logical OR on strings"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("LogicN_AndFloat"),
			TEXT(R"(
void Test() { bool X = 1.0f && 2.0f; }
)"),
			TEXT("Logical AND on floats"));
	}

	// ====================================================================
	// Comparison Operators — Positive
	// ====================================================================

	TEST_METHOD(Comparison_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_CmpPos"), TEXT(R"(
int Equal()         { return (1 == 1) ? 1 : 0; }
int NotEqual()      { return (1 != 2) ? 1 : 0; }
int LessThan()      { return (1 < 2) ? 1 : 0; }
int GreaterThan()   { return (2 > 1) ? 1 : 0; }
int LessEqual()     { return (1 <= 1) ? 1 : 0; }
int GreaterEqual()  { return (2 >= 1) ? 1 : 0; }
int ChainedCmp()    { int A = 1; int B = 2; int C = 3; return ((A < B) && (B < C)) ? 1 : 0; }
int FloatCompare()  { return (1.5f > 1.0f) ? 1 : 0; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Equal()"),        TEXT("1 == 1"),       1 },
			{ TEXT("int NotEqual()"),     TEXT("1 != 2"),       1 },
			{ TEXT("int LessThan()"),     TEXT("1 < 2"),        1 },
			{ TEXT("int GreaterThan()"),  TEXT("2 > 1"),        1 },
			{ TEXT("int LessEqual()"),    TEXT("1 <= 1"),       1 },
			{ TEXT("int GreaterEqual()"), TEXT("2 >= 1"),       1 },
			{ TEXT("int ChainedCmp()"),   TEXT("chained cmp"), 1 },
			{ TEXT("int FloatCompare()"), TEXT("1.5 > 1.0"),   1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Comparison Operators — Negative
	// ====================================================================

	TEST_METHOD(Comparison_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("CmpN_StrLtInt"),
			TEXT(R"(
void Test() { bool X = ("hello" < 5); }
)"),
			TEXT("Comparing string to int"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("CmpN_Missing"),
			TEXT(R"(
void Test() { bool X = (1 == ); }
)"),
			TEXT("Missing right operand"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("CmpN_TripleEq"),
			TEXT(R"(
void Test() { bool X = (1 === 1); }
)"),
			TEXT("Triple equals not valid"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("CmpN_VecLtVec"),
			TEXT(R"(
void Test() { bool X = FVector(1,0,0) < FVector(0,1,0); }
)"),
			TEXT("Comparing vectors with <"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("CmpN_BoolLtBool"),
			TEXT(R"(
void Test() { bool X = (true < false); }
)"),
			TEXT("Comparing booleans with <"));
	}

	// ====================================================================
	// Assignment Operators — Positive
	// ====================================================================

	TEST_METHOD(Assignment_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_AssignPos"), TEXT(R"(
int SimpleAssign()   { int X = 0; X = 5; return X; }
int AddAssign()      { int X = 0; X += 5; return X; }
int SubAssign()      { int X = 10; X -= 3; return X; }
int MulAssign()      { int X = 2; X *= 3; return X; }
int DivAssign()      { int X = 10; X /= 2; return X; }
int ModAssign()      { int X = 10; X %= 3; return X; }
int BitAndAssign()   { int X = 0xFF; X &= 0x0F; return X; }
int BitOrAssign()    { int X = 0xF0; X |= 0x0F; return X; }
int BitXorAssign()   { int X = 0xFF; X ^= 0x0F; return X; }
int ShiftLAssign()   { int X = 1; X <<= 4; return X; }
int ShiftRAssign()   { int X = 16; X >>= 2; return X; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int SimpleAssign()"), TEXT("X = 5"),            5 },
			{ TEXT("int AddAssign()"),    TEXT("0 += 5 = 5"),       5 },
			{ TEXT("int SubAssign()"),    TEXT("10 -= 3 = 7"),      7 },
			{ TEXT("int MulAssign()"),    TEXT("2 *= 3 = 6"),       6 },
			{ TEXT("int DivAssign()"),    TEXT("10 /= 2 = 5"),      5 },
			{ TEXT("int ModAssign()"),    TEXT("10 %= 3 = 1"),      1 },
			{ TEXT("int BitAndAssign()"), TEXT("0xFF &= 0x0F"),    15 },
			{ TEXT("int BitOrAssign()"),  TEXT("0xF0 |= 0x0F"),   255 },
			{ TEXT("int BitXorAssign()"),TEXT("0xFF ^= 0x0F"),    240 },
			{ TEXT("int ShiftLAssign()"),TEXT("1 <<= 4 = 16"),     16 },
			{ TEXT("int ShiftRAssign()"),TEXT("16 >>= 2 = 4"),      4 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Assignment Operators — Negative
	// ====================================================================

	TEST_METHOD(Assignment_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_Const"),
			TEXT(R"(
void Test() { const int X = 5; X = 10; }
)"),
			TEXT("Assignment to const"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_Literal"),
			TEXT(R"(
void Test() { 5 = 10; }
)"),
			TEXT("Assignment to literal"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_TypeMismatch"),
			TEXT(R"(
void Test() { int X = 0; X = "hello"; }
)"),
			TEXT("String assigned to int"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_AddAssignType"),
			TEXT(R"(
void Test() { int X = 0; X += "hello"; }
)"),
			TEXT("Add-assign string to int"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_FuncResult"),
			TEXT(R"(
int Foo() { return 1; }
void Test() { Foo() = 5; }
)"),
			TEXT("Assign to function return"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_Undeclared"),
			TEXT(R"(
void Test() { UndeclaredVar = 5; }
)"),
			TEXT("Assign to undeclared variable"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_ExprLhs"),
			TEXT(R"(
void Test() { int X = 0; int Y = 0; (X + Y) = 5; }
)"),
			TEXT("Assign to expression"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_ShiftAssignFloat"),
			TEXT(R"(
void Test() { float X = 1.0f; X <<= 2; }
)"),
			TEXT("Shift-assign on float"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 float 类型 %= 运算
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AssignN_ModAssignFloat"),
			TEXT(R"(
void Test() { float X = 1.0f; X %= 2.0f; }
)"),
			TEXT("Mod-assign on float"));
#endif
	}

	// ====================================================================
	// Ternary Operator — Positive
	// ====================================================================

	TEST_METHOD(Ternary_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_TernPos"), TEXT(R"(
int Basic()          { return true ? 1 : 0; }
int Nested()         { return true ? (false ? 1 : 2) : 3; }
int WithExpr()       { int A = 5; return (A > 3) ? A * 2 : A - 1; }
int FalseCondition() { return false ? 100 : 200; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Basic()"),          TEXT("true ? 1 : 0"),        1 },
			{ TEXT("int Nested()"),         TEXT("nested ternary"),      2 },
			{ TEXT("int WithExpr()"),       TEXT("5>3 ? 10 : 4"),       10 },
			{ TEXT("int FalseCondition()"), TEXT("false ? 100 : 200"), 200 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Ternary Operator — Negative
	// ====================================================================

	TEST_METHOD(Ternary_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("TernN_NonBool"),
			TEXT(R"(
void Test() { int X = 5 ? 1 : 0; }
)"),
			TEXT("Non-bool ternary condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("TernN_TypeMismatch"),
			TEXT(R"(
void Test() { auto X = true ? 1 : "hello"; }
)"),
			TEXT("Ternary branch type mismatch"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("TernN_NoColon"),
			TEXT(R"(
void Test() { int X = true ? 1; }
)"),
			TEXT("Ternary without colon"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("TernN_NoTrue"),
			TEXT(R"(
void Test() { int X = true ? : 0; }
)"),
			TEXT("Ternary without true branch"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("TernN_FloatCond"),
			TEXT(R"(
void Test() { int X = 1.0f ? 1 : 0; }
)"),
			TEXT("Float as ternary condition"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("TernN_StrCond"),
			TEXT(R"(
void Test() { int X = "yes" ? 1 : 0; }
)"),
			TEXT("String as ternary condition"));
	}

	// ====================================================================
	// Edge Cases — Mixed Operator Interactions
	// ====================================================================

	TEST_METHOD(EdgeCases)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Positive edge cases
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_Operators_Edge"), TEXT(R"(
int MaxParens()     { return ((((1 + 2)))); }
int LongChain()    { return 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10; }
int PrecedenceMix() { return 2 + 3 * 4 - 1; }
int BitAndLogic()  { int X = 5; return (X > 0 && (X & 1) == 1) ? 1 : 0; }
int AssignInExpr() { int X = 0; X = 5; int Y = X; return Y; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int MaxParens()"),     TEXT("deeply parenthesized"),  3 },
			{ TEXT("int LongChain()"),    TEXT("1+2+...+10 = 55"),      55 },
			{ TEXT("int PrecedenceMix()"),TEXT("2+3*4-1 = 13"),         13 },
			{ TEXT("int BitAndLogic()"),  TEXT("logic + bitwise mix"),    1 },
			{ TEXT("int AssignInExpr()"), TEXT("sequential assign"),       5 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);

		// Negative edge cases
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("EdgeN_UnmatchedParen"),
			TEXT(R"(
void Test() { int X = (1 + 2; }
)"),
			TEXT("Unmatched parenthesis"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("EdgeN_EmptyParens"),
			TEXT(R"(
void Test() { int X = (); }
)"),
			TEXT("Empty parentheses as value"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("EdgeN_TrailingOp"),
			TEXT(R"(
void Test() { int X = 1 +; }
)"),
			TEXT("Trailing operator"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("EdgeN_LeadingOp"),
			TEXT(R"(
void Test() { int X = * 2; }
)"),
			TEXT("Leading binary operator"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
