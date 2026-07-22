#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBytecodeGenerationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Bytecode.Generation",
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
		if (!BuildAndGetFunctionBytecode(*TestRunner, ScriptEngine, "BytecodeCompiledBranch", ScriptSource.c_str(), "int PickValue(int Input)", Module, Function, Bytecode, BytecodeLength))
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
