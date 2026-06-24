#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeBytecodeOpcodesTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode.Opcodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Push_PshC4_PshV4_PshRPtr)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesPush");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);
		Fixture.ByteCode->InstrSHORT(asBC_PshV4, 3);
		Fixture.ByteCode->Instr(asBC_PshRPtr);

		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_PshC4),
			TEXT("Push opcode sequence should include PshC4")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_PshV4),
			TEXT("Push opcode sequence should include PshV4")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_PshRPtr),
			TEXT("Push opcode sequence should include PshRPtr")));
	}

	TEST_METHOD(Load_LoadObj_LoadThisR)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesLoad");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrSHORT(asBC_LOADOBJ, 1);
		Fixture.ByteCode->InstrSHORT(asBC_LoadThisR, 2);
		Fixture.ByteCode->InstrSHORT(asBC_LOADOBJ, 3);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_LOADOBJ), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
			TEXT("Load opcode sequence should start with LOADOBJ")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_LoadThisR),
			TEXT("Load opcode sequence should include LoadThisR")));
		ASSERT_THAT(AreEqual(3, CountInstructions(*Fixture.ByteCode),
			TEXT("Load opcode sequence should contain three instructions")));
	}

	TEST_METHOD(Call_CALL_CALLSYS_CALLINTF)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesCall");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_CALL, 7);
		Fixture.ByteCode->InstrDWORD(asBC_CALLINTF, 8);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_DW_ARG), static_cast<int32>(asBCInfo[asBC_CALL].type),
			TEXT("CALL should use a DWORD argument encoding")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_PTR_ARG), static_cast<int32>(asBCInfo[asBC_CALLSYS].type),
			TEXT("CALLSYS should use a pointer argument encoding")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_DW_ARG), static_cast<int32>(asBCInfo[asBC_CALLINTF].type),
			TEXT("CALLINTF should use a DWORD argument encoding")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_CALL),
			TEXT("Emitted call bytecode should include CALL")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_CALLINTF),
			TEXT("Emitted call bytecode should include CALLINTF")));
	}

	TEST_METHOD(BranchOps_JZ_JNZ_JLowZ_JLowNZ)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesBranch");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JZ, 1);
		Fixture.ByteCode->InstrDWORD(asBC_JNZ, 2);
		Fixture.ByteCode->InstrDWORD(asBC_JLowZ, 3);
		Fixture.ByteCode->InstrDWORD(asBC_JLowNZ, 4);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_DW_ARG), static_cast<int32>(asBCInfo[asBC_JZ].type),
			TEXT("JZ should use a DWORD label operand")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_DW_ARG), static_cast<int32>(asBCInfo[asBC_JNZ].type),
			TEXT("JNZ should use a DWORD label operand")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_DW_ARG), static_cast<int32>(asBCInfo[asBC_JLowZ].type),
			TEXT("JLowZ should use a DWORD label operand")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_DW_ARG), static_cast<int32>(asBCInfo[asBC_JLowNZ].type),
			TEXT("JLowNZ should use a DWORD label operand")));
		ASSERT_THAT(AreEqual(4, CountInstructions(*Fixture.ByteCode),
			TEXT("Branch sequence should contain four instructions")));
	}

	TEST_METHOD(Misc_LINE_SUSPEND_JitEntry)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesMisc");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Line(12, 3, 0);
		Fixture.ByteCode->Instr(asBC_SUSPEND);
		Fixture.ByteCode->InstrPTR(asBC_JitEntry, nullptr);

		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_LINE),
			TEXT("Line helper should emit a LINE instruction")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_JitEntry),
			TEXT("Line helper or explicit emit should include JitEntry")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_SUSPEND),
			TEXT("Misc sequence should include SUSPEND")));
	}

	TEST_METHOD(RetVariants_RET_RetWithValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesRet");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Ret(0);
		Fixture.ByteCode->Ret(2);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_W_ARG), static_cast<int32>(asBCInfo[asBC_RET].type),
			TEXT("RET should use a word argument encoding")));
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("Two Ret helper calls should emit two RET instructions")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->next->wArg[0]),
			TEXT("Last emitted RET should preserve the pop count")));
	}

	TEST_METHOD(MathOps_AddInt_SubInt_MulInt_Float)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesMath");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrW_W_W(asBC_ADDi, 0, 1, 2);
		Fixture.ByteCode->InstrW_W_W(asBC_SUBi, 1, 2, 3);
		Fixture.ByteCode->InstrW_W_W(asBC_MULi, 2, 3, 4);
		Fixture.ByteCode->InstrW_W_W(asBC_ADDf, 3, 4, 5);

		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_ADDi),
			TEXT("Math sequence should include integer add")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_SUBi),
			TEXT("Math sequence should include integer subtract")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_MULi),
			TEXT("Math sequence should include integer multiply")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_ADDf),
			TEXT("Math sequence should include float add")));
	}

	TEST_METHOD(CompareOps_CMPi_CMPf)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesCompare");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrW_W(asBC_CMPi, 1, 2);
		Fixture.ByteCode->InstrW_W(asBC_CMPf, 3, 4);

		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_rW_rW_ARG), static_cast<int32>(asBCInfo[asBC_CMPi].type),
			TEXT("CMPi should use the two-register encoding")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBCTYPE_rW_rW_ARG), static_cast<int32>(asBCInfo[asBC_CMPf].type),
			TEXT("CMPf should use the two-register encoding")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_CMPi),
			TEXT("Compare sequence should include CMPi")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_CMPf),
			TEXT("Compare sequence should include CMPf")));
	}

	TEST_METHOD(InstrSizeMatchesInfoTable)
	{
		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOpcodesSize");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Fixture.ByteCode->InstrSHORT(asBC_PshV4, 2);
		Fixture.ByteCode->InstrW_W_W(asBC_ADDi, 0, 1, 2);
		Fixture.ByteCode->Instr(asBC_PshRPtr);

		int32 ExpectedDwordCount = 0;
		for (const asCByteInstruction* Instruction = Fixture.ByteCode->GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			ExpectedDwordCount += asBCTypeSize[asBCInfo[Instruction->op].type];
		}

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(ExpectedDwordCount, Buffer.Num(),
			TEXT("Instruction sizes should match the bytecode info table when serialized")));
	}

	TEST_METHOD(OpcodeCountsAcrossEachAsEBCType)
	{
		int32 CoveredTypes = 0;
		TSet<int32> SeenTypes;
		for (int32 OpcodeIndex = 0; OpcodeIndex <= static_cast<int32>(asBC_MAXBYTECODE); ++OpcodeIndex)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(OpcodeIndex);
			if (asBCInfo[Opcode].type != asBCTYPE_INFO)
			{
				SeenTypes.Add(static_cast<int32>(asBCInfo[Opcode].type));
			}
		}

		for (int32 TypeIndex = static_cast<int32>(asBCTYPE_NO_ARG); TypeIndex <= static_cast<int32>(asBCTYPE_W_rW_ARG); ++TypeIndex)
		{
			if (SeenTypes.Contains(TypeIndex))
			{
				++CoveredTypes;
			}
		}

		ASSERT_THAT(IsTrue(CoveredTypes >= 18,
			TEXT("Opcode info table should cover many concrete asEBCType buckets")));
		ASSERT_THAT(IsTrue(SeenTypes.Contains(static_cast<int32>(asBCTYPE_NO_ARG)),
			TEXT("Opcode info table should include no-arg opcodes")));
		ASSERT_THAT(IsTrue(SeenTypes.Contains(static_cast<int32>(asBCTYPE_DW_ARG)),
			TEXT("Opcode info table should include dword-arg opcodes")));
		ASSERT_THAT(IsTrue(SeenTypes.Contains(static_cast<int32>(asBCTYPE_QW_ARG)),
			TEXT("Opcode info table should include qword-arg opcodes")));
	}
};

#endif
