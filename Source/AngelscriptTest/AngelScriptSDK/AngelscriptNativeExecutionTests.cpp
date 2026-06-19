#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	bool BuildModuleForExecution(
		FAutomationTestBase& Test,
		FNoDiscardAsserter& Assert,
		FNativeTestEngine& Engine,
		const char* ModuleName,
		const char* Source,
		asIScriptModule*& OutModule)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assert.IsNotNull(ScriptEngine,
			TEXT("Native execution tests should create a standalone AngelScript engine")))
		{
			return false;
		}

		OutModule = BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!Assert.IsNotNull(OutModule,
			TEXT("Native execution tests should compile the requested module from memory")))
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
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteVoid", "void Test() {}", Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "void Test()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native void execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native void execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native void execution test should finish successfully")));
	}

	TEST_METHOD(ReturnValue)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteReturn");
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteReturn", "int Test() { return 42; }", Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native return-value execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native return-value execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native return-value execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native return-value execution test should return 42")));
	}

	TEST_METHOD(OneArg)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteOneArg");
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteOneArg", "int Test(int Value) { return Value * 2; }", Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native one-arg execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native one-arg execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native one-arg execution test should prepare the function")));

		Context->SetArgDWord(0, 21);
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native one-arg execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native one-arg execution test should preserve the provided input")));
	}

	TEST_METHOD(TwoArgs)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteTwoArgs");
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteTwoArgs", "int Test(int A, int B) { return A + B; }", Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int, int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native two-arg execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native two-arg execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native two-arg execution test should prepare the function")));

		Context->SetArgDWord(0, 20);
		Context->SetArgDWord(1, 22);
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native two-arg execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native two-arg execution test should sum both arguments")));
	}

	TEST_METHOD(ThreeArgs)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteThreeArgs");
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteThreeArgs", "int Test(int A, int B, int C) { return A + B + C; }", Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int, int, int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native three-arg execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native three-arg execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native three-arg execution test should prepare the function")));

		Context->SetArgDWord(0, 10);
		Context->SetArgDWord(1, 20);
		Context->SetArgDWord(2, 12);
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native three-arg execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native three-arg execution test should sum all arguments")));
	}
};

#endif
