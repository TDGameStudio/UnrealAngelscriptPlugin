#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeBytecodeOptimizeTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode.Optimize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OptimizeReducesOrPreservesSize)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeSize", true);
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

		TestRunner->TestTrue(TEXT("Optimize should reduce or preserve bytecode size"), AfterSize <= BeforeSize);
	}

	TEST_METHOD(OptimizeKeepsSemanticHeadAndTail)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeHeadTail", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);
		Fixture.ByteCode->Ret(0);
		Fixture.ByteCode->Optimize();

		TestRunner->TestEqual(TEXT("Optimize should keep the first semantic opcode"), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op), static_cast<int32>(asBC_PshC4));
		TestRunner->TestEqual(TEXT("Optimize should keep the tail RET opcode"), Fixture.ByteCode->GetLastInstr(), static_cast<int32>(asBC_RET));
	}

	TEST_METHOD(OutputBufferSizeMatchesGetSize)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeOutputSize", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 11);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 22);
		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);

		TestRunner->TestEqual(TEXT("Output buffer should have one dword per GetSize unit"), Buffer.Num(), Fixture.ByteCode->GetSize());
		TestRunner->TestTrue(TEXT("Output buffer should not be empty for emitted bytecode"), Buffer.Num() > 0);
	}

	TEST_METHOD(OutputBufferRoundTripStable)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeOutputStable", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 123);
		Fixture.ByteCode->Ret(0);
		const TArray<asDWORD> FirstBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		const TArray<asDWORD> SecondBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);

		TestRunner->TestEqual(TEXT("Repeated Output calls should produce the same buffer size"), SecondBuffer.Num(), FirstBuffer.Num());
		TestRunner->TestTrue(TEXT("Repeated Output calls should produce byte-for-byte stable buffers"), FirstBuffer == SecondBuffer);
	}

	TEST_METHOD(OutputAfterAppendIsContiguous)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeAppendOutput", true);
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
		TestRunner->TestEqual(TEXT("Appended output should equal head plus tail sizes"), Buffer.Num(), HeadSize + TailSize);
		TestRunner->TestEqual(TEXT("Appended output should keep the second payload after the second opcode"), static_cast<int32>(Buffer[HeadSize + 1]), 2);
	}

	TEST_METHOD(EmptyByteCodeGetSizeIsZero)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeEmpty", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Fresh bytecode should have zero emitted size"), Fixture.ByteCode->GetSize(), 0);
		TestRunner->TestEqual(TEXT("Fresh bytecode output helper should return an empty buffer"), AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode).Num(), 0);
		TestRunner->TestEqual(TEXT("Fresh bytecode should report no last instruction"), Fixture.ByteCode->GetLastInstr(), -1);
	}

	TEST_METHOD(LastInstrValueDwAfterMixedOps)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeLastValue", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 7);
		Fixture.ByteCode->InstrDWORD(asBC_JMP, 12);

		TestRunner->TestEqual(TEXT("GetLastInstr should report the final DWORD opcode"), Fixture.ByteCode->GetLastInstr(), static_cast<int32>(asBC_JMP));
		TestRunner->TestEqual(TEXT("GetLastInstrValueDW should read the final DWORD payload"), static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()), 12);
	}

	TEST_METHOD(GetLastInstrTypeAfterRet)
	{
		FBytecodeFixture Fixture("BytecodeOptimizeLastRet", true);
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 7);
		Fixture.ByteCode->Ret(0);

		TestRunner->TestEqual(TEXT("GetLastInstr should report RET after Ret helper"), Fixture.ByteCode->GetLastInstr(), static_cast<int32>(asBC_RET));
		TestRunner->TestTrue(TEXT("Bytecode should still contain the earlier PshC4"), ContainsOpcode(*Fixture.ByteCode, asBC_PshC4));
		TestRunner->TestEqual(TEXT("Ret helper should add exactly two instructions in this sequence"), CountInstructions(*Fixture.ByteCode), 2);
	}
};

#endif
