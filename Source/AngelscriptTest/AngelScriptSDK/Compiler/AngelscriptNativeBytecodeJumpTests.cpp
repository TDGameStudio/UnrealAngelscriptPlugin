#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

// Bytecode jump-resolution coverage.
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FBytecodeJumpTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Bytecode.Jumps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asCByteInstruction* FindOpcode(const asCByteCode& ByteCode, const asEBCInstr Opcode)
	{
		for (const asCByteInstruction* Instruction = const_cast<asCByteCode&>(ByteCode).GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			if (Instruction->op == Opcode)
			{
				return Instruction;
			}
		}

		return nullptr;
	}

	static int32 GetInstructionSize(const asCByteInstruction* Instruction)
	{
		return Instruction != nullptr
			? const_cast<asCByteInstruction*>(Instruction)->GetSize()
			: 0;
	}

public:
	TEST_METHOD(JumpTopologiesResolveOrRejectWithExactOffsets)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-BYTECODE-JUMP-RESOLUTION",
			ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		{
			FBytecodeFixture Fixture("BytecodeJumpDepthForward");
			if (!Fixture.IsValid(*TestRunner))
			{
				return;
			}

			Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
			Fixture.ByteCode->Label(1);

			ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
				TEXT("Forward jump depth cell should resolve")));
			const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
			const asCByteInstruction* Payload =
				FindOpcode(*Fixture.ByteCode, asBC_PshC4);
			ASSERT_THAT(IsNotNull(Jump, TEXT("Forward jump depth cell should retain JMP")));
			ASSERT_THAT(IsNotNull(
				Payload,
				TEXT("Forward jump depth cell should retain its payload")));
			ASSERT_THAT(AreEqual(
				GetInstructionSize(Payload),
				Jump != nullptr
					? *reinterpret_cast<const int*>(&Jump->arg)
					: 0,
				TEXT("Forward jump depth cell should publish the exact payload-sized offset")));
			ASSERT_THAT(AreEqual(
				Fixture.ByteCode->GetSize(),
				EmitToBuffer(*Fixture.ByteCode).Num(),
				TEXT("Forward jump depth cell should preserve serialized-size parity")));
		}

		{
			FBytecodeFixture Fixture("BytecodeJumpDepthBackward");
			if (!Fixture.IsValid(*TestRunner))
			{
				return;
			}

			Fixture.ByteCode->Label(2);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 20);
			Fixture.ByteCode->InstrDWORD(asBC_JMP, 2);

			ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
				TEXT("Backward jump depth cell should resolve")));
			const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
			const asCByteInstruction* Payload =
				FindOpcode(*Fixture.ByteCode, asBC_PshC4);
			ASSERT_THAT(IsNotNull(Jump, TEXT("Backward jump depth cell should retain JMP")));
			ASSERT_THAT(IsNotNull(
				Payload,
				TEXT("Backward jump depth cell should retain its payload instruction")));
			ASSERT_THAT(AreEqual(
				Jump != nullptr && Payload != nullptr
					? -(GetInstructionSize(Jump) + GetInstructionSize(Payload))
					: 0,
				Jump != nullptr
					? *reinterpret_cast<const int*>(&Jump->arg)
					: 0,
				TEXT("Backward jump depth cell should publish the exact signed offset")));
		}

		{
			FBytecodeFixture Fixture("BytecodeJumpDepthMultiple");
			if (!Fixture.IsValid(*TestRunner))
			{
				return;
			}

			Fixture.ByteCode->InstrDWORD(asBC_JZ, 3);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 30);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 31);
			Fixture.ByteCode->Label(3);
			Fixture.ByteCode->InstrDWORD(asBC_JNZ, 4);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 40);
			Fixture.ByteCode->Label(4);

			ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
				TEXT("Multiple-label depth cell should resolve both labels")));
			const asCByteInstruction* FirstJump = FindOpcode(*Fixture.ByteCode, asBC_JZ);
			const asCByteInstruction* SecondJump = FindOpcode(*Fixture.ByteCode, asBC_JNZ);
			ASSERT_THAT(IsNotNull(FirstJump, TEXT("Multiple-label depth cell should retain JZ")));
			ASSERT_THAT(IsNotNull(SecondJump, TEXT("Multiple-label depth cell should retain JNZ")));
			const asCByteInstruction* Payload =
				FindOpcode(*Fixture.ByteCode, asBC_PshC4);
			ASSERT_THAT(IsNotNull(
				Payload,
				TEXT("Multiple-label depth cell should retain its payload instructions")));
			const int FirstOffset = FirstJump != nullptr ? *reinterpret_cast<const int*>(&FirstJump->arg) : 0;
			const int SecondOffset = SecondJump != nullptr ? *reinterpret_cast<const int*>(&SecondJump->arg) : 0;
			const int PayloadSize =
				GetInstructionSize(Payload);
			ASSERT_THAT(AreEqual(
				2 * PayloadSize,
				FirstOffset,
				TEXT("First independent label should resolve across two payload instructions")));
			ASSERT_THAT(AreEqual(
				PayloadSize,
				SecondOffset,
				TEXT("Second independent label should resolve across one payload instruction")));
		}

		{
			FBytecodeFixture Fixture("BytecodeJumpDepthMissingRepair");
			if (!Fixture.IsValid(*TestRunner))
			{
				return;
			}

			Fixture.ByteCode->InstrDWORD(asBC_JMP, 99);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 50);
			const int SizeBeforeFailure = Fixture.ByteCode->GetSize();

			ASSERT_THAT(IsTrue(
				Fixture.ByteCode->ResolveJumpAddresses() < 0,
				TEXT("Missing-label depth cell should reject unresolved label 99")));
			ASSERT_THAT(AreEqual(
				SizeBeforeFailure,
				Fixture.ByteCode->GetSize(),
				TEXT("Missing-label rejection should preserve the linked instruction size")));
			const asCByteInstruction* JumpBeforeRepair = FindOpcode(*Fixture.ByteCode, asBC_JMP);
			ASSERT_THAT(IsNotNull(JumpBeforeRepair, TEXT("Missing-label rejection should retain JMP")));
			ASSERT_THAT(AreEqual(
				99,
				JumpBeforeRepair != nullptr ? static_cast<int32>(JumpBeforeRepair->arg) : INDEX_NONE,
				TEXT("Missing-label rejection should not partially rewrite the unresolved label id")));

			Fixture.ByteCode->Label(99);
			ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
				TEXT("Missing-label depth cell should resolve after appending the exact repair label")));
			const asCByteInstruction* JumpAfterRepair = FindOpcode(*Fixture.ByteCode, asBC_JMP);
			const asCByteInstruction* Payload =
				FindOpcode(*Fixture.ByteCode, asBC_PshC4);
			ASSERT_THAT(AreEqual(
				GetInstructionSize(Payload),
				JumpAfterRepair != nullptr
					? *reinterpret_cast<const int*>(&JumpAfterRepair->arg)
					: 0,
				TEXT("Repaired missing-label jump should publish the exact payload-sized offset")));
		}

		{
			FBytecodeFixture Fixture("BytecodeJumpDepthAppended");
			if (!Fixture.IsValid(*TestRunner))
			{
				return;
			}

			asCByteCode Tail(Fixture.Builder);
			Fixture.ByteCode->InstrDWORD(asBC_JMP, 5);
			Fixture.ByteCode->InstrDWORD(asBC_PshC4, 60);
			Tail.InstrDWORD(asBC_PshC4, 70);
			Tail.Label(5);
			const int HeadSize = Fixture.ByteCode->GetSize();
			const int TailSize = Tail.GetSize();

			Fixture.ByteCode->AddCode(&Tail);
			ASSERT_THAT(AreEqual(
				HeadSize + TailSize,
				Fixture.ByteCode->GetSize(),
				TEXT("Appended-sequence depth cell should retain both serialized payloads")));
			ASSERT_THAT(AreEqual(0, Tail.GetSize(),
				TEXT("AddCode should transfer the tail instructions into the owning sequence")));
			ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
				TEXT("Appended-sequence target should resolve after ownership transfer")));
			const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
			const asCByteInstruction* FirstPayload =
				FindOpcode(*Fixture.ByteCode, asBC_PshC4);
			ASSERT_THAT(IsNotNull(
				FirstPayload,
				TEXT("Appended-sequence depth cell should retain transferred payloads")));
			ASSERT_THAT(AreEqual(
				2 * GetInstructionSize(FirstPayload),
				Jump != nullptr
					? *reinterpret_cast<const int*>(&Jump->arg)
					: 0,
				TEXT("Appended-sequence jump should resolve across both payload instructions")));
			ASSERT_THAT(AreEqual(
				Fixture.ByteCode->GetSize(),
				EmitToBuffer(*Fixture.ByteCode).Num(),
				TEXT("Appended-sequence depth cell should preserve serialized-size parity")));
		}
	}

	TEST_METHOD(ForwardJumpResolves)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained representative forward-jump smoke; COMPILER-BYTECODE-JUMP-RESOLUTION owns signed direction, topology, failure repair, appended composition, and serialization.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpForward");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->Label(1);
		const int ResolveResult = Fixture.ByteCode->ResolveJumpAddresses();

		const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
		ASSERT_THAT(AreEqual(0, ResolveResult, TEXT("Forward jump should resolve")));
		ASSERT_THAT(IsNotNull(Jump, TEXT("Forward jump instruction should remain present")));

		ASSERT_THAT(IsTrue(static_cast<int32>(*reinterpret_cast<const int*>(&Jump->arg)) > 0,
			TEXT("Forward jump should be rewritten from label id to a positive relative offset")));
	}

	TEST_METHOD(BackwardJumpResolves)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained representative backward-jump smoke; COMPILER-BYTECODE-JUMP-RESOLUTION owns exact signed direction and the complete topology/recovery contract.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpBackward");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Label(1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
		const int ResolveResult = Fixture.ByteCode->ResolveJumpAddresses();

		const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
		ASSERT_THAT(AreEqual(0, ResolveResult, TEXT("Backward jump should resolve")));
		ASSERT_THAT(IsNotNull(Jump, TEXT("Backward jump instruction should remain present")));

		ASSERT_THAT(IsTrue(*reinterpret_cast<const int*>(&Jump->arg) < 0,
			TEXT("Backward jump should be rewritten to a negative relative offset")));
	}

	TEST_METHOD(MultipleLabelsResolveIndependently)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained representative multi-label smoke; COMPILER-BYTECODE-JUMP-RESOLUTION owns distinct rewritten offsets, topology, and serialization.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpMultiple");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JZ, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->Label(1);
		Fixture.ByteCode->InstrDWORD(asBC_JNZ, 2);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 20);
		Fixture.ByteCode->Label(2);

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
			TEXT("Multiple label jumps should resolve together")));
		ASSERT_THAT(IsNotNull(FindOpcode(*Fixture.ByteCode, asBC_JZ),
			TEXT("Resolved sequence should still contain JZ")));
		ASSERT_THAT(IsNotNull(FindOpcode(*Fixture.ByteCode, asBC_JNZ),
			TEXT("Resolved sequence should still contain JNZ")));
	}

	TEST_METHOD(JumpToUnresolvedLabelReturnsError)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained unresolved-label return-code smoke; COMPILER-BYTECODE-JUMP-RESOLUTION owns preserved failure state and same-fixture repair.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpUnresolved");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JMP, 99);
		Fixture.ByteCode->Instr(asBC_RET);

		ASSERT_THAT(IsTrue(Fixture.ByteCode->ResolveJumpAddresses() < 0,
			TEXT("Jump to an unknown label should fail resolution")));
	}

	TEST_METHOD(JumpAcrossAddedSequences)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained appended-sequence smoke; COMPILER-BYTECODE-JUMP-RESOLUTION owns exact transferred size, target direction, serialization, and tail ownership.");

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpAddedSequences");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		asCByteCode Tail(Fixture.Builder);
		Fixture.ByteCode->InstrDWORD(asBC_JMP, 5);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Tail.InstrDWORD(asBC_PshC4, 2);
		Tail.Label(5);

		const int InitialSize = Fixture.ByteCode->GetSize();
		Fixture.ByteCode->AddCode(&Tail);

		ASSERT_THAT(IsTrue(Fixture.ByteCode->GetSize() > InitialSize,
			TEXT("AddCode should append the tail sequence before jump resolution")));
		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
			TEXT("Jump target in an appended sequence should resolve")));
	}
};

#endif
