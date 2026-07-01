#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeBytecodeOptimizeTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode.Optimize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OptimizeReducesOrPreservesSize)
	{
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

#endif
