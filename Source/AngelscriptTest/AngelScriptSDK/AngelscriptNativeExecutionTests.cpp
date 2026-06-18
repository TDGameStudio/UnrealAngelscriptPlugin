#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	bool BuildModuleForExecution(
		FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const char* ModuleName,
		const char* Source,
		asIScriptModule*& OutModule)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Test.TestNotNull(TEXT("Native execution tests should create a standalone AngelScript engine"), ScriptEngine))
		{
			return false;
		}

		OutModule = BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!Test.TestNotNull(TEXT("Native execution tests should compile the requested module from memory"), OutModule))
		{
			Test.AddInfo(Engine.GetMessagesText());
			return false;
		}

		return true;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeExecutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeTestEngine Engine;

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

	TEST_METHOD(VoidFunction)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteVoid");
		if (!BuildModuleForExecution(*TestRunner, Engine, "NativeExecuteVoid", "void Test() {}", Module))
		{
			return;
		}

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
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteReturn");
		if (!BuildModuleForExecution(*TestRunner, Engine, "NativeExecuteReturn", "int Test() { return 42; }", Module))
		{
			return;
		}

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
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteOneArg");
		if (!BuildModuleForExecution(*TestRunner, Engine, "NativeExecuteOneArg", "int Test(int Value) { return Value * 2; }", Module))
		{
			return;
		}

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
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteTwoArgs");
		if (!BuildModuleForExecution(*TestRunner, Engine, "NativeExecuteTwoArgs", "int Test(int A, int B) { return A + B; }", Module))
		{
			return;
		}

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
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteThreeArgs");
		if (!BuildModuleForExecution(*TestRunner, Engine, "NativeExecuteThreeArgs", "int Test(int A, int B, int C) { return A + B + C; }", Module))
		{
			return;
		}

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
