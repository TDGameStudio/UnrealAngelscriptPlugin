#include "AngelscriptSDKTestUtilities.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;
using namespace AngelscriptSDKTestUtilities;

namespace
{
	bool ExpectSdkInt(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		const TCHAR* Label,
		const int32 Expected)
	{
		FSdkFunctionInvoker Invoker(Test, ScriptEngine, Module, Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}

		return Test.TestEqual(Label, Invoker.CallAndReturn<int32>(INDEX_NONE), Expected);
	}

	bool ExpectSdkUInt(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		const TCHAR* Label,
		const uint32 Expected)
	{
		FSdkFunctionInvoker Invoker(Test, ScriptEngine, Module, Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}

		return Test.TestEqual(Label, Invoker.CallAndReturn<uint32>(0), Expected);
	}

	bool ExpectSdkUInt8(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		const TCHAR* Label,
		const uint8 Expected)
	{
		FSdkFunctionInvoker Invoker(Test, ScriptEngine, Module, Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}

		return Test.TestEqual(Label, Invoker.CallAndReturn<uint8>(0), Expected);
	}

	bool ExpectSdkBool(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		const TCHAR* Label,
		const bool bExpected)
	{
		FSdkFunctionInvoker Invoker(Test, ScriptEngine, Module, Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}

		return Test.TestEqual(Label, Invoker.CallAndReturn<bool>(!bExpected), bExpected);
	}

	bool ExpectSdkDouble(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		const TCHAR* Label,
		const double Expected,
		const double Tolerance = 0.0001)
	{
		FSdkFunctionInvoker Invoker(Test, ScriptEngine, Module, Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}

		const double Actual = Invoker.CallAndReturn<double>(0.0);
		return Test.TestTrue(
			*FString::Printf(TEXT("%s should return %.9g, got %.9g"), Label, Expected, Actual),
			FMath::IsNearlyEqual(Actual, Expected, Tolerance));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKOperatorTests, "Angelscript.TestModule.AngelScriptSDK.Operator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}
	TEST_METHOD(Arithmetic)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator arithmetic test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorArithmetic", R"(
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
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int Add()", TEXT("SDK operator arithmetic test should preserve addition"), 15);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int Subtract()", TEXT("SDK operator arithmetic test should preserve subtraction"), 5);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int Multiply()", TEXT("SDK operator arithmetic test should preserve multiplication"), 50);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int Divide()", TEXT("SDK operator arithmetic test should preserve division"), 2);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int Modulo()", TEXT("SDK operator arithmetic test should preserve modulo"), 1);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int UnaryPlus()", TEXT("SDK operator arithmetic test should preserve unary plus"), 5);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int UnaryMinus()", TEXT("SDK operator arithmetic test should preserve unary minus"), -5);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int IncrementDecrement()", TEXT("SDK operator arithmetic test should preserve increment/decrement ordering"), 56666);
	}

	TEST_METHOD(Comparison)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator comparison test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorComparison", R"(
bool Equal() { int a = 10; int c = 10; return a == c; }
bool NotEqual() { int a = 10; int b = 20; return a != b; }
bool LessThan() { int a = 10; int b = 20; return a < b; }
bool LessEqualDifferent() { int a = 10; int b = 20; return a <= b; }
bool LessEqualSame() { int a = 10; int c = 10; return a <= c; }
bool GreaterThan() { int a = 10; int b = 20; return b > a; }
bool GreaterEqualDifferent() { int a = 10; int b = 20; return b >= a; }
bool GreaterEqualSame() { int a = 10; int c = 10; return a >= c; }
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool Equal()", TEXT("SDK operator comparison test should preserve =="), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool NotEqual()", TEXT("SDK operator comparison test should preserve !="), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool LessThan()", TEXT("SDK operator comparison test should preserve <"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool LessEqualDifferent()", TEXT("SDK operator comparison test should preserve <= for different values"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool LessEqualSame()", TEXT("SDK operator comparison test should preserve <= for equal values"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool GreaterThan()", TEXT("SDK operator comparison test should preserve >"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool GreaterEqualDifferent()", TEXT("SDK operator comparison test should preserve >= for different values"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool GreaterEqualSame()", TEXT("SDK operator comparison test should preserve >= for equal values"), true);
	}

	TEST_METHOD(Logical)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator logical test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorLogical", R"(
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
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool AndTrueTrue()", TEXT("SDK operator logical test should preserve true && true"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool AndTrueFalse()", TEXT("SDK operator logical test should preserve true && false"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool AndFalseTrue()", TEXT("SDK operator logical test should preserve false && true"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool AndFalseFalse()", TEXT("SDK operator logical test should preserve false && false"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool OrTrueTrue()", TEXT("SDK operator logical test should preserve true || true"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool OrTrueFalse()", TEXT("SDK operator logical test should preserve true || false"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool OrFalseTrue()", TEXT("SDK operator logical test should preserve false || true"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool OrFalseFalse()", TEXT("SDK operator logical test should preserve false || false"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool XorTrueTrue()", TEXT("SDK operator logical test should preserve true ^^ true"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool XorTrueFalse()", TEXT("SDK operator logical test should preserve true ^^ false"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool XorFalseTrue()", TEXT("SDK operator logical test should preserve false ^^ true"), true);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool XorFalseFalse()", TEXT("SDK operator logical test should preserve false ^^ false"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool NotTrue()", TEXT("SDK operator logical test should preserve !true"), false);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool NotFalse()", TEXT("SDK operator logical test should preserve !false"), true);
	}

	TEST_METHOD(Bitwise)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator bitwise test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorBitwise", R"(
uint BitAnd() { uint a = 0b11001100; uint b = 0b10101010; return a & b; }
uint BitOr() { uint a = 0b11001100; uint b = 0b10101010; return a | b; }
uint BitXor() { uint a = 0b11001100; uint b = 0b10101010; return a ^ b; }
uint8 BitNot() { uint8 c = 0b11110000; return ~c; }
uint ShiftLeft() { return 5 << 2; }
uint ShiftRight() { return 20 >> 2; }
uint CompoundBitwise()
{
	uint x = 0xFF;
	x &= 0x0F;
	x |= 0xF0;
	x ^= 0x55;
	x <<= 1;
	x >>= 2;
	return x;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkUInt(*TestRunner, ScriptEngine, Module, "uint BitAnd()", TEXT("SDK operator bitwise test should preserve &"), 136);
		ExpectSdkUInt(*TestRunner, ScriptEngine, Module, "uint BitOr()", TEXT("SDK operator bitwise test should preserve |"), 238);
		ExpectSdkUInt(*TestRunner, ScriptEngine, Module, "uint BitXor()", TEXT("SDK operator bitwise test should preserve ^"), 102);
		ExpectSdkUInt8(*TestRunner, ScriptEngine, Module, "uint8 BitNot()", TEXT("SDK operator bitwise test should preserve ~"), 15);
		ExpectSdkUInt(*TestRunner, ScriptEngine, Module, "uint ShiftLeft()", TEXT("SDK operator bitwise test should preserve <<"), 20);
		ExpectSdkUInt(*TestRunner, ScriptEngine, Module, "uint ShiftRight()", TEXT("SDK operator bitwise test should preserve >>"), 5);
		ExpectSdkUInt(*TestRunner, ScriptEngine, Module, "uint CompoundBitwise()", TEXT("SDK operator bitwise test should preserve compound bitwise operators"), 0x55);
	}

	TEST_METHOD(Assignment)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator assignment test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorAssignment", R"(
int SimpleAssign() { int a = 10; return a; }
int AddAssign() { int b = 10; b += 5; return b; }
int SubAssign() { int c = 10; c -= 3; return c; }
int MulAssign() { int d = 10; d *= 2; return d; }
int DivAssign() { int e = 10; e /= 2; return e; }
int ModAssign() { int f = 10; f %= 3; return f; }
int ChainedAssign() { int x = 0, y = 0, z = 0; x = y = z = 42; return x + y + z; }
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int SimpleAssign()", TEXT("SDK operator assignment test should preserve simple assignment"), 10);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int AddAssign()", TEXT("SDK operator assignment test should preserve +="), 15);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int SubAssign()", TEXT("SDK operator assignment test should preserve -="), 7);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int MulAssign()", TEXT("SDK operator assignment test should preserve *="), 20);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int DivAssign()", TEXT("SDK operator assignment test should preserve /="), 5);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int ModAssign()", TEXT("SDK operator assignment test should preserve %="), 1);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int ChainedAssign()", TEXT("SDK operator assignment test should preserve chained assignment"), 126);
	}

	TEST_METHOD(Ternary)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator ternary test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorTernary", R"(
int TrueBranch() { return true ? 10 : 20; }
int FalseBranch() { return false ? 10 : 20; }
int NestedBranch() { int x = 5; return x > 10 ? 1 : x > 5 ? 2 : x == 5 ? 3 : 4; }
int SideEffectCounter()
{
	int counter = 0;
	int c = (counter++ > 0) ? 100 : 200;
	return c + counter;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int TrueBranch()", TEXT("SDK operator ternary test should preserve true branch selection"), 10);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int FalseBranch()", TEXT("SDK operator ternary test should preserve false branch selection"), 20);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int NestedBranch()", TEXT("SDK operator ternary test should preserve nested branch selection"), 3);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int SideEffectCounter()", TEXT("SDK operator ternary test should preserve side-effect ordering"), 201);
	}

	TEST_METHOD(Pow)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator pow test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorPow", R"(
int IntPow() { return 3 ** 2; }
double SqrtPow() { return 9.0 ** 0.5; }
double FractionalPow() { return 2.5 ** 2; }
int WidePow() { return 2 ** 10; }

void Overflow()
{
	double x = 1.0e100;
	x = x ** 6.0;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int IntPow()", TEXT("SDK operator pow test should preserve integer exponent behavior"), 9);
		ExpectSdkDouble(*TestRunner, ScriptEngine, Module, "double SqrtPow()", TEXT("SDK operator pow test should preserve square-root exponent behavior"), 3.0);
		ExpectSdkDouble(*TestRunner, ScriptEngine, Module, "double FractionalPow()", TEXT("SDK operator pow test should preserve fractional base exponent behavior"), 6.25);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int WidePow()", TEXT("SDK operator pow test should preserve wide integer exponent behavior"), 1024);

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "void Overflow()");
		if (!TestRunner->TestNotNull(TEXT("SDK operator pow test should resolve the overflow function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK operator pow test should create a context"), Context))
		{
			return;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		Context->Release();

		if (!TestRunner->TestEqual(TEXT("SDK operator pow test should raise an execution exception on overflow"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK operator pow test should report overflow in exponent operation"), ExceptionString, FString(TEXT("Overflow in exponent operation")));
	}

	TEST_METHOD(Call)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator opCall test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorCall", R"(
class Adder
{
	int opCall(int a, int b)
	{
		return a + b;
	}

	int opCall(int a, int b, int c)
	{
		return a + b + c;
	}
}

bool InvokeAdderPair()
{
	Adder adder;
	return adder(2, 3) == 5;
}

bool InvokeAdderTriple()
{
	Adder adder;
	return adder(1, 2, 3) == 6;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		TestRunner->TestNotNull(
			TEXT("SDK operator opCall test should resolve the pair opCall wrapper after compiling opCall overloads"),
			GetNativeFunctionByDecl(Module, "bool InvokeAdderPair()"));
		TestRunner->TestNotNull(
			TEXT("SDK operator opCall test should resolve the triple opCall wrapper after compiling opCall overloads"),
			GetNativeFunctionByDecl(Module, "bool InvokeAdderTriple()"));
	}

	TEST_METHOD(Index)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator index test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorIndex", R"(
class SimpleArray
{
	int data0 = 10;
	int data1 = 20;
	int data2 = 30;

	int opIndex(int idx) const
	{
		if (idx == 0) return data0;
		if (idx == 1) return data1;
		if (idx == 2) return data2;
		return -1;
	}
}

int ReadSimpleArraySlot(int Index)
{
	SimpleArray arr;
	return arr[Index];
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		TestRunner->TestNotNull(
			TEXT("SDK operator index test should resolve the named opIndex wrapper after compiling opIndex"),
			GetNativeFunctionByDecl(Module, "int ReadSimpleArraySlot(int)"));
	}

	TEST_METHOD(Precedence)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator precedence test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorPrecedence", R"(
int MultiplicativeBeforeAdditive() { return 2 + 3 * 4; }
int ParenthesesOverride() { return (2 + 3) * 4; }
int UnaryMinusBeforeMultiply() { return -2 * 3; }
int ShiftAfterAdditive() { return 1 + 2 << 1; }
int BitwiseAndBeforeOr() { return 0xF0 | 0x0F & 0x33; }
bool ComparisonBeforeLogical() { return 2 + 2 == 4 && 3 * 3 > 8; }
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int MultiplicativeBeforeAdditive()", TEXT("SDK operator precedence test should bind multiplication before addition"), 14);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int ParenthesesOverride()", TEXT("SDK operator precedence test should let parentheses override precedence"), 20);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int UnaryMinusBeforeMultiply()", TEXT("SDK operator precedence test should preserve unary minus with multiplication"), -6);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int ShiftAfterAdditive()", TEXT("SDK operator precedence test should bind shift below additive"), 6);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int BitwiseAndBeforeOr()", TEXT("SDK operator precedence test should bind bitwise and before or"), 0xF3);
		ExpectSdkBool(*TestRunner, ScriptEngine, Module, "bool ComparisonBeforeLogical()", TEXT("SDK operator precedence test should bind comparison before logical"), true);
	}

	TEST_METHOD(ShortCircuit)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK operator short-circuit test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorShortCircuit", R"(
int AndCounter()
{
	int counter = 0;
	bool value = false && (++counter > 0);
	return value ? -1 : counter;
}

int OrCounter()
{
	int counter = 0;
	bool value = true || (++counter > 0);
	return value ? counter : -1;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int AndCounter()", TEXT("SDK operator short-circuit test should skip RHS for false && RHS"), 0);
		ExpectSdkInt(*TestRunner, ScriptEngine, Module, "int OrCounter()", TEXT("SDK operator short-circuit test should skip RHS for true || RHS"), 0);
	}
};

#endif
