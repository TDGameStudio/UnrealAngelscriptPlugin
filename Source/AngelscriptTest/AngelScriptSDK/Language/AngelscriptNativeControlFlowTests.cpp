#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FControlFlowTests, "Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ControlFlowShortCircuit)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-EXPR-LAZY-EVALUATION and LANG-OP-LOGICAL supersede this short-circuit smoke with selector, outcome, source-shape, truth-table, side-effect, runtime, and cleanup products");

		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorShortCircuit", ASTEST_AS_ANSI(R"AS(
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
		)AS"));
		if (!Module.IsValid()) return;
		AngelscriptSDKTestSupport::FSdkFunctionInvoker AndInvoker(*TestRunner, Engine.Get(), Module, "int AndCounter()");
		AngelscriptSDKTestSupport::FSdkFunctionInvoker OrInvoker(*TestRunner, Engine.Get(), Module, "int OrCounter()");
		ASSERT_THAT(IsTrue(AndInvoker.IsValid(), TEXT("And short-circuit function should resolve")));
		ASSERT_THAT(IsTrue(OrInvoker.IsValid(), TEXT("Or short-circuit function should resolve")));
		ASSERT_THAT(AreEqual(0, AndInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("False left operand should skip the and right operand")));
		ASSERT_THAT(AreEqual(0, OrInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("True left operand should skip the or right operand")));
	}
};

#endif
