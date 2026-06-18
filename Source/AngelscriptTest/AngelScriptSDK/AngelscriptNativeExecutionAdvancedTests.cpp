#include "AngelscriptNativeTestSupport.h"

#include "CoreMinimal.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeExecutionAdvancedTests,
	"Angelscript.TestModule.AngelScriptSDK.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeSdkEngineFixture EngineFixture;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
	}

	BEFORE_EACH()
	{
		EngineFixture.ResetMessages();
	}

	TEST_METHOD(FloatReturn)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Native float-return execution test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Source = bFloatUsesFloat64
			? "double Test() { return 42.5; }"
			: "float Test() { return 42.5f; }";
		const char* Declaration = bFloatUsesFloat64
			? "double Test()"
			: "float Test()";

		FScopedNativeModule Module(*TestRunner, EngineFixture, "NativeExecuteFloatReturn", Source);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!TestRunner->TestNotNull(TEXT("Native float-return execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native float-return execution test should create a context"), Context))
		{
			return;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		if (!TestRunner->TestEqual(TEXT("Native float-return execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return;
		}

		if (bFloatUsesFloat64)
		{
			const double ReturnValue = Context->GetReturnDouble();
			TestRunner->TestTrue(TEXT("Native float-return execution test should preserve double results"), FMath::IsNearlyEqual(ReturnValue, 42.5, 0.0001));
		}
		else
		{
			TestRunner->TestEqual(TEXT("Native float-return execution test should preserve float results"), Context->GetReturnFloat(), 42.5f, 0.0001f);
		}

		Context->Release();
	}

	TEST_METHOD(NegativeValue)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Native negative-value execution test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "NativeExecuteNegativeValue", "int Test(int Start, int Delta) { return Start + Delta; }");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int, int)");
		if (!TestRunner->TestNotNull(TEXT("Native negative-value execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native negative-value execution test should create a context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!TestRunner->TestEqual(TEXT("Native negative-value execution test should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return;
		}

		Context->SetArgDWord(0, static_cast<asDWORD>(10));
		Context->SetArgDWord(1, static_cast<asDWORD>(-52));
		const int ExecuteResult = Context->Execute();
		TestRunner->TestEqual(TEXT("Native negative-value execution test should finish successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Native negative-value execution test should preserve signed integer arguments"), static_cast<int32>(Context->GetReturnDWord()), -42);
		Context->Release();
	}

	TEST_METHOD(MultipleReturnPaths)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Native multiple-return-paths execution test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "NativeExecuteMultipleReturnPaths", "int Test(int Value) { if (Value > 0) { return 40; } return 2; }");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int)");
		if (!TestRunner->TestNotNull(TEXT("Native multiple-return-paths execution test should resolve the entry function"), Function))
		{
			return;
		}

		asIScriptContext* PositiveContext = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native multiple-return-paths execution test should create the positive-path context"), PositiveContext))
		{
			return;
		}

		const int PositivePrepareResult = PositiveContext->Prepare(Function);
		if (!TestRunner->TestEqual(TEXT("Native multiple-return-paths execution test should prepare the positive path"), PositivePrepareResult, static_cast<int32>(asSUCCESS)))
		{
			PositiveContext->Release();
			return;
		}

		PositiveContext->SetArgDWord(0, 1);
		const int PositiveExecuteResult = PositiveContext->Execute();
		if (!TestRunner->TestEqual(TEXT("Native multiple-return-paths execution test should finish the positive path"), PositiveExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			PositiveContext->Release();
			return;
		}

		const int32 PositiveResult = static_cast<int32>(PositiveContext->GetReturnDWord());
		PositiveContext->Release();

		asIScriptContext* FallbackContext = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Native multiple-return-paths execution test should create the fallback-path context"), FallbackContext))
		{
			return;
		}

		const int FallbackPrepareResult = FallbackContext->Prepare(Function);
		if (!TestRunner->TestEqual(TEXT("Native multiple-return-paths execution test should prepare the fallback path"), FallbackPrepareResult, static_cast<int32>(asSUCCESS)))
		{
			FallbackContext->Release();
			return;
		}

		FallbackContext->SetArgDWord(0, 0);
		const int FallbackExecuteResult = FallbackContext->Execute();
		if (!TestRunner->TestEqual(TEXT("Native multiple-return-paths execution test should finish the fallback path"), FallbackExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			FallbackContext->Release();
			return;
		}

		const int32 FallbackResult = static_cast<int32>(FallbackContext->GetReturnDWord());
		FallbackContext->Release();

		TestRunner->TestEqual(TEXT("Native multiple-return-paths execution test should take the positive branch when Value > 0"), PositiveResult, 40);
		TestRunner->TestEqual(TEXT("Native multiple-return-paths execution test should take the fallback branch when Value <= 0"), FallbackResult, 2);
	}
};

#endif
