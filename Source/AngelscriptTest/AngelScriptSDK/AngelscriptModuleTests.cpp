#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKModuleTests,
	"Angelscript.TestModule.AngelScriptSDK.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Create)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK module create test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = ScriptEngine->GetModule("SDKModuleCreate", asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("SDK module create test should create a module"), Module))
		{
			return;
		}

		const int AddResult = Module->AddScriptSection("test", "const int Value = 42; bool Entry() { return Value == 42; }");
		if (!TestRunner->TestTrue(TEXT("SDK module create test should add script section"), AddResult >= 0))
		{
			return;
		}

		const int BuildResult = Module->Build();
		if (!TestRunner->TestEqual(TEXT("SDK module create test should build successfully"), BuildResult, 0))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool Entry()");
		TestRunner->TestNotNull(TEXT("SDK module create test should find entry function"), Function);
	}

	TEST_METHOD(Discard)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK module discard test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKModuleDiscard",
			"const int Value = 100;                         \n");
		if (!TestRunner->TestNotNull(TEXT("SDK module discard test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		// Discard the module
		ScriptEngine->DiscardModule("SDKModuleDiscard");

		// Verify module is gone
		asIScriptModule* DiscardedModule = ScriptEngine->GetModule("SDKModuleDiscard", asGM_ONLY_IF_EXISTS);
		TestRunner->TestNull(TEXT("SDK module discard test should discard the module"), DiscardedModule);
	}

	TEST_METHOD(Multi)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// Create first module
		asIScriptModule* Module1 = BuildNativeModule(
			ScriptEngine,
			"SDKModuleMulti1",
			"int GetValue() { return 1; }                   \n");
		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should compile first module"), Module1))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		// Create second module
		asIScriptModule* Module2 = BuildNativeModule(
			ScriptEngine,
			"SDKModuleMulti2",
			"int GetValue() { return 2; }                   \n");
		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should compile second module"), Module2))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		// Verify both modules exist and have distinct functions
		asIScriptFunction* Func1 = GetNativeFunctionByDecl(Module1, "int GetValue()");
		asIScriptFunction* Func2 = GetNativeFunctionByDecl(Module2, "int GetValue()");

		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should find first module function"), Func1))
		{
			return;
		}

		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should find second module function"), Func2))
		{
			return;
		}

		TestRunner->TestNotEqual(TEXT("SDK module multi test should have distinct functions"), Func1, Func2);
	}
};

#endif
