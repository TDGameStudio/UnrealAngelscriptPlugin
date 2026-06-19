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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native smoke test should create a standalone AngelScript engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "NativeSmoke", "int Test() { return 1; }");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test()");
		if (!this->Assert.IsNotNull(Function, TEXT("Native smoke test should resolve the compiled function by declaration")))
		{
			TestRunner->AddInfo(FString::Printf(TEXT("Native smoke module functions: %s"), *CollectFunctionDeclarations(Module)));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Native smoke test should create a native execution context")));
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native smoke test should finish execution successfully")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native smoke test should return the expected integer result")));
	}
};

#endif
