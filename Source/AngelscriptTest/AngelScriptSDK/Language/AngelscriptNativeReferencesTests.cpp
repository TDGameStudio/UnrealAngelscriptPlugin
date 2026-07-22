#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferencesTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReferencesRefArgument)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
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

	TEST_METHOD(ReferencesByRefMutation)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
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

	TEST_METHOD(ReferencesConstInRef)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
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

};

#endif
