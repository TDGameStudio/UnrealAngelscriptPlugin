#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_BytecodeJumps_Private
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
			return Test.TestNotNull(TEXT("Bytecode jump fixture should create a bare engine"), Engine)
				&& Test.TestNotNull(TEXT("Bytecode jump fixture should create a module"), Module)
				&& Test.TestNotNull(TEXT("Bytecode jump fixture should create a builder"), Builder)
				&& Test.TestNotNull(TEXT("Bytecode jump fixture should create bytecode"), ByteCode);
		}

		asCScriptEngine* Engine = nullptr;
		asCModule* Module = nullptr;
		asCBuilder* Builder = nullptr;
		asCByteCode* ByteCode = nullptr;
	};

	const asCByteInstruction* FindOpcode(const asCByteCode& ByteCode, const asEBCInstr Opcode)
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeBytecodeJumpsTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode.Jumps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ForwardJumpResolves)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeJumps_Private;
		FBytecodeFixture Fixture("BytecodeJumpForward");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->Label(1);
		const int ResolveResult = Fixture.ByteCode->ResolveJumpAddresses();

		const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
		if (!TestRunner->TestEqual(TEXT("Forward jump should resolve"), ResolveResult, 0) || !TestRunner->TestNotNull(TEXT("Forward jump instruction should remain present"), Jump))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Forward jump should be rewritten from label id to a positive relative offset"), static_cast<int32>(*reinterpret_cast<const int*>(&Jump->arg)) > 0);
	}

	TEST_METHOD(BackwardJumpResolves)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeJumps_Private;
		FBytecodeFixture Fixture("BytecodeJumpBackward");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Label(1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
		const int ResolveResult = Fixture.ByteCode->ResolveJumpAddresses();

		const asCByteInstruction* Jump = FindOpcode(*Fixture.ByteCode, asBC_JMP);
		if (!TestRunner->TestEqual(TEXT("Backward jump should resolve"), ResolveResult, 0) || !TestRunner->TestNotNull(TEXT("Backward jump instruction should remain present"), Jump))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Backward jump should be rewritten to a negative relative offset"), *reinterpret_cast<const int*>(&Jump->arg) < 0);
	}

	TEST_METHOD(MultipleLabelsResolveIndependently)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeJumps_Private;
		FBytecodeFixture Fixture("BytecodeJumpMultiple");
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

		TestRunner->TestEqual(TEXT("Multiple label jumps should resolve together"), Fixture.ByteCode->ResolveJumpAddresses(), 0);
		TestRunner->TestNotNull(TEXT("Resolved sequence should still contain JZ"), FindOpcode(*Fixture.ByteCode, asBC_JZ));
		TestRunner->TestNotNull(TEXT("Resolved sequence should still contain JNZ"), FindOpcode(*Fixture.ByteCode, asBC_JNZ));
	}

	TEST_METHOD(JumpToUnresolvedLabelReturnsError)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeJumps_Private;
		FBytecodeFixture Fixture("BytecodeJumpUnresolved");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JMP, 99);
		Fixture.ByteCode->Instr(asBC_RET);

		TestRunner->TestTrue(TEXT("Jump to an unknown label should fail resolution"), Fixture.ByteCode->ResolveJumpAddresses() < 0);
	}

	TEST_METHOD(JumpAcrossAddedSequences)
	{
		using namespace AngelscriptTest_AngelScriptSDK_BytecodeJumps_Private;
		FBytecodeFixture Fixture("BytecodeJumpAddedSequences");
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

		TestRunner->TestTrue(TEXT("AddCode should append the tail sequence before jump resolution"), Fixture.ByteCode->GetSize() > InitialSize);
		TestRunner->TestEqual(TEXT("Jump target in an appended sequence should resolve"), Fixture.ByteCode->ResolveJumpAddresses(), 0);
	}
};

#endif
