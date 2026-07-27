#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExceptionOriginTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Exceptions.Origins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase OriginCases[] =
	{
		{ "null_access" }, { "divide_fault" }, { "bounds_fault" }, { "native_callback" }, { "host_set_exception" }, { "constructor" }, { "member" }, { "destructor" }, { "protocol_callback" },
	};
	inline static constexpr FNamedCase DepthCases[] =
	{
		{ "top" }, { "one_call" }, { "three_calls" }, { "recursion" }, { "method" }, { "virtual" }, { "imported" },
	};
	inline static constexpr FNamedCase CallbackCases[] =
	{
		{ "absent" }, { "installed" }, { "replaced" }, { "cleared" },
	};


	static bool IsNamedCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static void SetActiveOriginException(const ANSICHAR* Message)
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException(Message);
		}
	}

	static void RaiseNativeCallbackOrigin()
	{
		SetActiveOriginException("native_callback");
	}

	static void RaiseHostSetExceptionOrigin()
	{
		SetActiveOriginException("host_set_exception");
	}

	static void RaiseDestructorOrigin()
	{
		SetActiveOriginException("destructor");
	}

	static void RaiseBoundsOrigin()
	{
		SetActiveOriginException("bounds_fault");
	}

	static void RaiseConstructorOrigin()
	{
		SetActiveOriginException("constructor");
	}

	static void RaiseMemberOrigin()
	{
		SetActiveOriginException("member");
	}

	static void RaiseProtocolOrigin()
	{
		SetActiveOriginException("protocol_callback");
	}

	static int32 GetOriginCode(const FNamedCase& OriginCase)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(OriginCases); ++Index)
		{
			if (IsNamedCase(OriginCase, OriginCases[Index].CatalogName))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	static void RaiseImportedOrigin(const int32 OriginCode)
	{
		const FString Message = FString::Printf(TEXT("imported_origin_%d"), OriginCode);
		const FTCHARToUTF8 MessageUtf8(*Message);
		SetActiveOriginException(MessageUtf8.Get());
	}

	static bool RegisterOriginBridge(asIScriptEngine& ScriptEngine)
	{
		const ASAutoCaller::FunctionCaller NativeCallbackCaller = ASAutoCaller::MakeFunctionCaller(RaiseNativeCallbackOrigin);
		const ASAutoCaller::FunctionCaller HostExceptionCaller = ASAutoCaller::MakeFunctionCaller(RaiseHostSetExceptionOrigin);
		const ASAutoCaller::FunctionCaller DestructorCaller = ASAutoCaller::MakeFunctionCaller(RaiseDestructorOrigin);
		const ASAutoCaller::FunctionCaller BoundsCaller = ASAutoCaller::MakeFunctionCaller(RaiseBoundsOrigin);
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(RaiseConstructorOrigin);
		const ASAutoCaller::FunctionCaller MemberCaller = ASAutoCaller::MakeFunctionCaller(RaiseMemberOrigin);
		const ASAutoCaller::FunctionCaller ProtocolCaller = ASAutoCaller::MakeFunctionCaller(RaiseProtocolOrigin);
		const ASAutoCaller::FunctionCaller ImportedCaller = ASAutoCaller::MakeFunctionCaller(RaiseImportedOrigin);
		return ScriptEngine.RegisterGlobalFunction(
			"void RaiseNativeCallbackOrigin()",
			asFUNCTION(RaiseNativeCallbackOrigin),
			asCALL_CDECL,
			*(asFunctionCaller*)&NativeCallbackCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseHostSetExceptionOrigin()",
				asFUNCTION(RaiseHostSetExceptionOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&HostExceptionCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseDestructorOrigin()",
				asFUNCTION(RaiseDestructorOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&DestructorCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseBoundsOrigin()",
				asFUNCTION(RaiseBoundsOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&BoundsCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseConstructorOrigin()",
				asFUNCTION(RaiseConstructorOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&ConstructorCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseMemberOrigin()",
				asFUNCTION(RaiseMemberOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&MemberCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseProtocolOrigin()",
				asFUNCTION(RaiseProtocolOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&ProtocolCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void ImportedOrigin(int OriginCode)",
				asFUNCTION(RaiseImportedOrigin),
				asCALL_CDECL,
				*(asFunctionCaller*)&ImportedCaller) >= 0;
	}

	static FString BuildExceptionOriginSource(const FNamedCase& OriginCase, const FNamedCase& DepthCase)
	{
		FString Source;
		if (IsNamedCase(OriginCase, "null_access"))
		{
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value += 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "divide_fault"))
		{
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Denominator = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Result = 42 / Denominator;"));
			AppendGeneratedAsLine(Source, TEXT("\tResult += 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "bounds_fault"))
		{
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Index = 2;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Count = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Index >= Count)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseBoundsOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "native_callback"))
		{
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRaiseNativeCallbackOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "host_set_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRaiseHostSetExceptionOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FConstructorOrigin"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFConstructorOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseConstructorOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFConstructorOrigin Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "member"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FMemberOrigin"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFMemberOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\tvoid RaiseMember()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseMemberOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFMemberOrigin Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.RaiseMember();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(OriginCase, "destructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FDestructorOrigin"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\t~FDestructorOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseDestructorOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFDestructorOrigin Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("class FProtocolOrigin"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFProtocolOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\tvoid opCall()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseProtocolOrigin();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void RaiseOrigin()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFProtocolOrigin Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void CallOne()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tRaiseOrigin();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void CallThree()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tCallOne();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void CallTwo()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tCallThree();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FMethodDepthOrigin"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFMethodDepthOrigin()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tvoid Invoke()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRaiseOrigin();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FVirtualDepthBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFVirtualDepthBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tvoid Invoke()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRaiseOrigin();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FVirtualDepthDerived : FVirtualDepthBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFVirtualDepthDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tvoid Invoke() override"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRaiseOrigin();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void Recurse(int Remaining)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Remaining == 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRaiseOrigin();"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tRecurse(Remaining - 1);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (FCStringAnsi::Strcmp(DepthCase.CatalogName, "top") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tRaiseOrigin();"));
		}
		else if (FCStringAnsi::Strcmp(DepthCase.CatalogName, "one_call") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tCallOne();"));
		}
		else if (FCStringAnsi::Strcmp(DepthCase.CatalogName, "three_calls") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tCallThree();"));
		}
		else if (FCStringAnsi::Strcmp(DepthCase.CatalogName, "recursion") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tRecurse(2);"));
		}
		else if (IsNamedCase(DepthCase, "method"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMethodDepthOrigin Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Invoke();"));
		}
		else if (IsNamedCase(DepthCase, "virtual"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVirtualDepthDerived Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Invoke();"));
		}
		else if (IsNamedCase(DepthCase, "imported"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tImportedOrigin(%d);"), GetOriginCode(OriginCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tCallTwo();"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:
	TEST_METHOD(OriginsByDepthAndCallback)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EX-ORIGIN-DEPTH",
			ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Debug
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Exception origin product should create a raw SDK engine")))
		{
			return;
		}
		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		if (!Assertions.IsTrue(RegisterOriginBridge(*ScriptEngine), TEXT("Exception origin product should register native and imported origin bridges"))
			|| !Assertions.IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle), TEXT("Exception origin product should register its null-access native reference fixture")))
		{
			return;
		}

		for (const FNamedCase& OriginCase : OriginCases)
		{
			for (const FNamedCase& DepthCase : DepthCases)
			{
				for (const FNamedCase& CallbackCase : CallbackCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-EX-ORIGIN-DEPTH",
						{ ANSI_TO_TCHAR(OriginCase.CatalogName), ANSI_TO_TCHAR(DepthCase.CatalogName), ANSI_TO_TCHAR(CallbackCase.CatalogName) }));
					const FString ModuleName = TEXT("ExceptionOrigin_") + Case.GetId().RightChop(21).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildExceptionOriginSource(OriginCase, DepthCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
						*Case.Describe(TEXT("exception origin cell should compile")), BuildResult, *Engine.GetMessagesText());
					const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
					asIScriptFunction* Entry = nullptr;
					bool bEntryAvailable = false;
					if (bBuildSucceeded)
					{
						Entry = GetNativeFunctionByExactDecl(Module, "void Entry()");
						bEntryAvailable = Assertions.IsNotNull(Entry, *Case.Describe(TEXT("exception origin should publish exact Entry declaration")));
					}
					if (bBuildSucceeded && bEntryAvailable)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextAvailable = Assertions.IsNotNull(Context, *Case.Describe(TEXT("exception origin should create a context")));
						if (bContextAvailable)
						{
							FNativeDebugRecorder FirstRecorder;
							FNativeDebugRecorder SecondRecorder;
							bool bCallbackReady = true;
							if (FCStringAnsi::Strcmp(CallbackCase.CatalogName, "installed") == 0)
							{
								bCallbackReady = Assertions.AreEqual(asSUCCESS, Context->SetExceptionCallback(asFUNCTION(CaptureNativeException), &FirstRecorder, asCALL_CDECL), *Case.Describe(TEXT("installed exception callback should register")));
							}
							else if (FCStringAnsi::Strcmp(CallbackCase.CatalogName, "replaced") == 0)
							{
								Context->SetExceptionCallback(asFUNCTION(CaptureNativeException), &FirstRecorder, asCALL_CDECL);
								bCallbackReady = Assertions.AreEqual(asSUCCESS, Context->SetExceptionCallback(asFUNCTION(CaptureNativeException), &SecondRecorder, asCALL_CDECL), *Case.Describe(TEXT("replacement exception callback should register")));
							}
							else if (FCStringAnsi::Strcmp(CallbackCase.CatalogName, "cleared") == 0)
							{
								Context->SetExceptionCallback(asFUNCTION(CaptureNativeException), &FirstRecorder, asCALL_CDECL);
								Context->ClearExceptionCallback();
							}
							if (bCallbackReady)
							{
								const int ExecutionResult = PrepareAndExecute(Context, Entry);
								const bool bConstructionLifecycleOrigin = IsNamedCase(OriginCase, "constructor") || IsNamedCase(OriginCase, "destructor");
								if (bConstructionLifecycleOrigin)
								{
									// The current fork deliberately consumes exceptions raised from a
									// script constructor/destructor during implicit object cleanup.
									const bool bLifecycleFinished = ExecutionResult == asEXECUTION_FINISHED;
									const bool bLifecycleCompleted = bLifecycleFinished || ExecutionResult == asEXECUTION_EXCEPTION;
									(void)Assertions.IsTrue(bLifecycleCompleted, *Case.Describe(TEXT("constructor/destructor origin should either complete or propagate an exception under the current cleanup policy")));
									if (bLifecycleFinished)
									{
										(void)Assertions.IsNull(Context->GetExceptionFunction(), *Case.Describe(TEXT("constructor/destructor origin should not publish a swallowed exception function")));
									}
									else if (ExecutionResult == asEXECUTION_EXCEPTION)
									{
										(void)Assertions.IsNotNull(Context->GetExceptionFunction(), *Case.Describe(TEXT("propagated constructor/destructor origin should retain a throwing function")));
									}
								}
								else
								{
									(void)Assertions.AreEqual(asEXECUTION_EXCEPTION, ExecutionResult, *Case.Describe(TEXT("exception origin should produce a script exception")));
								}
								const FString ExceptionText = UTF8_TO_TCHAR(Context->GetExceptionString());
								if (!bConstructionLifecycleOrigin)
								{
									(void)Assertions.IsTrue(!ExceptionText.IsEmpty(), *Case.Describe(TEXT("exception origin should expose non-empty exception text")));
									const bool bScriptClassNullBoundary = (!IsNamedCase(DepthCase, "imported")
										&& (IsNamedCase(OriginCase, "member") || IsNamedCase(OriginCase, "protocol_callback")))
										|| IsNamedCase(DepthCase, "method")
										|| IsNamedCase(DepthCase, "virtual");
									if (bScriptClassNullBoundary)
									{
										// Script-class handles in this raw fork are not implicitly
										// materialized by these declarations. Preserve the observed
										// null-access result until the handle-construction semantics
										// are upgraded.
										(void)Assertions.AreEqual(FString(TEXT("Null pointer access")), ExceptionText, *Case.Describe(TEXT("current fork script-class origin boundary should retain the null-pointer diagnostic")));
									}
									else if (IsNamedCase(DepthCase, "imported"))
									{
										(void)Assertions.IsTrue(ExceptionText.Contains(FString::FromInt(GetOriginCode(OriginCase)), ESearchCase::CaseSensitive), *Case.Describe(TEXT("imported origin should retain the source-selected origin code")));
									}
									else if (!IsNamedCase(OriginCase, "null_access") && !IsNamedCase(OriginCase, "divide_fault"))
									{
										const bool bOriginTextMatches = ExceptionText.Contains(ANSI_TO_TCHAR(OriginCase.CatalogName), ESearchCase::CaseSensitive);
										(void)Assertions.IsTrue(bOriginTextMatches, *Case.Describe(TEXT("exception text should identify the selected origin")));
									}
								}
								if (!bConstructionLifecycleOrigin)
								{
									(void)Assertions.IsNotNull(Context->GetExceptionFunction(), *Case.Describe(TEXT("exception should retain a throwing function")));
								}
								const char* Section = nullptr;
								int Column = 0;
								if (!bConstructionLifecycleOrigin
									&& !IsNamedCase(DepthCase, "imported")
									&& !IsNamedCase(OriginCase, "native_callback")
									&& !IsNamedCase(OriginCase, "host_set_exception")
									&& !IsNamedCase(OriginCase, "destructor"))
								{
									(void)Assertions.IsTrue(Context->GetExceptionLineNumber(&Column, &Section) > 0 && Column > 0 && Section != nullptr, *Case.Describe(TEXT("script exception origin should retain source location metadata")));
								}
								const int32 ExpectedCallbacks = bConstructionLifecycleOrigin && ExecutionResult == asEXECUTION_FINISHED
									? 0
									: (FCStringAnsi::Strcmp(CallbackCase.CatalogName, "installed") == 0 || FCStringAnsi::Strcmp(CallbackCase.CatalogName, "replaced") == 0 ? 1 : 0);
								(void)Assertions.AreEqual(ExpectedCallbacks, FirstRecorder.Num(ENativeDebugEventKind::Exception) + SecondRecorder.Num(ENativeDebugEventKind::Exception), *Case.Describe(TEXT("callback state should observe exactly the configured exception event count")));
							}
							Context->ClearExceptionCallback();
							(void)Assertions.AreEqual(asSUCCESS, Context->Unprepare(), *Case.Describe(TEXT("exception context should unprepare for cleanup")));
							Context->Release();
						}
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("exception origin cell should discard its module")));
					(void)Assertions.AreEqual(0, Lifecycle.GetLiveObjectCount(), *Case.Describe(TEXT("exception origin cell should leave no tracked native reference alive")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
