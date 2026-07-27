#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Raw SDK context argument and return-value coverage.

#include "CoreMinimal.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FContextReturnValueTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ContextReturnValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ContextReturnValueFloatReturn)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-RETURN-ABI-SHAPES",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Native float-return execution test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const std::string ScriptSource = bFloatUsesFloat64
			? ASTEST_AS_ANSI(R"AS(
				double Test()
				{
					return 42.5;
				}
				)AS")
			: ASTEST_AS_ANSI(R"AS(
				float Test()
				{
					return 42.5f;
				}
				)AS");
		const char* Declaration = bFloatUsesFloat64
			? "double Test()"
			: "float Test()";

		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-RETURN-ABI-SHAPES-CONFIGURED-FLOAT"),
			TEXT("NativeExecuteFloatReturn"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"NativeExecuteFloatReturn",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native float-return execution test should resolve the entry function")));
		if (Function == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				TEXT("Native float-return execution test should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

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
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("Native float-return execution test should unprepare its primary context")));
		}

		{
			asIScriptContext* const ControlContext = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(ControlContext,
				TEXT("Native float-return execution test should create an independent control context")));
			if (ControlContext == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				ControlContext->Release();
			};

			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(ControlContext, Function),
				TEXT("Native float-return independent control should finish successfully")));
			if (bFloatUsesFloat64)
			{
				ASSERT_THAT(IsNear(42.5, ControlContext->GetReturnDouble(), 0.0001,
					TEXT("Native float-return independent control should preserve double results")));
			}
			else
			{
				ASSERT_THAT(IsNear(42.5f, ControlContext->GetReturnFloat(), 0.0001f,
					TEXT("Native float-return independent control should preserve float results")));
			}
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Unprepare(),
				TEXT("Native float-return independent control should unprepare cleanly")));
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Module.Discard(),
			TEXT("Native float-return execution test should explicitly discard its module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("NativeExecuteFloatReturn", asGM_ONLY_IF_EXISTS),
			TEXT("Native float-return module should be absent after explicit cleanup")));
	}

	TEST_METHOD(ContextReturnValueNegativeValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The signed integer argument and negative-return path is the signed-integer cell of RT-CTX-RETURN-ABI-SHAPES.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Native negative-value execution test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test(int Start, int Delta)
			{
				return Start + Delta;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-RETURN-ABI-SHAPES-SIGNED-INTEGER"),
			TEXT("NativeExecuteNegativeValue"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"NativeExecuteNegativeValue",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(const int, const int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native negative-value execution test should resolve the entry function")));
		if (Function == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				TEXT("Native negative-value execution test should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			const int PrepareResult = Context->Prepare(Function);
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
				TEXT("Native negative-value execution test should prepare the function")));

			ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, static_cast<asDWORD>(10)),
				TEXT("Native negative-value execution test should set the start argument")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(1, static_cast<asDWORD>(-52)),
				TEXT("Native negative-value execution test should set the delta argument")));
			const int ExecuteResult = Context->Execute();
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				TEXT("Native negative-value execution test should finish successfully")));
			ASSERT_THAT(AreEqual(-42, static_cast<int32>(Context->GetReturnDWord()),
				TEXT("Native negative-value execution test should preserve signed integer arguments")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("Native negative-value execution test should unprepare its primary context")));
		}

		{
			asIScriptContext* const ControlContext = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(ControlContext,
				TEXT("Native negative-value execution test should create an independent control context")));
			if (ControlContext == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				ControlContext->Release();
			};

			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Prepare(Function),
				TEXT("Native negative-value independent control should prepare the function")));
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->SetArgDWord(0, static_cast<asDWORD>(10)),
				TEXT("Native negative-value independent control should set the start argument")));
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->SetArgDWord(1, static_cast<asDWORD>(-52)),
				TEXT("Native negative-value independent control should set the delta argument")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ControlContext->Execute(),
				TEXT("Native negative-value independent control should finish successfully")));
			ASSERT_THAT(AreEqual(-42, static_cast<int32>(ControlContext->GetReturnDWord()),
				TEXT("Native negative-value independent control should preserve the signed result")));
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Unprepare(),
				TEXT("Native negative-value independent control should unprepare cleanly")));
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Module.Discard(),
			TEXT("Native negative-value execution test should explicitly discard its module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("NativeExecuteNegativeValue", asGM_ONLY_IF_EXISTS),
			TEXT("Native negative-value module should be absent after explicit cleanup")));
	}

	TEST_METHOD(MultipleReturnPaths)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-RETURN-CONTROL-PATHS",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Native multiple-return-paths execution test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test(int Value)
			{
				if (Value > 0)
				{
					return 40;
				}

				return 2;
			}
			)AS");
		for (const TCHAR* CaseId : {
			TEXT("RT-CTX-RETURN-CONTROL-PATHS-POSITIVE"),
			TEXT("RT-CTX-RETURN-CONTROL-PATHS-FALLBACK") })
		{
			PrintGeneratedAsSource(
				*TestRunner,
				CaseId,
				TEXT("NativeExecuteMultipleReturnPaths"),
				UTF8_TO_TCHAR(ScriptSource.c_str()));
		}
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"NativeExecuteMultipleReturnPaths",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(const int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native multiple-return-paths execution test should resolve the entry function")));
		if (Function == nullptr)
		{
			return;
		}

		asIScriptContext* PositiveContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(PositiveContext,
			TEXT("Native multiple-return-paths execution test should create the positive-path context")));
		if (PositiveContext == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			if (PositiveContext != nullptr)
			{
				PositiveContext->Release();
			}
		};

		const int PositivePrepareResult = PositiveContext->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PositivePrepareResult,
			TEXT("Native multiple-return-paths execution test should prepare the positive path")));

		ASSERT_THAT(AreEqual(asSUCCESS, PositiveContext->SetArgDWord(0, 1),
			TEXT("Native multiple-return-paths execution test should set the positive-path argument")));
		const int PositiveExecuteResult = PositiveContext->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PositiveExecuteResult,
			TEXT("Native multiple-return-paths execution test should finish the positive path")));

		const int32 PositiveResult = static_cast<int32>(PositiveContext->GetReturnDWord());
		ASSERT_THAT(AreEqual(asSUCCESS, PositiveContext->Unprepare(),
			TEXT("Native multiple-return-paths execution test should unprepare the positive-path context")));

		asIScriptContext* FallbackContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(FallbackContext,
			TEXT("Native multiple-return-paths execution test should create the fallback-path context")));
		if (FallbackContext == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			if (FallbackContext != nullptr)
			{
				FallbackContext->Release();
			}
		};

		const int FallbackPrepareResult = FallbackContext->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), FallbackPrepareResult,
			TEXT("Native multiple-return-paths execution test should prepare the fallback path")));

		ASSERT_THAT(AreEqual(asSUCCESS, FallbackContext->SetArgDWord(0, 0),
			TEXT("Native multiple-return-paths execution test should set the fallback-path argument")));
		const int FallbackExecuteResult = FallbackContext->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), FallbackExecuteResult,
			TEXT("Native multiple-return-paths execution test should finish the fallback path")));

		const int32 FallbackResult = static_cast<int32>(FallbackContext->GetReturnDWord());
		ASSERT_THAT(AreEqual(asSUCCESS, FallbackContext->Unprepare(),
			TEXT("Native multiple-return-paths execution test should unprepare the fallback-path context")));

		ASSERT_THAT(AreEqual(40, PositiveResult,
			TEXT("Native multiple-return-paths execution test should take the positive branch when Value > 0")));
		ASSERT_THAT(AreEqual(2, FallbackResult,
			TEXT("Native multiple-return-paths execution test should take the fallback branch when Value <= 0")));
		FallbackContext->Release();
		FallbackContext = nullptr;
		PositiveContext->Release();
		PositiveContext = nullptr;
		ASSERT_THAT(AreEqual(asSUCCESS, Module.Discard(),
			TEXT("Native multiple-return-paths execution test should explicitly discard its module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("NativeExecuteMultipleReturnPaths", asGM_ONLY_IF_EXISTS),
			TEXT("Native multiple-return-paths module should be absent after explicit cleanup")));
	}
};

#endif
