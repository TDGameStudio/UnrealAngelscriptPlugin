#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	bool CreateEngineAndBuildModule(
		FAutomationTestBase& Test,
		const char* ModuleName,
		const char* Source,
		FNativeMessageCollector& OutMessages,
		asIScriptEngine*& OutScriptEngine,
		asIScriptModule*& OutModule)
	{
		OutScriptEngine = CreateNativeEngine(&OutMessages);
		if (!Test.TestNotNull(TEXT("Native execution tests should create a standalone AngelScript engine"), OutScriptEngine))
		{
			return false;
		}

		OutModule = BuildNativeModule(OutScriptEngine, ModuleName, Source);
		if (!Test.TestNotNull(TEXT("Native execution tests should compile the requested module from memory"), OutModule))
		{
			Test.AddInfo(CollectMessages(OutMessages));
			return false;
		}

		return true;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeExecutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(VoidFunction)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!CreateEngineAndBuildModule(*TestRunner, "NativeExecuteVoid", "void Test() {}", Messages, ScriptEngine, Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "void Test()");
		if (!TestRunner->TestNotNull(TEXT("Native void execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native void execution test should create a context"), Context))
		{
			return;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		Context->Release();
		TestRunner->TestEqual(TEXT("Native void execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
	}

	TEST_METHOD(ReturnValue)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!CreateEngineAndBuildModule(*TestRunner, "NativeExecuteReturn", "int Test() { return 42; }", Messages, ScriptEngine, Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test()");
		if (!TestRunner->TestNotNull(TEXT("Native return-value execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native return-value execution test should create a context"), Context))
		{
			return;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		TestRunner->TestEqual(TEXT("Native return-value execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Native return-value execution test should return 42"), static_cast<int32>(Context->GetReturnDWord()), 42);
		Context->Release();
	}

	TEST_METHOD(OneArg)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!CreateEngineAndBuildModule(*TestRunner, "NativeExecuteOneArg", "int Test(int Value) { return Value * 2; }", Messages, ScriptEngine, Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int)");
		if (!TestRunner->TestNotNull(TEXT("Native one-arg execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native one-arg execution test should create a context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!TestRunner->TestEqual(TEXT("Native one-arg execution test should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return;
		}

		Context->SetArgDWord(0, 21);
		const int ExecuteResult = Context->Execute();
		TestRunner->TestEqual(TEXT("Native one-arg execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Native one-arg execution test should preserve the provided input"), static_cast<int32>(Context->GetReturnDWord()), 42);
		Context->Release();
	}

	TEST_METHOD(TwoArgs)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!CreateEngineAndBuildModule(*TestRunner, "NativeExecuteTwoArgs", "int Test(int A, int B) { return A + B; }", Messages, ScriptEngine, Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int, int)");
		if (!TestRunner->TestNotNull(TEXT("Native two-arg execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native two-arg execution test should create a context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!TestRunner->TestEqual(TEXT("Native two-arg execution test should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return;
		}

		Context->SetArgDWord(0, 20);
		Context->SetArgDWord(1, 22);
		const int ExecuteResult = Context->Execute();
		TestRunner->TestEqual(TEXT("Native two-arg execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Native two-arg execution test should sum both arguments"), static_cast<int32>(Context->GetReturnDWord()), 42);
		Context->Release();
	}

	TEST_METHOD(ThreeArgs)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!CreateEngineAndBuildModule(*TestRunner, "NativeExecuteThreeArgs", "int Test(int A, int B, int C) { return A + B + C; }", Messages, ScriptEngine, Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int, int, int)");
		if (!TestRunner->TestNotNull(TEXT("Native three-arg execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native three-arg execution test should create a context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!TestRunner->TestEqual(TEXT("Native three-arg execution test should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return;
		}

		Context->SetArgDWord(0, 10);
		Context->SetArgDWord(1, 20);
		Context->SetArgDWord(2, 12);
		const int ExecuteResult = Context->Execute();
		TestRunner->TestEqual(TEXT("Native three-arg execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Native three-arg execution test should sum all arguments"), static_cast<int32>(Context->GetReturnDWord()), 42);
		Context->Release();
	}
};

#endif
