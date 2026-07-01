#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBytecodeTests,
	"Angelscript.TestModule.AngelScriptSDK.Bytecode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asCByteInstruction* FindInstruction(asCByteCode& ByteCode, const asEBCInstr Opcode)
	{
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			if (Instruction->op == Opcode)
			{
				return Instruction;
			}
		}

		return nullptr;
	}

	static int32 ReadOpcodeAt(const TArray<asDWORD>& Buffer, const int32 DwordIndex)
	{
		if (!Buffer.IsValidIndex(DwordIndex))
		{
			return INDEX_NONE;
		}

		return static_cast<int32>(*reinterpret_cast<const asBYTE*>(&Buffer[DwordIndex]));
	}

	static bool BuildAndGetFunctionBytecode(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		const char* Declaration,
		asIScriptModule*& OutModule,
		asIScriptFunction*& OutFunction,
		asDWORD*& OutBytecode,
		asUINT& OutBytecodeLength)
	{
		FNoDiscardAsserter LocalAssert(Test);

		OutModule = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!LocalAssert.IsNotNull(OutModule, TEXT("Bytecode compile test should build the script module")))
		{
			return false;
		}

		OutFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(OutModule, Declaration);
		if (!LocalAssert.IsNotNull(OutFunction, TEXT("Bytecode compile test should resolve the requested function")))
		{
			return false;
		}

		OutBytecode = OutFunction->GetByteCode(&OutBytecodeLength);
		return LocalAssert.IsNotNull(OutBytecode, TEXT("Bytecode compile test should expose a bytecode buffer"))
			&& LocalAssert.IsTrue(OutBytecodeLength > 0, TEXT("Bytecode compile test should emit at least one bytecode dword"));
	}

	static bool ExecuteIntEntry(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptFunction* Function, int32& OutValue)
	{
		FNoDiscardAsserter LocalAssert(Test);

		asIScriptContext* Context = ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (!LocalAssert.IsNotNull(Context, TEXT("Bytecode compile test should create an execution context")))
		{
			return false;
		}

		const int ExecuteResult = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function);
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		return LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Bytecode compile test should execute successfully"));
	}

	static bool BytecodeContainsOpcode(const asDWORD* Bytecode, const asUINT BytecodeLength, const asEBCInstr Opcode)
	{
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr CurrentOpcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (CurrentOpcode == Opcode)
			{
				return true;
			}

			if (static_cast<int32>(CurrentOpcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[CurrentOpcode].type];
			if (InstructionSize <= 0)
			{
				break;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	static bool BytecodeContainsAnyOpcode(const asDWORD* Bytecode, const asUINT BytecodeLength, const TArray<asEBCInstr>& Opcodes)
	{
		for (const asEBCInstr Opcode : Opcodes)
		{
			if (BytecodeContainsOpcode(Bytecode, BytecodeLength, Opcode))
			{
				return true;
			}
		}

		return false;
	}

	static TArray<asDWORD> CopyBytecodeBuffer(const asDWORD* Bytecode, const asUINT BytecodeLength)
	{
		TArray<asDWORD> Result;
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return Result;
		}

		Result.SetNumUninitialized(static_cast<int32>(BytecodeLength));
		FMemory::Memcpy(Result.GetData(), Bytecode, BytecodeLength * sizeof(asDWORD));
		return Result;
	}

public:
	TEST_METHOD(InstructionSequence)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeInstructionSequence");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);
		Fixture.ByteCode->Instr(asBC_RET);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("Bytecode should contain the emitted push and ret instructions")));
		ASSERT_THAT(AreEqual(Buffer.Num(), Fixture.ByteCode->GetSize(),
			TEXT("Serialized bytecode dword count should match GetSize")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
			TEXT("First emitted opcode should match asBC_PshC4")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->arg),
			TEXT("First emitted opcode should keep the dword payload")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("Last emitted opcode should match asBC_RET")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), ReadOpcodeAt(Buffer, 0),
			TEXT("Serialized buffer should start with PshC4")));
	}

	TEST_METHOD(Append)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeAppend");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		asCByteCode Tail(Fixture.Builder);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Tail.InstrDWORD(asBC_PshC4, 20);

		const int32 HeadSize = Fixture.ByteCode->GetSize();
		const int32 TailSize = Tail.GetSize();
		Fixture.ByteCode->AddCode(&Tail);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(HeadSize + TailSize, Fixture.ByteCode->GetSize(),
			TEXT("AddCode should append the tail sequence size exactly")));
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("AddCode should preserve both instructions in order")));
		ASSERT_THAT(AreEqual(10, static_cast<int32>(Buffer[1]),
			TEXT("AddCode should preserve the head payload")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), ReadOpcodeAt(Buffer, HeadSize),
			TEXT("AddCode should place the tail opcode after the head sequence")));
		ASSERT_THAT(AreEqual(20, static_cast<int32>(Buffer[HeadSize + 1]),
			TEXT("AddCode should place the tail payload after the tail opcode")));
		ASSERT_THAT(AreEqual(20, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("The last dword payload should come from the appended sequence")));
	}

	TEST_METHOD(JumpResolution)
	{
		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeJumpResolution");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 10);
		Fixture.ByteCode->Label(1);

		const asCByteInstruction* JumpBeforeResolve = FindInstruction(*Fixture.ByteCode, asBC_JMP);
		ASSERT_THAT(IsNotNull(JumpBeforeResolve, TEXT("Jump resolution test should emit a JMP instruction")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(JumpBeforeResolve->arg),
			TEXT("JMP argument should initially store the label id")));

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
			TEXT("ResolveJumpAddresses should resolve a forward label jump")));

		const asCByteInstruction* JumpAfterResolve = FindInstruction(*Fixture.ByteCode, asBC_JMP);
		ASSERT_THAT(IsNotNull(JumpAfterResolve, TEXT("Resolved bytecode should retain the JMP instruction")));
		ASSERT_THAT(IsTrue(static_cast<int32>(*reinterpret_cast<const int*>(&JumpAfterResolve->arg)) > 0,
			TEXT("ResolveJumpAddresses should rewrite the label id to a positive relative offset")));
	}

	TEST_METHOD(Output)
	{
		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeOutput");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);

		const TArray<asDWORD> FirstBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		const TArray<asDWORD> SecondBuffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);

		ASSERT_THAT(AreEqual(Fixture.ByteCode->GetSize(), FirstBuffer.Num(),
			TEXT("Output should write one dword per GetSize unit")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), ReadOpcodeAt(FirstBuffer, 0),
			TEXT("Output should preserve the opcode in the first emitted dword")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(FirstBuffer[1]),
			TEXT("Output should preserve the dword payload for asBC_PshC4")));
		ASSERT_THAT(IsTrue(FirstBuffer == SecondBuffer,
			TEXT("Repeated Output calls should produce stable buffers")));
	}

	TEST_METHOD(EmptyBytecodeState)
	{
		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeEmptyState");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->GetSize(),
			TEXT("Fresh bytecode should report zero size")));
		ASSERT_THAT(IsNull(Fixture.ByteCode->GetFirstInstr(),
			TEXT("Fresh bytecode should not expose a first instruction")));
		ASSERT_THAT(AreEqual(-1, static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("Fresh bytecode should report no last instruction")));
		ASSERT_THAT(AreEqual(0, AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode).Num(),
			TEXT("Fresh bytecode should serialize to an empty buffer")));
	}

	TEST_METHOD(InsertFirstInstructionPrependsSequence)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeInsertFirst");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Instr(asBC_RET);
		Fixture.ByteCode->InsertFirstInstrDWORD(asBC_PshC4, 7);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("InsertFirstInstrDWORD should prepend one instruction before the existing tail")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetFirstInstr()->op),
			TEXT("InsertFirstInstrDWORD should become the first opcode")));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Buffer[1]),
			TEXT("InsertFirstInstrDWORD should preserve the inserted payload")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("InsertFirstInstrDWORD should preserve the existing tail opcode")));
	}

	TEST_METHOD(RemoveLastInstructionUpdatesTail)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeRemoveLast");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 2);
		Fixture.ByteCode->Instr(asBC_RET);

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->RemoveLastInstr(),
			TEXT("RemoveLastInstr should report success when removing the tail instruction")));
		ASSERT_THAT(AreEqual(2, CountInstructions(*Fixture.ByteCode),
			TEXT("RemoveLastInstr should remove exactly the tail instruction")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("RemoveLastInstr should update the tail to the previous instruction")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("RemoveLastInstr should preserve the previous instruction payload")));
	}

	TEST_METHOD(ClearAllResetsInstructionList)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeClearAll");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 1);
		Fixture.ByteCode->Instr(asBC_RET);
		Fixture.ByteCode->ClearAll();

		ASSERT_THAT(AreEqual(0, Fixture.ByteCode->GetSize(),
			TEXT("ClearAll should reset bytecode size")));
		ASSERT_THAT(IsNull(Fixture.ByteCode->GetFirstInstr(),
			TEXT("ClearAll should remove the first instruction")));
		ASSERT_THAT(AreEqual(-1, static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("ClearAll should reset the last instruction")));

		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 99);
		ASSERT_THAT(AreEqual(1, CountInstructions(*Fixture.ByteCode),
			TEXT("Bytecode should accept new instructions after ClearAll")));
		ASSERT_THAT(AreEqual(99, static_cast<int32>(Fixture.ByteCode->GetLastInstrValueDW()),
			TEXT("New instruction after ClearAll should preserve its payload")));
	}

	TEST_METHOD(LineInstructionSerializesDebugMarker)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FBytecodeFixture Fixture("BytecodeLineInstruction");
		if (!Fixture.IsValid(*TestRunner))
		{
			return;
		}

		Fixture.ByteCode->Line(12, 3, 0);
		Fixture.ByteCode->InstrDWORD(asBC_PshC4, 42);

		const TArray<asDWORD> Buffer = AngelscriptNativeTestSupport::EmitToBuffer(*Fixture.ByteCode);
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_LINE),
			TEXT("Line helper should emit a LINE instruction")));
		ASSERT_THAT(IsTrue(ContainsOpcode(*Fixture.ByteCode, asBC_PshC4),
			TEXT("Line helper should not prevent later instructions from being emitted")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_LINE), ReadOpcodeAt(Buffer, 0),
			TEXT("Serialized bytecode should start with the LINE marker")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_PshC4), static_cast<int32>(Fixture.ByteCode->GetLastInstr()),
			TEXT("Serialized bytecode should retain the later semantic tail")));
	}

	TEST_METHOD(CompiledFunctionExposesExecutableBytecode)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiled bytecode test should create a native engine")));
		ON_SCOPE_EXIT { AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return 42;
				}
			)AS");

		asIScriptModule* Module = nullptr;
		asIScriptFunction* Function = nullptr;
		asDWORD* Bytecode = nullptr;
		asUINT BytecodeLength = 0;
		if (!BuildAndGetFunctionBytecode(*TestRunner, ScriptEngine, "BytecodeCompiledExecutable", ScriptSource.c_str(), "int Entry()", Module, Function, Bytecode, BytecodeLength))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntEntry(*TestRunner, ScriptEngine, Function, Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, Result, TEXT("Compiled bytecode should execute the Entry function")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(Bytecode, BytecodeLength, asBC_RET),
			TEXT("Compiled bytecode should contain a RET instruction")));
	}

	TEST_METHOD(CompiledControlFlowProducesBranchOpcode)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiled branch bytecode test should create a native engine")));
		ON_SCOPE_EXIT { AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
				int PickValue(int Input)
				{
					if (Input > 0)
					{
						return 40;
					}

					return 2;
				}

				int Entry()
				{
					return PickValue(1) + PickValue(0);
				}
			)AS");

		asIScriptModule* Module = nullptr;
		asIScriptFunction* Function = nullptr;
		asDWORD* Bytecode = nullptr;
		asUINT BytecodeLength = 0;
		if (!BuildAndGetFunctionBytecode(*TestRunner, ScriptEngine, "BytecodeCompiledBranch", ScriptSource.c_str(), "int PickValue(int)", Module, Function, Bytecode, BytecodeLength))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		asIScriptFunction* EntryFunction = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Compiled branch bytecode test should resolve Entry")));

		int32 Result = 0;
		if (!ExecuteIntEntry(*TestRunner, ScriptEngine, EntryFunction, Result))
		{
			return;
		}

		const TArray<asEBCInstr> BranchOpcodes = { asBC_JZ, asBC_JNZ, asBC_JLowZ, asBC_JLowNZ, asBC_JS, asBC_JNS, asBC_JP, asBC_JNP };
		ASSERT_THAT(AreEqual(42, Result, TEXT("Compiled control-flow bytecode should execute both branches")));
		ASSERT_THAT(IsTrue(BytecodeContainsAnyOpcode(Bytecode, BytecodeLength, BranchOpcodes),
			TEXT("Compiled if/else function should emit a conditional branch opcode")));
	}

	TEST_METHOD(CompiledLoopProducesBackwardJump)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiled loop bytecode test should create a native engine")));
		ON_SCOPE_EXIT { AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					int Sum = 0;
					for (int Index = 0; Index < 6; ++Index)
					{
						Sum += Index;
					}

					return Sum;
				}
			)AS");

		asIScriptModule* Module = nullptr;
		asIScriptFunction* Function = nullptr;
		asDWORD* Bytecode = nullptr;
		asUINT BytecodeLength = 0;
		if (!BuildAndGetFunctionBytecode(*TestRunner, ScriptEngine, "BytecodeCompiledLoop", ScriptSource.c_str(), "int Entry()", Module, Function, Bytecode, BytecodeLength))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntEntry(*TestRunner, ScriptEngine, Function, Result))
		{
			return;
		}

		const TArray<asEBCInstr> JumpOpcodes = { asBC_JMP, asBC_JZ, asBC_JNZ, asBC_JLowZ, asBC_JLowNZ, asBC_JS, asBC_JNS, asBC_JP, asBC_JNP };
		ASSERT_THAT(AreEqual(15, Result, TEXT("Compiled loop bytecode should execute the summation loop")));
		ASSERT_THAT(IsTrue(BytecodeContainsAnyOpcode(Bytecode, BytecodeLength, JumpOpcodes),
			TEXT("Compiled loop function should emit jump bytecode")));
	}

	TEST_METHOD(CompiledArithmeticBytecodeDiffersFromConstantReturn)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiled arithmetic bytecode test should create a native engine")));
		ON_SCOPE_EXIT { AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine); };

		const std::string ConstantSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return 42;
				}
			)AS");
		const std::string ArithmeticSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					int A = 20;
					int B = 22;
					return A + B;
				}
			)AS");

		asIScriptModule* ConstantModule = nullptr;
		asIScriptFunction* ConstantFunction = nullptr;
		asDWORD* ConstantBytecode = nullptr;
		asUINT ConstantBytecodeLength = 0;
		if (!BuildAndGetFunctionBytecode(*TestRunner, ScriptEngine, "BytecodeCompiledConstant", ConstantSource.c_str(), "int Entry()", ConstantModule, ConstantFunction, ConstantBytecode, ConstantBytecodeLength))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}
		const TArray<asDWORD> ConstantBuffer = CopyBytecodeBuffer(ConstantBytecode, ConstantBytecodeLength);

		asIScriptModule* ArithmeticModule = nullptr;
		asIScriptFunction* ArithmeticFunction = nullptr;
		asDWORD* ArithmeticBytecode = nullptr;
		asUINT ArithmeticBytecodeLength = 0;
		if (!BuildAndGetFunctionBytecode(*TestRunner, ScriptEngine, "BytecodeCompiledArithmetic", ArithmeticSource.c_str(), "int Entry()", ArithmeticModule, ArithmeticFunction, ArithmeticBytecode, ArithmeticBytecodeLength))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}
		const TArray<asDWORD> ArithmeticBuffer = CopyBytecodeBuffer(ArithmeticBytecode, ArithmeticBytecodeLength);

		int32 ArithmeticResult = 0;
		if (!ExecuteIntEntry(*TestRunner, ScriptEngine, ArithmeticFunction, ArithmeticResult))
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, ArithmeticResult, TEXT("Compiled arithmetic bytecode should execute to the expected result")));
		ASSERT_THAT(IsTrue(ConstantBuffer.Num() > 0, TEXT("Constant function should expose bytecode")));
		ASSERT_THAT(IsTrue(ArithmeticBuffer.Num() > 0, TEXT("Arithmetic function should expose bytecode")));
		ASSERT_THAT(IsFalse(ConstantBuffer == ArithmeticBuffer,
			TEXT("Different compiled function bodies should produce different bytecode buffers")));
	}
};

#endif
