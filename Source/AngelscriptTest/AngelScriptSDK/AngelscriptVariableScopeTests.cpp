#include "AngelscriptSDKTestExecutionHelpers.h"
// AngelscriptSDKVariableScopeTests.cpp
// Tests for as_variablescope.cpp - variable scope isolation and shadowing.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.VariableScope.*

#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKVariableScopeTests, "Angelscript.TestModule.AngelScriptSDK.VariableScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Isolation)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// Variable declared in inner scope should not be visible in outer scope
		Messages.Reset();
		asIScriptModule* M = BuildNativeModule(SE, "ScopeIso", R"(
int Entry()
{
	{ int x = 5; }
	return x;
}
)");
		TestRunner->TestNull(TEXT("Access to out-of-scope variable should fail compilation"), M);
	}

	TEST_METHOD(Shadowing)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		asIScriptModule* M = BuildNativeModule(SE, "ScopeShadow", R"(
int Entry()
{
	int x = 10;
	{ int x = 20; }
	return x;
}
)");
		if (!TestRunner->TestNotNull(TEXT("Shadowing should compile"), M))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("Outer x should remain 10 after inner shadow"), Result, 10);
	}

	TEST_METHOD(NestedBlocks)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		asIScriptModule* M = BuildNativeModule(SE, "ScopeNested", R"(
int Entry()
{
	int sum = 0;
	{ int a = 1; sum += a; }
	{ int b = 2; sum += b; }
	{ int c = 3; { int d = 4; sum += d; } sum += c; }
	return sum;
}
)");
		if (!TestRunner->TestNotNull(TEXT("Nested blocks should compile"), M))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("sum = 1+2+4+3 = 10"), Result, 10);
	}

	TEST_METHOD(ForInitScope)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// Loop counter declared in for-init is scoped to the loop; a same-named
		// outer variable is unaffected, and two sequential loops may reuse the name.
		asIScriptModule* M = BuildNativeModule(SE, "ScopeForInit", R"(
int Entry()
{
	int i = 100;
	int sum = 0;
	for (int i = 0; i < 5; i++) { sum += i; }   // 0+1+2+3+4 = 10
	for (int i = 0; i < 3; i++) { sum += i; }   // +0+1+2 = 13
	return sum + i;                              // 13 + 100 = 113
}
)");
		if (!TestRunner->TestNotNull(TEXT("For-init scope should compile"), M))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("for-init counters stay loop-scoped; outer i preserved (13+100=113)"), Result, 113);
	}

	TEST_METHOD(ForInitLeakRejected)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// A for-init counter must not be visible after the loop body.
		Messages.Reset();
		asIScriptModule* M = BuildNativeModule(SE, "ScopeForInitLeak", R"(
int Entry()
{
	for (int k = 0; k < 3; k++) { }
	return k;
}
)");
		TestRunner->TestNull(TEXT("Referencing a for-init counter after the loop should fail compilation"), M);
	}

	TEST_METHOD(DeepShadowing)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// Each nested block may re-shadow the same name; the innermost value is
		// used within its block, and each outer value is restored on block exit.
		asIScriptModule* M = BuildNativeModule(SE, "ScopeDeepShadow", R"(
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
		if (!TestRunner->TestNotNull(TEXT("Deep shadowing should compile"), M))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("four-level shadow sums 4+3+2+1 = 10"), Result, 10);
	}

	TEST_METHOD(WhileAndIfBlockScope)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// Variables declared inside while/if bodies are block-scoped; the outer
		// accumulator survives across iterations.
		asIScriptModule* M = BuildNativeModule(SE, "ScopeWhileIf", R"(
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
		if (!TestRunner->TestNotNull(TEXT("While/if block scope should compile"), M))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("while/if block-scoped locals; outer sum = 12+100 = 112"), Result, 112);
	}

	TEST_METHOD(IfBlockLeakRejected)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// A variable declared inside an if body must not be visible afterward.
		Messages.Reset();
		asIScriptModule* M = BuildNativeModule(SE, "ScopeIfLeak", R"(
int Entry()
{
	if (true) { int inner = 7; }
	return inner;
}
)");
		TestRunner->TestNull(TEXT("Referencing an if-body local after the block should fail compilation"), M);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
