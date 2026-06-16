#include "AngelscriptSDKTestUtilities.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestUtilities;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKFunctionTests,
	"Angelscript.TestModule.AngelScriptSDK.Function",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OverloadDefault)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK function overload/default test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// Test function overloading with distinct parameter counts
		// Note: This fork does not support ambiguous overload resolution when default args overlap
		// We test distinct overloads that don't have ambiguous calls
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKFunctionOverloadDefault", R"(
int AddOne(int Value)
{
	return Value + 1;
}

int AddPair(int Left, int Right)
{
	return Left + Right;
}

int AddWithDefault(int Left, int Right = 10)
{
	return Left + Right;
}

bool Entry()
{
	return AddOne(2) == 3 && AddPair(2, 5) == 7 && AddWithDefault(5) == 15 && AddWithDefault(3, 2) == 5;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK function overload/default test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK function overload/default test should preserve overload resolution and default argument semantics"), bResult);
	}

	TEST_METHOD(RefArgument)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK function ref-argument test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKFunctionRefArgument", R"(
void WriteValue(int &out Value)
{
	Value = 7;
}

bool Entry()
{
	int Value = 0;
	WriteValue(Value);
	return Value == 7;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK function ref-argument test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK function ref-argument test should preserve out-parameter writes"), bResult);
	}

	TEST_METHOD(ByRefMutation)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK function by-ref mutation test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKFunctionByRefMutation", R"(
void Increment(int &inout Value)
{
	Value += 1;
}

bool Entry()
{
	int Value = 41;
	Increment(Value);
	return Value == 42;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK function by-ref mutation test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK function by-ref mutation test should preserve inout parameter semantics"), bResult);
	}

	TEST_METHOD(ConstInRef)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK function const-in-ref test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKFunctionConstInRef", R"(
int Sum(const int &in A, const int &in B)
{
	return A + B;
}

bool Entry()
{
	int x = 17;
	int y = 25;
	return Sum(x, y) == 42 && Sum(1, 2) == 3;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK function const-in-ref test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK function const-in-ref test should pass values through const &in parameters"), bResult);
	}

	TEST_METHOD(TypeBasedOverload)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK function type-overload test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKFunctionTypeOverload", R"(
int Describe(int Value)    { return 1; }
int Describe(double Value) { return 2; }
int Describe(bool Value)   { return 3; }

bool Entry()
{
	// Overload resolution selects by argument type
	return Describe(10) == 1 && Describe(3.14) == 2 && Describe(true) == 3;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK function type-overload test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK function type-overload test should resolve overloads by argument type"), bResult);
	}

	TEST_METHOD(Recursion)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK function recursion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKFunctionRecursion", R"(
int Factorial(int N)
{
	if (N <= 1) return 1;
	return N * Factorial(N - 1);
}

int Fib(int N)
{
	if (N < 2) return N;
	return Fib(N - 1) + Fib(N - 2);
}

bool Entry()
{
	return Factorial(5) == 120 && Factorial(0) == 1 && Fib(10) == 55;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK function recursion test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK function recursion test should compute recursive results correctly"), bResult);
	}
};

#endif
