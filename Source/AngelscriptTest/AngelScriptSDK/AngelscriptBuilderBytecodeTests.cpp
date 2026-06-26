#include "AngelscriptBuilderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderBytecodeTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.Bytecode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CompileBuilderBytecode(FAutomationTestBase& Test, AngelscriptNativeTestSupport::FNativeTestEngine& TestEngine, asCModule& Module, const FString& Stage)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asCBuilder* Builder = Module.builder;
		if (Builder == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("[Builder][%s] missing builder after script sections were added"), *Stage));
			return false;
		}

		if (!RunBuilderPipelineThroughLayout(Test, *Builder, &Module))
		{
			ReportBuilderFailureDiagnostics(Test, TestEngine);
			return false;
		}

		if (!RunBuilderStage(Test, *Builder, FString::Printf(TEXT("%s.BuildCompileCode"), *Stage), &asCBuilder::BuildCompileCode, &Module))
		{
			ReportBuilderFailureDiagnostics(Test, TestEngine);
			return false;
		}

		return true;
	}

	static bool BytecodeContainsOpcode(asIScriptFunction* Function, asEBCInstr Opcode)
	{
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
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

	static bool BytecodeContainsBranchOpcode(asIScriptFunction* Function)
	{
		static const asEBCInstr BranchOpcodes[] =
		{
			asBC_JMP,
			asBC_JZ,
			asBC_JNZ,
			asBC_JLowZ,
			asBC_JLowNZ,
			asBC_JS,
			asBC_JNS,
			asBC_JP,
			asBC_JNP,
			asBC_JMPP,
		};

		for (const asEBCInstr Opcode : BranchOpcodes)
		{
			if (BytecodeContainsOpcode(Function, Opcode))
			{
				return true;
			}
		}

		return false;
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(CompileCodeProducesExecutableEntryBytecode)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeEntry");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Add(int A, int B)
			{
				return A + B;
			}

			int Entry()
			{
				return Add(40, 2);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeEntry.as", Source.c_str(), TEXT("Entry.AddSection")),
			TEXT("Builder bytecode test should add the bytecode section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder bytecode test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module), TEXT("Builder bytecode test should build through layout")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("Entry.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module),
			TEXT("Builder bytecode test should compile code")));

		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		asIScriptFunction* Add = FindModuleFunctionByNameAndParamCount(Module, "Add", 2);
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder bytecode test should expose Entry")));
		ASSERT_THAT(IsNotNull(Add, TEXT("Builder bytecode test should expose Add")));
		ASSERT_THAT(IsTrue(HasBytecode(Entry), TEXT("Builder bytecode test should produce Entry bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(Add), TEXT("Builder bytecode test should produce Add bytecode")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("Entry.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder bytecode test should execute compiled Entry bytecode")));
	}

	TEST_METHOD(CrossSectionCallsKeepDeclaringSectionsAndExecute)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder cross-section bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeCrossSection");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder cross-section bytecode test should create a module")));

		const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
			int ProvideValue()
			{
				return 40;
			}
			)AS");
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return ProvideValue() + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeProvider.as", ProviderSource.c_str(), TEXT("CrossSection.AddProvider")),
			TEXT("Builder cross-section bytecode test should add the provider section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeConsumer.as", ConsumerSource.c_str(), TEXT("CrossSection.AddConsumer")),
			TEXT("Builder cross-section bytecode test should add the consumer section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder cross-section bytecode test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module), TEXT("Builder cross-section bytecode test should build through layout")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("CrossSection.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module),
			TEXT("Builder cross-section bytecode test should compile code")));

		asIScriptFunction* Provider = Module->GetFunctionByDecl("int ProvideValue()");
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Provider, TEXT("Builder cross-section bytecode test should expose provider function")));
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder cross-section bytecode test should expose entry function")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderBytecodeProvider.as")), FString(UTF8_TO_TCHAR(Provider != nullptr ? Provider->GetScriptSectionName() : "")),
			TEXT("Builder cross-section bytecode test should preserve provider section")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderBytecodeConsumer.as")), FString(UTF8_TO_TCHAR(Entry != nullptr ? Entry->GetScriptSectionName() : "")),
			TEXT("Builder cross-section bytecode test should preserve consumer section")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("CrossSection.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder cross-section bytecode test should execute cross-section call")));
	}

	TEST_METHOD(NamespaceAndOverloadDispatchExecuteThroughCompiledBytecode)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder namespace bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeNamespaceOverload");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder namespace bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			namespace Tools
			{
				int Pick()
				{
					return 30;
				}

				int Pick(int Value)
				{
					return Value + 2;
				}
			}

			int Entry()
			{
				return Tools::Pick() + Tools::Pick(10);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeNamespaceOverload.as", Source.c_str(), TEXT("NamespaceOverload.AddSection")),
			TEXT("Builder namespace bytecode test should add the namespace section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder namespace bytecode test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module), TEXT("Builder namespace bytecode test should build through layout")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("NamespaceOverload.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module),
			TEXT("Builder namespace bytecode test should compile code")));

		ASSERT_THAT(IsNotNull(FindModuleFunctionByNameAndParamCount(Module, "Pick", 0, "Tools"), TEXT("Builder namespace bytecode test should expose Tools::Pick()")));
		ASSERT_THAT(IsNotNull(FindModuleFunctionByNameAndParamCount(Module, "Pick", 1, "Tools"), TEXT("Builder namespace bytecode test should expose Tools::Pick(int)")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("NamespaceOverload.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder namespace bytecode test should execute namespaced overload dispatch")));
	}

	TEST_METHOD(LoopBreakContinueProducesBranchingBytecode)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder loop bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeLoopControl");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder loop bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int LoopScore()
			{
				int Total = 0;
				for (int Index = 0; Index < 10; ++Index)
				{
					if (Index == 7)
					{
						break;
					}

					if ((Index % 2) == 0)
					{
						continue;
					}

					Total += Index;
				}
				return Total;
			}

			int Entry()
			{
				return LoopScore() + 33;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeLoopControl.as", Source.c_str(), TEXT("LoopControl.AddSection")),
			TEXT("Builder loop bytecode test should add the loop section")));
		ASSERT_THAT(IsTrue(CompileBuilderBytecode(*TestRunner, Engine, *Module, TEXT("LoopControl")),
			TEXT("Builder loop bytecode test should compile code")));

		asIScriptFunction* LoopScore = Module->GetFunctionByDecl("int LoopScore()");
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(LoopScore, TEXT("Builder loop bytecode test should expose LoopScore")));
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder loop bytecode test should expose Entry")));
		ASSERT_THAT(IsTrue(HasBytecode(LoopScore), TEXT("Builder loop bytecode test should produce LoopScore bytecode")));
		ASSERT_THAT(IsTrue(BytecodeContainsBranchOpcode(LoopScore), TEXT("Builder loop bytecode test should emit branch opcodes for break/continue")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("LoopControl.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder loop bytecode test should execute loop control flow")));
	}

	TEST_METHOD(RecursiveFunctionEmitsCallBytecodeAndExecutes)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder recursion bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeRecursion");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder recursion bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Factorial(int Value)
			{
				if (Value <= 1)
				{
					return 1;
				}
				return Value * Factorial(Value - 1);
			}

			int Entry()
			{
				return Factorial(5) - 78;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeRecursion.as", Source.c_str(), TEXT("Recursion.AddSection")),
			TEXT("Builder recursion bytecode test should add the recursion section")));
		ASSERT_THAT(IsTrue(CompileBuilderBytecode(*TestRunner, Engine, *Module, TEXT("Recursion")),
			TEXT("Builder recursion bytecode test should compile code")));

		asIScriptFunction* Factorial = FindModuleFunctionByNameAndParamCount(Module, "Factorial", 1);
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Factorial, TEXT("Builder recursion bytecode test should expose Factorial")));
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder recursion bytecode test should expose Entry")));
		ASSERT_THAT(IsTrue(HasBytecode(Factorial), TEXT("Builder recursion bytecode test should produce Factorial bytecode")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(Factorial, asBC_CALL), TEXT("Builder recursion bytecode test should emit a script call opcode")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("Recursion.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder recursion bytecode test should execute recursive bytecode")));
	}

	TEST_METHOD(ShortCircuitBooleanExpressionsSkipUnreachedBytecode)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder short-circuit bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeShortCircuit");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder short-circuit bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int ShouldNotRun()
			{
				int Zero = 0;
				return 1 / Zero;
			}

			bool IsZero(int Value)
			{
				return Value == 0;
			}

			int Entry()
			{
				int Score = 0;
				if (false && IsZero(ShouldNotRun()))
				{
					Score += 1000;
				}
				if (true || IsZero(ShouldNotRun()))
				{
					Score += 40;
				}
				if (IsZero(0) && !IsZero(1))
				{
					Score += 2;
				}
				return Score;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeShortCircuit.as", Source.c_str(), TEXT("ShortCircuit.AddSection")),
			TEXT("Builder short-circuit bytecode test should add the short-circuit section")));
		ASSERT_THAT(IsTrue(CompileBuilderBytecode(*TestRunner, Engine, *Module, TEXT("ShortCircuit")),
			TEXT("Builder short-circuit bytecode test should compile code")));

		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder short-circuit bytecode test should expose Entry")));
		ASSERT_THAT(IsTrue(HasBytecode(Entry), TEXT("Builder short-circuit bytecode test should produce Entry bytecode")));
		ASSERT_THAT(IsTrue(BytecodeContainsBranchOpcode(Entry), TEXT("Builder short-circuit bytecode test should emit branch opcodes")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("ShortCircuit.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder short-circuit bytecode test should execute skipped branches without exceptions")));
	}

	TEST_METHOD(DefaultArgumentsCompileImplicitAndExplicitCallSites)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder default-argument bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeDefaultArgs");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder default-argument bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int AddWithDefaults(int Base, int Delta = 2, int Extra = 0)
			{
				return Base + Delta + Extra;
			}

			int EntryImplicit()
			{
				return AddWithDefaults(40);
			}

			int EntryPartial()
			{
				return AddWithDefaults(39, 3);
			}

			int EntryExplicit()
			{
				return AddWithDefaults(30, 10, 2);
			}

			int Entry()
			{
				return EntryImplicit() + EntryPartial() + EntryExplicit() - 84;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeDefaultArgs.as", Source.c_str(), TEXT("DefaultArgs.AddSection")),
			TEXT("Builder default-argument bytecode test should add the default-argument section")));
		ASSERT_THAT(IsTrue(CompileBuilderBytecode(*TestRunner, Engine, *Module, TEXT("DefaultArgs")),
			TEXT("Builder default-argument bytecode test should compile code")));

		asIScriptFunction* AddWithDefaults = FindModuleFunctionByNameAndParamCount(Module, "AddWithDefaults", 3);
		asIScriptFunction* EntryImplicit = Module->GetFunctionByDecl("int EntryImplicit()");
		asIScriptFunction* EntryPartial = Module->GetFunctionByDecl("int EntryPartial()");
		asIScriptFunction* EntryExplicit = Module->GetFunctionByDecl("int EntryExplicit()");
		ASSERT_THAT(IsNotNull(AddWithDefaults, TEXT("Builder default-argument bytecode test should expose AddWithDefaults")));
		ASSERT_THAT(IsNotNull(EntryImplicit, TEXT("Builder default-argument bytecode test should expose EntryImplicit")));
		ASSERT_THAT(IsNotNull(EntryPartial, TEXT("Builder default-argument bytecode test should expose EntryPartial")));
		ASSERT_THAT(IsNotNull(EntryExplicit, TEXT("Builder default-argument bytecode test should expose EntryExplicit")));
		ASSERT_THAT(IsTrue(HasBytecode(AddWithDefaults), TEXT("Builder default-argument bytecode test should produce AddWithDefaults bytecode")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(EntryImplicit, asBC_CALL), TEXT("Builder default-argument bytecode test should compile implicit default call")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(EntryPartial, asBC_CALL), TEXT("Builder default-argument bytecode test should compile partial default call")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(EntryExplicit, asBC_CALL), TEXT("Builder default-argument bytecode test should compile explicit call")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("DefaultArgs.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder default-argument bytecode test should execute all default-argument call sites")));
	}

	TEST_METHOD(EnumSwitchCompilesBranchingBytecodeAndExecutes)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder switch bytecode test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderBytecodeEnumSwitch");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder switch bytecode test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			enum EMode
			{
				Idle = 1,
				Active = 2,
				Paused = 3
			}

			int Score(EMode Mode)
			{
				switch (Mode)
				{
					case EMode::Idle:
						return 10;
					case EMode::Active:
						return 40;
					case EMode::Paused:
						return 20;
					default:
						return 0;
				}
			}

			int Entry()
			{
				return Score(EMode::Active) + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderBytecodeEnumSwitch.as", Source.c_str(), TEXT("EnumSwitch.AddSection")),
			TEXT("Builder switch bytecode test should add the switch section")));
		ASSERT_THAT(IsTrue(CompileBuilderBytecode(*TestRunner, Engine, *Module, TEXT("EnumSwitch")),
			TEXT("Builder switch bytecode test should compile code")));

		asIScriptFunction* Score = FindModuleFunctionByNameAndParamCount(Module, "Score", 1);
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Score, TEXT("Builder switch bytecode test should expose Score")));
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder switch bytecode test should expose Entry")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("EMode"), TEXT("Builder switch bytecode test should expose EMode")));
		ASSERT_THAT(IsTrue(HasBytecode(Score), TEXT("Builder switch bytecode test should produce Score bytecode")));
		ASSERT_THAT(IsTrue(BytecodeContainsBranchOpcode(Score), TEXT("Builder switch bytecode test should emit switch branch opcodes")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(*TestRunner, TEXT("EnumSwitch.Execute"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder switch bytecode test should execute enum switch bytecode")));
	}
};

#endif
