#include "../Support/AngelscriptNativeCoreTestSupport.h"

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

public:
	TEST_METHOD(ForwardJumpResolves)
	{
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
