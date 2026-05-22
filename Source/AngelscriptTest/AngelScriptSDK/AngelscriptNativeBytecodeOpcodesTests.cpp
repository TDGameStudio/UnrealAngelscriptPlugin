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

namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private
{
	struct FBytecodeFixture
	{
		explicit FBytecodeFixture(const char* ModuleName)
		{
			Engine = AngelscriptNativeTestSupport::CreateBareSdkEngine();
			Module = Engine != nullptr ? static_cast<asCModule*>(Engine->GetModule(ModuleName, asGM_ALWAYS_CREATE)) : nullptr;
			Builder = Module != nullptr ? new asCBuilder(Engine, Module) : nullptr;
			ByteCode = Builder != nullptr ? new asCByteCode(Builder) : nullptr;
		}

		~FBytecodeFixture()
		{
			delete ByteCode;
			delete Builder;
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Bytecode opcode fixture should create a bare engine"), Engine)
				&& Test.TestNotNull(TEXT("Bytecode opcode fixture should create a module"), Module)
				&& Test.TestNotNull(TEXT("Bytecode opcode fixture should create a builder"), Builder)
				&& Test.TestNotNull(TEXT("Bytecode opcode fixture should create bytecode"), ByteCode);
		}

		asCScriptEngine* Engine = nullptr;
		asCModule* Module = nullptr;
		asCBuilder* Builder = nullptr;
		asCByteCode* ByteCode = nullptr;
	};

	int32 CountInstructions(asCByteCode& ByteCode)
	{
		int32 Count = 0;
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			++Count;
		}
		return Count;
	}

	bool ContainsOpcode(asCByteCode& ByteCode, const asEBCInstr Opcode)
	{
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			if (Instruction->op == Opcode)
			{
				return true;
			}
		}

		return false;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeBytecodeOpcodesTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode.Opcodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Push_PshC4_PshV4_PshRPtr)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesPush");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);
		Fixture.ByteCode->InstrSHORT(asBC_PshV4, 3);
		Fixture.ByteCode->Instr(asBC_PshRPtr);

		TestRunner->TestTrue(TEXT("Push opcode sequence should include PshC4"), ContainsOpcode(*Fixture.ByteCode, asBC_PshC4));
		TestRunner->TestTrue(TEXT("Push opcode sequence should include PshV4"), ContainsOpcode(*Fixture.ByteCode, asBC_PshV4));
		TestRunner->TestTrue(TEXT("Push opcode sequence should include PshRPtr"), ContainsOpcode(*Fixture.ByteCode, asBC_PshRPtr));
	}

	TEST_METHOD(Load_LoadObj_LoadThisR)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesLoad");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrSHORT(asBC_LOADOBJ, 1);
		Fixture.ByteCode->InstrSHORT(asBC_LoadThisR, 2);
		Fixture.ByteCode->InstrSHORT(asBC_LOADOBJ, 3);

		TestRunner->TestEqual(TEXT("Load opcode sequence should start with LOADOBJ"), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op), static_cast<int32>(asBC_LOADOBJ));
		TestRunner->TestTrue(TEXT("Load opcode sequence should include LoadThisR"), ContainsOpcode(*Fixture.ByteCode, asBC_LoadThisR));
		TestRunner->TestEqual(TEXT("Load opcode sequence should contain three instructions"), CountInstructions(*Fixture.ByteCode), 3);
	}

	TEST_METHOD(Call_CALL_CALLSYS_CALLINTF)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesCall");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_CALL, 7);
		Fixture.ByteCode->InstrDWORD(asBC_CALLINTF, 8);

		TestRunner->TestEqual(TEXT("CALL should use a DWORD argument encoding"), static_cast<int32>(asBCInfo[asBC_CALL].type), static_cast<int32>(asBCTYPE_DW_ARG));
		TestRunner->TestEqual(TEXT("CALLSYS should use a pointer argument encoding"), static_cast<int32>(asBCInfo[asBC_CALLSYS].type), static_cast<int32>(asBCTYPE_PTR_ARG));
		TestRunner->TestEqual(TEXT("CALLINTF should use a DWORD argument encoding"), static_cast<int32>(asBCInfo[asBC_CALLINTF].type), static_cast<int32>(asBCTYPE_DW_ARG));
		TestRunner->TestTrue(TEXT("Emitted call bytecode should include CALL"), ContainsOpcode(*Fixture.ByteCode, asBC_CALL));
		TestRunner->TestTrue(TEXT("Emitted call bytecode should include CALLINTF"), ContainsOpcode(*Fixture.ByteCode, asBC_CALLINTF));
	}

	TEST_METHOD(BranchOps_JZ_JNZ_JLowZ_JLowNZ)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesBranch");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JZ, 1);
		Fixture.ByteCode->InstrDWORD(asBC_JNZ, 2);
		Fixture.ByteCode->InstrDWORD(asBC_JLowZ, 3);
		Fixture.ByteCode->InstrDWORD(asBC_JLowNZ, 4);

		TestRunner->TestEqual(TEXT("JZ should use a DWORD label operand"), static_cast<int32>(asBCInfo[asBC_JZ].type), static_cast<int32>(asBCTYPE_DW_ARG));
		TestRunner->TestEqual(TEXT("JNZ should use a DWORD label operand"), static_cast<int32>(asBCInfo[asBC_JNZ].type), static_cast<int32>(asBCTYPE_DW_ARG));
		TestRunner->TestEqual(TEXT("JLowZ should use a DWORD label operand"), static_cast<int32>(asBCInfo[asBC_JLowZ].type), static_cast<int32>(asBCTYPE_DW_ARG));
		TestRunner->TestEqual(TEXT("JLowNZ should use a DWORD label operand"), static_cast<int32>(asBCInfo[asBC_JLowNZ].type), static_cast<int32>(asBCTYPE_DW_ARG));
		TestRunner->TestEqual(TEXT("Branch sequence should contain four instructions"), CountInstructions(*Fixture.ByteCode), 4);
	}

	TEST_METHOD(Misc_LINE_SUSPEND_JitEntry)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesMisc");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Line(12, 3, 0);
		Fixture.ByteCode->Instr(asBC_SUSPEND);
		Fixture.ByteCode->InstrPTR(asBC_JitEntry, nullptr);

		TestRunner->TestTrue(TEXT("Line helper should emit a LINE instruction"), ContainsOpcode(*Fixture.ByteCode, asBC_LINE));
		TestRunner->TestTrue(TEXT("Line helper or explicit emit should include JitEntry"), ContainsOpcode(*Fixture.ByteCode, asBC_JitEntry));
		TestRunner->TestTrue(TEXT("Misc sequence should include SUSPEND"), ContainsOpcode(*Fixture.ByteCode, asBC_SUSPEND));
	}

	TEST_METHOD(RetVariants_RET_RetWithValue)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesRet");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Ret(0);
		Fixture.ByteCode->Ret(2);

		TestRunner->TestEqual(TEXT("RET should use a word argument encoding"), static_cast<int32>(asBCInfo[asBC_RET].type), static_cast<int32>(asBCTYPE_W_ARG));
		TestRunner->TestEqual(TEXT("Two Ret helper calls should emit two RET instructions"), CountInstructions(*Fixture.ByteCode), 2);
		TestRunner->TestEqual(TEXT("Last emitted RET should preserve the pop count"), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->next->wArg[0]), 2);
	}

	TEST_METHOD(MathOps_AddInt_SubInt_MulInt_Float)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesMath");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrW_W_W(asBC_ADDi, 0, 1, 2);
		Fixture.ByteCode->InstrW_W_W(asBC_SUBi, 1, 2, 3);
		Fixture.ByteCode->InstrW_W_W(asBC_MULi, 2, 3, 4);
		Fixture.ByteCode->InstrW_W_W(asBC_ADDf, 3, 4, 5);

		TestRunner->TestTrue(TEXT("Math sequence should include integer add"), ContainsOpcode(*Fixture.ByteCode, asBC_ADDi));
		TestRunner->TestTrue(TEXT("Math sequence should include integer subtract"), ContainsOpcode(*Fixture.ByteCode, asBC_SUBi));
		TestRunner->TestTrue(TEXT("Math sequence should include integer multiply"), ContainsOpcode(*Fixture.ByteCode, asBC_MULi));
		TestRunner->TestTrue(TEXT("Math sequence should include float add"), ContainsOpcode(*Fixture.ByteCode, asBC_ADDf));
	}

	TEST_METHOD(CompareOps_CMPi_CMPf)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesCompare");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrW_W(asBC_CMPi, 1, 2);
		Fixture.ByteCode->InstrW_W(asBC_CMPf, 3, 4);

		TestRunner->TestEqual(TEXT("CMPi should use the two-register encoding"), static_cast<int32>(asBCInfo[asBC_CMPi].type), static_cast<int32>(asBCTYPE_rW_rW_ARG));
		TestRunner->TestEqual(TEXT("CMPf should use the two-register encoding"), static_cast<int32>(asBCInfo[asBC_CMPf].type), static_cast<int32>(asBCTYPE_rW_rW_ARG));
		TestRunner->TestTrue(TEXT("Compare sequence should include CMPi"), ContainsOpcode(*Fixture.ByteCode, asBC_CMPi));
		TestRunner->TestTrue(TEXT("Compare sequence should include CMPf"), ContainsOpcode(*Fixture.ByteCode, asBC_CMPf));
	}

	TEST_METHOD(InstrSizeMatchesInfoTable)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOpcodes_Private;
		FBytecodeFixture Fixture("BytecodeOpcodesSize");
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
		TestRunner->TestEqual(TEXT("Instruction sizes should match the bytecode info table when serialized"), Buffer.Num(), ExpectedDwordCount);
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

		TestRunner->TestTrue(TEXT("Opcode info table should cover many concrete asEBCType buckets"), CoveredTypes >= 18);
		TestRunner->TestTrue(TEXT("Opcode info table should include no-arg opcodes"), SeenTypes.Contains(static_cast<int32>(asBCTYPE_NO_ARG)));
		TestRunner->TestTrue(TEXT("Opcode info table should include dword-arg opcodes"), SeenTypes.Contains(static_cast<int32>(asBCTYPE_DW_ARG)));
		TestRunner->TestTrue(TEXT("Opcode info table should include qword-arg opcodes"), SeenTypes.Contains(static_cast<int32>(asBCTYPE_QW_ARG)));
	}
};

#endif
