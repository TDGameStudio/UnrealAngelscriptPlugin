#include "AngelscriptBuilderTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptBuilderTestSupport;
using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	void LogBuilderState(const FString& Stage, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true, bool bIncludeDiagnosticCounters = true) const
	{
		AddInfo(FString::Printf(TEXT("[Builder][%s] %s | %s"), *Stage, *DescribeBuilderCounts(Builder, bIncludeDiagnosticCounters), *DescribeModuleCounts(Module)));
		if (bExpandBuilderDescriptions)
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] classDecls: %s"), *Stage, *DescribeClassDeclarations(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] namedTypes: %s"), *Stage, *DescribeNamedTypeDeclarations(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] functionDescs: %s"), *Stage, *DescribeFunctionDescriptions(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] globalDescs: %s"), *Stage, *DescribeGlobalDescriptions(Builder)));
		}
		if (Module != nullptr)
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleTypes: %s"), *Stage, *DescribeModuleTypes(Module)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleFunctions: %s"), *Stage, *DescribeModuleFunctions(Module)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleGlobals: %s"), *Stage, *DescribeModuleGlobals(Module)));
		}
	}

	void LogBuilderSectionInput(const FString& Stage, const char* SectionName, const char* Source) const
	{
		AddInfo(FString::Printf(
			TEXT("[Builder][%s] add section name=%s bytes=%d lines=%d"),
			*Stage,
			*ToTestString(SectionName),
			Source != nullptr ? static_cast<int32>(std::strlen(Source)) : 0,
			CountSourceLines(Source)));
	}

	void LogBuilderStageResult(const FString& Stage, int Result, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true) const
	{
		AddInfo(FString::Printf(TEXT("[Builder][%s] result=%d"), *Stage, Result));
		LogBuilderState(Stage, Builder, Module, bExpandBuilderDescriptions);
	}

	void LogScriptExecutionResult(const FString& Stage, const char* Declaration, int32 Result) const
	{
		AddInfo(FString::Printf(TEXT("[Builder][%s] executed %s => %d"), *Stage, *ToTestString(Declaration), Result));
	}

	void ReportBuilderFailureDiagnostics() const
	{
		const FString Messages = Engine.GetMessagesText();
		if (!Messages.IsEmpty())
		{
			AddInfo(Messages);
		}
	}

	bool AddBuilderSectionWithLog(asCModule& Module, const char* SectionName, const char* Source, const FString& Stage) const
	{
		LogBuilderSectionInput(Stage, SectionName, Source);
		const bool bAdded = AddBuilderSection(Module, SectionName, Source);
		AddInfo(FString::Printf(TEXT("[Builder][%s] AddScriptSection result=%s"), *Stage, BoolText(bAdded)));
		if (Module.builder != nullptr)
		{
			LogBuilderState(Stage, *Module.builder, &Module, true, false);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] %s"), *Stage, *DescribeModuleCounts(&Module)));
		}
		return bAdded;
	}

	bool RunBuilderStage(asCBuilder& Builder, const FString& Stage, int (asCBuilder::*StageMethod)(), const asCModule* Module = nullptr) const
	{
		LogBuilderState(FString::Printf(TEXT("%s.before"), *Stage), Builder, Module, true, false);
		const int Result = (Builder.*StageMethod)();
		LogBuilderStageResult(FString::Printf(TEXT("%s.after"), *Stage), Result, Builder, Module);
		return Result == asSUCCESS;
	}

	bool RunBuilderPipelineThroughLayout(asCBuilder& Builder, const asCModule* Module = nullptr) const
	{
		if (!RunBuilderStage(Builder, TEXT("BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, Module))
		{
			return false;
		}
		LogBuilderState(TEXT("BuildAllocateGlobalVariables.before"), Builder, Module, true, false);
		Builder.BuildAllocateGlobalVariables();
		LogBuilderState(TEXT("BuildAllocateGlobalVariables.after"), Builder, Module);
		return RunBuilderStage(Builder, TEXT("BuildLayoutFunctions"), &asCBuilder::BuildLayoutFunctions, Module);
	}

public:
	inline static FNativeTestEngine Engine;

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

	TEST_METHOD(CompileFunctionUsesProvidedSectionName)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction section test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionSection");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder CompileFunction section test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		LogBuilderState(TEXT("CompileFunctionUsesProvidedSectionName.initial"), Builder, Module, true, false);
		asCScriptFunction* Function = nullptr;
		const std::string EntryFunctionSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		LogBuilderSectionInput(TEXT("CompileFunctionUsesProvidedSectionName.input"), "BuilderCompileFunctionSection_A", EntryFunctionSource.c_str());
		const int CompileResult = Builder.CompileFunction("BuilderCompileFunctionSection_A", EntryFunctionSource.c_str(), 20, asCOMP_ADD_TO_MODULE, &Function);
		LogBuilderStageResult(TEXT("CompileFunctionUsesProvidedSectionName.CompileFunction"), CompileResult, Builder, Module, false);
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), CompileResult, TEXT("Builder CompileFunction section test should compile one function")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}
		ASSERT_THAT(IsNotNull(Function, TEXT("Builder CompileFunction section test should return the compiled function")));

		ASSERT_THAT(AreEqual(FString(TEXT("BuilderCompileFunctionSection_A")), FString(UTF8_TO_TCHAR(Function->GetScriptSectionName())),
			TEXT("Builder CompileFunction section test should preserve the provided section name")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder CompileFunction section test should add the function to the module")));
	}

	TEST_METHOD(CompileFunctionFailureDoesNotLeakFunction)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction failure test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionFailure");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder CompileFunction failure test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		Builder.silent = true;
		LogBuilderState(TEXT("CompileFunctionFailureDoesNotLeakFunction.initial"), Builder, Module, true, false);
		asCScriptFunction* Function = nullptr;
		const std::string BrokenEntryFunctionSource = ASTEST_AS_ANSI(R"AS(
			int Entry(
			{
				return 42;
			}
			)AS");
		LogBuilderSectionInput(TEXT("CompileFunctionFailureDoesNotLeakFunction.input"), "BuilderCompileFunctionFailure_A", BrokenEntryFunctionSource.c_str());
		const int CompileResult = Builder.CompileFunction("BuilderCompileFunctionFailure_A", BrokenEntryFunctionSource.c_str(), 0, asCOMP_ADD_TO_MODULE, &Function);
		LogBuilderStageResult(TEXT("CompileFunctionFailureDoesNotLeakFunction.CompileFunction"), CompileResult, Builder, Module, false);
		ReportBuilderFailureDiagnostics();
		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Builder CompileFunction failure test should fail the invalid function")));
		ASSERT_THAT(IsNull(Function, TEXT("Builder CompileFunction failure test should not return a function")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder CompileFunction failure test should not leak a module function")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Builder CompileFunction failure test should not expose Entry")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder CompileFunction failure test should not leak global variables")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()),
			TEXT("Builder CompileFunction failure test should not leak object types")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder.functions.GetLength()),
			TEXT("Builder CompileFunction failure test should not retain function descriptions")));
	}

	TEST_METHOD(ParseScriptsCreatesParserNodes)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder parse test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderParseScripts");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder parse test should create a backing module")));

		const std::string TypeSectionSource = ASTEST_AS_ANSI(R"AS(
			namespace BuilderParse
			{
				class ActorState
				{
					int Value;

					int GetValue()
					{
						return Value;
					}
				}

				enum EParseState
				{
					Idle,
					Busy
				}
			}
			)AS");
		const std::string FunctionSectionSource = ASTEST_AS_ANSI(R"AS(
			const int ParseBase = 40;

			int ParseEntry()
			{
				return ParseBase + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderParse_Types", TypeSectionSource.c_str(), TEXT("ParseScriptsCreatesParserNodes.AddTypes")),
			TEXT("Builder test should add a script section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderParse_Functions", FunctionSectionSource.c_str(), TEXT("ParseScriptsCreatesParserNodes.AddFunctions")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Adding script sections should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("ParseScriptsCreatesParserNodes.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module), TEXT("Builder parse should parse both sections")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Builder->parsers.GetLength()),
			TEXT("Builder parse should create one parser per section")));
		ASSERT_THAT(IsNotNull(Builder->parsers[0]->GetScriptNode(),
			TEXT("Builder parse should retain the first section AST root")));
		ASSERT_THAT(IsNotNull(Builder->parsers[1]->GetScriptNode(),
			TEXT("Builder parse should retain the second section AST root")));
		ASSERT_THAT(AreEqual(static_cast<int32>(snScript), static_cast<int32>(Builder->parsers[0]->GetScriptNode()->nodeType),
			TEXT("Builder parse first section root should be a script node")));
		ASSERT_THAT(AreEqual(static_cast<int32>(snScript), static_cast<int32>(Builder->parsers[1]->GetScriptNode()->nodeType),
			TEXT("Builder parse second section root should be a script node")));
		ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(Builder->parsers[0]->GetScriptNode(), snNamespace),
			TEXT("Builder parse should preserve namespace nodes in the AST")));
		ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(Builder->parsers[0]->GetScriptNode(), snClass),
			TEXT("Builder parse should preserve class nodes in the AST")));
		ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(Builder->parsers[0]->GetScriptNode(), snEnum),
			TEXT("Builder parse should preserve enum nodes in the AST")));
		ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(Builder->parsers[1]->GetScriptNode(), snFunction),
			TEXT("Builder parse should preserve global function nodes in the AST")));
		ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(Builder->parsers[1]->GetScriptNode(), snDeclaration),
			TEXT("Builder parse should preserve global variable declaration nodes in the AST")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->classDeclarations.GetLength()),
			TEXT("Builder parse should not populate class declarations before type generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->namedTypeDeclarations.GetLength()),
			TEXT("Builder parse should not populate named type declarations before type generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder parse should not populate function descriptions before function generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder parse should not populate global variable descriptions before function generation")));
		ASSERT_THAT(IsNull(Module->GetTypeInfoByDecl("BuilderParse::ActorState"),
			TEXT("Builder parse should not register class types before type generation")));
		ASSERT_THAT(IsNull(Module->GetTypeInfoByDecl("BuilderParse::EParseState"),
			TEXT("Builder parse should not register enum types before type generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder parse should not register global functions before function generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder parse should not register global variables before function generation")));
	}

	TEST_METHOD(GenerateTypesRegistersDeclarations)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder type-generation test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateTypes");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder type-generation test should create a backing module")));

		const std::string TypeDeclarationsSource = ASTEST_AS_ANSI(R"AS(
			enum EState
			{
				Idle,
				Busy
			}

			class ActorState
			{
				int Value;

				int GetValue()
				{
					return Value;
				}
			}

			namespace Types
			{
				class NestedState
				{
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderTypes", TypeDeclarationsSource.c_str(), TEXT("GenerateTypesRegistersDeclarations.AddTypes")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder type-generation test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateTypesRegistersDeclarations.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module), TEXT("Builder type-generation test should parse scripts")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateTypesRegistersDeclarations.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module), TEXT("Builder type-generation test should generate types")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		ASSERT_THAT(IsTrue(Builder->classDeclarations.GetLength() >= 2,
			TEXT("Builder type-generation should discover class declarations")));
		sClassDeclaration* ActorStateDeclaration = FindClassDeclarationByName(*Builder, "ActorState");
		ASSERT_THAT(IsNotNull(ActorStateDeclaration,
			TEXT("Builder type-generation should retain the ActorState class declaration")));
		ASSERT_THAT(IsNotNull(ActorStateDeclaration != nullptr ? ActorStateDeclaration->node : nullptr,
			TEXT("Builder type-generation should keep the ActorState declaration AST node")));
		ASSERT_THAT(IsNotNull(ActorStateDeclaration != nullptr ? ActorStateDeclaration->typeInfo : nullptr,
			TEXT("Builder type-generation should attach ActorState type info to the declaration")));
		sClassDeclaration* NestedStateDeclaration = FindClassDeclarationByName(*Builder, "NestedState");
		ASSERT_THAT(IsNotNull(NestedStateDeclaration,
			TEXT("Builder type-generation should retain the namespaced NestedState class declaration")));
		ASSERT_THAT(IsNotNull(NestedStateDeclaration != nullptr ? NestedStateDeclaration->typeInfo : nullptr,
			TEXT("Builder type-generation should attach NestedState type info to the declaration")));
		sClassDeclaration* EnumDeclaration = FindNamedTypeDeclarationByName(*Builder, "EState");
		ASSERT_THAT(IsNotNull(EnumDeclaration,
			TEXT("Builder type-generation should retain the EState named type declaration")));
		ASSERT_THAT(IsNotNull(EnumDeclaration != nullptr ? EnumDeclaration->typeInfo : nullptr,
			TEXT("Builder type-generation should attach EState type info to the named declaration")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->interfaceDeclarations.GetLength()),
			TEXT("Builder type-generation should not require rejected interface syntax")));
		asITypeInfo* ActorStateType = Module->GetTypeInfoByDecl("ActorState");
		ASSERT_THAT(IsNotNull(ActorStateType,
			TEXT("Builder type-generation should register the class type on the module")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ActorStateType != nullptr ? ActorStateType->GetPropertyCount() : 0),
			TEXT("Builder type-generation should not layout class properties before class layout")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ActorStateType != nullptr ? ActorStateType->GetMethodCount() : 0),
			TEXT("Builder type-generation should not register class methods before function generation")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("Types::NestedState"),
			TEXT("Builder type-generation should register the namespaced class type on the module")));
		asITypeInfo* StateEnumType = Module->GetTypeInfoByDecl("EState");
		ASSERT_THAT(IsNotNull(StateEnumType,
			TEXT("Builder type-generation should register the enum type on the module")));
		ASSERT_THAT(IsTrue(StateEnumType != nullptr && StateEnumType->GetEnumValueCount() >= 0,
			TEXT("Builder type-generation should expose enum metadata before later layout stages")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder type-generation should not register global functions before function generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder type-generation should not register script globals before function generation")));
	}

	TEST_METHOD(GenerateFunctionsRegistersGlobalsAndFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder function-generation test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateFunctions");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder function-generation test should create a backing module")));

		const std::string BuilderFunctionsSource = ASTEST_AS_ANSI(R"AS(
			const int Base = 40;

			int AddTwo()
			{
				return Base + 2;
			}

			int AddThree()
			{
				return Base + 3;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderFunctions", BuilderFunctionsSource.c_str(), TEXT("GenerateFunctionsRegistersGlobalsAndFunctions.AddFunctions")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder function-generation test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateFunctionsRegistersGlobalsAndFunctions.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module), TEXT("Builder function-generation test should parse scripts")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateFunctionsRegistersGlobalsAndFunctions.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module), TEXT("Builder function-generation test should generate types")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateFunctionsRegistersGlobalsAndFunctions.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module), TEXT("Builder function-generation test should generate functions")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "AddTwo"),
			TEXT("Builder function-generation should retain AddTwo in builder function descriptions")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "AddThree"),
			TEXT("Builder function-generation should retain AddThree in builder function descriptions")));
		sGlobalVariableDescription* BaseGlobal = FindGlobalVariableDescriptionByName(*Builder, "Base");
		ASSERT_THAT(IsNotNull(BaseGlobal,
			TEXT("Builder function-generation should retain Base in builder global descriptions")));
		ASSERT_THAT(IsNotNull(BaseGlobal != nullptr ? BaseGlobal->property : nullptr,
			TEXT("Builder function-generation should allocate a global property for Base")));
		ASSERT_THAT(IsFalse(BaseGlobal != nullptr && BaseGlobal->isCompiled,
			TEXT("Builder function-generation should not compile the Base global initializer before code generation")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder function-generation should register both global functions")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder function-generation should register one global variable")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int AddTwo()"),
			TEXT("Builder function-generation should expose AddTwo by declaration")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int AddThree()"),
			TEXT("Builder function-generation should expose AddThree by declaration")));
		const int GlobalIndex = Module->GetGlobalVarIndexByName("Base");
		ASSERT_THAT(IsTrue(GlobalIndex >= 0,
			TEXT("Builder function-generation should expose Base by name")));
		const char* GlobalName = nullptr;
		int GlobalTypeId = asINVALID_TYPE;
		bool bIsConst = false;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->GetGlobalVar(static_cast<asUINT>(GlobalIndex), &GlobalName, nullptr, &GlobalTypeId, &bIsConst),
			TEXT("Builder function-generation should return Base global metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Base")), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")),
			TEXT("Builder function-generation should preserve Base global name")));
		ASSERT_THAT(IsTrue(bIsConst,
			TEXT("Builder function-generation should preserve Base constness")));
		ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl("int"), GlobalTypeId,
			TEXT("Builder function-generation should preserve Base int type")));
	}

	TEST_METHOD(LayoutAndCompileProduceExecutableBytecode)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder compile test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileCode");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder compile test should create a backing module")));

		const std::string CompilePipelineSource = ASTEST_AS_ANSI(R"AS(
			class Counter
			{
				int Value;

				int Get()
				{
					return Value;
				}
			}

			int Helper(int Value)
			{
				return Value * 2;
			}

			int UseCounter()
			{
				return Helper(0);
			}

			int Entry()
			{
				return Helper(21);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCompile", CompilePipelineSource.c_str(), TEXT("LayoutAndCompileProduceExecutableBytecode.AddCompileSection")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder compile test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder should build through layout")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}
		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("LayoutAndCompileProduceExecutableBytecode.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder compile test should compile function bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		asIScriptFunction* Function = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Builder compile should expose Entry after code generation")));
		asIScriptFunction* HelperFunction = FindModuleFunctionByNameAndParamCount(Module, "Helper", 1);
		ASSERT_THAT(IsNotNull(HelperFunction, TEXT("Builder compile should expose Helper after code generation")));
		asIScriptFunction* CounterEntryFunction = FindModuleFunctionByNameAndParamCount(Module, "UseCounter", 0);
		ASSERT_THAT(IsNotNull(CounterEntryFunction, TEXT("Builder compile should expose UseCounter after code generation")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "Entry"),
			TEXT("Builder compile should retain Entry function description")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "Helper"),
			TEXT("Builder compile should retain Helper function description")));

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Builder compile should emit bytecode for Entry")));
		ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Builder compile should emit at least one bytecode dword")));
		ASSERT_THAT(IsTrue(HasBytecode(HelperFunction), TEXT("Builder compile should emit bytecode for Helper")));
		ASSERT_THAT(IsTrue(HasBytecode(CounterEntryFunction), TEXT("Builder compile should emit bytecode for UseCounter")));
		asITypeInfo* CounterType = Module->GetTypeInfoByDecl("Counter");
		ASSERT_THAT(IsNotNull(CounterType, TEXT("Builder compile should expose Counter type metadata")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(CounterType != nullptr ? CounterType->GetPropertyCount() : 0),
			TEXT("Builder class layout should expose Counter.Value")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(CounterType, "Get", 0),
			TEXT("Builder function layout should expose Counter.Get")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "Get", "Counter"),
			TEXT("Builder compile should retain Counter.Get function description")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("LayoutAndCompileProduceExecutableBytecode.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder compile should execute compiled bytecode")));

		int32 CounterResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int UseCounter()", CounterResult))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("LayoutAndCompileProduceExecutableBytecode.UseCounter"), "int UseCounter()", CounterResult);
		ASSERT_THAT(AreEqual(0, CounterResult, TEXT("Builder compile should execute additional compiled helper-dependent bytecode")));
	}

	TEST_METHOD(StageFailureStopsBeforeExecutableCode)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder stage failure test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderStageFailure");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder stage failure test should create a backing module")));

		const std::string BrokenStageSource = ASTEST_AS_ANSI(R"AS(
			int Entry(
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderStageFailure", BrokenStageSource.c_str(), TEXT("StageFailureStopsBeforeExecutableCode.AddBrokenSection")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder stage failure test should create a builder")));

		Builder->silent = true;
		LogBuilderState(TEXT("StageFailureStopsBeforeExecutableCode.BuildParallelParseScripts.before"), *Builder, Module, true, false);
		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(TEXT("StageFailureStopsBeforeExecutableCode.BuildParallelParseScripts.after"), ParseResult, *Builder, Module);
		ReportBuilderFailureDiagnostics();
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Builder stage failure should fail during parse")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder stage failure should record at least one builder error")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Builder->parsers.GetLength()),
			TEXT("Builder stage failure should retain the failing parser for diagnostics")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->classDeclarations.GetLength()),
			TEXT("Builder stage failure should not produce class declarations")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->namedTypeDeclarations.GetLength()),
			TEXT("Builder stage failure should not produce named type declarations")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder stage failure should not produce function descriptions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder stage failure should not produce global variable descriptions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder stage failure should not register executable functions")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Builder stage failure should not expose Entry")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()),
			TEXT("Builder stage failure should not register object types")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder stage failure should not register global variables")));
	}

	TEST_METHOD(CrossSectionDependenciesCompileAndKeepSections)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder cross-section dependency test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCrossSectionDependencies");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder cross-section dependency test should create a backing module")));

		const std::string TypesSectionSource = ASTEST_AS_ANSI(R"AS(
			class SharedState
			{
				int Value = 39;

				int Read()
				{
					return Value;
				}
			}
			)AS");
		const std::string HelpersSectionSource = ASTEST_AS_ANSI(R"AS(
			int AddOne(int Value)
			{
				return Value + 1;
			}
			)AS");
		const std::string EntrySectionSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return AddOne(39) + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCrossSection_Types", TypesSectionSource.c_str(), TEXT("CrossSectionDependenciesCompileAndKeepSections.AddTypes")),
			TEXT("Builder cross-section dependency test should add the type section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCrossSection_Helpers", HelpersSectionSource.c_str(), TEXT("CrossSectionDependenciesCompileAndKeepSections.AddHelpers")),
			TEXT("Builder cross-section dependency test should add the helper section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCrossSection_Entry", EntrySectionSource.c_str(), TEXT("CrossSectionDependenciesCompileAndKeepSections.AddEntry")),
			TEXT("Builder cross-section dependency test should add the entry section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder cross-section dependency test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder cross-section dependency test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("CrossSectionDependenciesCompileAndKeepSections.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder cross-section dependency test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		asITypeInfo* SharedStateType = Module->GetTypeInfoByDecl("SharedState");
		asIScriptFunction* HelperFunction = FindModuleFunctionByNameAndParamCount(Module, "AddOne", 1);
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(SharedStateType, TEXT("Builder cross-section dependency test should expose SharedState type metadata")));
		ASSERT_THAT(IsNotNull(HelperFunction, TEXT("Builder cross-section dependency test should expose AddOne")));
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder cross-section dependency test should expose Entry")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderCrossSection_Helpers")), FString(UTF8_TO_TCHAR(HelperFunction != nullptr ? HelperFunction->GetScriptSectionName() : "")),
			TEXT("Builder cross-section dependency test should preserve AddOne section name")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderCrossSection_Entry")), FString(UTF8_TO_TCHAR(EntryFunction != nullptr ? EntryFunction->GetScriptSectionName() : "")),
			TEXT("Builder cross-section dependency test should preserve Entry section name")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(SharedStateType != nullptr ? SharedStateType->GetPropertyCount() : 0),
			TEXT("Builder cross-section dependency test should layout SharedState.Value")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(SharedStateType, "Read", 0),
			TEXT("Builder cross-section dependency test should layout SharedState.Read")));
		ASSERT_THAT(IsTrue(HasBytecode(HelperFunction), TEXT("Builder cross-section dependency test should compile AddOne bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(EntryFunction), TEXT("Builder cross-section dependency test should compile Entry bytecode")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("CrossSectionDependenciesCompileAndKeepSections.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder cross-section dependency test should execute across section boundaries")));
	}

	TEST_METHOD(NamespaceResolutionSeparatesTypesFunctionsAndGlobals)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder namespace test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderNamespaceResolution");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder namespace test should create a backing module")));

		const std::string NamespaceTypesSource = ASTEST_AS_ANSI(R"AS(
			namespace Inventory
			{
				const int Bonus = 5;

				class Item
				{
					int Count = 37;

					int Read()
					{
						return Count;
					}
				}

				int Score(Item Value)
				{
					return Value.Read() + Bonus;
				}

				int ScoreBase()
				{
					return 37 + Bonus;
				}
			}
			)AS");
		const std::string NamespaceEntrySource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return Inventory::ScoreBase();
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderNamespace_Types", NamespaceTypesSource.c_str(), TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.AddTypes")),
			TEXT("Builder namespace test should add the namespace type section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderNamespace_Entry", NamespaceEntrySource.c_str(), TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.AddEntry")),
			TEXT("Builder namespace test should add the entry section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder namespace test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder namespace test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder namespace test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		asITypeInfo* ItemType = Module->GetTypeInfoByDecl("Inventory::Item");
		asIScriptFunction* ScoreFunction = FindModuleFunctionByNameAndParamCount(Module, "Score", 1, "Inventory");
		asIScriptFunction* ScoreBaseFunction = FindModuleFunctionByNameAndParamCount(Module, "ScoreBase", 0, "Inventory");
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(ItemType, TEXT("Builder namespace test should expose the namespaced Item type")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(ItemType != nullptr ? ItemType->GetNamespace() : "")),
			TEXT("Builder namespace test should preserve Item namespace")));
		ASSERT_THAT(IsNotNull(ScoreFunction, TEXT("Builder namespace test should expose Inventory::Score")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(ScoreFunction != nullptr ? ScoreFunction->GetNamespace() : "")),
			TEXT("Builder namespace test should preserve Score namespace")));
		ASSERT_THAT(IsNotNull(ScoreBaseFunction, TEXT("Builder namespace test should expose Inventory::ScoreBase")));
		ASSERT_THAT(IsTrue(HasBytecode(ScoreBaseFunction), TEXT("Builder namespace test should compile ScoreBase bytecode")));
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder namespace test should expose global Entry")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), FString(UTF8_TO_TCHAR(EntryFunction != nullptr ? EntryFunction->GetNamespace() : "")),
			TEXT("Builder namespace test should keep Entry in the global namespace")));

		const int BonusGlobalIndex = FindGlobalVarIndexByNameAndNamespace(Module, "Bonus", "Inventory");
		ASSERT_THAT(IsTrue(BonusGlobalIndex >= 0, TEXT("Builder namespace test should expose Bonus global by name")));
		const char* GlobalName = nullptr;
		const char* GlobalNamespace = nullptr;
		int GlobalTypeId = asINVALID_TYPE;
		bool bIsConst = false;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->GetGlobalVar(static_cast<asUINT>(BonusGlobalIndex), &GlobalName, &GlobalNamespace, &GlobalTypeId, &bIsConst),
			TEXT("Builder namespace test should read Bonus global metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Bonus")), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")),
			TEXT("Builder namespace test should preserve Bonus global name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(GlobalNamespace != nullptr ? GlobalNamespace : "")),
			TEXT("Builder namespace test should preserve Bonus namespace")));
		ASSERT_THAT(IsTrue(bIsConst, TEXT("Builder namespace test should preserve Bonus constness")));
		ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl("int"), GlobalTypeId,
			TEXT("Builder namespace test should preserve Bonus int type")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder namespace test should execute namespace-qualified references")));
	}

	TEST_METHOD(ClassInheritanceResolvesBaseTypesAndInheritedCalls)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder inheritance test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderClassInheritance");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder inheritance test should create a backing module")));

		const std::string InheritanceSource = ASTEST_AS_ANSI(R"AS(
			class BaseState
			{
				int BaseValue = 40;

				int ReadBase()
				{
					return BaseValue;
				}
			}

			class DerivedState : BaseState
			{
				int Delta = 2;

				int ReadDelta()
				{
					return Delta;
				}
			}

			int UseInheritedMember()
			{
				DerivedState Value;
				return Value.ReadBase() + Value.ReadDelta();
			}

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderClassInheritance", InheritanceSource.c_str(), TEXT("ClassInheritanceResolvesBaseTypesAndInheritedCalls.AddInheritanceSection")),
			TEXT("Builder inheritance test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder inheritance test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder inheritance test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("ClassInheritanceResolvesBaseTypesAndInheritedCalls.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder inheritance test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		sClassDeclaration* BaseDeclaration = FindClassDeclarationByName(*Builder, "BaseState");
		sClassDeclaration* DerivedDeclaration = FindClassDeclarationByName(*Builder, "DerivedState");
		ASSERT_THAT(IsNotNull(BaseDeclaration,
			TEXT("Builder inheritance test should retain the BaseState declaration")));
		ASSERT_THAT(IsNotNull(DerivedDeclaration,
			TEXT("Builder inheritance test should retain the DerivedState declaration")));
		ASSERT_THAT(IsNotNull(BaseDeclaration != nullptr ? BaseDeclaration->typeInfo : nullptr,
			TEXT("Builder inheritance test should attach type info to BaseState")));
		ASSERT_THAT(IsNotNull(DerivedDeclaration != nullptr ? DerivedDeclaration->typeInfo : nullptr,
			TEXT("Builder inheritance test should attach type info to DerivedState")));

		asITypeInfo* BaseType = Module->GetTypeInfoByDecl("BaseState");
		asITypeInfo* DerivedType = Module->GetTypeInfoByDecl("DerivedState");
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Builder inheritance test should expose BaseState type metadata")));
		ASSERT_THAT(IsNotNull(DerivedType, TEXT("Builder inheritance test should expose DerivedState type metadata")));
		ASSERT_THAT(AreEqual(BaseType, DerivedType != nullptr ? DerivedType->GetBaseType() : nullptr,
			TEXT("Builder inheritance test should preserve the direct base type")));
		ASSERT_THAT(IsTrue(DerivedType != nullptr && BaseType != nullptr && DerivedType->DerivesFrom(BaseType),
			TEXT("Builder inheritance test should preserve the derives-from relationship")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(BaseType != nullptr ? BaseType->GetPropertyCount() : 0),
			TEXT("Builder inheritance test should layout BaseState.BaseValue")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(DerivedType != nullptr ? DerivedType->GetPropertyCount() : 0),
			TEXT("Builder inheritance test should layout DerivedState.Delta plus inherited BaseState.BaseValue")));

		asIScriptFunction* ReadBaseFunction = FindTypeMethodByNameAndParamCount(BaseType, "ReadBase", 0);
		asIScriptFunction* ReadDeltaFunction = FindTypeMethodByNameAndParamCount(DerivedType, "ReadDelta", 0);
		asIScriptFunction* InheritedEntryFunction = FindModuleFunctionByNameAndParamCount(Module, "UseInheritedMember", 0);
		ASSERT_THAT(IsNotNull(ReadBaseFunction, TEXT("Builder inheritance test should expose BaseState.ReadBase")));
		ASSERT_THAT(IsNotNull(ReadDeltaFunction, TEXT("Builder inheritance test should expose DerivedState.ReadDelta")));
		ASSERT_THAT(IsNotNull(InheritedEntryFunction, TEXT("Builder inheritance test should compile a function using inherited members")));
		ASSERT_THAT(IsTrue(HasBytecode(ReadBaseFunction), TEXT("Builder inheritance test should compile ReadBase bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(ReadDeltaFunction), TEXT("Builder inheritance test should compile ReadDelta bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(InheritedEntryFunction), TEXT("Builder inheritance test should compile inherited-member call bytecode")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "ReadBase", "BaseState"),
			TEXT("Builder inheritance test should retain BaseState.ReadBase function description")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "ReadDelta", "DerivedState"),
			TEXT("Builder inheritance test should retain DerivedState.ReadDelta function description")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("ClassInheritanceResolvesBaseTypesAndInheritedCalls.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder inheritance test should execute independent compiled bytecode")));
	}

	TEST_METHOD(ScriptInterfaceDeclarationFailsWithoutLeakingState)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder interface-boundary test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderScriptInterfaceBoundary");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder interface-boundary test should create a backing module")));

		const std::string InterfaceSource = ASTEST_AS_ANSI(R"AS(
			interface IBuilderThing
			{
				void Run();
			}

			class BuilderThing : IBuilderThing
			{
				void Run()
				{
				}
			}

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderScriptInterfaceBoundary", InterfaceSource.c_str(), TEXT("ScriptInterfaceDeclarationFailsWithoutLeakingState.AddInterfaceSection")),
			TEXT("Builder interface-boundary test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder interface-boundary test should create a builder")));

		LogBuilderState(TEXT("ScriptInterfaceDeclarationFailsWithoutLeakingState.BuildParallelParseScripts.before"), *Builder, Module, true, false);
		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(TEXT("ScriptInterfaceDeclarationFailsWithoutLeakingState.BuildParallelParseScripts.after"), ParseResult, *Builder, Module);
		ReportBuilderFailureDiagnostics();
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Builder interface-boundary test should reject script-level interface declarations at parse time")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0,
			TEXT("Builder interface-boundary test should record a builder error")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Builder->parsers.GetLength()),
			TEXT("Builder interface-boundary test should retain the failing parser for diagnostics")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->interfaceDeclarations.GetLength()),
			TEXT("Builder interface-boundary test should not register interface declarations after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->classDeclarations.GetLength()),
			TEXT("Builder interface-boundary test should not register class declarations after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder interface-boundary test should not produce function descriptions after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder interface-boundary test should not produce global descriptions after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()),
			TEXT("Builder interface-boundary test should not register object types after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder interface-boundary test should not register functions after parse failure")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"),
			TEXT("Builder interface-boundary test should not expose Entry after parse failure")));

		const FNativeMessageEntry* InterfaceMessage = Engine.GetMessages().Entries.Num() > 0 ? &Engine.GetMessages().Entries[0] : nullptr;
		ASSERT_THAT(IsNotNull(InterfaceMessage,
			TEXT("Builder interface-boundary test should report a diagnostic")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderScriptInterfaceBoundary")), InterfaceMessage != nullptr ? InterfaceMessage->Section : FString(),
			TEXT("Builder interface-boundary test should report the failing section")));
		ASSERT_THAT(IsTrue(InterfaceMessage != nullptr && InterfaceMessage->Row > 0,
			TEXT("Builder interface-boundary test should report a positive diagnostic row")));
		ASSERT_THAT(IsTrue(InterfaceMessage != nullptr && InterfaceMessage->Column > 0,
			TEXT("Builder interface-boundary test should report a positive diagnostic column")));
	}

	TEST_METHOD(DuplicateDeclarationsFailWithoutLeakingModuleState)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder duplicate-declaration test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderDuplicateDeclarations");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder duplicate-declaration test should create a backing module")));

		const std::string DuplicateDeclarationsSource = ASTEST_AS_ANSI(R"AS(
			class DuplicateType
			{
			}

			class DuplicateType
			{
			}

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderDuplicateDeclarations", DuplicateDeclarationsSource.c_str(), TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.AddDuplicateSection")),
			TEXT("Builder duplicate-declaration test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder duplicate-declaration test should create a builder")));

		LogBuilderState(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildParallelParseScripts.before"), *Builder, Module, true, false);
		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildParallelParseScripts.after"), ParseResult, *Builder, Module);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ParseResult,
			TEXT("Builder duplicate-declaration test should parse syntactically valid duplicate declarations")));
		LogBuilderState(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildGenerateTypes.before"), *Builder, Module, true, false);
		const int GenerateTypesResult = Builder->BuildGenerateTypes();
		LogBuilderStageResult(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildGenerateTypes.after"), GenerateTypesResult, *Builder, Module);
		ReportBuilderFailureDiagnostics();
		ASSERT_THAT(IsTrue(GenerateTypesResult < 0,
			TEXT("Builder duplicate-declaration test should reject duplicate class declarations during type generation")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0,
			TEXT("Builder duplicate-declaration test should record a builder error")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder duplicate-declaration test should not register functions after type-generation failure")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"),
			TEXT("Builder duplicate-declaration test should not expose Entry after type-generation failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder duplicate-declaration test should not produce function descriptions after type-generation failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder duplicate-declaration test should not produce global descriptions after type-generation failure")));

		const FNativeMessageEntry* DuplicateMessage = Engine.GetMessages().Entries.Num() > 0 ? &Engine.GetMessages().Entries[0] : nullptr;
		ASSERT_THAT(IsNotNull(DuplicateMessage,
			TEXT("Builder duplicate-declaration test should report a diagnostic")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderDuplicateDeclarations")), DuplicateMessage != nullptr ? DuplicateMessage->Section : FString(),
			TEXT("Builder duplicate-declaration test should report the failing section")));
		ASSERT_THAT(IsTrue(DuplicateMessage != nullptr && DuplicateMessage->Row > 0,
			TEXT("Builder duplicate-declaration test should report a positive diagnostic row")));
		ASSERT_THAT(IsTrue(DuplicateMessage != nullptr && DuplicateMessage->Column > 0,
			TEXT("Builder duplicate-declaration test should report a positive diagnostic column")));
	}

	TEST_METHOD(PropertyInitializersAndMethodOverloadsCompile)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder initializer and overload test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderInitializersAndOverloads");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder initializer and overload test should create a backing module")));

		const std::string InitializersAndOverloadsSource = ASTEST_AS_ANSI(R"AS(
			class Accumulator
			{
				int Base = 40;
				int Delta = 2;

				int Add()
				{
					return Base + Delta;
				}

				int Add(int Extra)
				{
					return Base + Delta + Extra;
				}
			}

			int Entry()
			{
				return 40 + 2;
			}

			int EntryWithArg()
			{
				return 40 + 2 + 5;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderInitializersAndOverloads", InitializersAndOverloadsSource.c_str(), TEXT("PropertyInitializersAndMethodOverloadsCompile.AddInitializersAndOverloads")),
			TEXT("Builder initializer and overload test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder initializer and overload test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder initializer and overload test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("PropertyInitializersAndMethodOverloadsCompile.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder initializer and overload test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		sClassDeclaration* AccumulatorDeclaration = FindClassDeclarationByName(*Builder, "Accumulator");
		ASSERT_THAT(IsNotNull(AccumulatorDeclaration,
			TEXT("Builder initializer and overload test should retain the Accumulator declaration")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorDeclaration != nullptr ? AccumulatorDeclaration->propInits.GetLength() : 0),
			TEXT("Builder initializer and overload test should retain both property initializers")));

		asITypeInfo* AccumulatorType = Module->GetTypeInfoByDecl("Accumulator");
		ASSERT_THAT(IsNotNull(AccumulatorType, TEXT("Builder initializer and overload test should expose Accumulator type metadata")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorType != nullptr ? AccumulatorType->GetPropertyCount() : 0),
			TEXT("Builder initializer and overload test should layout both properties")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorType != nullptr ? AccumulatorType->GetMethodCount() : 0),
			TEXT("Builder initializer and overload test should expose both overload methods")));
		asIScriptFunction* AddNoArg = FindTypeMethodByNameAndParamCount(AccumulatorType, "Add", 0);
		asIScriptFunction* AddWithArg = FindTypeMethodByNameAndParamCount(AccumulatorType, "Add", 1);
		ASSERT_THAT(IsNotNull(AddNoArg, TEXT("Builder initializer and overload test should expose Add()")));
		ASSERT_THAT(IsNotNull(AddWithArg, TEXT("Builder initializer and overload test should expose Add(int)")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(AddNoArg != nullptr ? AddNoArg->GetParamCount() : 0),
			TEXT("Builder initializer and overload test should preserve Add() parameter count")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(AddWithArg != nullptr ? AddWithArg->GetParamCount() : 0),
			TEXT("Builder initializer and overload test should preserve Add(int) parameter count")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("PropertyInitializersAndMethodOverloadsCompile.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder initializer and overload test should execute property initializers through Add()")));

		int32 ResultWithArg = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int EntryWithArg()", ResultWithArg))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("PropertyInitializersAndMethodOverloadsCompile.EntryWithArg"), "int EntryWithArg()", ResultWithArg);
		ASSERT_THAT(AreEqual(47, ResultWithArg, TEXT("Builder initializer and overload test should execute Add(int) overload")));
	}

	TEST_METHOD(OverloadedGlobalFunctionsRetainDistinctDescriptions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global overload test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalOverloads");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder global overload test should create a backing module")));

		const std::string GlobalOverloadsSource = ASTEST_AS_ANSI(R"AS(
			int Pick()
			{
				return 40;
			}

			int Pick(int Value)
			{
				return Value + 2;
			}

			int Entry()
			{
				return Pick() + Pick(0);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderGlobalOverloads", GlobalOverloadsSource.c_str(), TEXT("OverloadedGlobalFunctionsRetainDistinctDescriptions.AddGlobalOverloads")),
			TEXT("Builder global overload test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder global overload test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder global overload test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("OverloadedGlobalFunctionsRetainDistinctDescriptions.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder global overload test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		ASSERT_THAT(AreEqual(2, CountGlobalFunctionDescriptions(*Builder, "Pick"),
			TEXT("Builder global overload test should retain both Pick descriptions")));
		asIScriptFunction* PickNoArg = FindModuleFunctionByNameAndParamCount(Module, "Pick", 0);
		asIScriptFunction* PickWithArg = FindModuleFunctionByNameAndParamCount(Module, "Pick", 1);
		ASSERT_THAT(IsNotNull(PickNoArg, TEXT("Builder global overload test should expose Pick()")));
		ASSERT_THAT(IsNotNull(PickWithArg, TEXT("Builder global overload test should expose Pick(int)")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(PickNoArg != nullptr ? PickNoArg->GetParamCount() : 0),
			TEXT("Builder global overload test should preserve Pick() parameter count")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(PickWithArg != nullptr ? PickWithArg->GetParamCount() : 0),
			TEXT("Builder global overload test should preserve Pick(int) parameter count")));
		ASSERT_THAT(IsTrue(HasBytecode(PickNoArg), TEXT("Builder global overload test should compile Pick() bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(PickWithArg), TEXT("Builder global overload test should compile Pick(int) bytecode")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("OverloadedGlobalFunctionsRetainDistinctDescriptions.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder global overload test should dispatch both global overloads")));
	}
};

#endif
