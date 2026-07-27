#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FByteInstructionTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.ByteInstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asCByteInstruction* FindInstruction(asCByteCode& ByteCode, const asEBCInstr Opcode)
	{
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			if (Instruction->op == Opcode)
			{
				return Instruction;
			}
		}

		return nullptr;
	}

	static int32 ReadOpcodeAt(const TArray<asDWORD>& Buffer, const int32 DwordIndex)
	{
		if (!Buffer.IsValidIndex(DwordIndex))
		{
			return INDEX_NONE;
		}

		return static_cast<int32>(*reinterpret_cast<const asBYTE*>(&Buffer[DwordIndex]));
	}

	static bool BuildAndGetFunctionBytecode(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		const char* Declaration,
		asIScriptModule*& OutModule,
		asIScriptFunction*& OutFunction,
		asDWORD*& OutBytecode,
		asUINT& OutBytecodeLength)
	{
		FNoDiscardAsserter LocalAssert(Test);

		OutModule = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!LocalAssert.IsNotNull(OutModule, TEXT("Bytecode compile test should build the script module")))
		{
			return false;
		}

		OutFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(OutModule, Declaration);
		if (!LocalAssert.IsNotNull(OutFunction, TEXT("Bytecode compile test should resolve the requested function")))
		{
			return false;
		}

		OutBytecode = OutFunction->GetByteCode(&OutBytecodeLength);
		return LocalAssert.IsNotNull(OutBytecode, TEXT("Bytecode compile test should expose a bytecode buffer"))
			&& LocalAssert.IsTrue(OutBytecodeLength > 0, TEXT("Bytecode compile test should emit at least one bytecode dword"));
	}

	static bool ExecuteIntEntry(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptFunction* Function, int32& OutValue)
	{
		FNoDiscardAsserter LocalAssert(Test);

		asIScriptContext* Context = ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (!LocalAssert.IsNotNull(Context, TEXT("Bytecode compile test should create an execution context")))
		{
			return false;
		}

		const int ExecuteResult = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function);
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		return LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Bytecode compile test should execute successfully"));
	}

	static bool BytecodeContainsOpcode(const asDWORD* Bytecode, const asUINT BytecodeLength, const asEBCInstr Opcode)
	{
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr CurrentOpcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (CurrentOpcode == Opcode)
			{
				return true;
			}

			if (static_cast<int32>(CurrentOpcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[CurrentOpcode].type];
			if (InstructionSize <= 0)
			{
				break;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	static bool BytecodeContainsAnyOpcode(const asDWORD* Bytecode, const asUINT BytecodeLength, const TArray<asEBCInstr>& Opcodes)
	{
		for (const asEBCInstr Opcode : Opcodes)
		{
			if (BytecodeContainsOpcode(Bytecode, BytecodeLength, Opcode))
			{
				return true;
			}
		}

		return false;
	}

	static TArray<asDWORD> CopyBytecodeBuffer(const asDWORD* Bytecode, const asUINT BytecodeLength)
	{
		TArray<asDWORD> Result;
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return Result;
		}

		Result.SetNumUninitialized(static_cast<int32>(BytecodeLength));
		FMemory::Memcpy(Result.GetData(), Bytecode, BytecodeLength * sizeof(asDWORD));
		return Result;
	}

public:
	TEST_METHOD(InstructionContainersBySeedMutationAndPayload)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BYTECODE-CONTAINER-OPERATIONS",
			ENativeEvidence::Bytecode
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FSeedCase
		{
			const TCHAR* Id;
			int32 Count;
		};
		struct FMutationCase
		{
			const TCHAR* Id;
			int32 Kind;
		};
		struct FPayloadCase
		{
			const TCHAR* Id;
			int32 Value;
		};

		const FSeedCase Seeds[] =
		{
			{ TEXT("empty"), 0 },
			{ TEXT("one"), 1 },
			{ TEXT("two"), 2 },
			{ TEXT("four"), 4 },
		};
		const FMutationCase Mutations[] =
		{
			{ TEXT("append"), 0 },
			{ TEXT("prepend"), 1 },
			{ TEXT("remove"), 2 },
			{ TEXT("clear_reappend"), 3 },
			{ TEXT("jump_resolve"), 4 },
		};
		const FPayloadCase Payloads[] =
		{
			{ TEXT("zero"), 0 },
			{ TEXT("positive"), 42 },
			{ TEXT("maximum"), MAX_int32 },
		};

		int32 ObservedCaseCount = 0;
		for (const FSeedCase& Seed : Seeds)
		{
			for (const FMutationCase& Mutation : Mutations)
			{
				for (const FPayloadCase& Payload : Payloads)
				{
					const FString CaseId = MakeNativeCaseId(
						"COMPILER-BYTECODE-CONTAINER-OPERATIONS",
						{ Seed.Id, Mutation.Id, Payload.Id });
					FString ReviewSource;
					AppendGeneratedAsLine(
						ReviewSource,
						FString::Printf(
							TEXT("// seed=%d mutation=%s payload=%d"),
							Seed.Count,
							Mutation.Id,
							Payload.Value));
					const int32 SelectedRemovalSeedIndex = Mutation.Kind == 2 && Seed.Count > 0
						? (Payload.Value == 0 ? 0 : Payload.Value == MAX_int32 ? Seed.Count - 1 : Seed.Count / 2)
						: INDEX_NONE;
					if (Mutation.Kind == 2)
					{
						AppendGeneratedAsLine(
							ReviewSource,
							FString::Printf(
								TEXT("// remove_seed_index=%d (payload selects the logical node removed from the tail)"),
								SelectedRemovalSeedIndex));
					}
					PrintGeneratedAsSource(
						*TestRunner,
						CaseId,
						TEXT("CompilerBytecodeContainerOperations"),
						ReviewSource);

					FBytecodeFixture Fixture(TCHAR_TO_UTF8(*CaseId));
					if (!Fixture.IsValid(*TestRunner))
					{
						continue;
					}

					TArray<int32> ExpectedPayloads;
					int32 RemovalSeedIndex = INDEX_NONE;
					if (Mutation.Kind == 2 && Seed.Count > 0)
					{
						// RemoveLastInstr is the public removal primitive in this fork. Rotate
						// the seed sequence so Payload selects which logical seed is the tail;
						// the post-removal sequence then proves that selection was effective.
						RemovalSeedIndex = SelectedRemovalSeedIndex;
						for (int32 Offset = 0; Offset < Seed.Count; ++Offset)
						{
							const int32 SeedIndex = (RemovalSeedIndex + 1 + Offset) % Seed.Count;
							const int32 SeedPayload = SeedIndex + 1;
							Fixture.ByteCode->InstrDWORD(asBC_PshC4, static_cast<asDWORD>(SeedPayload));
							ExpectedPayloads.Add(SeedPayload);
						}
					}
					else
					{
						for (int32 SeedIndex = 0; SeedIndex < Seed.Count; ++SeedIndex)
						{
							Fixture.ByteCode->InstrDWORD(
								asBC_PshC4,
								static_cast<asDWORD>(SeedIndex + 1));
						}
					}

					int32 ExpectedInstructionCount = Seed.Count;
					switch (Mutation.Kind)
					{
					case 0:
						Fixture.ByteCode->InstrDWORD(
							asBC_PshC4,
							static_cast<asDWORD>(Payload.Value));
						ExpectedInstructionCount = Seed.Count + 1;
						break;
					case 1:
						Fixture.ByteCode->InsertFirstInstrDWORD(
							asBC_PshC4,
							static_cast<asDWORD>(Payload.Value));
						ExpectedInstructionCount = Seed.Count + 1;
						break;
					case 2:
						if (Seed.Count == 0)
						{
							ASSERT_THAT(IsTrue(
								Fixture.ByteCode->RemoveLastInstr() < 0,
								*FString::Printf(TEXT("%s should reject removing an empty tail"), *CaseId)));
							// There is no seed node to select in the empty case. Still exercise
							// the Payload axis through a transient node, observe its payload and
							// serialized representation, then remove it and restore emptiness.
							Fixture.ByteCode->InstrDWORD(
								asBC_PshC4,
								static_cast<asDWORD>(Payload.Value));
							ASSERT_THAT(AreEqual(
								Payload.Value,
								static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
								*FString::Printf(TEXT("%s should retain the empty-case probe payload"), *CaseId)));
							const TArray<asDWORD> ProbeBuffer = EmitToBuffer(*Fixture.ByteCode);
							ASSERT_THAT(IsTrue(
								ProbeBuffer.Num() >= 2,
								*FString::Printf(TEXT("%s should serialize the empty-case probe"), *CaseId)));
							if (ProbeBuffer.Num() >= 2)
							{
								ASSERT_THAT(AreEqual(
									Payload.Value,
									static_cast<int32>(ProbeBuffer[1]),
									*FString::Printf(TEXT("%s should serialize the selected empty-case probe payload"), *CaseId)));
							}
							ASSERT_THAT(AreEqual(
								0,
								Fixture.ByteCode->RemoveLastInstr(),
								*FString::Printf(TEXT("%s should remove the empty-case probe"), *CaseId)));
							ExpectedInstructionCount = 0;
						}
						else
						{
							const asCByteInstruction* LastBeforeRemoval = Fixture.ByteCode->GetFirstInstr();
							while (LastBeforeRemoval != nullptr && LastBeforeRemoval->next != nullptr)
							{
								LastBeforeRemoval = LastBeforeRemoval->next;
							}
							ASSERT_THAT(IsNotNull(
								LastBeforeRemoval,
								*FString::Printf(TEXT("%s should expose the selected tail before removal"), *CaseId)));
							if (LastBeforeRemoval != nullptr)
							{
								ASSERT_THAT(AreEqual(
									RemovalSeedIndex + 1,
									static_cast<int32>(LastBeforeRemoval->arg),
									*FString::Printf(TEXT("%s should place the payload-selected seed at the removable tail"), *CaseId)));
							}
							ASSERT_THAT(AreEqual(
								0,
								Fixture.ByteCode->RemoveLastInstr(),
								*FString::Printf(TEXT("%s should remove the existing tail"), *CaseId)));
							ASSERT_THAT(AreEqual(
								RemovalSeedIndex + 1,
								ExpectedPayloads.Last(),
								*FString::Printf(TEXT("%s should remove the payload-selected seed"), *CaseId)));
							ExpectedPayloads.Pop();
							ExpectedInstructionCount = Seed.Count - 1;
						}
						break;
					case 3:
						Fixture.ByteCode->ClearAll();
						Fixture.ByteCode->InstrDWORD(
							asBC_PshC4,
							static_cast<asDWORD>(Payload.Value));
						ExpectedInstructionCount = 1;
						break;
					default:
						Fixture.ByteCode->InstrDWORD(asBC_JMP, 100);
						Fixture.ByteCode->InstrDWORD(
							asBC_PshC4,
							static_cast<asDWORD>(Payload.Value));
						Fixture.ByteCode->Label(100);
						ASSERT_THAT(AreEqual(
							0,
							Fixture.ByteCode->ResolveJumpAddresses(),
							*FString::Printf(TEXT("%s should resolve its generated jump label"), *CaseId)));
						// LABEL is a zero-size linked pseudo-instruction. It is
						// intentionally counted by linked traversal but omitted
						// from GetSize and serialized output.
						ExpectedInstructionCount = Seed.Count + 3;
						break;
					}

					const TArray<asDWORD> Buffer = EmitToBuffer(*Fixture.ByteCode);
					ASSERT_THAT(AreEqual(
						ExpectedInstructionCount,
						CountInstructions(*Fixture.ByteCode),
						*FString::Printf(TEXT("%s should preserve linked instruction count"), *CaseId)));
					ASSERT_THAT(AreEqual(
						Fixture.ByteCode->GetSize(),
						Buffer.Num(),
						*FString::Printf(TEXT("%s should serialize exactly GetSize dwords"), *CaseId)));
					ASSERT_THAT(AreEqual(
						ExpectedInstructionCount == 0,
						Fixture.ByteCode->GetFirstInstr() == nullptr,
						*FString::Printf(TEXT("%s should preserve empty/non-empty head state"), *CaseId)));
					ASSERT_THAT(AreEqual(
						ExpectedInstructionCount == 0,
						Fixture.ByteCode->GetLastInstr() < 0,
						*FString::Printf(TEXT("%s should preserve empty/non-empty tail state"), *CaseId)));

					if (Mutation.Kind == 0 || Mutation.Kind == 3)
					{
						ASSERT_THAT(AreEqual(
							Payload.Value,
							static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
							*FString::Printf(TEXT("%s should preserve the appended payload"), *CaseId)));
					}
					else if (Mutation.Kind == 1)
					{
						ASSERT_THAT(AreEqual(
							static_cast<int32>(asBC_PshC4),
							static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
							*FString::Printf(TEXT("%s should prepend the selected opcode"), *CaseId)));
						ASSERT_THAT(AreEqual(
							Payload.Value,
							static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->arg),
							*FString::Printf(TEXT("%s should preserve the prepended payload"), *CaseId)));
					}
					else if (Mutation.Kind == 2)
					{
						int32 ObservedPayloadCount = 0;
						for (const asCByteInstruction* Instruction = Fixture.ByteCode->GetFirstInstr();
							Instruction != nullptr;
							Instruction = Instruction->next)
						{
							ASSERT_THAT(AreEqual(
								static_cast<int32>(asBC_PshC4),
								static_cast<int32>(Instruction->op),
								*FString::Printf(TEXT("%s should retain PshC4 at remaining sequence index %d"), *CaseId, ObservedPayloadCount)));
							if (ExpectedPayloads.IsValidIndex(ObservedPayloadCount))
							{
								ASSERT_THAT(AreEqual(
									ExpectedPayloads[ObservedPayloadCount],
									static_cast<int32>(Instruction->arg),
									*FString::Printf(TEXT("%s should retain the remaining payload sequence at index %d"), *CaseId, ObservedPayloadCount)));
							}
							++ObservedPayloadCount;
						}
						ASSERT_THAT(AreEqual(
							ExpectedPayloads.Num(),
							ObservedPayloadCount,
							*FString::Printf(TEXT("%s should retain exactly the expected number of remaining payloads"), *CaseId)));

						const int32 SerializedInstructionSize = asBCTypeSize[asBCInfo[asBC_PshC4].type];
						ASSERT_THAT(AreEqual(
							2,
							SerializedInstructionSize,
							*FString::Printf(TEXT("%s should use the two-dword PshC4 encoding"), *CaseId)));
						ASSERT_THAT(AreEqual(
							ExpectedPayloads.Num() * SerializedInstructionSize,
							Buffer.Num(),
							*FString::Printf(TEXT("%s should serialize the remaining payload sequence without gaps"), *CaseId)));
						for (int32 PayloadIndex = 0; PayloadIndex < ExpectedPayloads.Num(); ++PayloadIndex)
						{
							const int32 SerializedIndex = PayloadIndex * SerializedInstructionSize;
							if (Buffer.IsValidIndex(SerializedIndex + 1))
							{
								ASSERT_THAT(AreEqual(
									static_cast<int32>(asBC_PshC4),
									ReadOpcodeAt(Buffer, SerializedIndex),
									*FString::Printf(TEXT("%s should serialize PshC4 at remaining sequence index %d"), *CaseId, PayloadIndex)));
								ASSERT_THAT(AreEqual(
									ExpectedPayloads[PayloadIndex],
									static_cast<int32>(Buffer[SerializedIndex + 1]),
									*FString::Printf(TEXT("%s should serialize the remaining payload at index %d"), *CaseId, PayloadIndex)));
							}
						}
					}
					else if (Mutation.Kind == 4)
					{
						const asCByteInstruction* const Jump = FindInstruction(
							*Fixture.ByteCode,
							asBC_JMP);
						ASSERT_THAT(IsNotNull(
							Jump,
							*FString::Printf(TEXT("%s should retain the resolved jump"), *CaseId)));
						if (Jump != nullptr)
						{
							ASSERT_THAT(IsTrue(
								static_cast<int32>(*reinterpret_cast<const int*>(&Jump->arg)) > 0,
								*FString::Printf(TEXT("%s should rewrite the label to a forward offset"), *CaseId)));
						}
					}

					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			60,
			ObservedCaseCount,
			TEXT("Seed count × mutation × payload should execute every bytecode-container cell")));
	}

	TEST_METHOD(InstructionSequence)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained representative instruction-sequence smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns seed, mutation, and payload combinations.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeInstructionSequence");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);
		Fixture.ByteCode->Instr(asBC_RET);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("Bytecode should contain the emitted push and ret instructions")));
		ASSERT_THAT(AreEqual(Buffer.Num(), Fixture.ByteCode->GetSize(),
			TEXT("Serialized bytecode dword count should match GetSize")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
			TEXT("First emitted opcode should match asBC_PshC4")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->arg),
			TEXT("First emitted opcode should keep the dword payload")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("Last emitted opcode should match asBC_RET")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), ReadOpcodeAt(Buffer, 0),
			TEXT("Serialized buffer should start with PshC4")));
	}

	TEST_METHOD(ByteInstructionAppend)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained append smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns append across seed and payload combinations.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeAppend");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		asCByteCode Tail(Fixture.Builder);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Tail.InstrDWORD(asBC_PshC4, 20);

		const int32 HeadSize = Fixture.ByteCode->GetSize();
		const int32 TailSize = Tail.GetSize();
		Fixture.ByteCode->AddCode(&Tail);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(HeadSize + TailSize, Fixture.ByteCode->GetSize(),
			TEXT("AddCode should append the tail sequence size exactly")));
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("AddCode should preserve both instructions in order")));
		ASSERT_THAT(AreEqual(10, static_cast<int32>(Buffer[1]),
			TEXT("AddCode should preserve the head payload")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), ReadOpcodeAt(Buffer, HeadSize),
			TEXT("AddCode should place the tail opcode after the head sequence")));
		ASSERT_THAT(AreEqual(20, static_cast<int32>(Buffer[HeadSize + 1]),
			TEXT("AddCode should place the tail payload after the tail opcode")));
		ASSERT_THAT(AreEqual(20, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("The last dword payload should come from the appended sequence")));
	}

	TEST_METHOD(ByteInstructionJumpResolution)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained forward-jump smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS and COMPILER-BYTECODE-MUTATION own generated jump resolution and linked invariants.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpResolution");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->Label(1);

		const asCByteInstruction* JumpBeforeResolve = FindInstruction(*Fixture.ByteCode, asBC_JMP);
		ASSERT_THAT(IsNotNull(JumpBeforeResolve, TEXT("Jump resolution test should emit a JMP instruction")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(JumpBeforeResolve->arg),
			TEXT("JMP argument should initially store the label id")));

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
			TEXT("ResolveJumpAddresses should resolve a forward label jump")));

		const asCByteInstruction* JumpAfterResolve = FindInstruction(*Fixture.ByteCode, asBC_JMP);
		ASSERT_THAT(IsNotNull(JumpAfterResolve, TEXT("Resolved bytecode should retain the JMP instruction")));
		ASSERT_THAT(IsTrue(static_cast<int32>(*reinterpret_cast<const int*>(&JumpAfterResolve->arg)) > 0,
			TEXT("ResolveJumpAddresses should rewrite the label id to a positive relative offset")));
	}

	TEST_METHOD(ByteInstructionOutput)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained repeated-output smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns serialization parity across generated container states.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOutput");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);

		const TArray<asDWORD> FirstBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		const TArray<asDWORD> SecondBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);

		ASSERT_THAT(AreEqual(Fixture.ByteCode->GetSize(), FirstBuffer.Num(),
			TEXT("Output should write one dword per GetSize unit")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), ReadOpcodeAt(FirstBuffer, 0),
			TEXT("Output should preserve the opcode in the first emitted dword")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(FirstBuffer[1]),
			TEXT("Output should preserve the dword payload for asBC_PshC4")));
		ASSERT_THAT(IsTrue(FirstBuffer == SecondBuffer,
			TEXT("Repeated Output calls should produce stable buffers")));
	}

	TEST_METHOD(EmptyBytecodeState)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained empty-state smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns empty and populated container states.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeEmptyState");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->GetSize(),
			TEXT("Fresh bytecode should report zero size")));
		ASSERT_THAT(IsNull(Fixture.ByteCode->GetFirstInstr(),
			TEXT("Fresh bytecode should not expose a first instruction")));
		ASSERT_THAT(AreEqual(-1, static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("Fresh bytecode should report no last instruction")));
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode).Num(),
			TEXT("Fresh bytecode should serialize to an empty buffer")));
	}

	TEST_METHOD(InsertFirstInstructionPrependsSequence)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained prepend smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns prepend across seed and payload combinations.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeInsertFirst");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Instr(asBC_RET);
		Fixture.ByteCode->InsertFirstInstrDWORD(asBC_PshC4, 7);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("InsertFirstInstrDWORD should prepend one instruction before the existing tail")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
			TEXT("InsertFirstInstrDWORD should become the first opcode")));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Buffer[1]),
			TEXT("InsertFirstInstrDWORD should preserve the inserted payload")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("InsertFirstInstrDWORD should preserve the existing tail opcode")));
	}

	TEST_METHOD(RemoveLastInstructionUpdatesTail)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained remove-tail smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns empty and populated removal states.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeRemoveLast");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 2);
		Fixture.ByteCode->Instr(asBC_RET);

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->RemoveLastInstr(),
			TEXT("RemoveLastInstr should report success when removing the tail instruction")));
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("RemoveLastInstr should remove exactly the tail instruction")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("RemoveLastInstr should update the tail to the previous instruction")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("RemoveLastInstr should preserve the previous instruction payload")));
	}

	TEST_METHOD(ClearAllResetsInstructionList)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained clear/reuse smoke; COMPILER-BYTECODE-CONTAINER-OPERATIONS owns clear-and-reappend across seed and payload combinations.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeClearAll");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Fixture.ByteCode->Instr(asBC_RET);
		Fixture.ByteCode->ClearAll();

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->GetSize(),
			TEXT("ClearAll should reset bytecode size")));
		ASSERT_THAT(IsNull(Fixture.ByteCode->GetFirstInstr(),
			TEXT("ClearAll should remove the first instruction")));
		ASSERT_THAT(AreEqual(-1, static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("ClearAll should reset the last instruction")));

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 99);
		ASSERT_THAT(AreEqual(1, CountInstructions(*Fixture.ByteCode),
			TEXT("Bytecode should accept new instructions after ClearAll")));
		ASSERT_THAT(AreEqual(99, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("New instruction after ClearAll should preserve its payload")));
	}

	TEST_METHOD(LineInstructionSerializesDebugMarker)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained LINE-marker smoke; COMPILER-BYTECODE-OPTIMIZATION owns compiled debug metadata and this method preserves the direct marker regression.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeLineInstruction");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Line(12, 3, 0);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_LINE),
			TEXT("Line helper should emit a LINE instruction")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_PshC4),
			TEXT("Line helper should not prevent later instructions from being emitted")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_LINE), ReadOpcodeAt(Buffer, 0),
			TEXT("Serialized bytecode should start with the LINE marker")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("Serialized bytecode should retain the later semantic tail")));
	}

};

#endif
