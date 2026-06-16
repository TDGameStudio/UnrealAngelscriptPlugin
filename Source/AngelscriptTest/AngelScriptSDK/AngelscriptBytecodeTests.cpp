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
		if (!TestRunner->TestNotNull(TEXT("Bytecode instruction test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCByteCode ByteCode(&Builder);
		ByteCode.InstrDWORD(asBC_PshC4, 42);
		ByteCode.Instr(asBC_RET);

		TestRunner->TestTrue(TEXT("Bytecode should contain at least one dword after emitting instructions"), ByteCode.GetSize() > 0);
		TestRunner->TestNotNull(TEXT("Bytecode should expose the first instruction"), ByteCode.GetFirstInstr());
		TestRunner->TestEqual(TEXT("First emitted opcode should match asBC_PshC4"), static_cast<int32>(ByteCode.GetFirstInstr()->op), static_cast<int32>(asBC_PshC4));
		TestRunner->TestEqual(TEXT("Last emitted opcode should match asBC_RET"), ByteCode.GetLastInstr(), static_cast<int32>(asBC_RET));
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
		if (!TestRunner->TestNotNull(TEXT("Bytecode append test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCByteCode First(&Builder);
		asCByteCode Second(&Builder);
		First.InstrDWORD(asBC_PshC4, 10);
		Second.InstrDWORD(asBC_PshC4, 20);

		const int32 InitialSize = First.GetSize();
		First.AddCode(&Second);

		TestRunner->TestTrue(TEXT("AddCode should append the second sequence to the first one"), First.GetSize() > InitialSize);
		TestRunner->TestEqual(TEXT("The last dword payload should come from the appended sequence"), static_cast<int32>(First.GetLastInstrValueDW()), 20);
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
		if (!TestRunner->TestNotNull(TEXT("Bytecode jump test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCByteCode ByteCode(&Builder);
		ByteCode.InstrDWORD(asBC_JMP, 1);
		ByteCode.Label(1);

		TestRunner->TestEqual(TEXT("ResolveJumpAddresses should resolve a forward label jump"), ByteCode.ResolveJumpAddresses(), 0);
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
		if (!TestRunner->TestNotNull(TEXT("Bytecode output test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCByteCode ByteCode(&Builder);
		ByteCode.InstrDWORD(asBC_PshC4, 42);

		TArray<asDWORD> Buffer;
		Buffer.SetNumZeroed(ByteCode.GetSize());
		ByteCode.Output(Buffer.GetData());

		TestRunner->TestEqual(TEXT("Output should preserve the opcode in the first emitted dword"), static_cast<int32>(*reinterpret_cast<asBYTE*>(&Buffer[0])), static_cast<int32>(asBC_PshC4));
		TestRunner->TestEqual(TEXT("Output should preserve the dword payload for asBC_PshC4"), static_cast<int32>(Buffer[1]), 42);
		}
	}
};

#endif
