#include "Shared/AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace AngelscriptTest_Learning_Native_BytecodeVm_Private
{
	struct FObservedVmInstructionState
	{
		asDWORD* ProgramPointer = nullptr;
		asDWORD* StackPointer = nullptr;
		asDWORD* StackFramePointer = nullptr;
		asDWORD* StackSnapshotBase = nullptr;
		asCArray<asDWORD> StackSnapshot;
		asQWORD ValueRegister = 0;
		void* ObjectRegister = nullptr;
		asITypeInfo* ObjectType = nullptr;
		asCScriptFunction* CurrentFunction = nullptr;
		int32 BytecodeOffset = -1;
		int32 LineNumber = -1;
		asEContextState Status = asEXECUTION_UNINITIALIZED;
		asUINT CallstackDepth = 0;
	};

	struct FObservedVmInstruction
	{
		FString Opcode;
		int32 BeforeOffset = -1;
		int32 AfterOffset = -1;
		int32 BeforeLine = -1;
		int32 AfterLine = -1;
		int32 BeforeDepth = 0;
		int32 AfterDepth = 0;
		int32 BeforeStatus = asEXECUTION_UNINITIALIZED;
		int32 AfterStatus = asEXECUTION_UNINITIALIZED;
		uint64 BeforeValueRegister = 0;
		uint64 AfterValueRegister = 0;
		void* BeforeObjectRegister = nullptr;
		void* AfterObjectRegister = nullptr;
		asDWORD* BeforeProgramPointer = nullptr;
		asDWORD* AfterProgramPointer = nullptr;
		asDWORD* BeforeStackPointer = nullptr;
		asDWORD* AfterStackPointer = nullptr;
		asDWORD* BeforeFramePointer = nullptr;
		asDWORD* AfterFramePointer = nullptr;
		int32 BeforeStackSnapshotWords = 0;
		int32 AfterStackSnapshotWords = 0;
		FString BeforeFunction;
		FString AfterFunction;
		FString BeforeLocals;
		FString AfterLocals;
	};

	struct FPendingVmInstruction
	{
		FString Opcode;
		FObservedVmInstructionState State;
	};

	struct FVmInstructionRecorder
	{
		TArray<FObservedVmInstruction> Events;
		int32 MaxEvents = 96;
		FPendingVmInstruction PendingBefore;
		bool bHasPendingBefore = false;
	};
	FString FormatPointer(const void* Pointer)
	{
		return FString::Printf(TEXT("0x%p"), Pointer);
	}

	FString FormatFunctionName(asCScriptFunction* Function)
	{
		return Function != nullptr ? UTF8_TO_TCHAR(Function->GetDeclaration()) : TEXT("<none>");
	}

	struct FScopedVmInstructionState
	{
		FScopedVmInstructionState(asCContext* InContext, const FObservedVmInstructionState& State)
			: Context(InContext)
			, PreviousFunction(InContext->m_currentFunction)
			, PreviousStatus(InContext->m_status)
			, PreviousProgramPointer(InContext->m_regs.programPointer)
			, PreviousStackPointer(InContext->m_regs.stackPointer)
			, PreviousFramePointer(InContext->m_regs.stackFramePointer)
		{
			Context->m_currentFunction = State.CurrentFunction;
			Context->m_status = State.Status;
			Context->m_regs.programPointer = State.ProgramPointer;
			Context->m_regs.stackPointer = State.StackPointer;
			Context->m_regs.stackFramePointer = State.StackFramePointer;
		}

		~FScopedVmInstructionState()
		{
			Context->m_currentFunction = PreviousFunction;
			Context->m_status = PreviousStatus;
			Context->m_regs.programPointer = PreviousProgramPointer;
			Context->m_regs.stackPointer = PreviousStackPointer;
			Context->m_regs.stackFramePointer = PreviousFramePointer;
		}

		asCContext* Context;
		asCScriptFunction* PreviousFunction;
		asEContextState PreviousStatus;
		asDWORD* PreviousProgramPointer;
		asDWORD* PreviousStackPointer;
		asDWORD* PreviousFramePointer;
	};

	bool IsFunctionParameter(asCScriptFunction* Function, const asSScriptVariable* Variable)
	{
		if (Function == nullptr || Variable == nullptr)
		{
			return false;
		}

		const int32 StackOffset = Variable->stackOffset;
		return StackOffset < 0 || Function->parameterOffsets.IndexOf(-StackOffset) != -1;
	}

	bool DoesInstructionWriteStackOffset(asDWORD* Instruction, int32 StackOffset)
	{
		if (Instruction == nullptr)
		{
			return false;
		}

		const asBYTE Op = *(asBYTE*)Instruction;
		switch (asBCInfo[Op].type)
		{
		case asBCTYPE_wW_ARG:
		case asBCTYPE_wW_DW_ARG:
		case asBCTYPE_wW_QW_ARG:
		case asBCTYPE_wW_rW_ARG:
		case asBCTYPE_wW_W_ARG:
		case asBCTYPE_wW_rW_DW_ARG:
		case asBCTYPE_wW_rW_rW_ARG:
			return asBC_SWORDARG0(Instruction) == StackOffset;
		default:
			return false;
		}
	}

	bool IsIntLocalInitializedAt(asCScriptFunction* Function, const asSScriptVariable* Variable, int32 BytecodeOffset)
	{
		if (Function == nullptr || Function->scriptData == nullptr || Variable == nullptr || BytecodeOffset < 0)
		{
			return false;
		}

		if (IsFunctionParameter(Function, Variable))
		{
			return true;
		}

		const int32 DeclaredOffset = FMath::Max(0, static_cast<int32>(Variable->declaredAtProgramPos));
		if (BytecodeOffset <= DeclaredOffset)
		{
			return false;
		}

		asCArray<asDWORD>& ByteCode = Function->scriptData->byteCode;
		const int32 ByteCodeLength = static_cast<int32>(ByteCode.GetLength());
		const int32 EndOffset = FMath::Clamp(BytecodeOffset, 0, ByteCodeLength);
		for (int32 Offset = 0; Offset < EndOffset;)
		{
			asDWORD* Instruction = &ByteCode[static_cast<asUINT>(Offset)];
			const asBYTE Op = *(asBYTE*)Instruction;
			if (Offset >= DeclaredOffset && DoesInstructionWriteStackOffset(Instruction, Variable->stackOffset))
			{
				return true;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Op].type];
			Offset += InstructionSize > 0 ? InstructionSize : 1;
		}

		return false;
	}

	FObservedVmInstructionState CaptureVmState(asCContext* Context, const asSVMInstructionInfo& State)
	{
		FObservedVmInstructionState Snapshot;
		Snapshot.ProgramPointer = const_cast<asDWORD*>(State.ProgramPointer);
		Snapshot.StackPointer = const_cast<asDWORD*>(State.StackPointer);
		Snapshot.StackFramePointer = const_cast<asDWORD*>(State.StackFramePointer);
		Snapshot.ValueRegister = State.ValueRegister;
		Snapshot.ObjectRegister = State.ObjectRegister;
		Snapshot.ObjectType = State.ObjectType;
		Snapshot.CurrentFunction = static_cast<asCScriptFunction*>(State.CurrentFunction);
		Snapshot.Status = State.Status;
		Snapshot.CallstackDepth = State.CallstackDepth;

		if (Snapshot.CurrentFunction != nullptr && Snapshot.CurrentFunction->scriptData != nullptr && State.BytecodeOffset >= 0)
		{
			Snapshot.BytecodeOffset = State.BytecodeOffset;
			int SectionIdx = -1;
			Snapshot.LineNumber = Snapshot.CurrentFunction->GetLineNumber(Snapshot.BytecodeOffset, &SectionIdx) & 0xFFFFF;
		}

		if (Context != nullptr && State.StackIndex < Context->m_stackBlocks.GetLength() && Context->m_stackBlocks[State.StackIndex] != nullptr && Context->m_stackBlockSize > 0)
		{
			const asUINT StackFrameSize = Context->GetStackFrameSize(State.StackIndex);
			Snapshot.StackSnapshotBase = Context->m_stackBlocks[State.StackIndex];
			Snapshot.StackSnapshot.Copy(Context->m_stackBlocks[State.StackIndex], StackFrameSize);
		}

		return Snapshot;
	}

	FString CaptureVisibleIntLocals(asCContext* Context, const FObservedVmInstructionState& State)
	{
		if (Context == nullptr || State.CurrentFunction == nullptr || State.CurrentFunction->scriptData == nullptr || State.StackSnapshotBase == nullptr || State.StackSnapshot.GetLength() == 0)
		{
			return TEXT("<none>");
		}

		FScopedVmInstructionState ScopedState(Context, State);
		TArray<FString> LocalPairs;
		const asUINT VarCount = Context->GetVarCount(0);
		for (asUINT VarIndex = 0; VarIndex < VarCount; ++VarIndex)
		{
			if (!Context->IsVarInScope(VarIndex, 0))
			{
				continue;
			}

			const int TypeId = Context->GetVarTypeId(VarIndex, 0);
			if (TypeId != asTYPEID_INT32)
			{
				continue;
			}

			void* Address = Context->GetAddressOfVar(VarIndex, 0);
			if (Address == nullptr)
			{
				continue;
			}

			const int64 DwordOffset = static_cast<const asDWORD*>(Address) - State.StackSnapshotBase;
			if (DwordOffset < 0 || DwordOffset >= static_cast<int64>(State.StackSnapshot.GetLength()))
			{
				continue;
			}

			const char* Name = Context->GetVarName(VarIndex, 0);
			const TCHAR* DisplayName = UTF8_TO_TCHAR(Name != nullptr ? Name : "<unnamed>");
			const asSScriptVariable* Variable = State.CurrentFunction->scriptData->variables[VarIndex];
			if (!IsIntLocalInitializedAt(State.CurrentFunction, Variable, State.BytecodeOffset))
			{
				LocalPairs.Add(FString::Printf(TEXT("%s=<uninitialized>"), DisplayName));
				continue;
			}

			const int32 SnapshotValue = static_cast<int32>(State.StackSnapshot[static_cast<asUINT>(DwordOffset)]);
			LocalPairs.Add(FString::Printf(TEXT("%s=%d"), DisplayName, SnapshotValue));
		}

		return LocalPairs.Num() > 0 ? FString::Join(LocalPairs, TEXT("; ")) : TEXT("<none>");
	}
	void CaptureVmInstruction(asIScriptContext* ScriptContext, const asSVMInstructionInfo* State, void* UserData)
	{
		FVmInstructionRecorder* Recorder = static_cast<FVmInstructionRecorder*>(UserData);
		asCContext* Context = static_cast<asCContext*>(ScriptContext);
		if (Context == nullptr || State == nullptr || Recorder == nullptr)
		{
			return;
		}

		if (State->Phase == asVM_BEFORE_INSTRUCTION)
		{
			if (Recorder->Events.Num() >= Recorder->MaxEvents)
			{
				Recorder->bHasPendingBefore = false;
				return;
			}

			const char* InstructionName = State->InstructionName != nullptr ? State->InstructionName : "<unknown>";
			Recorder->PendingBefore.Opcode = UTF8_TO_TCHAR(InstructionName);
			Recorder->PendingBefore.State = CaptureVmState(Context, *State);
			Recorder->bHasPendingBefore = true;
			return;
		}

		if (!Recorder->bHasPendingBefore || Recorder->Events.Num() >= Recorder->MaxEvents)
		{
			Recorder->bHasPendingBefore = false;
			return;
		}

		const FObservedVmInstructionState After = CaptureVmState(Context, *State);
		const FPendingVmInstruction& Before = Recorder->PendingBefore;

		FObservedVmInstruction& Observed = Recorder->Events.AddDefaulted_GetRef();
		Observed.Opcode = Before.Opcode;
		Observed.BeforeOffset = Before.State.BytecodeOffset;
		Observed.AfterOffset = After.BytecodeOffset;
		Observed.BeforeLine = Before.State.LineNumber;
		Observed.AfterLine = After.LineNumber;
		Observed.BeforeDepth = static_cast<int32>(Before.State.CallstackDepth);
		Observed.AfterDepth = static_cast<int32>(After.CallstackDepth);
		Observed.BeforeStatus = static_cast<int32>(Before.State.Status);
		Observed.AfterStatus = static_cast<int32>(After.Status);
		Observed.BeforeValueRegister = Before.State.ValueRegister;
		Observed.AfterValueRegister = After.ValueRegister;
		Observed.BeforeObjectRegister = Before.State.ObjectRegister;
		Observed.AfterObjectRegister = After.ObjectRegister;
		Observed.BeforeProgramPointer = Before.State.ProgramPointer;
		Observed.AfterProgramPointer = After.ProgramPointer;
		Observed.BeforeStackPointer = Before.State.StackPointer;
		Observed.AfterStackPointer = After.StackPointer;
		Observed.BeforeFramePointer = Before.State.StackFramePointer;
		Observed.AfterFramePointer = After.StackFramePointer;
		Observed.BeforeStackSnapshotWords = static_cast<int32>(Before.State.StackSnapshot.GetLength());
		Observed.AfterStackSnapshotWords = static_cast<int32>(After.StackSnapshot.GetLength());
		Observed.BeforeFunction = FormatFunctionName(Before.State.CurrentFunction);
		Observed.AfterFunction = FormatFunctionName(After.CurrentFunction);
		Observed.BeforeLocals = CaptureVisibleIntLocals(Context, Before.State);
		Observed.AfterLocals = CaptureVisibleIntLocals(Context, After);
		Recorder->bHasPendingBefore = false;
	}

	FString FormatInstructionState(const FObservedVmInstruction& Event)
	{
		return FString::Printf(
			TEXT("Before{Status=%d Function=%s Offset=%d Line=%d Depth=%d PC=%s SP=%s FP=%s Value=0x%016llX Object=%s Locals=%s StackWords=%d} -> After{Status=%d Function=%s Offset=%d Line=%d Depth=%d PC=%s SP=%s FP=%s Value=0x%016llX Object=%s Locals=%s StackWords=%d}"),
			Event.BeforeStatus,
			*Event.BeforeFunction,
			Event.BeforeOffset,
			Event.BeforeLine,
			Event.BeforeDepth,
			*FormatPointer(Event.BeforeProgramPointer),
			*FormatPointer(Event.BeforeStackPointer),
			*FormatPointer(Event.BeforeFramePointer),
			Event.BeforeValueRegister,
			*FormatPointer(Event.BeforeObjectRegister),
			*Event.BeforeLocals,
			Event.BeforeStackSnapshotWords,
			Event.AfterStatus,
			*Event.AfterFunction,
			Event.AfterOffset,
			Event.AfterLine,
			Event.AfterDepth,
			*FormatPointer(Event.AfterProgramPointer),
			*FormatPointer(Event.AfterStackPointer),
			*FormatPointer(Event.AfterFramePointer),
			Event.AfterValueRegister,
			*FormatPointer(Event.AfterObjectRegister),
			*Event.AfterLocals,
			Event.AfterStackSnapshotWords);
	}

	bool AnyOpcode(const TArray<FObservedVmInstruction>& Events, const TCHAR* Opcode)
	{
		return Events.ContainsByPredicate([Opcode](const FObservedVmInstruction& Event)
		{
			return Event.Opcode == Opcode;
		});
	}

	bool AnyDepthTransition(const TArray<FObservedVmInstruction>& Events, int32 BeforeDepth, int32 AfterDepth)
	{
		return Events.ContainsByPredicate([BeforeDepth, AfterDepth](const FObservedVmInstruction& Event)
		{
			return Event.BeforeDepth == BeforeDepth && Event.AfterDepth == AfterDepth;
		});
	}

	bool AnyLocalValue(const TArray<FObservedVmInstruction>& Events, const TCHAR* LocalPair)
	{
		return Events.ContainsByPredicate([LocalPair](const FObservedVmInstruction& Event)
		{
			return Event.BeforeLocals.Contains(LocalPair) || Event.AfterLocals.Contains(LocalPair);
		});
	}

	bool AnyExpandedStackSnapshot(const TArray<FObservedVmInstruction>& Events, int32 InitialStackWords)
	{
		return Events.ContainsByPredicate([InitialStackWords](const FObservedVmInstruction& Event)
		{
			return Event.BeforeStackSnapshotWords > InitialStackWords || Event.AfterStackSnapshotWords > InitialStackWords;
		});
	}

	FString BuildLargeStackProbeSource(int32 LocalCount)
	{
		FString Source = TEXT("int StackSnapshotProbe(int Start)\n{\n");
		Source += TEXT("  int V00 = Start;\n");
		for (int32 Index = 1; Index < LocalCount; ++Index)
		{
			Source += FString::Printf(TEXT("  int V%02d = V%02d + 1;\n"), Index, Index - 1);
		}
		Source += FString::Printf(TEXT("  return V%02d;\n}\n"), LocalCount - 1);
		return Source;
	}

	bool AnyOpcodeLocalTransition(const TArray<FObservedVmInstruction>& Events, const TCHAR* Opcode, const TCHAR* BeforeLocal, const TCHAR* AfterLocal)
	{
		return Events.ContainsByPredicate([Opcode, BeforeLocal, AfterLocal](const FObservedVmInstruction& Event)
		{
			return Event.Opcode == Opcode && Event.BeforeLocals.Contains(BeforeLocal) && Event.AfterLocals.Contains(AfterLocal);
		});
	}

	bool AnyTopLevelFinishedReturn(const TArray<FObservedVmInstruction>& Events)
	{
		return Events.ContainsByPredicate([](const FObservedVmInstruction& Event)
		{
			return Event.Opcode == TEXT("RET") && Event.BeforeDepth == 1 && Event.AfterStatus == asEXECUTION_FINISHED;
		});
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningGuideNativeBytecodeVmTest,
	"Angelscript.TestModule.Learning.Native.BytecodeVm.Execution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningGuideNativeBytecodeVmStackSnapshotTest,
	"Angelscript.TestModule.Learning.Native.BytecodeVm.StackSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningGuideNativeBytecodeVmTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Native_BytecodeVm_Private;

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;
	SinkConfig.DetailLevel = EAngelscriptLearningTraceDetailLevel::Verbose;

	FAngelscriptLearningTraceSession Guide(TEXT("LearningGuideNativeBytecodeVm"), SinkConfig);
	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(ScriptEngine);
	};

	if (!TestNotNull(TEXT("LearningGuide BytecodeVm should create a native AngelScript engine"), ScriptEngine))
	{
		return false;
	}

	const ANSICHAR* Source =
		"int AddOne(int Value)\n"
		"{\n"
		"  int Next = Value + 1;\n"
		"  return Next;\n"
		"}\n"
		"\n"
		"int DoubleAfterIncrement(int Start)\n"
		"{\n"
		"  int Local = AddOne(Start);\n"
		"  int Result = Local * 2;\n"
		"  return Result;\n"
		"}\n";

	Guide.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	asIScriptModule* Module = nullptr;
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningGuideNativeBytecodeVm", Source, Module);
	Guide.AddStep(TEXT("BuildModule"), BuildResult >= 0 ? TEXT("Compiled the VM learning guide module") : TEXT("Failed to compile the VM learning guide module"));
	Guide.AddKeyValue(TEXT("BuildResult"), FString::FromInt(BuildResult));
	Guide.AddKeyValue(TEXT("MessageCount"), FString::FromInt(Messages.Entries.Num()));
	Guide.AddKeyValue(TEXT("Functions"), Module != nullptr ? CollectFunctionDeclarations(Module) : TEXT("<null module>"));
	Guide.AddCodeBlock(UTF8_TO_TCHAR(Source));

	if (!TestTrue(TEXT("LearningGuide BytecodeVm source should compile"), BuildResult >= 0) || !TestNotNull(TEXT("LearningGuide BytecodeVm module should exist"), Module))
	{
		Guide.FlushToAutomation(*this);
		Guide.FlushToLog();
		return false;
	}

	asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int DoubleAfterIncrement(int)");
	if (!TestNotNull(TEXT("LearningGuide BytecodeVm should resolve DoubleAfterIncrement(int)"), Function))
	{
		Guide.FlushToAutomation(*this);
		Guide.FlushToLog();
		return false;
	}

	Guide.BeginPhase(EAngelscriptLearningTracePhase::Bytecode);
	asUINT BytecodeLength = 0;
	asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	TArray<uint32> BytecodeWords;
	for (asUINT Index = 0; Index < BytecodeLength; ++Index)
	{
		BytecodeWords.Add(Bytecode[Index]);
	}
	Guide.AddStep(TEXT("InspectEntryFunction"), TEXT("Captured function metadata and raw bytecode buffer"));
	Guide.AddKeyValue(TEXT("Declaration"), UTF8_TO_TCHAR(Function->GetDeclaration()));
	Guide.AddKeyValue(TEXT("ParamCount"), FString::FromInt(Function->GetParamCount()));
	Guide.AddKeyValue(TEXT("VarCount"), FString::FromInt(Function->GetVarCount()));
	Guide.AddKeyValue(TEXT("BytecodeLength"), FString::FromInt(static_cast<int32>(BytecodeLength)));
	Guide.AddCodeBlock(FormatLearningTraceBytecode(BytecodeWords, 16));

	asIScriptContext* ScriptContext = ScriptEngine->CreateContext();
	if (!TestNotNull(TEXT("LearningGuide BytecodeVm should create a context"), ScriptContext))
	{
		Guide.FlushToAutomation(*this);
		Guide.FlushToLog();
		return false;
	}
	ON_SCOPE_EXIT
	{
		ScriptContext->Release();
	};

	asCContext* Context = static_cast<asCContext*>(ScriptContext);
	FVmInstructionRecorder Recorder;
	ScriptContext->SetInstructionCallback(CaptureVmInstruction, &Recorder);
	ON_SCOPE_EXIT
	{
		ScriptContext->ClearInstructionCallback();
	};

	Guide.BeginPhase(EAngelscriptLearningTracePhase::Execution);
	const int32 PrepareResult = Context->Prepare(Function);
	Guide.AddStep(TEXT("Prepare"), PrepareResult == asSUCCESS ? TEXT("Prepared the entry function and initialized the VM stack frame") : TEXT("Failed to prepare the entry function"));
	Guide.AddKeyValue(TEXT("PrepareResult"), FString::FromInt(PrepareResult));
	if (!TestEqual(TEXT("LearningGuide BytecodeVm prepare should succeed"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Guide.FlushToAutomation(*this);
		Guide.FlushToLog();
		return false;
	}

	Context->SetArgDWord(0, 20);
	Guide.AddStep(TEXT("SetArgDWord"), TEXT("Wrote the Start argument into the prepared stack frame"));
	Guide.AddKeyValue(TEXT("ArgIndex"), TEXT("0"));
	Guide.AddKeyValue(TEXT("ArgValue"), TEXT("20"));

	const int32 ExecuteResult = Context->Execute();
	const int32 ReturnValue = static_cast<int32>(Context->GetReturnDWord());
	Guide.AddStep(TEXT("Execute"), ExecuteResult == asEXECUTION_FINISHED ? TEXT("Executed bytecode and captured VM instruction state changes") : TEXT("Execution did not finish successfully"));
	Guide.AddKeyValue(TEXT("ExecuteResult"), FString::FromInt(ExecuteResult));
	Guide.AddKeyValue(TEXT("ReturnValue"), FString::FromInt(ReturnValue));
	Guide.AddKeyValue(TEXT("InstructionEvents"), FString::FromInt(Recorder.Events.Num()));

	for (int32 EventIndex = 0; EventIndex < Recorder.Events.Num(); ++EventIndex)
	{
		const FObservedVmInstruction& Event = Recorder.Events[EventIndex];
		Guide.AddStep(FString::Printf(TEXT("VM.%03d.%s"), EventIndex + 1, *Event.Opcode), TEXT("Observed one VM opcode before/after state transition"));
		Guide.AddKeyValue(TEXT("Opcode"), Event.Opcode);
		Guide.AddKeyValue(TEXT("BeforeOffset"), FString::FromInt(Event.BeforeOffset));
		Guide.AddKeyValue(TEXT("AfterOffset"), FString::FromInt(Event.AfterOffset));
		Guide.AddKeyValue(TEXT("BeforeDepth"), FString::FromInt(Event.BeforeDepth));
		Guide.AddKeyValue(TEXT("AfterDepth"), FString::FromInt(Event.AfterDepth));
		Guide.AddKeyValue(TEXT("BeforeStatus"), FString::FromInt(Event.BeforeStatus));
		Guide.AddKeyValue(TEXT("AfterStatus"), FString::FromInt(Event.AfterStatus));
		Guide.AddKeyValue(TEXT("BeforeStackSnapshotWords"), FString::FromInt(Event.BeforeStackSnapshotWords));
		Guide.AddKeyValue(TEXT("AfterStackSnapshotWords"), FString::FromInt(Event.AfterStackSnapshotWords));
		Guide.AddKeyValue(TEXT("State"), FormatInstructionState(Event));
	}

	const bool bBytecodePresent = TestTrue(TEXT("LearningGuide BytecodeVm should expose bytecode"), Bytecode != nullptr && BytecodeLength > 0);
	const bool bExecutionFinished = TestEqual(TEXT("LearningGuide BytecodeVm should finish execution"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
	const bool bReturnValue = TestEqual(TEXT("LearningGuide BytecodeVm should return 42"), ReturnValue, 42);
	const bool bCapturedInstructions = TestTrue(TEXT("LearningGuide BytecodeVm should capture VM instruction events"), Recorder.Events.Num() > 0);
	const bool bCapturedCall = TestTrue(TEXT("LearningGuide BytecodeVm should capture CALL opcode"), AnyOpcode(Recorder.Events, TEXT("CALL")));
	const bool bCapturedReturn = TestTrue(TEXT("LearningGuide BytecodeVm should capture RET opcode"), AnyOpcode(Recorder.Events, TEXT("RET")));
	const bool bEnteredNestedCall = TestTrue(TEXT("LearningGuide BytecodeVm should show callstack entering AddOne"), AnyDepthTransition(Recorder.Events, 1, 2));
	const bool bReturnedToCaller = TestTrue(TEXT("LearningGuide BytecodeVm should show callstack returning to caller"), AnyDepthTransition(Recorder.Events, 2, 1));
	const bool bCapturedTopLevelReturn = TestTrue(TEXT("LearningGuide BytecodeVm should show top-level RET finishing execution"), AnyTopLevelFinishedReturn(Recorder.Events));
	const bool bCapturedArgument = TestTrue(TEXT("LearningGuide BytecodeVm should expose Start=20 in locals"), AnyLocalValue(Recorder.Events, TEXT("Start=20")));
	const bool bCapturedIntermediate = TestTrue(TEXT("LearningGuide BytecodeVm should expose Local=21 or Next=21 in locals"), AnyLocalValue(Recorder.Events, TEXT("Local=21")) || AnyLocalValue(Recorder.Events, TEXT("Next=21")));
	const bool bCapturedResult = TestTrue(TEXT("LearningGuide BytecodeVm should expose Result=42 in locals"), AnyLocalValue(Recorder.Events, TEXT("Result=42")));
	const bool bCapturedLocalTransition = TestTrue(TEXT("LearningGuide BytecodeVm should show Local changing from uninitialized to 21 after AddOne returns"), AnyOpcodeLocalTransition(Recorder.Events, TEXT("CpyRtoV4"), TEXT("Local=<uninitialized>"), TEXT("Local=21")));
	const bool bContainsRegisters = AssertLearningTraceContainsKeyword(*this, Guide.GetEvents(), TEXT("BeforeDepth"));
	const bool bMinimumEvents = AssertLearningTraceMinimumEventCount(*this, Guide.GetEvents(), 8);

	Guide.FlushToAutomation(*this);
	Guide.FlushToLog();

	return bBytecodePresent
		&& bExecutionFinished
		&& bReturnValue
		&& bCapturedInstructions
		&& bCapturedCall
		&& bCapturedReturn
		&& bEnteredNestedCall
		&& bReturnedToCaller
		&& bCapturedTopLevelReturn
		&& bCapturedArgument
		&& bCapturedIntermediate
		&& bCapturedResult
		&& bCapturedLocalTransition
		&& bContainsRegisters
		&& bMinimumEvents;
}

bool FAngelscriptLearningGuideNativeBytecodeVmStackSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Native_BytecodeVm_Private;

	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(ScriptEngine);
	};

	if (!TestNotNull(TEXT("LearningGuide BytecodeVm stack snapshot should create a native AngelScript engine"), ScriptEngine))
	{
		return false;
	}

	constexpr int32 InitialStackBytes = 64;
	constexpr int32 InitialStackWords = InitialStackBytes / static_cast<int32>(sizeof(asDWORD));
	ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0);
	ScriptEngine->SetEngineProperty(asEP_INIT_STACK_SIZE, InitialStackBytes);

	const FString Source = BuildLargeStackProbeSource(128);
	const FTCHARToUTF8 SourceUtf8(*Source);
	asIScriptModule* Module = nullptr;
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningGuideNativeBytecodeVmStackSnapshot", SourceUtf8.Get(), Module);
	if (!TestTrue(TEXT("LearningGuide BytecodeVm stack snapshot source should compile"), BuildResult >= 0) || !TestNotNull(TEXT("LearningGuide BytecodeVm stack snapshot module should exist"), Module))
	{
		AddInfo(CollectMessages(Messages));
		return false;
	}

	asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int StackSnapshotProbe(int)");
	if (!TestNotNull(TEXT("LearningGuide BytecodeVm stack snapshot should resolve StackSnapshotProbe(int)"), Function))
	{
		return false;
	}

	asIScriptContext* ScriptContext = ScriptEngine->CreateContext();
	if (!TestNotNull(TEXT("LearningGuide BytecodeVm stack snapshot should create a context"), ScriptContext))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		ScriptContext->Release();
	};

	asCContext* Context = static_cast<asCContext*>(ScriptContext);
	FVmInstructionRecorder Recorder;
	Recorder.MaxEvents = 64;
	ScriptContext->SetInstructionCallback(CaptureVmInstruction, &Recorder);
	ON_SCOPE_EXIT
	{
		ScriptContext->ClearInstructionCallback();
	};

	const int32 PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("LearningGuide BytecodeVm stack snapshot prepare should succeed"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	const bool bPreparedOnExpandedStack = TestTrue(TEXT("LearningGuide BytecodeVm stack snapshot fixture should force an expanded VM stack block"), Context->m_stackIndex > 0);
	Context->SetArgDWord(0, 1);
	const int32 ExecuteResult = Context->Execute();
	const int32 ReturnValue = static_cast<int32>(Context->GetReturnDWord());

	const bool bExecutionFinished = TestEqual(TEXT("LearningGuide BytecodeVm stack snapshot should finish execution"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
	const bool bReturnValue = TestEqual(TEXT("LearningGuide BytecodeVm stack snapshot should return the local chain result"), ReturnValue, 128);
	const bool bCapturedInstructions = TestTrue(TEXT("LearningGuide BytecodeVm stack snapshot should capture VM instruction events"), Recorder.Events.Num() > 0);
	const bool bCapturedExpandedSnapshot = TestTrue(TEXT("LearningGuide BytecodeVm stack snapshot should copy the active expanded stack block"), AnyExpandedStackSnapshot(Recorder.Events, InitialStackWords));

	return bPreparedOnExpandedStack
		&& bExecutionFinished
		&& bReturnValue
		&& bCapturedInstructions
		&& bCapturedExpandedSnapshot;
}
#endif
