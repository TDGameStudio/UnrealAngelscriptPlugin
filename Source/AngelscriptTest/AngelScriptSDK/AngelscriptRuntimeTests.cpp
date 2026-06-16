#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKRuntimeTests, "Angelscript.TestModule.AngelScriptSDK.Runtime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Context)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK runtime context test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKRuntimeContext", R"(
int Compute(int N)
{
	int Result = 0;
	for (int i = 1; i <= N; i++)
	{
		Result += i;
	}
	return Result;
}

bool Entry()
{
	return Compute(10) == 55;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK runtime context test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK runtime context test should execute context operations"), bResult);
	}

	TEST_METHOD(Exception)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK runtime exception test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKRuntimeException", R"(
void ThrowException()
{
	int a = 0;
	int b = 1 / a;
}

bool Entry()
{
	ThrowException();
	return true;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK runtime exception test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool Entry()");
		if (!TestRunner->TestNotNull(TEXT("SDK runtime exception test should resolve entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK runtime exception test should create context"), Context))
		{
			return;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		Context->Release();

		// Expect exception from divide by zero
		TestRunner->TestEqual(TEXT("SDK runtime exception test should detect exception"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION));
	}

	TEST_METHOD(Suspend)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK runtime suspend test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKRuntimeSuspend", R"(
int Sum(int N)
{
	int Result = 0;
	for (int i = 1; i <= N; i++)
	{
		Result += i;
	}
	return Result;
}

bool Entry()
{
	return Sum(10) == 55;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK runtime suspend test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK runtime suspend test should execute loop with suspend support"), bResult);
	}
};

#endif
