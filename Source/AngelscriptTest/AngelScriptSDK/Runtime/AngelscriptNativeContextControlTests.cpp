#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeDebugTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FContextControlTests, "Angelscript.TestModule.AngelScriptSDK.Runtime.ContextControl", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static int LastSuspendResult = asERROR;

	static void SuspendActiveContext()
	{
		if (asIScriptContext* Context = asGetActiveContext())
		{
			LastSuspendResult = Context->Suspend();
		}
	}

	static void CountSuspendLine(asCContext*)
	{
		++SuspendLineCallbackCount;
	}

	inline static int32 SuspendLineCallbackCount = 0;

	TEST_METHOD(ContextControlContext)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-CONTROL-FLOW-EXECUTION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime context test should create a standalone engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
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
				return Compute(0) == 0
					&& Compute(1) == 1
					&& Compute(10) == 55;
			}
			)AS");
		for (const TCHAR* CaseId : {
			TEXT("RT-CTX-CONTROL-FLOW-EXECUTION-ZERO"),
			TEXT("RT-CTX-CONTROL-FLOW-EXECUTION-ONE"),
			TEXT("RT-CTX-CONTROL-FLOW-EXECUTION-TEN") })
		{
			PrintGeneratedAsSource(
				*TestRunner,
				CaseId,
				TEXT("SDKRuntimeContext"),
				UTF8_TO_TCHAR(ScriptSource.c_str()));
		}
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKRuntimeContext",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "bool Entry()");
		ASSERT_THAT(IsNotNull(Entry,
			TEXT("SDK runtime context test should resolve the context entry function")));
		if (Entry == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				TEXT("SDK runtime context test should create its primary context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			const int ExecuteResult = PrepareAndExecute(Context, Entry);
			const bool bResult = Context->GetReturnByte() != 0;
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				TEXT("SDK runtime context test should execute context operations")));
			ASSERT_THAT(IsTrue(bResult,
				TEXT("SDK runtime context test should execute context operations")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("SDK runtime context test should unprepare its primary context after execution")));
		}

		{
			asIScriptContext* const ControlContext = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(ControlContext,
				TEXT("SDK runtime context test should create an independent control context")));
			if (ControlContext == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				ControlContext->Release();
			};

			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(ControlContext, Entry),
				TEXT("SDK runtime context control should execute independently of the primary context")));
			ASSERT_THAT(IsTrue(ControlContext->GetReturnByte() != 0,
				TEXT("SDK runtime context control should retain the loop result independently")));
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Unprepare(),
				TEXT("SDK runtime context control should unprepare after its independent execution")));
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Module.Discard(),
			TEXT("SDK runtime context test should explicitly discard its module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("SDKRuntimeContext", asGM_ONLY_IF_EXISTS),
			TEXT("SDK runtime context module should be absent after explicit cleanup")));
	}

	TEST_METHOD(UnhandledExceptionReturnsExceptionOutcome)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The legacy unhandled divide-by-zero outcome is retained as an independent compatibility assertion for RT-CTX-ARITHMETIC-EXCEPTION-DETAILS.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime exception test should create a standalone engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
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
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-ARITHMETIC-EXCEPTION-DETAILS-DIVIDE-NESTED-COMPAT"),
			TEXT("SDKRuntimeException"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKRuntimeException",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool Entry()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK runtime exception test should resolve entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime exception test should create context")));
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Function);

		// Expect exception from divide by zero
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
			TEXT("SDK runtime exception test should detect exception")));
	}

	TEST_METHOD(SuspendAndResumePreserveContextState)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-SUSPEND-FORK-REJECTION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime suspend test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FScopedNativeDebugCallbacks DebugCallbacks;

		const ASAutoCaller::FunctionCaller SuspendCaller = ASAutoCaller::MakeFunctionCaller(SuspendActiveContext);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction("void PauseExecution()", asFUNCTION(SuspendActiveContext), asCALL_CDECL, *(asFunctionCaller*)&SuspendCaller) >= 0,
			TEXT("SDK runtime suspend test should register a native suspension callback")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				PauseExecution();
				return 42;
			}

			int Control()
			{
				return 7;
			}
			)AS");
		for (const TCHAR* CaseId : {
			TEXT("RT-CTX-SUSPEND-FORK-REJECTION-CALLBACK-RESULT"),
			TEXT("RT-CTX-SUSPEND-FORK-REJECTION-EXECUTION-STATE"),
			TEXT("RT-CTX-SUSPEND-FORK-REJECTION-RETURN-VALUE") })
		{
			PrintGeneratedAsSource(
				*TestRunner,
				CaseId,
				TEXT("SDKRuntimeSuspend"),
				UTF8_TO_TCHAR(ScriptSource.c_str()));
		}
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKRuntimeSuspend",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		asIScriptFunction* ControlFunction = GetNativeFunctionByExactDecl(Module, "int Control()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK runtime suspend test should resolve the entry function")));
		ASSERT_THAT(IsNotNull(ControlFunction,
			TEXT("SDK runtime suspend test should resolve the independent control function")));
		if (Function == nullptr || ControlFunction == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				TEXT("SDK runtime suspend test should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			asCContext* const RawContext = static_cast<asCContext*>(Context);
			SuspendLineCallbackCount = 0;
			ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CountSuspendLine),
				TEXT("SDK runtime suspend test should install a callback before the rejected suspend request")));

			LastSuspendResult = asSUCCESS;
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function),
				TEXT("SDK runtime suspend test should keep executing when the current fork rejects suspension")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asERROR), LastSuspendResult,
				TEXT("SDK runtime suspend test should expose the current fork's explicit unsupported Suspend result")));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
				TEXT("SDK runtime suspend test should preserve the return after the rejected suspend request")));
			ASSERT_THAT(IsTrue(SuspendLineCallbackCount > 0,
				TEXT("SDK runtime suspend test callback should observe the rejected-suspend execution")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("SDK runtime suspend test should unprepare after the rejected suspend request")));

			RawContext->ClearLineCallback();
			const int32 CallbackCountBeforeClearControl = SuspendLineCallbackCount;
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, ControlFunction),
				TEXT("SDK runtime suspend test should reuse the cleared-callback context for control execution")));
			ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord()),
				TEXT("SDK runtime suspend test cleared-callback control should retain its return value")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("SDK runtime suspend test cleared-callback control should unprepare cleanly")));
			ASSERT_THAT(AreEqual(CallbackCountBeforeClearControl, SuspendLineCallbackCount,
				TEXT("SDK runtime suspend test callback should not fire after ClearLineCallback")));
		}

		{
			asIScriptContext* const ControlContext = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(ControlContext,
				TEXT("SDK runtime suspend test should create an independent control context")));
			if (ControlContext == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				ControlContext->Release();
			};

			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(ControlContext, ControlFunction),
				TEXT("SDK runtime suspend test independent control should finish without the suspension callback")));
			ASSERT_THAT(AreEqual(7, static_cast<int32>(ControlContext->GetReturnDWord()),
				TEXT("SDK runtime suspend test independent control should retain its own return value")));
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Unprepare(),
				TEXT("SDK runtime suspend test independent control should unprepare cleanly")));
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Module.Discard(),
			TEXT("SDK runtime suspend test should explicitly discard its module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("SDKRuntimeSuspend", asGM_ONLY_IF_EXISTS),
			TEXT("SDK runtime suspend module should be absent after explicit cleanup")));
	}

	TEST_METHOD(ExceptionDetails)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-ARITHMETIC-EXCEPTION-DETAILS",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime exception-details test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		struct FExceptionCase
		{
			const TCHAR* OperationId;
			const TCHAR* Operator;
			const TCHAR* PlacementId;
			bool bNested;
		};
		const FExceptionCase Cases[] =
		{
			{ TEXT("divide"), TEXT("/"), TEXT("direct"), false },
			{ TEXT("divide"), TEXT("/"), TEXT("nested"), true },
			{ TEXT("modulo"), TEXT("%"), TEXT("direct"), false },
			{ TEXT("modulo"), TEXT("%"), TEXT("nested"), true },
		};

		for (const FExceptionCase& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"RT-CTX-ARITHMETIC-EXCEPTION-DETAILS",
				{ Case.OperationId, Case.PlacementId });
			const FString ModuleName = FString::Printf(
				TEXT("SDKRuntimeException_%s_%s"),
				Case.OperationId,
				Case.PlacementId);
			FString Source;
			if (Case.bNested)
			{
				AppendGeneratedAsLine(Source, TEXT("int Fault(int A, int B)"));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\treturn A %s B;"),
					Case.Operator));
				AppendGeneratedAsLine(Source, TEXT("}"));
				AppendGeneratedAsLine(Source);
			}
			AppendGeneratedAsLine(Source, TEXT("int Entry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			if (Case.bNested)
			{
				AppendGeneratedAsLine(Source, TEXT("\treturn Fault(10, 0);"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tint Left = 10;"));
				AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\treturn Left %s Zero;"),
					Case.Operator));
			}
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int Recover()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 7;"));
			AppendGeneratedAsLine(Source, TEXT("}"));

			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);
			FTCHARToUTF8 SourceUtf8(*Source);
			FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get());
			ASSERT_THAT(IsTrue(
				Module.IsValid(),
				*FString::Printf(TEXT("%s should compile"), *CaseId)));
			if (!Module.IsValid())
			{
				continue;
			}

			asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
			asIScriptFunction* const RecoveryFunction = GetNativeFunctionByExactDecl(Module, "int Recover()");
			ASSERT_THAT(IsNotNull(
				Function,
				*FString::Printf(TEXT("%s should resolve Entry exactly"), *CaseId)));
			ASSERT_THAT(IsNotNull(
				RecoveryFunction,
				*FString::Printf(TEXT("%s should resolve its recovery function exactly"), *CaseId)));
			if (Function == nullptr || RecoveryFunction == nullptr)
			{
				continue;
			}
			{
				asIScriptContext* const Context = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(
					Context,
					*FString::Printf(TEXT("%s should create a context"), *CaseId)));
				if (Context == nullptr)
				{
					continue;
				}
				ON_SCOPE_EXIT
				{
					Context->Release();
				};

				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_EXCEPTION),
					PrepareAndExecute(Context, Function),
					*FString::Printf(TEXT("%s should raise an execution exception"), *CaseId)));
				const FString ExceptionString = UTF8_TO_TCHAR(
					Context->GetExceptionString() != nullptr
						? Context->GetExceptionString()
						: "");
				ASSERT_THAT(AreEqual(
					FString(TEXT("Divide by zero")),
					ExceptionString,
					*FString::Printf(TEXT("%s should report the exact fork exception text"), *CaseId)));
				ASSERT_THAT(IsTrue(
					Context->GetExceptionLineNumber() > 0,
					*FString::Printf(TEXT("%s should report a positive exception line"), *CaseId)));
				asIScriptFunction* const ExceptionFunction = Context->GetExceptionFunction();
				ASSERT_THAT(IsNotNull(
					ExceptionFunction,
					*FString::Printf(TEXT("%s should report the exception function"), *CaseId)));
				if (ExceptionFunction != nullptr)
				{
					ASSERT_THAT(AreEqual(
						FString(Case.bNested ? TEXT("Fault") : TEXT("Entry")),
						FString(UTF8_TO_TCHAR(ExceptionFunction->GetName())),
						*FString::Printf(TEXT("%s should attribute the exact failing frame"), *CaseId)));
				}
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
					*FString::Printf(TEXT("%s should unprepare its exception state before recovery"), *CaseId)));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, RecoveryFunction),
					*FString::Printf(TEXT("%s should execute recovery on the same context"), *CaseId)));
				ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord()),
					*FString::Printf(TEXT("%s recovery should retain its independent return value"), *CaseId)));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
					*FString::Printf(TEXT("%s recovery should unprepare the reused context"), *CaseId)));
			}

			{
				asIScriptContext* const ControlContext = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(ControlContext,
					*FString::Printf(TEXT("%s should create an independent control context"), *CaseId)));
				if (ControlContext == nullptr)
				{
					continue;
				}
				ON_SCOPE_EXIT
				{
					ControlContext->Release();
				};

				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(ControlContext, RecoveryFunction),
					*FString::Printf(TEXT("%s independent control should finish without the prior exception"), *CaseId)));
				ASSERT_THAT(AreEqual(7, static_cast<int32>(ControlContext->GetReturnDWord()),
					*FString::Printf(TEXT("%s independent control should retain its return value"), *CaseId)));
				ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Unprepare(),
					*FString::Printf(TEXT("%s independent control should unprepare cleanly"), *CaseId)));
			}

			ASSERT_THAT(AreEqual(asSUCCESS, Module.Discard(),
				*FString::Printf(TEXT("%s should explicitly discard its module"), *CaseId)));
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*FString::Printf(TEXT("%s module should be absent after explicit cleanup"), *CaseId)));
		}
	}

	TEST_METHOD(ContextControlModuloByZero)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The direct modulo-by-zero compatibility assertion is retained under RT-CTX-ARITHMETIC-EXCEPTION-DETAILS.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime modulo-by-zero test should create a standalone engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int a = 7;
				int b = 0;
				return a % b;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-ARITHMETIC-EXCEPTION-DETAILS-MODULO-DIRECT-COMPAT"),
			TEXT("SDKRuntimeModuloByZero"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKRuntimeModuloByZero",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK runtime modulo-by-zero test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime modulo-by-zero test should create a context")));
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
			TEXT("SDK runtime modulo-by-zero test should raise an execution exception")));

		ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), ExceptionString,
			TEXT("SDK runtime modulo-by-zero test should report the divide-by-zero exception text")));
	}

	TEST_METHOD(ContextReuseAfterException)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-EXCEPTION-RECOVERY-SIGNATURE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime context-reuse test should create a standalone engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Boom()
			{
				int a = 0;
				return 1 / a;
			}

			int SafeSum()
			{
				int total = 0;
				for (int i = 1; i <= 5; i++)
				{
					total += i;
				}
				return total;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-EXCEPTION-RECOVERY-SIGNATURE-SHALLOW-SAME-ARITY"),
			TEXT("SDKRuntimeContextReuse"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKRuntimeContextReuse",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* BoomFn = GetNativeFunctionByDecl(Module, "int Boom()");
		asIScriptFunction* SafeFn = GetNativeFunctionByDecl(Module, "int SafeSum()");
		ASSERT_THAT(IsNotNull(BoomFn,
			TEXT("SDK runtime context-reuse test should resolve Boom")));
		ASSERT_THAT(IsNotNull(SafeFn,
			TEXT("SDK runtime context-reuse test should resolve SafeSum")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime context-reuse test should create a context")));
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		// First execution throws.
		const int FirstResult = PrepareAndExecute(Context, BoomFn);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), FirstResult,
			TEXT("SDK runtime context-reuse test should throw on the first call")));

		// The same context must be reusable for a fresh, successful execution.
		const int SecondResult = PrepareAndExecute(Context, SafeFn);
		const int32 Sum = static_cast<int32>(Context->GetReturnDWord());

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), SecondResult,
			TEXT("SDK runtime context-reuse test should finish the second call after re-Prepare")));
		ASSERT_THAT(AreEqual(15, Sum,
			TEXT("SDK runtime context-reuse test should compute SafeSum = 15 after recovering from exception")));
	}
};

#endif
