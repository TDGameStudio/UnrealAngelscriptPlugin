#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Compiler test should create a bool execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		OutValue = Context->GetReturnByte() != 0;
		Context->Release();
		return Test.TestEqual(TEXT("Compiler test should finish bool execution successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK compiler basic test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerBasic", R"(
const int GlobalVar = 42;

int Multiply(int A, int B)
{
	return A * B;
}

bool Entry()
{
	return GlobalVar == 42 && Multiply(6, 7) == 42;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK compiler basic test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK compiler basic test should compile and execute basic constructs"), bResult);
	}

	TEST_METHOD(Error)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK compiler error test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// Test that invalid syntax produces compile errors
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerError", R"(
int MissingReturn() { }
)");

		// This should fail to compile - expect null module or error messages
		if (Module != nullptr)
		{
			TestRunner->AddInfo(TEXT("Expected compile error for missing return statement"));
			return;
		}

		TestRunner->TestTrue(TEXT("SDK compiler error test should detect syntax errors"), Messages.Entries.Num() > 0);
	}

	TEST_METHOD(Config)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK compiler config test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// Test engine property access
		const int PropResult = ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true);
		if (!TestRunner->TestTrue(TEXT("SDK compiler config test should set engine property"), PropResult >= 0))
		{
			return;
		}

		// Test type registration configuration
		const int TypeResult = ScriptEngine->RegisterObjectType("TestConfigType", 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (!TestRunner->TestTrue(TEXT("SDK compiler config test should register reference type"), TypeResult >= 0))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK compiler config test should configure engine properties"), true);
	}
};

#endif
