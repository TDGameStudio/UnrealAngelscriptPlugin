#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	asCModule* CreateBytecodeModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBytecodeTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InstructionSequence)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateBytecodeModule(BareEngine, "BytecodeInstructionSequence");
		ASSERT_THAT(IsNotNull(Module, TEXT("Bytecode instruction test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCByteCode ByteCode(&Builder);
		ByteCode.InstrDWORD(asBC_PshC4, 42);
		ByteCode.Instr(asBC_RET);

		ASSERT_THAT(IsTrue(ByteCode.GetSize() > 0,
			TEXT("Bytecode should contain at least one dword after emitting instructions")));
		ASSERT_THAT(IsNotNull(ByteCode.GetFirstInstr(), TEXT("Bytecode should expose the first instruction")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(ByteCode.GetFirstInstr()->op),
			TEXT("First emitted opcode should match asBC_PshC4")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), ByteCode.GetLastInstr(),
			TEXT("Last emitted opcode should match asBC_RET")));
		}
	}

	TEST_METHOD(Append)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateBytecodeModule(BareEngine, "BytecodeAppend");
		ASSERT_THAT(IsNotNull(Module, TEXT("Bytecode append test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCByteCode First(&Builder);
		asCByteCode Second(&Builder);
		First.InstrDWORD(asBC_PshC4, 10);
		Second.InstrDWORD(asBC_PshC4, 20);

		const int32 InitialSize = First.GetSize();
		First.AddCode(&Second);

		ASSERT_THAT(IsTrue(First.GetSize() > InitialSize,
			TEXT("AddCode should append the second sequence to the first one")));
		ASSERT_THAT(AreEqual(20, static_cast<int32>(First.GetLastInstrValueDW()),
			TEXT("The last dword payload should come from the appended sequence")));
		}
	}

	TEST_METHOD(JumpResolution)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateBytecodeModule(BareEngine, "BytecodeJumpResolution");
		ASSERT_THAT(IsNotNull(Module, TEXT("Bytecode jump test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCByteCode ByteCode(&Builder);
		ByteCode.InstrDWORD(asBC_JMP, 1);
		ByteCode.Label(1);

		ASSERT_THAT(AreEqual(0, ByteCode.ResolveJumpAddresses(),
			TEXT("ResolveJumpAddresses should resolve a forward label jump")));
		}
	}

	TEST_METHOD(Output)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateBytecodeModule(BareEngine, "BytecodeOutput");
		ASSERT_THAT(IsNotNull(Module, TEXT("Bytecode output test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCByteCode ByteCode(&Builder);
		ByteCode.InstrDWORD(asBC_PshC4, 42);

		TArray<asDWORD> Buffer;
		Buffer.SetNumZeroed(ByteCode.GetSize());
		ByteCode.Output(Buffer.GetData());

		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(*reinterpret_cast<asBYTE*>(&Buffer[0])),
			TEXT("Output should preserve the opcode in the first emitted dword")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Buffer[1]),
			TEXT("Output should preserve the dword payload for asBC_PshC4")));
		}
	}
};

#endif
