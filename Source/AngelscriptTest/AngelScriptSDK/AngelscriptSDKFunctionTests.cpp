#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptSDKFunctionTests,
	"Angelscript.TestModule.AngelScriptSDK.Function",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

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
	TEST_METHOD(OverloadDefault)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function overload/default test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionOverloadDefault", R"(
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

int AddWithDefaultImplicit()
{
	return AddWithDefault(5);
}

int AddWithDefaultExplicit()
{
	return AddWithDefault(3, 2);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddOne(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2));
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should call AddOne directly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddPair(int, int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2)).AddArg(static_cast<int32>(5));
			ASSERT_THAT(AreEqual(7, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should call AddPair directly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddWithDefaultImplicit()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(15, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should preserve default arguments")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddWithDefaultExplicit()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(5, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should allow explicit default-argument override")));
		}
	}

	TEST_METHOD(RefArgument)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function ref-argument test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionRefArgument", R"(
void WriteValue(int &out Value)
{
	Value = 7;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		int32 Value = 0;
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "void WriteValue(int&out)");
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddArgRef(Value);
		ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("SDK function ref-argument test should execute the writer")));
		ASSERT_THAT(AreEqual(7, Value, TEXT("SDK function ref-argument test should preserve out-parameter writes")));
	}

	TEST_METHOD(ByRefMutation)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function by-ref mutation test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionByRefMutation", R"(
void Increment(int &inout Value)
{
	Value += 1;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		int32 Value = 41;
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "void Increment(int&inout)");
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddArgRef(Value);
		ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("SDK function by-ref mutation test should execute the mutator")));
		ASSERT_THAT(AreEqual(42, Value, TEXT("SDK function by-ref mutation test should preserve inout parameter semantics")));
	}

	TEST_METHOD(ConstInRef)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function const-in-ref test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionConstInRef", R"(
int Sum(const int &in A, const int &in B)
{
	return A + B;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		int32 Left = 17;
		int32 Right = 25;
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Sum(const int&in, const int&in)");
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddArgRef(Left).AddArgRef(Right);
		ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("SDK function const-in-ref test should pass values through const &in parameters")));
	}

	TEST_METHOD(TypeBasedOverload)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function type-overload test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionTypeOverload", R"(
int Describe(int Value)    { return 1; }
int Describe(double Value) { return 2; }
int Describe(bool Value)   { return 3; }

int DescribeInt()
{
	return Describe(10);
}

int DescribeDouble()
{
	return Describe(3.14);
}

int DescribeBool()
{
	return Describe(true);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int DescribeInt()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function type-overload test should resolve int overloads by argument type")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int DescribeDouble()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(2, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function type-overload test should resolve double overloads by argument type")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int DescribeBool()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function type-overload test should resolve bool overloads by argument type")));
		}
	}

	TEST_METHOD(Recursion)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function recursion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionRecursion", R"(
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
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Factorial(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(5));
			ASSERT_THAT(AreEqual(120, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should compute factorial correctly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Factorial(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(0));
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should handle the factorial base case")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Fib(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(10));
			ASSERT_THAT(AreEqual(55, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should compute fibonacci correctly")));
		}
	}
};

#endif
