#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderFunctionTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderFunction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	void LogBuilderState(const FString& Stage, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true, bool bIncludeDiagnosticCounters = true) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(
			TEXT("[Builder][%s] add section name=%s bytes=%d lines=%d"),
			*Stage,
			*ToTestString(SectionName),
			Source != nullptr ? static_cast<int32>(std::strlen(Source)) : 0,
			CountSourceLines(Source)));
	}

	void LogBuilderStageResult(const FString& Stage, int Result, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(TEXT("[Builder][%s] result=%d"), *Stage, Result));
		LogBuilderState(Stage, Builder, Module, bExpandBuilderDescriptions);
	}

	void LogScriptExecutionResult(const FString& Stage, const char* Declaration, int32 Result) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		LogBuilderState(FString::Printf(TEXT("%s.before"), *Stage), Builder, Module, true, false);
		const int Result = (Builder.*StageMethod)();
		LogBuilderStageResult(FString::Printf(TEXT("%s.after"), *Stage), Result, Builder, Module);
		return Result == asSUCCESS;
	}

	bool RunBuilderPipelineThroughLayout(asCBuilder& Builder, const asCModule* Module = nullptr) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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

	TEST_METHOD(CompileFunctionUsesProvidedSectionName)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction section test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionSection");
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
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction failure test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionFailure");
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
	TEST_METHOD(GenerateFunctionsRegistersGlobalsAndFunctions)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder function-generation test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateFunctions");
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
	TEST_METHOD(OverloadedGlobalFunctionsRetainDistinctDescriptions)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global overload test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalOverloads");
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
