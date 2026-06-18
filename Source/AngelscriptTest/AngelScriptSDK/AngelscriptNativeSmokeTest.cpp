#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeSmokeTest,
	"Angelscript.TestModule.AngelScriptSDK",
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

	TEST_METHOD(Smoke)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Native smoke test should create a standalone AngelScript engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "NativeSmoke", "int Test() { return 1; }");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test()");
		if (!TestRunner->TestNotNull(TEXT("Native smoke test should resolve the compiled function by declaration"), Function))
		{
			TestRunner->AddInfo(FString::Printf(TEXT("Native smoke module functions: %s"), *CollectFunctionDeclarations(Module)));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native smoke test should create a native execution context"), Context))
		{
			return;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		TestRunner->TestEqual(TEXT("Native smoke test should finish execution successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Native smoke test should return the expected integer result"), static_cast<int32>(Context->GetReturnDWord()), 1);
		Context->Release();
	}
};

#endif
