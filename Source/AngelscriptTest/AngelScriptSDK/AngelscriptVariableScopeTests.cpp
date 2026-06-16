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
};

#endif // WITH_DEV_AUTOMATION_TESTS
