#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionsTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionsMixinNamespace)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function mixin test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "FunctionsMixinNamespace", R"(
struct Counter { int Value = 0; }
mixin void AddToCounter(Counter& Self, int Delta) { Self.Value += Delta; }
bool ApplyMixin() { Counter Value; Value.AddToCounter(3); return Value.Value == 3; }
)");
		if (!Module.IsValid())
		{
			return;
		}

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool ApplyMixin()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Function mixin test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Function mixin test should execute extension-style member syntax")));
		}
	}

	TEST_METHOD(FunctionsOverloadDefault)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
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
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddOne(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2));
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should call AddOne directly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddPair(const int, const int)");
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

	TEST_METHOD(TypeBasedOverload)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
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

	TEST_METHOD(FunctionsRecursion)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
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
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Factorial(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(5));
			ASSERT_THAT(AreEqual(120, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should compute factorial correctly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Factorial(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(0));
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should handle the factorial base case")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Fib(const int)");
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
