#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeNestedContextTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.NestedContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	inline static constexpr asPWORD NestedRecorderUserDataSlot = static_cast<asPWORD>(0x4E4154444E455354ull);

	struct FNestedTransition
	{
		FString CallerDeclarationBefore;
		FString CallerDeclarationAfter;
		FString SelectedInnerDeclaration;
		asEContextState StateBefore = asEXECUTION_UNINITIALIZED;
		asEContextState StateAfter = asEXECUTION_UNINITIALIZED;
		asUINT CallstackDepthBefore = 0;
		asUINT CallstackDepthAfter = 0;
		asUINT NestedCountAfterPush = 0;
		int32 PushResult = asERROR;
		int32 InnerPrepareResult = asERROR;
		int32 InnerExecuteResult = asERROR;
		int32 PopResult = asERROR;
		int32 AbortResult = asERROR;
		bool bNestedAfterPush = false;
		void* DebugFrameBefore = nullptr;
		void* DebugFrameAfter = nullptr;
	};

	struct FNestedObservation
	{
		asIScriptFunction* InnerFunction = nullptr;
		asIScriptFunction* ReenterFunction = nullptr;
		FString OuterFunctionBefore;
		FString OuterFunctionAfter;
		asEContextState OuterStateBefore = asEXECUTION_UNINITIALIZED;
		asEContextState OuterStateAfter = asEXECUTION_UNINITIALIZED;
		asUINT OuterDepthBefore = 0;
		asUINT OuterDepthAfter = 0;
		asUINT NestedCount = 0;
		asUINT EnteredCount = 0;
		bool bUseDoubleNest = false;
		TArray<asUINT> NestedCounts;
		TArray<FString> NestedCallerDeclarations;
		TArray<FNestedTransition> Transitions;
		int32 PushResult = asERROR;
		int32 InnerPrepareResult = asERROR;
		int32 InnerExecuteResult = asERROR;
		int32 PopResult = asERROR;
		int32 AbortResult = asERROR;
		int32 SuspendResult = asERROR;
		bool bNested = false;
		void* OuterDebugFrameBefore = nullptr;
		void* OuterDebugFrameAfter = nullptr;

		void Reset(asIScriptFunction* InInnerFunction)
		{
			InnerFunction = InInnerFunction;
			ReenterFunction = nullptr;
			OuterFunctionBefore.Reset();
			OuterFunctionAfter.Reset();
			OuterStateBefore = asEXECUTION_UNINITIALIZED;
			OuterStateAfter = asEXECUTION_UNINITIALIZED;
			OuterDepthBefore = 0;
			OuterDepthAfter = 0;
			NestedCount = 0;
			EnteredCount = 0;
			bUseDoubleNest = false;
			NestedCounts.Reset();
			NestedCallerDeclarations.Reset();
			Transitions.Reset();
			PushResult = asERROR;
			InnerPrepareResult = asERROR;
			InnerExecuteResult = asERROR;
			PopResult = asERROR;
			AbortResult = asERROR;
			SuspendResult = asERROR;
			bNested = false;
			OuterDebugFrameBefore = nullptr;
			OuterDebugFrameAfter = nullptr;
		}
	};

	static void EnterNestedContext()
	{
		asIScriptContext* const Context = asGetActiveContext();
		FNestedObservation* const Observation = Context != nullptr
			? static_cast<FNestedObservation*>(Context->GetUserData(NestedRecorderUserDataSlot))
			: nullptr;
		if (Context == nullptr || Observation == nullptr || Observation->InnerFunction == nullptr)
		{
			return;
		}

		asCContext* const RawContext = static_cast<asCContext*>(Context);
		const int32 TransitionIndex = Observation->Transitions.AddDefaulted();
		FNestedTransition& TransitionBeforePush = Observation->Transitions[TransitionIndex];
		TransitionBeforePush.StateBefore = Context->GetState();
		TransitionBeforePush.CallstackDepthBefore = Context->GetCallstackSize();
		TransitionBeforePush.DebugFrameBefore = RawContext->DebugFramePtr;
		Observation->OuterStateBefore = TransitionBeforePush.StateBefore;
		Observation->OuterDepthBefore = TransitionBeforePush.CallstackDepthBefore;
		Observation->OuterDebugFrameBefore = TransitionBeforePush.DebugFrameBefore;
		if (asIScriptFunction* const OuterFunction = Context->GetFunction(0))
		{
			TransitionBeforePush.CallerDeclarationBefore = UTF8_TO_TCHAR(OuterFunction->GetDeclaration());
			Observation->OuterFunctionBefore = TransitionBeforePush.CallerDeclarationBefore;
		}

		TransitionBeforePush.PushResult = Context->PushState();
		Observation->PushResult = TransitionBeforePush.PushResult;
		if (TransitionBeforePush.PushResult != asSUCCESS)
		{
			return;
		}
		TransitionBeforePush.bNestedAfterPush = Context->IsNested(&TransitionBeforePush.NestedCountAfterPush);
		Observation->bNested = TransitionBeforePush.bNestedAfterPush;
		Observation->NestedCount = TransitionBeforePush.NestedCountAfterPush;
		const asIScriptFunction* const OuterFunctionForStage = Context->GetFunction(0);
		if (OuterFunctionForStage != nullptr)
		{
			Observation->NestedCallerDeclarations.Add(UTF8_TO_TCHAR(OuterFunctionForStage->GetDeclaration()));
		}
		Observation->NestedCounts.Add(TransitionBeforePush.NestedCountAfterPush);
		asIScriptFunction* const SelectedInner = Observation->bUseDoubleNest
			&& Observation->EnteredCount == 0
			&& Observation->ReenterFunction != nullptr
			? Observation->ReenterFunction
			: Observation->InnerFunction;
		++Observation->EnteredCount;
		TransitionBeforePush.SelectedInnerDeclaration = SelectedInner != nullptr
			? UTF8_TO_TCHAR(SelectedInner->GetDeclaration())
			: FString();
		TransitionBeforePush.InnerPrepareResult = Context->Prepare(SelectedInner);
		Observation->InnerPrepareResult = TransitionBeforePush.InnerPrepareResult;
		int32 InnerExecuteResult = asERROR;
		if (TransitionBeforePush.InnerPrepareResult == asSUCCESS)
		{
			InnerExecuteResult = Context->Execute();
		}

		// The inner execution can re-enter this function and grow Transitions, so reacquire by index.
		FNestedTransition& TransitionAfterExecute = Observation->Transitions[TransitionIndex];
		TransitionAfterExecute.InnerExecuteResult = InnerExecuteResult;
		TransitionAfterExecute.PopResult = Context->PopState();
		TransitionAfterExecute.StateAfter = Context->GetState();
		TransitionAfterExecute.CallstackDepthAfter = Context->GetCallstackSize();
		TransitionAfterExecute.DebugFrameAfter = RawContext->DebugFramePtr;
		Observation->InnerExecuteResult = TransitionAfterExecute.InnerExecuteResult;
		Observation->PopResult = TransitionAfterExecute.PopResult;
		Observation->OuterStateAfter = TransitionAfterExecute.StateAfter;
		Observation->OuterDepthAfter = TransitionAfterExecute.CallstackDepthAfter;
		Observation->OuterDebugFrameAfter = TransitionAfterExecute.DebugFrameAfter;
		if (asIScriptFunction* const OuterFunction = Context->GetFunction(0))
		{
			TransitionAfterExecute.CallerDeclarationAfter = UTF8_TO_TCHAR(OuterFunction->GetDeclaration());
			Observation->OuterFunctionAfter = TransitionAfterExecute.CallerDeclarationAfter;
		}
	}

	static void AbortNestedContext()
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			FNestedObservation* const Observation = static_cast<FNestedObservation*>(Context->GetUserData(NestedRecorderUserDataSlot));
			const int32 AbortResult = Context->Abort();
			if (Observation != nullptr)
			{
				Observation->AbortResult = AbortResult;
			}
		}
	}

	static void SuspendNestedContext()
	{
		asIScriptContext* const Context = asGetActiveContext();
		FNestedObservation* const Observation = Context != nullptr
			? static_cast<FNestedObservation*>(Context->GetUserData(NestedRecorderUserDataSlot))
			: nullptr;
		if (Context != nullptr && Observation != nullptr)
		{
			Observation->SuspendResult = Context->Suspend();
		}
	}

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int NestedInnerSuccess()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedInnerFault()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedInnerAbort()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugAbortInner();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 99;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedInnerReenter()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugEnterInner();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 17;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedInnerSuspend()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugSuspendInner();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 31;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedOuter()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugEnterInner();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 42;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

public:
	TEST_METHOD(OuterStatesByNestOutcomeAndObservation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-NEST-STATE",
			ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Nested context product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller EnterCaller = ASAutoCaller::MakeFunctionCaller(EnterNestedContext);
		const ASAutoCaller::FunctionCaller AbortCaller = ASAutoCaller::MakeFunctionCaller(AbortNestedContext);
		const ASAutoCaller::FunctionCaller SuspendCaller = ASAutoCaller::MakeFunctionCaller(SuspendNestedContext);
		const int EnterResult = ScriptEngine->RegisterGlobalFunction("void NativeDebugEnterInner()", asFUNCTION(EnterNestedContext), asCALL_CDECL, *(asFunctionCaller*)&EnterCaller);
		ASSERT_THAT(IsTrue(EnterResult >= 0,
			*FString::Printf(TEXT("Nested context product should register its raw active-context bridge. Result=%d Messages={%s}"),
				EnterResult, *Engine.GetMessagesText())));
		const int AbortResult = ScriptEngine->RegisterGlobalFunction("void NativeDebugAbortInner()", asFUNCTION(AbortNestedContext), asCALL_CDECL, *(asFunctionCaller*)&AbortCaller);
		ASSERT_THAT(IsTrue(AbortResult >= 0,
			*FString::Printf(TEXT("Nested context product should register its raw nested-abort bridge. Result=%d Messages={%s}"),
				AbortResult, *Engine.GetMessagesText())));
		const int SuspendResult = ScriptEngine->RegisterGlobalFunction("void NativeDebugSuspendInner()", asFUNCTION(SuspendNestedContext), asCALL_CDECL, *(asFunctionCaller*)&SuspendCaller);
		ASSERT_THAT(IsTrue(SuspendResult >= 0,
			*FString::Printf(TEXT("Nested context product should register its raw nested-suspend bridge. Result=%d Messages={%s}"),
				SuspendResult, *Engine.GetMessagesText())));

		const FString ModuleName = TEXT("NativeDebugNestedContext");
		const FString Source = BuildSource();
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		PrintGeneratedAsSource(*TestRunner, TEXT("DBG-NEST-STATE"), ModuleName, Source);
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0, TEXT("Nested context source should compile")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Nested context source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		};

		asIScriptFunction* const Outer = GetNativeFunctionByExactDecl(Module, "int NestedOuter()");
		asIScriptFunction* const Success = GetNativeFunctionByExactDecl(Module, "int NestedInnerSuccess()");
		asIScriptFunction* const Fault = GetNativeFunctionByExactDecl(Module, "int NestedInnerFault()");
		asIScriptFunction* const Abort = GetNativeFunctionByExactDecl(Module, "int NestedInnerAbort()");
		asIScriptFunction* const Reenter = GetNativeFunctionByExactDecl(Module, "int NestedInnerReenter()");
		asIScriptFunction* const Suspend = GetNativeFunctionByExactDecl(Module, "int NestedInnerSuspend()");
		ASSERT_THAT(IsNotNull(Outer, TEXT("Nested context product should resolve the outer function exactly")));
		ASSERT_THAT(IsNotNull(Success, TEXT("Nested context product should resolve the successful inner function exactly")));
		ASSERT_THAT(IsNotNull(Fault, TEXT("Nested context product should resolve the faulting inner function exactly")));
		ASSERT_THAT(IsNotNull(Abort, TEXT("Nested context product should resolve the aborting inner function exactly")));
		ASSERT_THAT(IsNotNull(Reenter, TEXT("Nested context product should resolve the re-entering inner function exactly")));
		ASSERT_THAT(IsNotNull(Suspend, TEXT("Nested context product should resolve the suspend-requesting inner function exactly")));
		if (Outer == nullptr || Success == nullptr || Fault == nullptr || Abort == nullptr || Reenter == nullptr || Suspend == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Nested context product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Outer), TEXT("Nested context invalid-state probe should prepare the outer function")));
		ASSERT_THAT(AreEqual(asERROR, Context->PushState(), TEXT("Nested context push should reject a merely prepared context")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Nested context invalid-state probe should unprepare cleanly")));

		FNestedObservation Observation;
		Context->SetUserData(&Observation, NestedRecorderUserDataSlot);
		Observation.Reset(Success);
		const int SuccessfulOuterResult = PrepareAndExecute(Context, Outer);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), SuccessfulOuterResult,
			*FString::Printf(TEXT("Nested context successful inner execution should preserve the outer return path. Result=%d InnerPrepare=%d InnerExecute=%d Pop=%d StateAfter=%d Exception={%s}"),
				SuccessfulOuterResult, Observation.InnerPrepareResult, Observation.InnerExecuteResult,
				Observation.PopResult, static_cast<int32>(Observation.OuterStateAfter),
				UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : ""))));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Nested context successful outer execution should return after restoration")));
		ASSERT_THAT(AreEqual(asSUCCESS, Observation.PushResult, TEXT("Nested context active bridge should push the outer state")));
		ASSERT_THAT(IsTrue(Observation.bNested, TEXT("Nested context active bridge should report nesting")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Observation.NestedCount), TEXT("Nested context active bridge should expose one nesting marker")));
		ASSERT_THAT(AreEqual(asSUCCESS, Observation.InnerPrepareResult, TEXT("Nested context successful inner execution should prepare")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Observation.InnerExecuteResult, TEXT("Nested context successful inner execution should finish")));
		ASSERT_THAT(AreEqual(asSUCCESS, Observation.PopResult, TEXT("Nested context successful inner execution should pop and restore")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(Observation.OuterStateBefore), TEXT("Nested context outer state should be active before push")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(Observation.OuterStateAfter), TEXT("Nested context outer state should be active after pop")));
		ASSERT_THAT(AreEqual(Observation.OuterDepthBefore, Observation.OuterDepthAfter, TEXT("Nested context pop should restore the outer callstack depth")));
		ASSERT_THAT(AreEqual(Observation.OuterFunctionBefore, Observation.OuterFunctionAfter, TEXT("Nested context pop should restore the outer function identity")));
		ASSERT_THAT(AreEqual(Observation.OuterDebugFrameBefore, Observation.OuterDebugFrameAfter, TEXT("Nested context pop should restore the exported debug-frame pointer value")));
		ASSERT_THAT(AreEqual(asERROR, Context->PushState(), TEXT("Nested context push should reject the finished outer state")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Nested context successful path should unprepare for reuse")));

		Observation.Reset(Fault);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Outer), TEXT("Nested context faulting inner execution should restore and continue the outer path")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Nested context faulting inner execution should retain the outer runtime result")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Observation.InnerExecuteResult, TEXT("Nested context faulting inner execution should expose its exception result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Observation.PopResult, TEXT("Nested context faulting inner execution should pop and restore")));
		ASSERT_THAT(AreEqual(Observation.OuterFunctionBefore, Observation.OuterFunctionAfter, TEXT("Nested context faulting inner execution should restore the outer function")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Nested context faulting path should unprepare for cleanup")));

		Observation.Reset(Success);
		Observation.bUseDoubleNest = true;
		Observation.ReenterFunction = Reenter;
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Outer), TEXT("Nested context two-level inner execution should restore and continue the outer path")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Nested context two-level inner execution should retain the outer runtime result")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Observation.NestedCounts.Num()), TEXT("Nested context two-level inner execution should record both nested transitions")));
		ASSERT_THAT(AreEqual(2, Observation.Transitions.Num(), TEXT("Nested context two-level inner execution should record an independently restored transition at each level")));
		if (Observation.NestedCounts.Num() == 2 && Observation.NestedCallerDeclarations.Num() == 2)
		{
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Observation.NestedCounts[0]), TEXT("Nested context first transition should report one nested state")));
			ASSERT_THAT(AreEqual(2, static_cast<int32>(Observation.NestedCounts[1]), TEXT("Nested context second transition should report two nested states")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NestedOuter()")), Observation.NestedCallerDeclarations[0], TEXT("Nested context first transition should originate in the outer function")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NestedInnerReenter()")), Observation.NestedCallerDeclarations[1], TEXT("Nested context second transition should originate in the re-entering inner function")));
		}
		if (Observation.Transitions.Num() == 2)
		{
			const FNestedTransition& FirstTransition = Observation.Transitions[0];
			const FNestedTransition& SecondTransition = Observation.Transitions[1];
			ASSERT_THAT(AreEqual(asSUCCESS, FirstTransition.PushResult, TEXT("Nested context first transition should push the active outer state")));
			ASSERT_THAT(AreEqual(asSUCCESS, FirstTransition.PopResult, TEXT("Nested context first transition should pop back to the outer state")));
			ASSERT_THAT(IsTrue(FirstTransition.bNestedAfterPush, TEXT("Nested context first transition should report a nested state after push")));
			ASSERT_THAT(AreEqual(1, static_cast<int32>(FirstTransition.NestedCountAfterPush), TEXT("Nested context first transition should expose exactly one saved state")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(FirstTransition.StateBefore), TEXT("Nested context first transition should begin while the outer frame is active")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(FirstTransition.StateAfter), TEXT("Nested context first transition should restore the active outer frame")));
			ASSERT_THAT(AreEqual(FirstTransition.CallstackDepthBefore, FirstTransition.CallstackDepthAfter, TEXT("Nested context first transition should restore the outer callstack depth")));
			ASSERT_THAT(AreEqual(FirstTransition.CallerDeclarationBefore, FirstTransition.CallerDeclarationAfter, TEXT("Nested context first transition should restore the outer declaration")));
			ASSERT_THAT(AreEqual(FirstTransition.DebugFrameBefore, FirstTransition.DebugFrameAfter, TEXT("Nested context first transition should restore the outer debug-frame pointer")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NestedOuter()")), FirstTransition.CallerDeclarationBefore, TEXT("Nested context first transition should capture the outer caller declaration")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NestedInnerReenter()")), FirstTransition.SelectedInnerDeclaration, TEXT("Nested context first transition should select the re-entering inner declaration")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), FirstTransition.InnerExecuteResult, TEXT("Nested context first transition should finish after the second-level restoration")));

			ASSERT_THAT(AreEqual(asSUCCESS, SecondTransition.PushResult, TEXT("Nested context second transition should push the active re-entering state")));
			ASSERT_THAT(AreEqual(asSUCCESS, SecondTransition.PopResult, TEXT("Nested context second transition should pop back to the re-entering state")));
			ASSERT_THAT(IsTrue(SecondTransition.bNestedAfterPush, TEXT("Nested context second transition should report a nested state after push")));
			ASSERT_THAT(AreEqual(2, static_cast<int32>(SecondTransition.NestedCountAfterPush), TEXT("Nested context second transition should expose two saved states")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(SecondTransition.StateBefore), TEXT("Nested context second transition should begin while the re-entering frame is active")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(SecondTransition.StateAfter), TEXT("Nested context second transition should restore the re-entering frame")));
			ASSERT_THAT(AreEqual(SecondTransition.CallstackDepthBefore, SecondTransition.CallstackDepthAfter, TEXT("Nested context second transition should restore the re-entering callstack depth")));
			ASSERT_THAT(AreEqual(SecondTransition.CallerDeclarationBefore, SecondTransition.CallerDeclarationAfter, TEXT("Nested context second transition should restore the re-entering declaration")));
			ASSERT_THAT(AreEqual(SecondTransition.DebugFrameBefore, SecondTransition.DebugFrameAfter, TEXT("Nested context second transition should restore the re-entering debug-frame pointer")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NestedInnerReenter()")), SecondTransition.CallerDeclarationBefore, TEXT("Nested context second transition should capture the re-entering caller declaration")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NestedInnerSuccess()")), SecondTransition.SelectedInnerDeclaration, TEXT("Nested context second transition should select the successful inner declaration")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), SecondTransition.InnerExecuteResult, TEXT("Nested context second transition should finish the selected inner declaration")));
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Nested context two-level path should unprepare for cleanup")));

		Observation.Reset(Suspend);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Outer), TEXT("Nested context rejected-suspend inner execution should restore and continue the outer path")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Nested context rejected-suspend inner execution should retain the outer runtime result")));
		ASSERT_THAT(AreEqual(asERROR, Observation.SuspendResult, TEXT("Nested context should preserve the current fork's explicit unsupported Suspend result")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Observation.InnerExecuteResult, TEXT("Nested context rejected-suspend inner execution should remain executable")));
		ASSERT_THAT(AreEqual(asSUCCESS, Observation.PopResult, TEXT("Nested context rejected-suspend inner execution should pop and restore")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Nested context rejected-suspend path should unprepare for cleanup")));

		Observation.Reset(Abort);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Outer), TEXT("Nested context aborting inner execution should restore and continue the outer path")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Nested context aborting inner execution should retain the outer runtime result")));
		// Abort is intentionally an unsupported operation in this fork's raw context
		// implementation (asCContext::Abort returns asERROR and leaves execution active).
		ASSERT_THAT(AreEqual(asERROR, Observation.AbortResult, TEXT("Nested context should expose the fork's unsupported Abort result")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Observation.InnerExecuteResult, TEXT("Nested context unsupported Abort should leave inner execution executable")));
		ASSERT_THAT(AreEqual(asSUCCESS, Observation.PopResult, TEXT("Nested context aborting inner execution should pop and restore")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(Observation.OuterStateAfter), TEXT("Nested context aborting inner execution should restore the active outer state")));
		ASSERT_THAT(AreEqual(Observation.OuterFunctionBefore, Observation.OuterFunctionAfter, TEXT("Nested context aborting inner execution should restore the outer function")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Nested context aborting path should unprepare for cleanup")));
		ASSERT_THAT(AreEqual(asERROR, Context->PopState(), TEXT("Nested context pop at nesting zero should return the documented error")));
		ASSERT_THAT(AreEqual(asERROR, Context->PushState(), TEXT("Nested context push should reject the unprepared outer state")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
