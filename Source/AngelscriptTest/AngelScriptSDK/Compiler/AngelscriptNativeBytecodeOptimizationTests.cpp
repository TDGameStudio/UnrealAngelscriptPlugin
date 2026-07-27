#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Bytecode optimization coverage.
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FBytecodeOptimizationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Bytecode.Optimize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 SumStackChange(asCByteCode& ByteCode)
	{
		int32 StackChange = 0;
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr();
			 Instruction != nullptr;
			 Instruction = Instruction->next)
		{
			StackChange += Instruction->stackInc;
		}
		return StackChange;
	}

	static const void* GetPointerOperand(const asCByteInstruction& Instruction)
	{
		return reinterpret_cast<const void*>(static_cast<asPWORD>(Instruction.arg));
	}

	static FString DescribeLinkedBytecode(asCByteCode& ByteCode)
	{
		TArray<FString> Entries;
		int32 Index = 0;
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr();
			 Instruction != nullptr;
			 Instruction = Instruction->next, ++Index)
		{
			Entries.Add(FString::Printf(
				TEXT("%d:%hs<w0=%d ptr=0x%llx size=%d stack=%d>"),
				Index,
				asBCInfo[Instruction->op].name,
				static_cast<int32>(Instruction->wArg[0]),
				static_cast<uint64>(Instruction->arg),
				Instruction->size,
				Instruction->stackInc));
		}
		return FString::Join(Entries, TEXT("; "));
	}

	struct FCompiledTypedOpcodeObservation
	{
		int32 Count = 0;
		bool bAllTypeOperandsMatch = true;
		int32 FirstDestinationOffset = INDEX_NONE;
	};

	static FString DescribeCompiledBytecode(asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return TEXT("<null function>");
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return TEXT("<empty bytecode>");
		}

		TArray<FString> Instructions;
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:<invalid=%u>"),
					DwordIndex,
					static_cast<uint32>(static_cast<asBYTE>(Opcode))));
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:%hs<size=%d outside length=%u>"),
					DwordIndex,
					asBCInfo[Opcode].name,
					InstructionSize,
					BytecodeLength));
				break;
			}

			Instructions.Add(FString::Printf(
				TEXT("%u:%hs<size=%d>"),
				DwordIndex,
				asBCInfo[Opcode].name,
				InstructionSize));
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return FString::Join(Instructions, TEXT("; "));
	}

	static FCompiledTypedOpcodeObservation ObserveCompiledTypedOpcode(
		asIScriptFunction* Function,
		asEBCInstr ExpectedOpcode,
		const asITypeInfo* ExpectedType)
	{
		FCompiledTypedOpcodeObservation Observation;
		if (Function == nullptr || ExpectedType == nullptr)
		{
			Observation.bAllTypeOperandsMatch = false;
			return Observation;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			Observation.bAllTypeOperandsMatch = false;
			return Observation;
		}

		asUINT DwordIndex = 0;
		asEBCInstr PreviousOpcode = asBC_MAXBYTECODE;
		int32 PreviousDestinationOffset = INDEX_NONE;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				Observation.bAllTypeOperandsMatch = false;
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				Observation.bAllTypeOperandsMatch = false;
				break;
			}

			if (Opcode == ExpectedOpcode)
			{
				++Observation.Count;
				Observation.bAllTypeOperandsMatch &=
					reinterpret_cast<const asITypeInfo*>(
						asBC_PTRARG(&Bytecode[DwordIndex])) == ExpectedType;
				if ((Opcode == asBC_RefCpyV || Opcode == asBC_FREE)
					&& Observation.FirstDestinationOffset == INDEX_NONE)
				{
					Observation.FirstDestinationOffset =
						static_cast<int32>(asBC_SWORDARG0(&Bytecode[DwordIndex]));
				}
				else if (Opcode == asBC_REFCPY
					&& PreviousOpcode == asBC_PSF
					&& Observation.FirstDestinationOffset == INDEX_NONE)
				{
					Observation.FirstDestinationOffset = PreviousDestinationOffset;
				}
			}

			PreviousOpcode = Opcode;
			PreviousDestinationOffset = Opcode == asBC_PSF
				? static_cast<int32>(asBC_SWORDARG0(&Bytecode[DwordIndex]))
				: INDEX_NONE;
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return Observation;
	}

public:
	TEST_METHOD(ReferenceCopyCollapsesToTypedVariableCopyWithEqualStackEffect)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-BYTECODE-REFERENCE-COPY-EXACT-SHAPES",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		// This hand-built linked-bytecode fixture proves the local rewrite structure only.
		// The compiled Entry assertions below own the production compiler/optimizer behavior.
		FBytecodeFixture Fixture("BytecodeOptimizeTypedReferenceCopy", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		asCObjectType ReferenceType(Fixture.Engine);
		static constexpr short DestinationOffset = 7;
		Fixture.ByteCode->InstrSHORT(asBC_PSF, DestinationOffset);
		Fixture.ByteCode->InstrPTR(asBC_REFCPY, &ReferenceType);
		Fixture.ByteCode->Instr(asBC_PopPtr);

		const int32 RawStackChange = SumStackChange(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(3, CountInstructions(*Fixture.ByteCode),
			TEXT("Raw counted-reference copy fixture should contain exactly PSF, REFCPY, and PopPtr")));
		const asCByteInstruction* const RawPsf = Fixture.ByteCode->GetFirstInstr();
		const asCByteInstruction* const RawRefCopy = RawPsf != nullptr ? RawPsf->next : nullptr;
		const asCByteInstruction* const RawPop = RawRefCopy != nullptr ? RawRefCopy->next : nullptr;
		ASSERT_THAT(IsTrue(
			RawPsf != nullptr && RawPsf->op == asBC_PSF
				&& RawRefCopy != nullptr && RawRefCopy->op == asBC_REFCPY
				&& RawPop != nullptr && RawPop->op == asBC_PopPtr
				&& RawPop->next == nullptr,
			TEXT("Raw counted-reference copy fixture should expose the exact optimizer input window")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(&ReferenceType),
			RawRefCopy != nullptr ? GetPointerOperand(*RawRefCopy) : nullptr,
			TEXT("Raw REFCPY should carry the exact counted object type operand")));

		asCArray<int> TemporaryVariableOffsets;
		Fixture.ByteCode->OptimizeLocally(TemporaryVariableOffsets);
		const FString OptimizedBytecode = DescribeLinkedBytecode(*Fixture.ByteCode);
		TestRunner->AddInfo(FString::Printf(
			TEXT("[COMPILER-BYTECODE-REFERENCE-COPY-EXACT-SHAPES] shape=PSF_REFCPY_TO_REFCPYV structural-only=true bytecode=[%s]"),
			*OptimizedBytecode));

		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("Optimized counted-reference copy should collapse PSF plus REFCPY into one RefCpyV")));
		const asCByteInstruction* const OptimizedRefCopy = Fixture.ByteCode->GetFirstInstr();
		const asCByteInstruction* const OptimizedPop =
			OptimizedRefCopy != nullptr ? OptimizedRefCopy->next : nullptr;
		ASSERT_THAT(IsTrue(
			OptimizedRefCopy != nullptr && OptimizedRefCopy->op == asBC_RefCpyV
				&& OptimizedPop != nullptr && OptimizedPop->op == asBC_PopPtr
				&& OptimizedPop->next == nullptr,
			TEXT("Optimized counted-reference copy should expose the exact RefCpyV plus PopPtr output window")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(DestinationOffset),
			OptimizedRefCopy != nullptr
				? static_cast<int32>(OptimizedRefCopy->wArg[0])
				: INDEX_NONE,
			TEXT("Optimized RefCpyV should retain the PSF destination variable offset")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(&ReferenceType),
			OptimizedRefCopy != nullptr ? GetPointerOperand(*OptimizedRefCopy) : nullptr,
			TEXT("Optimized RefCpyV should retain the exact REFCPY counted object type operand")));
		ASSERT_THAT(AreEqual(
			RawStackChange,
			SumStackChange(*Fixture.ByteCode),
			TEXT("PSF plus REFCPY collapse should preserve the complete window stack effect")));
		ASSERT_THAT(AreEqual(
			asBCInfo[asBC_PSF].stackInc + asBCInfo[asBC_REFCPY].stackInc,
			asBCInfo[asBC_RefCpyV].stackInc,
			TEXT("RefCpyV descriptor should encode the combined PSF plus REFCPY stack effect")));

		FString ScriptSource;
		AppendGeneratedAsLine(ScriptSource, TEXT("int RunReferenceCopyOptimization()"));
		AppendGeneratedAsLine(ScriptSource, TEXT("{"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\tFNativeCaseReference Source = CreateNativeCaseReference(610);"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\tFNativeCaseReference Value;"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\tValue = Source;"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(ScriptSource, TEXT("}"));
		for (int32 OptimizationMode = 0; OptimizationMode < 2; ++OptimizationMode)
		{
			const bool bOptimize = OptimizationMode == 1;
			FNativeLifecycleRecorder Lifecycle;
			FNativeTestEngine RuntimeEngine;
			RuntimeEngine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				RuntimeEngine.Destroy();
			};
			asIScriptEngine* const ScriptEngine = RuntimeEngine.Get();
			ASSERT_THAT(IsNotNull(ScriptEngine,
				TEXT("Reference-copy runtime oracle should create a case-owned raw SDK engine")));
			if (ScriptEngine == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, bOptimize ? 1 : 0),
				TEXT("Reference-copy runtime oracle should select its exact optimization mode")));
			ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
				TEXT("Reference-copy runtime oracle should register its counted reference fixture")));
			asITypeInfo* const RegisteredReferenceType =
				ScriptEngine->GetTypeInfoByDecl("FNativeCaseReference");
			ASSERT_THAT(IsNotNull(RegisteredReferenceType,
				TEXT("Reference-copy runtime oracle should publish the registered counted-reference type")));
			if (RegisteredReferenceType == nullptr)
			{
				continue;
			}
			const FString CaseId = FString::Printf(
				TEXT("COMPILER-BYTECODE-OPTIMIZATION-REFCPY-%s"),
				bOptimize ? TEXT("ON") : TEXT("OFF"));
			const FString ModuleName = FString::Printf(
				TEXT("BytecodeReferenceCopyRuntime_%s"),
				bOptimize ? TEXT("On") : TEXT("Off"));
			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, ScriptSource);
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);
			asIScriptModule* Module = nullptr;
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					ScriptSourceUtf8.Get(),
					Module),
				TEXT("Reference-copy runtime oracle should compile in both optimization modes")));
			ASSERT_THAT(IsNotNull(Module,
				TEXT("Reference-copy runtime oracle should publish a module in both optimization modes")));
			if (Module == nullptr)
			{
				continue;
			}

			asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(
				Module,
				"int RunReferenceCopyOptimization()");
			ASSERT_THAT(IsNotNull(Entry,
				TEXT("Reference-copy runtime oracle should publish its exact entry declaration")));
			const asEBCInstr ExpectedCopyOpcode =
				bOptimize ? asBC_RefCpyV : asBC_REFCPY;
			const asEBCInstr RejectedCopyOpcode =
				bOptimize ? asBC_REFCPY : asBC_RefCpyV;
			const FCompiledTypedOpcodeObservation ExpectedCopies =
				ObserveCompiledTypedOpcode(
					Entry,
					ExpectedCopyOpcode,
					RegisteredReferenceType);
			const FCompiledTypedOpcodeObservation RejectedCopies =
				ObserveCompiledTypedOpcode(
					Entry,
					RejectedCopyOpcode,
					RegisteredReferenceType);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] compiled-entry mode=%s bytecode=[%s] expected-copy=%d rejected-copy=%d destination-offset=%d"),
				*CaseId,
				bOptimize ? TEXT("optimized") : TEXT("raw"),
				*DescribeCompiledBytecode(Entry),
				ExpectedCopies.Count,
				RejectedCopies.Count,
				ExpectedCopies.FirstDestinationOffset));
			ASSERT_THAT(IsTrue(ExpectedCopies.Count > 0,
				TEXT("Compiled reference-copy Entry should contain its mode-specific copy opcode")));
			ASSERT_THAT(AreEqual(0, RejectedCopies.Count,
				TEXT("Compiled reference-copy Entry should not mix raw and optimized copy opcodes")));
			ASSERT_THAT(IsTrue(ExpectedCopies.bAllTypeOperandsMatch,
				TEXT("Compiled reference-copy Entry should retain the exact registered TypeInfo pointer operand")));
			ASSERT_THAT(IsTrue(ExpectedCopies.FirstDestinationOffset > 0,
				TEXT("Compiled reference-copy Entry should retain a positive destination variable offset")));
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				TEXT("Reference-copy runtime oracle should create an execution context")));
			if (Entry != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, Entry),
					TEXT("Reference-copy runtime oracle should finish in both optimization modes")));
				ASSERT_THAT(AreEqual(
					610,
					static_cast<int32>(Context->GetReturnDWord()),
					TEXT("Reference-copy optimization should preserve the copied native value")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asSUCCESS),
					Context->Unprepare(),
					TEXT("Reference-copy runtime oracle should unprepare its context")));
			}
			if (Context != nullptr)
			{
				Context->Release();
			}
			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
				TEXT("Reference-copy runtime oracle should construct one factory object")));
			ASSERT_THAT(AreEqual(2, Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				TEXT("Reference-copy runtime oracle should add two local owners")));
			ASSERT_THAT(AreEqual(3, Lifecycle.Num(ENativeLifecycleEvent::Release),
				TEXT("Reference-copy runtime oracle should release factory and local owners exactly once")));
			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				TEXT("Reference-copy runtime oracle should destroy its factory object exactly once")));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
				TEXT("Reference-copy runtime oracle should restore its object-lifetime baseline")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
				TEXT("Reference-copy runtime oracle should discard its exact module")));
		}
	}

	TEST_METHOD(NullVariableCopyCollapsesToTypedFreeWithEqualStackEffect)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"COMPILER-BYTECODE-REFERENCE-COPY-EXACT-SHAPES",
			"null_free");

		// This hand-built linked-bytecode fixture proves the local rewrite structure only.
		// The compiled Entry assertions below own the production compiler/optimizer behavior.
		FBytecodeFixture Fixture("BytecodeOptimizeTypedNullFree", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		asCObjectType ReferenceType(Fixture.Engine);
		static constexpr short DestinationOffset = 9;
		Fixture.ByteCode->Instr(asBC_PshNull);
		Fixture.ByteCode->InstrW_PTR(asBC_RefCpyV, DestinationOffset, &ReferenceType);
		Fixture.ByteCode->Instr(asBC_PopPtr);

		const int32 RawStackChange = SumStackChange(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(3, CountInstructions(*Fixture.ByteCode),
			TEXT("Raw null counted-reference copy should contain exactly PshNull, RefCpyV, and PopPtr")));
		const asCByteInstruction* const RawNull = Fixture.ByteCode->GetFirstInstr();
		const asCByteInstruction* const RawRefCopy = RawNull != nullptr ? RawNull->next : nullptr;
		const asCByteInstruction* const RawPop = RawRefCopy != nullptr ? RawRefCopy->next : nullptr;
		ASSERT_THAT(IsTrue(
			RawNull != nullptr && RawNull->op == asBC_PshNull
				&& RawRefCopy != nullptr && RawRefCopy->op == asBC_RefCpyV
				&& RawPop != nullptr && RawPop->op == asBC_PopPtr
				&& RawPop->next == nullptr,
			TEXT("Raw null counted-reference copy should expose the exact optimizer input window")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(&ReferenceType),
			RawRefCopy != nullptr ? GetPointerOperand(*RawRefCopy) : nullptr,
			TEXT("Raw null RefCpyV should carry the exact counted object type operand")));

		asCArray<int> TemporaryVariableOffsets;
		Fixture.ByteCode->OptimizeLocally(TemporaryVariableOffsets);
		const FString OptimizedBytecode = DescribeLinkedBytecode(*Fixture.ByteCode);
		TestRunner->AddInfo(FString::Printf(
			TEXT("[COMPILER-BYTECODE-REFERENCE-COPY-EXACT-SHAPES] shape=PSHNULL_REFCPYV_POPPTR_TO_FREE structural-only=true bytecode=[%s]"),
			*OptimizedBytecode));

		ASSERT_THAT(AreEqual(1, CountInstructions(*Fixture.ByteCode),
			TEXT("Optimized null counted-reference copy should collapse to one FREE instruction")));
		const asCByteInstruction* const OptimizedFree = Fixture.ByteCode->GetFirstInstr();
		ASSERT_THAT(IsTrue(
			OptimizedFree != nullptr && OptimizedFree->op == asBC_FREE
				&& OptimizedFree->next == nullptr,
			TEXT("Optimized null counted-reference copy should expose the exact typed FREE output")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(DestinationOffset),
			OptimizedFree != nullptr
				? static_cast<int32>(OptimizedFree->wArg[0])
				: INDEX_NONE,
			TEXT("Optimized FREE should retain the RefCpyV destination variable offset")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(&ReferenceType),
			OptimizedFree != nullptr ? GetPointerOperand(*OptimizedFree) : nullptr,
			TEXT("Optimized FREE should retain the exact RefCpyV counted object type operand")));
		ASSERT_THAT(AreEqual(
			RawStackChange,
			SumStackChange(*Fixture.ByteCode),
			TEXT("Null RefCpyV collapse should preserve the complete window stack effect")));
		ASSERT_THAT(AreEqual(
			asBCInfo[asBC_PshNull].stackInc
				+ asBCInfo[asBC_RefCpyV].stackInc
				+ asBCInfo[asBC_PopPtr].stackInc,
			asBCInfo[asBC_FREE].stackInc,
			TEXT("FREE descriptor should encode the complete null RefCpyV window stack effect")));

		FString ScriptSource;
		AppendGeneratedAsLine(ScriptSource, TEXT("int RunNullReferenceCopyOptimization()"));
		AppendGeneratedAsLine(ScriptSource, TEXT("{"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(620);"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\tValue = nullptr;"));
		AppendGeneratedAsLine(ScriptSource, TEXT("\treturn Value == nullptr ? 1 : 0;"));
		AppendGeneratedAsLine(ScriptSource, TEXT("}"));
		int32 RawModeReferenceCopyCount = INDEX_NONE;
		int32 RawModeTypedFreeCount = INDEX_NONE;
		for (int32 OptimizationMode = 0; OptimizationMode < 2; ++OptimizationMode)
		{
			const bool bOptimize = OptimizationMode == 1;
			FNativeLifecycleRecorder Lifecycle;
			FNativeTestEngine RuntimeEngine;
			RuntimeEngine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				RuntimeEngine.Destroy();
			};
			asIScriptEngine* const ScriptEngine = RuntimeEngine.Get();
			ASSERT_THAT(IsNotNull(ScriptEngine,
				TEXT("Null-copy runtime oracle should create a case-owned raw SDK engine")));
			if (ScriptEngine == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, bOptimize ? 1 : 0),
				TEXT("Null-copy runtime oracle should select its exact optimization mode")));
			ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
				TEXT("Null-copy runtime oracle should register its counted reference fixture")));
			asITypeInfo* const RegisteredReferenceType =
				ScriptEngine->GetTypeInfoByDecl("FNativeCaseReference");
			ASSERT_THAT(IsNotNull(RegisteredReferenceType,
				TEXT("Null-copy runtime oracle should publish the registered counted-reference type")));
			if (RegisteredReferenceType == nullptr)
			{
				continue;
			}
			const FString CaseId = FString::Printf(
				TEXT("COMPILER-BYTECODE-OPTIMIZATION-NULL-FREE-%s"),
				bOptimize ? TEXT("ON") : TEXT("OFF"));
			const FString ModuleName = FString::Printf(
				TEXT("BytecodeNullReferenceCopyRuntime_%s"),
				bOptimize ? TEXT("On") : TEXT("Off"));
			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, ScriptSource);
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);
			asIScriptModule* Module = nullptr;
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					ScriptSourceUtf8.Get(),
					Module),
				TEXT("Null-copy runtime oracle should compile in both optimization modes")));
			ASSERT_THAT(IsNotNull(Module,
				TEXT("Null-copy runtime oracle should publish a module in both optimization modes")));
			if (Module == nullptr)
			{
				continue;
			}

			asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(
				Module,
				"int RunNullReferenceCopyOptimization()");
			ASSERT_THAT(IsNotNull(Entry,
				TEXT("Null-copy runtime oracle should publish its exact entry declaration")));
			const FCompiledTypedOpcodeObservation RawCopies =
				ObserveCompiledTypedOpcode(
					Entry,
					asBC_REFCPY,
					RegisteredReferenceType);
			const FCompiledTypedOpcodeObservation VariableCopies =
				ObserveCompiledTypedOpcode(
					Entry,
					asBC_RefCpyV,
					RegisteredReferenceType);
			const FCompiledTypedOpcodeObservation TypedFrees =
				ObserveCompiledTypedOpcode(
					Entry,
					asBC_FREE,
					RegisteredReferenceType);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] compiled-entry mode=%s bytecode=[%s] raw-copy=%d variable-copy=%d typed-free=%d destination-offset=%d"),
				*CaseId,
				bOptimize ? TEXT("optimized") : TEXT("raw"),
				*DescribeCompiledBytecode(Entry),
				RawCopies.Count,
				VariableCopies.Count,
				TypedFrees.Count,
				bOptimize
					? TypedFrees.FirstDestinationOffset
					: RawCopies.FirstDestinationOffset));
			if (bOptimize)
			{
				ASSERT_THAT(AreEqual(0, RawCopies.Count,
					TEXT("Compiled optimized null-copy Entry should remove raw REFCPY")));
				ASSERT_THAT(IsTrue(VariableCopies.Count > 0,
					TEXT("Compiled optimized null-copy Entry should retain the unrelated initialized-value RefCpyV")));
				ASSERT_THAT(IsTrue(VariableCopies.bAllTypeOperandsMatch,
					TEXT("Compiled optimized remaining RefCpyV should retain the exact registered TypeInfo pointer operand")));
				ASSERT_THAT(IsTrue(TypedFrees.Count > 0,
					TEXT("Compiled optimized null-copy Entry should contain its typed FREE opcode")));
				ASSERT_THAT(IsTrue(TypedFrees.bAllTypeOperandsMatch,
					TEXT("Compiled optimized FREE should retain the exact registered TypeInfo pointer operand")));
				ASSERT_THAT(IsTrue(TypedFrees.FirstDestinationOffset > 0,
					TEXT("Compiled optimized FREE should retain a positive destination variable offset")));
				ASSERT_THAT(AreEqual(
					RawModeReferenceCopyCount - 1,
					VariableCopies.Count,
					TEXT("Compiled optimized null assignment should consume exactly one raw reference-copy operation")));
				ASSERT_THAT(AreEqual(
					RawModeTypedFreeCount + 1,
					TypedFrees.Count,
					TEXT("Compiled optimized null assignment should replace the consumed copy with exactly one typed FREE")));
			}
			else
			{
				ASSERT_THAT(IsTrue(RawCopies.Count > 0,
					TEXT("Compiled raw null-copy Entry should retain REFCPY before optimization")));
				ASSERT_THAT(AreEqual(0, VariableCopies.Count,
					TEXT("Compiled raw null-copy Entry should not contain optimized RefCpyV")));
				ASSERT_THAT(IsTrue(RawCopies.bAllTypeOperandsMatch,
					TEXT("Compiled raw null-copy REFCPY should retain the exact registered TypeInfo pointer operand")));
				ASSERT_THAT(IsTrue(RawCopies.FirstDestinationOffset > 0,
					TEXT("Compiled raw null-copy REFCPY should retain its preceding PSF destination offset")));
				RawModeReferenceCopyCount = RawCopies.Count;
				RawModeTypedFreeCount = TypedFrees.Count;
			}
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				TEXT("Null-copy runtime oracle should create an execution context")));
			if (Entry != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, Entry),
					TEXT("Null-copy runtime oracle should finish in both optimization modes")));
				ASSERT_THAT(AreEqual(
					1,
					static_cast<int32>(Context->GetReturnDWord()),
					TEXT("Null-copy optimization should preserve the null assignment result")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asSUCCESS),
					Context->Unprepare(),
					TEXT("Null-copy runtime oracle should unprepare its context")));
			}
			if (Context != nullptr)
			{
				Context->Release();
			}
			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
				TEXT("Null-copy runtime oracle should construct one factory object")));
			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				TEXT("Null-copy runtime oracle should add the local owner exactly once")));
			ASSERT_THAT(AreEqual(2, Lifecycle.Num(ENativeLifecycleEvent::Release),
				TEXT("Null-copy runtime oracle should release the factory and null-assigned owner exactly once")));
			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				TEXT("Null-copy runtime oracle should destroy its factory object exactly once")));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
				TEXT("Null-copy runtime oracle should restore its object-lifetime baseline")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
				TEXT("Null-copy runtime oracle should discard its exact module")));
		}
	}

	TEST_METHOD(OptimizeReducesOrPreservesSize)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained raw optimizer size smoke; COMPILER-BYTECODE-OPTIMIZATION owns off/on compiled optimization with runtime, bytecode, and debug evidence.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeSize", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Instr(asBC_PshRPtr);
		Fixture.ByteCode->Instr(asBC_PopPtr);
		Fixture.ByteCode->Ret(0);
		const int32 BeforeSize = Fixture.ByteCode->GetSize();
		Fixture.ByteCode->Optimize();
		const int32 AfterSize = Fixture.ByteCode->GetSize();

		ASSERT_THAT(IsTrue(AfterSize <= BeforeSize,
			TEXT("Optimize should reduce or preserve bytecode size")));
	}

	TEST_METHOD(OptimizeKeepsSemanticHeadAndTail)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained raw optimizer head/tail smoke; COMPILER-BYTECODE-OPTIMIZATION owns compiled semantic preservation and COMPILER-BYTECODE-MUTATION owns optimized linked-container invariants.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeHeadTail", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);
		Fixture.ByteCode->Ret(0);
		Fixture.ByteCode->Optimize();

		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
			TEXT("Optimize should keep the first semantic opcode")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), Fixture.ByteCode->GetLastInstr(),
			TEXT("Optimize should keep the tail RET opcode")));
	}

	TEST_METHOD(OutputBufferSizeMatchesGetSize)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained representative output-size smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns serialized size across seed, mutation, and payload combinations.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeOutputSize", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 11);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 22);
		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);

		ASSERT_THAT(AreEqual(Fixture.ByteCode->GetSize(), Buffer.Num(),
			TEXT("Output buffer should have one dword per GetSize unit")));
		ASSERT_THAT(IsTrue(Buffer.Num() > 0,
			TEXT("Output buffer should not be empty for emitted bytecode")));
	}

	TEST_METHOD(OutputBufferRoundTripStable)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained repeated-output stability smoke; COMPILER-BYTECODE-MUTATION owns serialized linked-sequence invariants and COMPILER-BYTECODE-CONTAINER-OPERATIONS owns payload preservation.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeOutputStable", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 123);
		Fixture.ByteCode->Ret(0);
		const TArray<asDWORD> FirstBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		const TArray<asDWORD> SecondBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);

		ASSERT_THAT(AreEqual(FirstBuffer.Num(), SecondBuffer.Num(),
			TEXT("Repeated Output calls should produce the same buffer size")));
		ASSERT_THAT(IsTrue(FirstBuffer == SecondBuffer,
			TEXT("Repeated Output calls should produce byte-for-byte stable buffers")));
	}

	TEST_METHOD(OutputAfterAppendIsContiguous)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained append-output smoke; COMPILER-BYTECODE-MUTATION owns add-code mutation and COMPILER-BYTECODE-CONTAINER-OPERATIONS owns serialized payload ordering.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeAppendOutput", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		asCByteCode Tail(Fixture.Builder);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Tail.InstrDWORD(asBC_PshC4, 2);
		const int32 HeadSize = Fixture.ByteCode->GetSize();
		const int32 TailSize = Tail.GetSize();
		Fixture.ByteCode->AddCode(&Tail);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(HeadSize + TailSize, Buffer.Num(),
			TEXT("Appended output should equal head plus tail sizes")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Buffer[HeadSize + 1]),
			TEXT("Appended output should keep the second payload after the second opcode")));
	}

	TEST_METHOD(EmptyByteCodeGetSizeIsZero)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained empty-container smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns zero-seed size, head, tail, removal, and serialization behavior.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeEmpty", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->GetSize(),
			TEXT("Fresh bytecode should have zero emitted size")));
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode).Num(),
			TEXT("Fresh bytecode output helper should return an empty buffer")));
		ASSERT_THAT(AreEqual(-1, Fixture.ByteCode->GetLastInstr(),
			TEXT("Fresh bytecode should report no last instruction")));
	}

	TEST_METHOD(LastInstrValueDwAfterMixedOps)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained terminal-DWORD query smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns terminal payload behavior across seeds and mutations.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeLastValue", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 7);
		Fixture.ByteCode->InstrDWORD(asBC_JMP, 12);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_JMP), Fixture.ByteCode->GetLastInstr(),
			TEXT("GetLastInstr should report the final DWORD opcode")));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("GetLastInstrValueDW should read the final DWORD payload")));
	}

	TEST_METHOD(GetLastInstrTypeAfterRet)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained terminal-RET query smoke; COMPILER-BYTECODE-MUTATION owns linked tail preservation and COMPILER-BYTECODE-OPCODE-DESCRIPTORS owns the RET descriptor.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOptimizeLastRet", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 7);
		Fixture.ByteCode->Ret(0);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), Fixture.ByteCode->GetLastInstr(),
			TEXT("GetLastInstr should report RET after Ret helper")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_PshC4),
			TEXT("Bytecode should still contain the earlier PshC4")));
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("Ret helper should add exactly two instructions in this sequence")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
