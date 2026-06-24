#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeExecutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool BuildModuleForExecution(
		FAutomationTestBase& Test,
		FNoDiscardAsserter& Assert,
		AngelscriptNativeTestSupport::FNativeTestEngine& NativeEngine,
		const char* ModuleName,
		const char* Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		if (!Assert.IsNotNull(ScriptEngine,
			TEXT("Native execution tests should create a standalone AngelScript engine")))
		{
			return false;
		}

		OutModule = BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!Assert.IsNotNull(OutModule,
			TEXT("Native execution tests should compile the requested module from memory")))
		{
			Test.AddInfo(NativeEngine.GetMessagesText());
			return false;
		}

		return true;
	}

public:
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

	TEST_METHOD(VoidFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteVoid");
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
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteReturn");
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
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteOneArg");
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
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteTwoArgs");
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
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteThreeArgs");
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
