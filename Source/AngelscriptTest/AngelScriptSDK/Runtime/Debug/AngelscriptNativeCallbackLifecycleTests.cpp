#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeCallbackLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.NativeDebug.CallbackLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int CallbackStraight()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallbackBranch(bool TakeBranch)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (TakeBranch)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 11;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn 2;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallbackLoop(int Iterations)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Total = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < Iterations; ++Index)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTotal += Index;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn Total;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallbackNested(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value + 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallbackRecursion(int Remaining)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Remaining == 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 + CallbackRecursion(Remaining - 1);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class CallbackMethodOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Run(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn CallbackNested(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallbackMethod()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tCallbackMethodOwner Owner = CallbackMethodOwner();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Owner.Run(30);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallbackFault()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 10 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasValidStackPopRange(const AngelscriptNativeTestSupport::FNativeDebugRecorder& Recorder)
	{
		for (const AngelscriptNativeTestSupport::FNativeDebugEvent& Event : Recorder.GetEvents())
		{
			if (Event.Kind == AngelscriptNativeTestSupport::ENativeDebugEventKind::StackPop
				&& Event.PointerBegin != 0
				&& Event.PointerEnd != 0
				&& Event.PointerBegin != Event.PointerEnd)
			{
				return true;
			}
		}
		return false;
	}

public:
	TEST_METHOD(FamiliesByStatePathAndUserData)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-CALLBACK-STATE-PATH",
			ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Callback lifecycle product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeDebugCallbackLifecycle");
		const FString Source = BuildSource();
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0),
			TEXT("Callback lifecycle should disable bytecode optimization so line callbacks remain observable")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, 0),
			TEXT("Callback lifecycle should retain line cues for raw line callbacks")));
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		PrintGeneratedAsSource(*TestRunner, TEXT("DBG-CALLBACK-STATE-PATH"), ModuleName, Source);
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("Callback lifecycle source should compile. Build=%d Messages={%s}"),
				BuildResult, *Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(Module, TEXT("Callback lifecycle source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		};

		asIScriptFunction* const Straight = GetNativeFunctionByExactDecl(Module, "int CallbackStraight()");
		asIScriptFunction* const Branch = GetNativeFunctionByExactDecl(Module, "int CallbackBranch(const bool)");
		asIScriptFunction* const Loop = GetNativeFunctionByExactDecl(Module, "int CallbackLoop(const int)");
		asIScriptFunction* const Nested = GetNativeFunctionByExactDecl(Module, "int CallbackNested(const int)");
		asIScriptFunction* const Recursion = GetNativeFunctionByExactDecl(Module, "int CallbackRecursion(const int)");
		asIScriptFunction* const Method = GetNativeFunctionByExactDecl(Module, "int CallbackMethod()");
		asIScriptFunction* const Fault = GetNativeFunctionByExactDecl(Module, "int CallbackFault()");
		ASSERT_THAT(IsNotNull(Straight, TEXT("Callback lifecycle straight path should resolve exactly")));
		ASSERT_THAT(IsNotNull(Branch, TEXT("Callback lifecycle branch path should resolve exactly")));
		ASSERT_THAT(IsNotNull(Loop, TEXT("Callback lifecycle loop path should resolve exactly")));
		ASSERT_THAT(IsNotNull(Nested, TEXT("Callback lifecycle nested path should resolve exactly")));
		ASSERT_THAT(IsNotNull(Recursion, TEXT("Callback lifecycle recursion path should resolve exactly")));
		ASSERT_THAT(IsNotNull(Method, TEXT("Callback lifecycle method path should resolve exactly")));
		ASSERT_THAT(IsNotNull(Fault, TEXT("Callback lifecycle fault function should resolve exactly")));
		if (Straight == nullptr || Branch == nullptr || Loop == nullptr || Nested == nullptr || Recursion == nullptr || Method == nullptr || Fault == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Callback lifecycle product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		asCContext* const RawContext = static_cast<asCContext*>(Context);

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Straight), TEXT("Callback lifecycle straight path should prepare without callbacks")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Callback lifecycle straight path should finish without callbacks")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord()), TEXT("Callback lifecycle straight path should retain its independent result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callback lifecycle absent state should cleanly unprepare")));

		Context->SetUserData(nullptr, NativeDebugRecorderUserDataSlot);
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureNativeLine), TEXT("Callback lifecycle null-recorder state should install a line callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLoopDetectionCallback(CaptureNativeLoop), TEXT("Callback lifecycle null-recorder state should install a loop callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetStackPopCallback(CaptureNativeStackPop), TEXT("Callback lifecycle null-recorder state should install a stack-pop callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Straight), TEXT("Callback lifecycle null-recorder state should prepare the straight path")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Callback lifecycle null-recorder state should execute safely without a recorder")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callback lifecycle null-recorder state should unprepare safely")));
		RawContext->ClearLineCallback();
		RawContext->SetLoopDetectionCallback(nullptr);
		RawContext->ClearStackPopCallback();

		FNativeDebugRecorder PrimaryRecorder;
		FNativeDebugRecorder ReplacementRecorder;
		Context->SetUserData(&PrimaryRecorder, NativeDebugRecorderUserDataSlot);
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetExceptionCallback(asFUNCTION(CaptureNativeException), &PrimaryRecorder, asCALL_CDECL), TEXT("Callback lifecycle should install its exception callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetInstructionCallback(CaptureNativeInstruction, &PrimaryRecorder), TEXT("Callback lifecycle should install its instruction callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureNativeLine), TEXT("Callback lifecycle should install its line callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLoopDetectionCallback(CaptureNativeLoop), TEXT("Callback lifecycle should install its loop callback")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetStackPopCallback(CaptureNativeStackPop), TEXT("Callback lifecycle should install its stack-pop callback")));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Branch), TEXT("Installed callback branch path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgByte(0, 1), TEXT("Installed callback branch path should set its taken branch argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Installed callback branch-taken path should finish")));
		ASSERT_THAT(AreEqual(11, static_cast<int32>(Context->GetReturnDWord()), TEXT("Installed callback branch-taken path should retain its independent runtime result")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.Num(ENativeDebugEventKind::Instruction) > 0, TEXT("Installed instruction callback should record execution")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.Num(ENativeDebugEventKind::Line) > 0, TEXT("Installed line callback should record source locations")));
		ASSERT_THAT(AreEqual(0, PrimaryRecorder.Num(ENativeDebugEventKind::Loop), TEXT("Installed callback branch path should not fabricate loop events")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Installed callback branch path should cleanly unprepare")));

		PrimaryRecorder.Reset();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Branch), TEXT("Installed callback alternate branch should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgByte(0, 0), TEXT("Installed callback alternate branch should set its not-taken argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Installed callback branch-not-taken path should finish")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord()), TEXT("Installed callback branch-not-taken path should retain its independent runtime result")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.Num(ENativeDebugEventKind::Line) > 0, TEXT("Branch-not-taken path should still emit line events")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Installed callback alternate branch should unprepare")));

		const int32 LoopCounts[] = { 0, 1, 100001 };
		for (int32 LoopIndex = 0; LoopIndex < UE_ARRAY_COUNT(LoopCounts); ++LoopIndex)
		{
			PrimaryRecorder.Reset();
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Loop), TEXT("Installed callback loop path should prepare")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, LoopCounts[LoopIndex]), TEXT("Installed callback loop path should set its iteration count")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Installed callback loop path should finish")));
			if (LoopCounts[LoopIndex] < 100000)
			{
				const int32 ExpectedResult = LoopCounts[LoopIndex] * (LoopCounts[LoopIndex] - 1) / 2;
				ASSERT_THAT(AreEqual(ExpectedResult, static_cast<int32>(Context->GetReturnDWord()), TEXT("Short loop path should retain its selected iteration result")));
				ASSERT_THAT(AreEqual(0, PrimaryRecorder.Num(ENativeDebugEventKind::Loop), TEXT("Zero-iteration path should not emit a loop callback")));
			}
			else
			{
				ASSERT_THAT(IsTrue(PrimaryRecorder.Num(ENativeDebugEventKind::Loop) > 0, TEXT("Loop-detection threshold path should emit a loop callback")));
			}
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Installed callback loop path should unprepare")));
		}

		PrimaryRecorder.Reset();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Nested), TEXT("Installed callback nested path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 40), TEXT("Installed callback nested path should set its argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Installed callback nested path should finish")));
		ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord()), TEXT("Installed callback nested path should enter its distinct callee")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.GetEvents().ContainsByPredicate([](const FNativeDebugEvent& Event)
		{
			return Event.FunctionDeclaration == TEXT("int CallbackNested(const int)");
		}), TEXT("Nested callback path should record the nested callee declaration")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.Num(ENativeDebugEventKind::StackPop) > 0, TEXT("Installed stack-pop callback should record nested frame cleanup")));
		ASSERT_THAT(IsTrue(HasValidStackPopRange(PrimaryRecorder), TEXT("Installed stack-pop callback should expose a non-empty old-frame range without dereferencing it")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Installed callback nested path should unprepare")));

		PrimaryRecorder.Reset();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recursion), TEXT("Installed callback recursion path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 3), TEXT("Installed callback recursion path should set its depth")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Installed callback recursion path should finish")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Context->GetReturnDWord()), TEXT("Installed callback recursion path should retain its selected depth result")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.GetEvents().ContainsByPredicate([](const FNativeDebugEvent& Event)
		{
			return Event.FunctionDeclaration == TEXT("int CallbackRecursion(const int)") && Event.CallstackDepth >= 4;
		}), TEXT("Recursive callback path should observe a deeper active callstack")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Installed callback recursion path should unprepare")));

		PrimaryRecorder.Reset();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Method), TEXT("Installed callback method path should prepare")));
		const int MethodExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), MethodExecuteResult,
			*FString::Printf(TEXT("Installed callback method path should finish. Result=%d Exception={%s}"),
				MethodExecuteResult, UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : ""))));
		ASSERT_THAT(AreEqual(31, static_cast<int32>(Context->GetReturnDWord()), TEXT("Installed callback method path should dispatch through the member receiver")));
		ASSERT_THAT(IsTrue(PrimaryRecorder.GetEvents().ContainsByPredicate([](const FNativeDebugEvent& Event)
		{
			return Event.FunctionDeclaration == TEXT("int CallbackMethodOwner::Run(const int)");
		}), TEXT("Method callback path should record the member declaration")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Installed callback method path should unprepare")));

		const int32 PrimaryEventCountBeforeReplacement = PrimaryRecorder.GetEvents().Num();
		Context->SetUserData(&ReplacementRecorder, NativeDebugRecorderUserDataSlot);
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetExceptionCallback(asFUNCTION(CaptureNativeException), &ReplacementRecorder, asCALL_CDECL), TEXT("Callback lifecycle should replace its exception recorder")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetInstructionCallback(CaptureNativeInstruction, &ReplacementRecorder), TEXT("Callback lifecycle should replace its instruction recorder")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureNativeLine), TEXT("Callback lifecycle should replace its line callback state")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLoopDetectionCallback(CaptureNativeLoop), TEXT("Callback lifecycle should replace its loop callback state")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetStackPopCallback(CaptureNativeStackPop), TEXT("Callback lifecycle should replace its stack-pop callback state")));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Branch), TEXT("Replacement callback branch path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgByte(0, 1), TEXT("Replacement callback branch path should set its taken argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Replacement callback branch path should finish")));
		ASSERT_THAT(AreEqual(11, static_cast<int32>(Context->GetReturnDWord()), TEXT("Replacement callback branch path should retain its runtime result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Replacement callback branch path should unprepare")));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Loop), TEXT("Replacement callback loop path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 100001), TEXT("Replacement callback loop path should set its threshold iteration count")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Replacement callback loop path should finish")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Replacement callback loop path should unprepare")));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Nested), TEXT("Replacement callback nested path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 40), TEXT("Replacement callback nested path should set its argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Replacement callback nested path should finish")));
		ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord()), TEXT("Replacement callback nested path should retain its runtime result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Replacement callback nested path should unprepare")));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Fault), TEXT("Replacement callback fault path should prepare")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(), TEXT("Replacement callback fault path should raise the script exception")));
		ASSERT_THAT(AreEqual(0, PrimaryRecorder.GetEvents().Num() - PrimaryEventCountBeforeReplacement, TEXT("Replaced callbacks should not append events to the prior recorder")));
		ASSERT_THAT(IsTrue(ReplacementRecorder.Num(ENativeDebugEventKind::Exception) == 1, TEXT("Replacement exception recorder should observe the exact one fault")));
		ASSERT_THAT(IsTrue(ReplacementRecorder.Num(ENativeDebugEventKind::Instruction) > 0, TEXT("Replacement instruction recorder should observe the fault path")));
		ASSERT_THAT(IsTrue(ReplacementRecorder.Num(ENativeDebugEventKind::Line) > 0, TEXT("Replacement line callback should observe the replacement branch and loop paths")));
		ASSERT_THAT(IsTrue(ReplacementRecorder.Num(ENativeDebugEventKind::Loop) > 0, TEXT("Replacement loop callback should observe the replacement loop path")));
		ASSERT_THAT(IsTrue(ReplacementRecorder.Num(ENativeDebugEventKind::StackPop) > 0, TEXT("Replacement stack-pop callback should observe the replacement nested path")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Exception path should unprepare for context reuse")));

		Context->ClearExceptionCallback();
		Context->ClearExceptionCallback();
		RawContext->ClearInstructionCallback();
		RawContext->ClearInstructionCallback();
		RawContext->ClearLineCallback();
		RawContext->SetLoopDetectionCallback(nullptr);
		RawContext->ClearStackPopCallback();
		RawContext->ClearStackPopCallback();
		RawContext->ClearLineCallback();
		const int32 ReplacementEventCountBeforeClear = ReplacementRecorder.GetEvents().Num();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Loop), TEXT("Cleared callback loop path should prepare for context reuse")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 2), TEXT("Cleared callback loop path should set its iteration count")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Cleared callback loop path should finish")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord()), TEXT("Cleared callback loop path should retain its runtime result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Cleared callback loop path should leave the context reusable")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Nested), TEXT("Cleared callback nested path should prepare for stack-pop verification")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 40), TEXT("Cleared callback nested path should set its argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Cleared callback nested path should finish")));
		ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord()), TEXT("Cleared callback nested path should retain its runtime result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Cleared callback nested path should leave the context reusable")));
		ASSERT_THAT(AreEqual(0, ReplacementRecorder.GetEvents().Num() - ReplacementEventCountBeforeClear, TEXT("Cleared line, loop, and stack-pop callbacks should emit no further events")));
		Context->SetUserData(nullptr, NativeDebugRecorderUserDataSlot);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
