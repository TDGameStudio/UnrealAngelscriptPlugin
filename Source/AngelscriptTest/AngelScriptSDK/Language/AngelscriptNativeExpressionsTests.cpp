#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	bool ExpectExpressionInt(FAutomationTestBase& Test, asIScriptEngine* Engine, asIScriptModule* Module, const char* Declaration, int32 Expected, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(Test, Engine, Module, Declaration);
		return Assert.IsTrue(Invoker.IsValid(), Context) && Assert.AreEqual(Expected, Invoker.CallAndReturn<int32>(INDEX_NONE), Context);
	}

	bool ExpectExpressionBool(FAutomationTestBase& Test, asIScriptEngine* Engine, asIScriptModule* Module, const char* Declaration, bool bExpected, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(Test, Engine, Module, Declaration);
		return Assert.IsTrue(Invoker.IsValid(), Context) && Assert.AreEqual(bExpected, Invoker.CallAndReturn<bool>(!bExpected), Context);
	}
}

TEST_CLASS_WITH_FLAGS(FExpressionsTests, "Angelscript.TestModule.AngelScriptSDK.Language.Expressions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExpressionsArithmetic)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorArithmetic", R"AS(
int Add() { return 10 + 5; }
int Subtract() { return 10 - 5; }
int Multiply() { return 10 * 5; }
int Divide() { return 10 / 5; }
int Modulo() { return 10 % 3; }
int UnaryPlus() { return +5; }
int UnaryMinus() { return -5; }
int IncrementDecrement()
{
	int a = 5;
	int b = ++a;
	int c = a++;
	int d = --a;
	int e = a--;
	return a * 10000 + b * 1000 + c * 100 + d * 10 + e;
}
)AS");
		if (!Module.IsValid()) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int Add()", 15, TEXT("Arithmetic should preserve addition"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int Subtract()", 5, TEXT("Arithmetic should preserve subtraction"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int Multiply()", 50, TEXT("Arithmetic should preserve multiplication"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int Divide()", 2, TEXT("Arithmetic should preserve division"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int Modulo()", 1, TEXT("Arithmetic should preserve modulo"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int UnaryPlus()", 5, TEXT("Arithmetic should preserve unary plus"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int UnaryMinus()", -5, TEXT("Arithmetic should preserve unary minus"))) return;
		ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int IncrementDecrement()", 56666, TEXT("Arithmetic should preserve increment and decrement ordering"));
	}

	TEST_METHOD(ExpressionsComparison)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorComparison", R"AS(
bool Equal() { int a = 10; int c = 10; return a == c; }
bool NotEqual() { int a = 10; int b = 20; return a != b; }
bool LessThan() { return 10 < 20; }
bool LessEqualDifferent() { return 10 <= 20; }
bool LessEqualSame() { return 10 <= 10; }
bool GreaterThan() { return 20 > 10; }
bool GreaterEqualDifferent() { return 20 >= 10; }
bool GreaterEqualSame() { return 10 >= 10; }
)AS");
		if (!Module.IsValid()) return;
		for (const char* Declaration : { "bool Equal()", "bool NotEqual()", "bool LessThan()", "bool LessEqualDifferent()", "bool LessEqualSame()", "bool GreaterThan()", "bool GreaterEqualDifferent()", "bool GreaterEqualSame()" })
		{
			if (!ExpectExpressionBool(*TestRunner, Engine.Get(), Module, Declaration, true, TEXT("Comparison should evaluate each relation"))) return;
		}
	}

	TEST_METHOD(ExpressionsLogical)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorLogical", R"AS(
bool AndTrueTrue() { return true && true; }
bool AndTrueFalse() { return true && false; }
bool AndFalseTrue() { return false && true; }
bool AndFalseFalse() { return false && false; }
bool OrTrueTrue() { return true || true; }
bool OrTrueFalse() { return true || false; }
bool OrFalseTrue() { return false || true; }
bool OrFalseFalse() { return false || false; }
bool XorTrueTrue() { return true ^^ true; }
bool XorTrueFalse() { return true ^^ false; }
bool XorFalseTrue() { return false ^^ true; }
bool XorFalseFalse() { return false ^^ false; }
bool NotTrue() { return !true; }
bool NotFalse() { return !false; }
)AS");
		if (!Module.IsValid()) return;
		const TPair<const char*, bool> Cases[] = {
			{ "bool AndTrueTrue()", true }, { "bool AndTrueFalse()", false }, { "bool AndFalseTrue()", false }, { "bool AndFalseFalse()", false },
			{ "bool OrTrueTrue()", true }, { "bool OrTrueFalse()", true }, { "bool OrFalseTrue()", true }, { "bool OrFalseFalse()", false },
			{ "bool XorTrueTrue()", false }, { "bool XorTrueFalse()", true }, { "bool XorFalseTrue()", true }, { "bool XorFalseFalse()", false },
			{ "bool NotTrue()", false }, { "bool NotFalse()", true },
		};
		for (const TPair<const char*, bool>& Case : Cases)
		{
			if (!ExpectExpressionBool(*TestRunner, Engine.Get(), Module, Case.Key, Case.Value, TEXT("Logical expression should preserve its truth value"))) return;
		}
	}

	TEST_METHOD(ExpressionsTernary)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorTernary", R"AS(
int TrueBranch() { return true ? 10 : 20; }
int FalseBranch() { return false ? 10 : 20; }
int NestedBranch() { int x = 5; return x > 10 ? 1 : x > 5 ? 2 : x == 5 ? 3 : 4; }
int SideEffectCounter() { int counter = 0; int c = (counter++ > 0) ? 100 : 200; return c + counter; }
)AS");
		if (!Module.IsValid()) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int TrueBranch()", 10, TEXT("Ternary should select its true branch"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int FalseBranch()", 20, TEXT("Ternary should select its false branch"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int NestedBranch()", 3, TEXT("Ternary should associate nested conditions"))) return;
		ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int SideEffectCounter()", 201, TEXT("Ternary should evaluate its condition once"));
	}

	TEST_METHOD(ExpressionsCall)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKExpressionCall", R"AS(
class Adder
{
	int opCall(int A, int B) { return A + B; }
	int opCall(int A, int B, int C) { return A + B + C; }
}
bool InvokeAdderPair() { Adder Value; return Value(2, 3) == 5; }
bool InvokeAdderTriple() { Adder Value; return Value(1, 2, 3) == 6; }
)AS");
		if (!Module.IsValid()) return;
		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool InvokeAdderPair()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Call expression target should resolve")));
		asIScriptContext* Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Call expression test should create a context")));
		if (Function == nullptr || Context == nullptr) return;
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), PrepareAndExecute(Context, Function), TEXT("The current isolated native engine should report the opCall object-construction limitation")));
		ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())), TEXT("The opCall object-construction limitation should preserve its exception text")));
	}

	TEST_METHOD(ExpressionsIndex)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKExpressionIndex", R"AS(
class SimpleArray
{
	int Data0 = 10;
	int Data1 = 20;
	int Data2 = 30;
	int opIndex(int Index) const
	{
		if (Index == 0) return Data0;
		if (Index == 1) return Data1;
		if (Index == 2) return Data2;
		return -1;
	}
}
int ReadSlot(int Index) { SimpleArray Value; return Value[Index]; }
)AS");
		if (!Module.IsValid()) return;
		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int ReadSlot(const int)");
		ASSERT_THAT(IsNotNull(Function, TEXT("Index expression target should resolve")));
		asIScriptContext* Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Index expression test should create a context")));
		if (Function == nullptr || Context == nullptr) return;
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(Function), TEXT("Index expression context should prepare")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->SetArgDWord(0, 1), TEXT("Index expression context should receive an index argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(), TEXT("The current isolated native engine should report the opIndex object-construction limitation")));
		ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())), TEXT("The opIndex object-construction limitation should preserve its exception text")));
	}

	TEST_METHOD(ExpressionsPrecedence)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorPrecedence", R"AS(
int MultiplicativeBeforeAdditive() { return 2 + 3 * 4; }
int ParenthesesOverride() { return (2 + 3) * 4; }
int UnaryMinusBeforeMultiply() { return -2 * 3; }
int ShiftAfterAdditive() { return 1 + 2 << 1; }
int BitwiseAndBeforeOr() { return 0xF0 | 0x0F & 0x33; }
bool ComparisonBeforeLogical() { return 2 + 2 == 4 && 3 * 3 > 8; }
)AS");
		if (!Module.IsValid()) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int MultiplicativeBeforeAdditive()", 14, TEXT("Precedence should bind multiplication first"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int ParenthesesOverride()", 20, TEXT("Parentheses should override precedence"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int UnaryMinusBeforeMultiply()", -6, TEXT("Unary minus should bind before multiplication"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int ShiftAfterAdditive()", 6, TEXT("Additive operations should bind before shifts"))) return;
		if (!ExpectExpressionInt(*TestRunner, Engine.Get(), Module, "int BitwiseAndBeforeOr()", 0xF3, TEXT("Bitwise and should bind before or"))) return;
		ExpectExpressionBool(*TestRunner, Engine.Get(), Module, "bool ComparisonBeforeLogical()", true, TEXT("Comparison should bind before logical and"));
	}
};

#endif
