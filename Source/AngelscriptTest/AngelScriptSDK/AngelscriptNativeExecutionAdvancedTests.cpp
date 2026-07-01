#include "AngelscriptNativeTestSupport.h"

#include "CoreMinimal.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeExecutionAdvancedTests,
	"Angelscript.TestModule.AngelScriptSDK.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(FloatReturn)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Native float-return execution test should create a standalone engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Source = bFloatUsesFloat64
			? "double Test() { return 42.5; }"
			: "float Test() { return 42.5f; }";
		const char* Declaration = bFloatUsesFloat64
			? "double Test()"
			: "float Test()";

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "NativeExecuteFloatReturn", Source);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native float-return execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native float-return execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native float-return execution test should finish successfully")));

		if (bFloatUsesFloat64)
		{
			const double ReturnValue = Context->GetReturnDouble();
			ASSERT_THAT(IsNear(42.5, ReturnValue, 0.0001,
				TEXT("Native float-return execution test should preserve double results")));
		}
		else
		{
			ASSERT_THAT(IsNear(42.5f, Context->GetReturnFloat(), 0.0001f,
				TEXT("Native float-return execution test should preserve float results")));
		}
	}

	TEST_METHOD(NegativeValue)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Native negative-value execution test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "NativeExecuteNegativeValue", "int Test(int Start, int Delta) { return Start + Delta; }");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int, int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native negative-value execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native negative-value execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native negative-value execution test should prepare the function")));

		Context->SetArgDWord(0, static_cast<asDWORD>(10));
		Context->SetArgDWord(1, static_cast<asDWORD>(-52));
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native negative-value execution test should finish successfully")));
		ASSERT_THAT(AreEqual(-42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native negative-value execution test should preserve signed integer arguments")));
	}

	TEST_METHOD(MultipleReturnPaths)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Native multiple-return-paths execution test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "NativeExecuteMultipleReturnPaths", "int Test(int Value) { if (Value > 0) { return 40; } return 2; }");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native multiple-return-paths execution test should resolve the entry function")));

		asIScriptContext* PositiveContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(PositiveContext,
			TEXT("Native multiple-return-paths execution test should create the positive-path context")));
		ON_SCOPE_EXIT { PositiveContext->Release(); };

		const int PositivePrepareResult = PositiveContext->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PositivePrepareResult,
			TEXT("Native multiple-return-paths execution test should prepare the positive path")));

		PositiveContext->SetArgDWord(0, 1);
		const int PositiveExecuteResult = PositiveContext->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PositiveExecuteResult,
			TEXT("Native multiple-return-paths execution test should finish the positive path")));

		const int32 PositiveResult = static_cast<int32>(PositiveContext->GetReturnDWord());

		asIScriptContext* FallbackContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(FallbackContext,
			TEXT("Native multiple-return-paths execution test should create the fallback-path context")));
		ON_SCOPE_EXIT { FallbackContext->Release(); };

		const int FallbackPrepareResult = FallbackContext->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), FallbackPrepareResult,
			TEXT("Native multiple-return-paths execution test should prepare the fallback path")));

		FallbackContext->SetArgDWord(0, 0);
		const int FallbackExecuteResult = FallbackContext->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), FallbackExecuteResult,
			TEXT("Native multiple-return-paths execution test should finish the fallback path")));

		const int32 FallbackResult = static_cast<int32>(FallbackContext->GetReturnDWord());

		ASSERT_THAT(AreEqual(40, PositiveResult,
			TEXT("Native multiple-return-paths execution test should take the positive branch when Value > 0")));
		ASSERT_THAT(AreEqual(2, FallbackResult,
			TEXT("Native multiple-return-paths execution test should take the fallback branch when Value <= 0")));
	}
};

#endif
