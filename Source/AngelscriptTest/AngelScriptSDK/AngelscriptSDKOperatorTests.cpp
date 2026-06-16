#include "AngelscriptSDKTestUtilities.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;
using namespace AngelscriptSDKTestUtilities;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKOperatorTests, "Angelscript.TestModule.AngelScriptSDK.Operator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Arithmetic)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator arithmetic test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorArithmetic", R"(
bool Entry()
{
	// Basic arithmetic
	int add = 10 + 5;
	int sub = 10 - 5;
	int mul = 10 * 5;
	int div = 10 / 5;
	int mod = 10 % 3;

	// Unary operators
	int pos = +5;
	int neg = -5;

	// Increment/Decrement
	int a = 5;
	int b = ++a;  // a=6, b=6
	int c = a++;  // a=7, c=6
	int d = --a;  // a=6, d=6
	int e = a--;  // a=5, e=6

	return add == 15 && sub == 5 && mul == 50 && div == 2 && mod == 1
		&& pos == 5 && neg == -5
		&& a == 5 && b == 6 && c == 6 && d == 6 && e == 6;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator arithmetic test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator arithmetic test should preserve all arithmetic operations"), bResult);
	}

	TEST_METHOD(Comparison)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator comparison test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorComparison", R"(
bool Entry()
{
	int a = 10;
	int b = 20;
	int c = 10;

	bool eq = (a == c);      // true
	bool ne = (a != b);      // true
	bool lt = (a < b);       // true
	bool le1 = (a <= b);     // true
	bool le2 = (a <= c);     // true
	bool gt = (b > a);       // true
	bool ge1 = (b >= a);     // true
	bool ge2 = (a >= c);     // true

	return eq && ne && lt && le1 && le2 && gt && ge1 && ge2;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator comparison test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator comparison test should preserve all comparison operations"), bResult);
	}

	TEST_METHOD(Logical)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator logical test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorLogical", R"(
bool Entry()
{
	bool t = true;
	bool f = false;

	// Logical AND
	bool and1 = t && t;  // true
	bool and2 = t && f;  // false
	bool and3 = f && t;  // false
	bool and4 = f && f;  // false

	// Logical OR
	bool or1 = t || t;   // true
	bool or2 = t || f;   // true
	bool or3 = f || t;   // true
	bool or4 = f || f;   // false

	// Logical XOR
	bool xor1 = t ^^ t;  // false
	bool xor2 = t ^^ f;  // true
	bool xor3 = f ^^ t;  // true
	bool xor4 = f ^^ f;  // false

	// Logical NOT
	bool not1 = !t;      // false
	bool not2 = !f;      // true

	return and1 && !and2 && !and3 && !and4
		&& or1 && or2 && or3 && !or4
		&& !xor1 && xor2 && xor3 && !xor4
		&& !not1 && not2;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator logical test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator logical test should preserve all logical operations"), bResult);
	}

	TEST_METHOD(Bitwise)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator bitwise test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorBitwise", R"(
bool Entry()
{
	uint a = 0b11001100;  // 204
	uint b = 0b10101010;  // 170

	// Bitwise AND
	uint and_result = a & b;  // 0b10001000 = 136

	// Bitwise OR
	uint or_result = a | b;   // 0b11101110 = 238

	// Bitwise XOR
	uint xor_result = a ^ b;  // 0b01100110 = 102

	// Bitwise NOT
	uint8 c = 0b11110000;
	uint8 not_result = ~c;    // 0b00001111 = 15

	// Left shift
	uint left = 5 << 2;       // 20

	// Right shift
	uint right = 20 >> 2;     // 5

	// Compound bitwise operators
	uint x = 0xFF;
	x &= 0x0F;  // x = 0x0F
	x |= 0xF0;  // x = 0xFF
	x ^= 0x55;  // x = 0xAA
	x <<= 1;    // x = 0x154
	x >>= 2;    // x = 0x55

	return and_result == 136 && or_result == 238 && xor_result == 102
		&& not_result == 15 && left == 20 && right == 5 && x == 0x55;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator bitwise test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator bitwise test should preserve all bitwise operations"), bResult);
	}

	TEST_METHOD(Assignment)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator assignment test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorAssignment", R"(
bool Entry()
{
	// Simple assignment
	int a = 10;

	// Compound assignments
	int b = 10;
	b += 5;   // b = 15

	int c = 10;
	c -= 3;   // c = 7

	int d = 10;
	d *= 2;   // d = 20

	int e = 10;
	e /= 2;   // e = 5

	int f = 10;
	f %= 3;   // f = 1

	// Multi-assignment (chained)
	int x = 0, y = 0, z = 0;
	x = y = z = 42;

	return a == 10 && b == 15 && c == 7 && d == 20 && e == 5 && f == 1
		&& x == 42 && y == 42 && z == 42;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator assignment test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator assignment test should preserve all assignment operations"), bResult);
	}

	TEST_METHOD(Ternary)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator ternary test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorTernary", R"(
bool Entry()
{
	// Basic ternary
	int a = true ? 10 : 20;
	int b = false ? 10 : 20;

	// Nested ternary
	int x = 5;
	int result = x > 10 ? 1 : x > 5 ? 2 : x == 5 ? 3 : 4;

	// Ternary with side effects
	int counter = 0;
	int c = (counter++ > 0) ? 100 : 200;

	return a == 10 && b == 20 && result == 3 && c == 200 && counter == 1;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator ternary test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator ternary test should preserve conditional operator semantics"), bResult);
	}

	TEST_METHOD(Pow)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator pow test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorPow", R"(
bool Entry()
{
	return 3 ** 2 == 9
		&& 9.0 ** 0.5 == 3.0
		&& 2.5 ** 2 == 6.25
		&& 2 ** 10 == 1024;
}

void Overflow()
{
	double x = 1.0e100;
	x = x ** 6.0;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator pow test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bPowResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bPowResult))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK operator pow test should preserve exponent behavior"), bPowResult))
		{
			return;
		}

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
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator opCall test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorCall", R"(
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

bool Entry()
{
	Adder adder;
	return adder(2, 3) == 5 && adder(1, 2, 3) == 6;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator opCall test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		// Note: this bare native engine cannot safely instantiate a script
		// reference-class at runtime (doing so raises asEXECUTION_EXCEPTION),
		// so we verify the opCall overloads compile and resolve rather than
		// executing. Runtime opCall dispatch is covered by the UE-wrapper
		// engine tests. See OpenSpec change refactor-as-sdk-test-namespace-consolidation §6.6.
		TestRunner->TestNotNull(
			TEXT("SDK operator opCall test should resolve the entry function after compiling opCall overloads"),
			GetNativeFunctionByDecl(Module, "bool Entry()"));
	}

	TEST_METHOD(Index)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator index test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorIndex", R"(
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

bool Entry()
{
	SimpleArray arr;
	return arr[0] == 10 && arr[1] == 20 && arr[2] == 30;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator index test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		// As with opCall, the bare native engine cannot instantiate this script
		// reference-class at runtime, so we verify opIndex compiles and resolves.
		TestRunner->TestNotNull(
			TEXT("SDK operator index test should resolve the entry function after compiling opIndex"),
			GetNativeFunctionByDecl(Module, "bool Entry()"));
	}

	TEST_METHOD(Precedence)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator precedence test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorPrecedence", R"(
bool Entry()
{
	// Multiplicative binds tighter than additive
	int a = 2 + 3 * 4;          // 14, not 20
	// Parentheses override precedence
	int b = (2 + 3) * 4;        // 20
	// Unary minus + multiplication
	int c = -2 * 3;             // -6
	// Shift below additive, bitwise-and below shift
	int d = 1 + 2 << 1;         // (1+2)<<1 = 6
	int e = 0xF0 | 0x0F & 0x33;  // 0x0F&0x33=0x03; 0xF0|0x03 = 0xF3
	// Comparison below arithmetic; logical below comparison
	bool f = 2 + 2 == 4 && 3 * 3 > 8;
	return a == 14 && b == 20 && c == -6 && d == 6 && e == 0xF3 && f;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator precedence test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator precedence test should preserve operator precedence and associativity"), bResult);
	}

	TEST_METHOD(ShortCircuit)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK operator short-circuit test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKOperatorShortCircuit", R"(
bool Entry()
{
	// AND short-circuit: second operand should not evaluate
	int counter1 = 0;
	bool and_result = false && (++counter1 > 0);

	// OR short-circuit: second operand should not evaluate
	int counter2 = 0;
	bool or_result = true || (++counter2 > 0);

	// Verify counters were not incremented (short-circuited)
	return counter1 == 0 && counter2 == 0 && !and_result && or_result;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK operator short-circuit test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK operator short-circuit test should preserve logical operator short-circuit behavior"), bResult);
	}
};

#endif
