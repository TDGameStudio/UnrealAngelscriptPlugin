#include "AngelscriptSDKTestExecutionHelpers.h"
// AngelscriptSDKVariableScopeTests.cpp
// Tests for as_variablescope.cpp - variable scope isolation and shadowing.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.VariableScope.*

#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKVariableScopeTests, "Angelscript.TestModule.AngelScriptSDK.VariableScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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
	TEST_METHOD(Isolation)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Variable declared in inner scope should not be visible in outer scope
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScopeIso");
		asIScriptModule* M = BuildNativeModule(SE, "ScopeIso", R"(
int Entry()
{
	{ int x = 5; }
	return x;
}
)");
		ASSERT_THAT(IsNull(M, TEXT("Access to out-of-scope variable should fail compilation")));
	}

	TEST_METHOD(Shadowing)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeShadow", R"(
int Entry()
{
	int x = 10;
	{ int x = 20; }
	return x;
}
)");
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(10, Result, TEXT("Outer x should remain 10 after inner shadow")));
	}

	TEST_METHOD(NestedBlocks)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeNested", R"(
int Entry()
{
	int sum = 0;
	{ int a = 1; sum += a; }
	{ int b = 2; sum += b; }
	{ int c = 3; { int d = 4; sum += d; } sum += c; }
	return sum;
}
)");
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(10, Result, TEXT("sum = 1+2+4+3 = 10")));
	}

	TEST_METHOD(ForInitScope)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Loop counter declared in for-init is scoped to the loop; a same-named
		// outer variable is unaffected, and two sequential loops may reuse the name.
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeForInit", R"(
int Entry()
{
	int i = 100;
	int sum = 0;
	for (int i = 0; i < 5; i++) { sum += i; }   // 0+1+2+3+4 = 10
	for (int i = 0; i < 3; i++) { sum += i; }   // +0+1+2 = 13
	return sum + i;                              // 13 + 100 = 113
}
)");
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(113, Result,
			TEXT("for-init counters stay loop-scoped; outer i preserved (13+100=113)")));
	}

	TEST_METHOD(ForInitLeakRejected)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// A for-init counter must not be visible after the loop body.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScopeForInitLeak");
		asIScriptModule* M = BuildNativeModule(SE, "ScopeForInitLeak", R"(
int Entry()
{
	for (int k = 0; k < 3; k++) { }
	return k;
}
)");
		ASSERT_THAT(IsNull(M, TEXT("Referencing a for-init counter after the loop should fail compilation")));
	}

	TEST_METHOD(DeepShadowing)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Each nested block may re-shadow the same name; the innermost value is
		// used within its block, and each outer value is restored on block exit.
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeDeepShadow", R"(
int Entry()
{
	int x = 1;
	int captured = 0;
	{
		int x = 2;
		{
			int x = 3;
			{
				int x = 4;
				captured += x;   // 4
			}
			captured += x;       // +3 = 7
		}
		captured += x;           // +2 = 9
	}
	captured += x;               // +1 = 10
	return captured;
}
)");
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(10, Result, TEXT("four-level shadow sums 4+3+2+1 = 10")));
	}

	TEST_METHOD(WhileAndIfBlockScope)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Variables declared inside while/if bodies are block-scoped; the outer
		// accumulator survives across iterations.
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeWhileIf", R"(
int Entry()
{
	int sum = 0;
	int i = 0;
	while (i < 4)
	{
		int step = i * 2;   // block-scoped to the loop body
		sum += step;
		i++;
	}
	if (sum > 0)
	{
		int bonus = 100;    // block-scoped to the if body
		sum += bonus;
	}
	return sum;             // (0+2+4+6) + 100 = 112
}
)");
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(112, Result,
			TEXT("while/if block-scoped locals; outer sum = 12+100 = 112")));
	}

	TEST_METHOD(IfBlockLeakRejected)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// A variable declared inside an if body must not be visible afterward.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScopeIfLeak");
		asIScriptModule* M = BuildNativeModule(SE, "ScopeIfLeak", R"(
int Entry()
{
	if (true) { int inner = 7; }
	return inner;
}
)");
		ASSERT_THAT(IsNull(M, TEXT("Referencing an if-body local after the block should fail compilation")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
