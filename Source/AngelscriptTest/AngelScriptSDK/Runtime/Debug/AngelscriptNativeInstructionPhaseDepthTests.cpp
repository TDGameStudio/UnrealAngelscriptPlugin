#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeInstructionPhaseDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.InstructionPhaseDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int InstructionCallee(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value + 2;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int InstructionProbe(int Count)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < Count; ++Index)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif ((Index & 1) == 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\tResult += InstructionCallee(Index);"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:

	TEST_METHOD(InstructionPhaseOpcodeAndCallbackClear)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-INSTRUCTION-PHASE-FAMILY",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Instruction phase product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeDebugInstructionPhaseDepth");
		const FString Source = BuildSource();
		PrintGeneratedAsSource(*TestRunner, TEXT("DBG-INSTRUCTION-PHASE-FAMILY"), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("Instruction phase source should compile. Build=%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(Module, TEXT("Instruction phase source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleNameUtf8.Get()); };

		asIScriptFunction* const Probe = GetNativeFunctionByExactDecl(Module, "int InstructionProbe(const int)");
		asIScriptFunction* const Callee = GetNativeFunctionByExactDecl(Module, "int InstructionCallee(const int)");
		ASSERT_THAT(IsNotNull(Probe, TEXT("Instruction phase product should resolve the loop probe exactly")));
		ASSERT_THAT(IsNotNull(Callee, TEXT("Instruction phase product should resolve the nested callee exactly")));
		if (Probe == nullptr || Callee == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Instruction phase product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		asCContext* const RawContext = static_cast<asCContext*>(Context);
		FNativeDebugRecorder Recorder;
		ASSERT_THAT(IsNull(Context->SetUserData(&Recorder, NativeDebugRecorderUserDataSlot), TEXT("Instruction phase product should install a null previous user-data slot")));
		const int CallbackInstallResult = RawContext->SetInstructionCallback(CaptureNativeInstruction, &Recorder);
		ASSERT_THAT(AreEqual(asSUCCESS, CallbackInstallResult, TEXT("Instruction phase product should install the raw instruction callback")));
		if (CallbackInstallResult != asSUCCESS)
		{
			return;
		}
		const int ProbePrepareResult = Context->Prepare(Probe);
		ASSERT_THAT(AreEqual(asSUCCESS, ProbePrepareResult, TEXT("Instruction phase product should prepare the branch and loop probe")));
		if (ProbePrepareResult != asSUCCESS)
		{
			return;
		}
		const int ProbeSetResult = Context->SetArgDWord(0, 6);
		ASSERT_THAT(AreEqual(asSUCCESS, ProbeSetResult, TEXT("Instruction phase product should set the loop count")));
		if (ProbeSetResult != asSUCCESS)
		{
			Context->Unprepare();
			return;
		}
		const int ProbeExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ProbeExecuteResult, TEXT("Instruction phase product should execute the callback probe")));
		if (ProbeExecuteResult != asEXECUTION_FINISHED)
		{
			Context->Unprepare();
			return;
		}
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Context->GetReturnDWord()), TEXT("Instruction phase product should preserve the selected even-index call result")));

		int32 BeforeCount = 0;
		int32 AfterCount = 0;
		int32 MaximumDepth = 0;
		TSet<FString> FunctionDeclarations;
		TMap<FString, int32> BeforeCountByFunction;
		TMap<FString, int32> AfterCountByFunction;
		for (const FNativeDebugEvent& Event : Recorder.GetEvents())
		{
			if (Event.Kind != ENativeDebugEventKind::Instruction)
			{
				continue;
			}

			if (Event.InstructionPhase == asVM_BEFORE_INSTRUCTION)
			{
				++BeforeCount;
				++BeforeCountByFunction.FindOrAdd(Event.FunctionDeclaration);
			}
			else if (Event.InstructionPhase == asVM_AFTER_INSTRUCTION)
			{
				++AfterCount;
				++AfterCountByFunction.FindOrAdd(Event.FunctionDeclaration);
			}
			ASSERT_THAT(IsTrue(Event.BytecodeOffset >= 0, TEXT("Instruction phase product should expose a non-negative bytecode offset")));
			ASSERT_THAT(IsTrue(!Event.Text.IsEmpty(), TEXT("Instruction phase product should expose an opcode name")));
			ASSERT_THAT(IsTrue(!Event.FunctionDeclaration.IsEmpty(), TEXT("Instruction phase product should expose the current function declaration")));
			FunctionDeclarations.Add(Event.FunctionDeclaration);
			MaximumDepth = FMath::Max(MaximumDepth, static_cast<int32>(Event.CallstackDepth));
		}
		ASSERT_THAT(IsTrue(BeforeCount > 0, TEXT("Instruction phase product should observe before-instruction callbacks")));
		ASSERT_THAT(IsTrue(AfterCount > 0, TEXT("Instruction phase product should observe after-instruction callbacks")));
		ASSERT_THAT(AreEqual(BeforeCount, AfterCount, TEXT("Instruction phase product should pair every before callback with an after callback")));
		ASSERT_THAT(IsTrue(FunctionDeclarations.Contains(TEXT("int InstructionProbe(const int)")), TEXT("Instruction phase product should observe the probe function")));
		ASSERT_THAT(IsTrue(FunctionDeclarations.Contains(TEXT("int InstructionCallee(const int)")), TEXT("Instruction phase product should observe the nested callee")));
		ASSERT_THAT(IsTrue(BeforeCountByFunction.FindRef(TEXT("int InstructionProbe(const int)")) > 0,
			TEXT("Instruction phase product should observe a before phase in the loop owner")));
		ASSERT_THAT(AreEqual(
			BeforeCountByFunction.FindRef(TEXT("int InstructionProbe(const int)")),
			AfterCountByFunction.FindRef(TEXT("int InstructionProbe(const int)")),
			TEXT("Instruction phase product should pair the loop owner's before and after phases")));
		ASSERT_THAT(IsTrue(BeforeCountByFunction.FindRef(TEXT("int InstructionCallee(const int)")) > 0,
			TEXT("Instruction phase product should observe a before phase in the nested callee")));
		ASSERT_THAT(AreEqual(
			BeforeCountByFunction.FindRef(TEXT("int InstructionCallee(const int)")),
			AfterCountByFunction.FindRef(TEXT("int InstructionCallee(const int)")),
			TEXT("Instruction phase product should pair the nested callee's before and after phases")));
		ASSERT_THAT(IsTrue(MaximumDepth >= 2, TEXT("Instruction phase product should observe a nested callstack depth")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Instruction phase product should unprepare after callback observation")));

		const int32 EventCountBeforeClear = Recorder.GetEvents().Num();
		RawContext->ClearInstructionCallback();
		const int CalleePrepareResult = Context->Prepare(Callee);
		ASSERT_THAT(AreEqual(asSUCCESS, CalleePrepareResult, TEXT("Instruction phase product should prepare a second call after callback clear")));
		if (CalleePrepareResult != asSUCCESS)
		{
			return;
		}
		const int CalleeSetResult = Context->SetArgDWord(0, 3);
		ASSERT_THAT(AreEqual(asSUCCESS, CalleeSetResult, TEXT("Instruction phase product should set the second call argument")));
		if (CalleeSetResult != asSUCCESS)
		{
			Context->Unprepare();
			return;
		}
		const int CalleeExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), CalleeExecuteResult, TEXT("Instruction phase product should execute the second call after callback clear")));
		if (CalleeExecuteResult != asEXECUTION_FINISHED)
		{
			Context->Unprepare();
			return;
		}
		ASSERT_THAT(AreEqual(5, static_cast<int32>(Context->GetReturnDWord()), TEXT("Instruction phase product should preserve the second call result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Instruction phase product should unprepare after callback clear")));
		ASSERT_THAT(AreEqual(EventCountBeforeClear, Recorder.GetEvents().Num(), TEXT("Instruction phase product should emit no further events after clearing the callback")));
		ASSERT_THAT(AreEqual(static_cast<void*>(&Recorder), Context->SetUserData(nullptr, NativeDebugRecorderUserDataSlot), TEXT("Instruction phase product should clear and return its recorder user-data slot")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
