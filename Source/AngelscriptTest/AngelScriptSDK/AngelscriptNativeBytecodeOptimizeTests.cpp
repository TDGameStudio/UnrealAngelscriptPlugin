#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private
{
	struct FBytecodeFixture
	{
		explicit FBytecodeFixture(const char* ModuleName)
		{
			Engine = AngelscriptNativeTestSupport::CreateBareSdkEngine();
			if (Engine != nullptr)
			{
				Engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1);
			}

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
			return Test.TestNotNull(TEXT("Bytecode optimize fixture should create a bare engine"), Engine)
				&& Test.TestNotNull(TEXT("Bytecode optimize fixture should create a module"), Module)
				&& Test.TestNotNull(TEXT("Bytecode optimize fixture should create a builder"), Builder)
				&& Test.TestNotNull(TEXT("Bytecode optimize fixture should create bytecode"), ByteCode);
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

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeBytecodeOptimizeTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode.Optimize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OptimizeReducesOrPreservesSize)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeSize");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeHeadTail");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeOutputSize");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeOutputStable");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeAppendOutput");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeEmpty");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeLastValue");
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
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeOptimize_Private;
		FBytecodeFixture Fixture("BytecodeOptimizeLastRet");
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
