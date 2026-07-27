#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferencesTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReferencesRefArgument)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-PARAM-DIRECTION and LANG-REF-DIRECTION supersede this single int out-parameter sample across core types, directions, alias/null states, writeback, runtime, and cleanup");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function ref-argument test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionRefArgument", ASTEST_AS_ANSI(R"AS(
			void WriteValue(int &out Value)
			{
				Value = 7;
			}
		)AS"));
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
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-PARAM-DIRECTION and LANG-REF-DIRECTION supersede this single int inout sample across transfer directions, alias states, mutation, runtime, and cleanup");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function by-ref mutation test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionByRefMutation", ASTEST_AS_ANSI(R"AS(
			void Increment(int &inout Value)
			{
				Value += 1;
			}
		)AS"));
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
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-PARAM-DIRECTION, LANG-FN-PARAM-POSITION, and LANG-REF-DIRECTION supersede this two-int const-in sample across types, parameter positions, directions, identity, runtime, and cleanup");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function const-in-ref test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionConstInRef", ASTEST_AS_ANSI(R"AS(
			int Sum(const int &in A, const int &in B)
			{
				return A + B;
			}
		)AS"));
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
