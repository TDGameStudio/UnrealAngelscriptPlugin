#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeStackPopCallbackDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.StackPopDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FDepthCase
	{
		const ANSICHAR* Name;
		int32 MinimumStackPopEvents;
	};

	struct FExitCase
	{
		const ANSICHAR* Name;
		bool bException;
		bool bEarlyReturn;
		bool bRecovery;
	};

	struct FCallbackStateCase
	{
		const ANSICHAR* Name;
		bool bInstall;
	};


	inline static constexpr FDepthCase DepthCases[] =
	{
		{ "one", 1 },
		{ "nested", 2 },
		{ "recursive", 2 },
	};

	inline static constexpr FExitCase ExitCases[] =
	{
		{ "normal", false, false, false },
		{ "early_return", false, true, false },
		{ "exception", true, false, false },
		{ "recovery", true, false, true },
	};

	inline static constexpr FCallbackStateCase CallbackStateCases[] =
	{
		{ "installed", true },
		{ "cleared", false },
	};

	static FString MakeCaseId(
		const FDepthCase& DepthCase,
		const FExitCase& ExitCase,
		const FCallbackStateCase& StateCase)
	{
		return FString::Printf(
			TEXT("DBG-STACK-POP-EXIT-%hs-%hs-%hs"),
			DepthCase.Name,
			ExitCase.Name,
			StateCase.Name);
	}

	static FString MakeModuleName(
		const FDepthCase& DepthCase,
		const FExitCase& ExitCase,
		const FCallbackStateCase& StateCase)
	{
		return FString::Printf(
			TEXT("NativeDebugStackPop_%hs_%hs_%hs"),
			DepthCase.Name,
			ExitCase.Name,
			StateCase.Name);
	}

	static FString BuildSource(const FDepthCase& DepthCase, const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// depth=%hs exit=%hs"),
			DepthCase.Name,
			ExitCase.Name));

		if (FCStringAnsi::Strcmp(DepthCase.Name, "nested") == 0)
		{
			if (ExitCase.bException)
			{
				AppendGeneratedAsLine(Source, TEXT("int PopFault()"));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
				AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
				AppendGeneratedAsLine(Source, TEXT("}"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("int PopLeaf()"));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source, TEXT("\treturn 42;"));
				AppendGeneratedAsLine(Source, TEXT("}"));
			}
			AppendGeneratedAsLine(Source);
		}
		else if (FCStringAnsi::Strcmp(DepthCase.Name, "recursive") == 0)
		{
			if (ExitCase.bException)
			{
				AppendGeneratedAsLine(Source, TEXT("int PopRecursiveFault(int Remaining)"));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source, TEXT("\tif (Remaining == 0)"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendGeneratedAsLine(Source, TEXT("\t\tint Zero = 0;"));
				AppendGeneratedAsLine(Source, TEXT("\t\treturn 1 / Zero;"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
				AppendGeneratedAsLine(Source);
				AppendGeneratedAsLine(Source, TEXT("\treturn PopRecursiveFault(Remaining - 1);"));
				AppendGeneratedAsLine(Source, TEXT("}"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("int PopRecursive(int Remaining)"));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source, TEXT("\tif (Remaining == 0)"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendGeneratedAsLine(Source, TEXT("\t\treturn 42;"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
				AppendGeneratedAsLine(Source);
				AppendGeneratedAsLine(Source, TEXT("\treturn PopRecursive(Remaining - 1);"));
				AppendGeneratedAsLine(Source, TEXT("}"));
			}
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (ExitCase.bException && FCStringAnsi::Strcmp(DepthCase.Name, "one") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		}
		else
		{
			FString CallExpression;
			if (FCStringAnsi::Strcmp(DepthCase.Name, "one") == 0)
			{
				CallExpression = ExitCase.bException ? TEXT("1 / 0") : TEXT("42");
			}
			else if (FCStringAnsi::Strcmp(DepthCase.Name, "nested") == 0)
			{
				CallExpression = ExitCase.bException ? TEXT("PopFault()") : TEXT("PopLeaf()");
			}
			else
			{
				CallExpression = ExitCase.bException
					? TEXT("PopRecursiveFault(3)")
					: TEXT("PopRecursive(3)");
			}

			if (ExitCase.bEarlyReturn)
			{
				AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *CallExpression));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
				AppendGeneratedAsLine(Source);
				AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Result = %s;"), *CallExpression));
				AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Recovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int32 CountStackPopEvents(const AngelscriptNativeTestSupport::FNativeDebugRecorder& Recorder)
	{
		return Recorder.Num(AngelscriptNativeTestSupport::ENativeDebugEventKind::StackPop);
	}

	static int32 GetMinimumStackPopEvents(const FDepthCase& DepthCase, const FExitCase& ExitCase)
	{
		// Recovery is intentionally executed after the exception recorder reset;
		// it has one top-level context-frame pop regardless of the original depth.
		if (ExitCase.bRecovery)
		{
			return 1;
		}

		// An exception stops the nested callee before the caller's normal return
		// cleanup is reached in this fork, so that combination has one observable
		// pop instead of the two pops seen on normal/early-return paths.
		if (ExitCase.bException && FCStringAnsi::Strcmp(DepthCase.Name, "nested") == 0)
		{
			return 1;
		}

		return DepthCase.MinimumStackPopEvents;
	}

public:
	TEST_METHOD(ExitsByDepthAndCallbackState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-STACK-POP-EXIT",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Stack-pop product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FDepthCase& DepthCase : DepthCases)
		{
			for (const FExitCase& ExitCase : ExitCases)
			{
				for (const FCallbackStateCase& StateCase : CallbackStateCases)
				{
					const FString CaseId = MakeCaseId(DepthCase, ExitCase, StateCase);
					const FString ModuleName = MakeModuleName(DepthCase, ExitCase, StateCase);
					const FString Source = BuildSource(DepthCase, ExitCase);
					PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);

					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*FString::Printf(TEXT("%s should compile. Build=%d Messages={%s}"),
							*CaseId,
							BuildResult,
							*Engine.GetMessagesText())));
					ASSERT_THAT(IsNotNull(Module, *FString::Printf(TEXT("%s should publish a module"), *CaseId)));
					if (BuildResult < 0 || Module == nullptr)
					{
						continue;
					}
					ON_SCOPE_EXIT
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					};

					asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
					asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(Module, "int Recovery()");
					ASSERT_THAT(IsNotNull(Entry, *FString::Printf(TEXT("%s should resolve Entry exactly"), *CaseId)));
					ASSERT_THAT(IsNotNull(Recovery, *FString::Printf(TEXT("%s should resolve Recovery exactly"), *CaseId)));
					if (Entry == nullptr || Recovery == nullptr)
					{
						continue;
					}

					asIScriptContext* const Context = ScriptEngine->CreateContext();
					ASSERT_THAT(IsNotNull(Context, *FString::Printf(TEXT("%s should create a context"), *CaseId)));
					if (Context == nullptr)
					{
						continue;
					}
					ON_SCOPE_EXIT
					{
						Context->Release();
					};

					asCContext* const RawContext = static_cast<asCContext*>(Context);
					FNativeDebugRecorder Recorder;
					Context->SetUserData(&Recorder, NativeDebugRecorderUserDataSlot);
					if (StateCase.bInstall)
					{
						ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetStackPopCallback(CaptureNativeStackPop),
							*FString::Printf(TEXT("%s should install its stack-pop callback"), *CaseId)));
					}
					else
					{
						ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetStackPopCallback(CaptureNativeStackPop),
							*FString::Printf(TEXT("%s should install before exercising clear"), *CaseId)));
						RawContext->ClearStackPopCallback();
					}

					if (ExitCase.bRecovery)
					{
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry),
							*FString::Printf(TEXT("%s exception phase should prepare"), *CaseId)));
						ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(),
							*FString::Printf(TEXT("%s exception phase should stop with an exception"), *CaseId)));
						ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
							*FString::Printf(TEXT("%s exception phase should retain its fault function"), *CaseId)));
						ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0',
							*FString::Printf(TEXT("%s exception phase should retain diagnostic text"), *CaseId)));
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
							*FString::Printf(TEXT("%s exception phase should unprepare before recovery"), *CaseId)));
						Recorder.Reset();
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recovery),
							*FString::Printf(TEXT("%s recovery phase should prepare on the same context"), *CaseId)));
						ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
							*FString::Printf(TEXT("%s recovery phase should finish"), *CaseId)));
						ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord()),
							*FString::Printf(TEXT("%s recovery phase should return its independent value"), *CaseId)));
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
							*FString::Printf(TEXT("%s recovery phase should unprepare cleanly"), *CaseId)));
					}
					else
					{
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry),
							*FString::Printf(TEXT("%s should prepare its entry"), *CaseId)));
						const int ExecuteResult = Context->Execute();
						if (ExitCase.bException)
						{
							ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
								*FString::Printf(TEXT("%s should stop with an exception"), *CaseId)));
							ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
								*FString::Printf(TEXT("%s should retain its fault function"), *CaseId)));
						}
						else
						{
							ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
								*FString::Printf(TEXT("%s should finish"), *CaseId)));
							ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
								*FString::Printf(TEXT("%s should retain the selected return value"), *CaseId)));
						}
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
							*FString::Printf(TEXT("%s should unprepare after its selected exit"), *CaseId)));
					}

					const int32 StackPopCount = CountStackPopEvents(Recorder);
					if (StateCase.bInstall)
					{
						const int32 MinimumStackPopEvents = GetMinimumStackPopEvents(DepthCase, ExitCase);
						ASSERT_THAT(IsTrue(StackPopCount >= MinimumStackPopEvents,
							*FString::Printf(TEXT("%s should expose at least %d stack-pop events, actual=%d"),
								*CaseId,
								MinimumStackPopEvents,
								StackPopCount)));
						for (const FNativeDebugEvent& Event : Recorder.GetEvents())
						{
							if (Event.Kind != ENativeDebugEventKind::StackPop)
							{
								continue;
							}
							ASSERT_THAT(IsTrue(Event.PointerBegin != 0
								&& Event.PointerEnd != 0
								&& Event.PointerBegin != Event.PointerEnd,
								*FString::Printf(TEXT("%s should expose a non-empty old-frame pointer range"), *CaseId)));
						}
					}
					else
					{
						ASSERT_THAT(AreEqual(0, StackPopCount,
							*FString::Printf(TEXT("%s should emit no stack-pop events after clear"), *CaseId)));
					}

					RawContext->ClearStackPopCallback();
					Context->SetUserData(nullptr, NativeDebugRecorderUserDataSlot);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
