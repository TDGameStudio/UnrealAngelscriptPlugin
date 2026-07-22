#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderDependencyTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(DirectMarkDependencyRecordsModuleAndSourceLocation)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Direct dependency test should create an engine")));
		FScopedNativeModule Dependency(*TestRunner, Engine, "DependencyDirect", "int DependencyValue() { return 7; }");
		if (!Dependency.IsValid()) return;
		FScopedNativeModuleName ModuleScope(Engine, "DependentDirect");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Direct dependency test should create a dependent module")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "DependentDirect.as", "int Entry() { return 1; }", TEXT("DirectDependency.Add")), TEXT("Direct dependency test should create a builder")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Direct dependency test should expose the builder")));
		asCModule* DependencyModule = static_cast<asCModule*>(Dependency.Get());
		Builder->MarkDependency(DependencyModule, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(DependencyModule);
		ASSERT_THAT(IsNotNull(Info, TEXT("Direct dependency should record the dependency module")));
		ASSERT_THAT(AreEqual(0, Info != nullptr ? Info->FirstLineNumber : INDEX_NONE, TEXT("Dependency without a node should record a zero source line")));
		ASSERT_THAT(AreEqual(0, Info != nullptr ? Info->FirstColumn : INDEX_NONE, TEXT("Dependency without a node should record a zero source column")));
		Builder->MarkDependency(DependencyModule, nullptr, nullptr);
		ASSERT_THAT(AreEqual(1, Module->moduleDependencies.Num(), TEXT("Repeated direct dependency marking should not duplicate the record")));
	}

	TEST_METHOD(ExplicitMarkStructuralDependencyRecordsStructuralFlag)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Structural dependency test should create an engine")));
		FScopedNativeModule Dependency(*TestRunner, Engine, "DependencyStructural", "class DependencyType { int Value; }");
		if (!Dependency.IsValid()) return;
		FScopedNativeModuleName ModuleScope(Engine, "DependentStructural");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Structural dependency test should create a dependent module")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "DependentStructural.as", "class UserType { int Value; }", TEXT("StructuralDependency.Add")), TEXT("Structural dependency test should create a builder")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Structural dependency test should expose the builder")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildParallelParseScripts(), TEXT("Structural dependency test should parse the dependent source")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildGenerateTypes(), TEXT("Structural dependency test should register the user type")));
		asCTypeInfo* UserType = static_cast<asCTypeInfo*>(Module->GetTypeInfoByDecl("UserType"));
		asCTypeInfo* DependencyType = static_cast<asCTypeInfo*>(Dependency.Get()->GetTypeInfoByDecl("DependencyType"));
		ASSERT_THAT(IsNotNull(UserType, TEXT("Structural dependency test should register the user type")));
		ASSERT_THAT(IsNotNull(DependencyType, TEXT("Structural dependency test should expose the dependency type")));
		Builder->MarkStructuralDependency(UserType, DependencyType, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(static_cast<asCModule*>(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Info, TEXT("Structural dependency should record the dependency module")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsStructuralDependency, TEXT("Structural dependency should set its structural flag")));
	}

	TEST_METHOD(DefaultConstructorCallMarksHardValueDependency)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Hard-value dependency test should create an engine")));
		FScopedNativeModule Dependency(*TestRunner, Engine, "DependencyHardValue", "int ConstructedValue() { return 42; }");
		if (!Dependency.IsValid()) return;
		FScopedNativeModuleName ModuleScope(Engine, "DependentHardValue");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Hard-value dependency test should create a dependent module")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "DependentHardValue.as", "int Entry() { return 0; }", TEXT("HardValueDependency.Add")), TEXT("Hard-value dependency test should create a builder")));
		asCBuilder* Builder = Module->builder;
		asCScriptFunction* DependencyFunction = static_cast<asCScriptFunction*>(Dependency.Get()->GetFunctionByDecl("int ConstructedValue()"));
		ASSERT_THAT(IsNotNull(Builder, TEXT("Hard-value dependency test should expose the builder")));
		ASSERT_THAT(IsNotNull(DependencyFunction, TEXT("Hard-value dependency test should expose the dependency function")));
		Builder->bValueDependenciesAreHard = true;
		Builder->MarkDependency(DependencyFunction, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(static_cast<asCModule*>(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Info, TEXT("Hard-value dependency should record the dependency module")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsHardValueDependency, TEXT("Value dependency under the hard-value policy should set its hard flag")));
	}

	TEST_METHOD(GlobalInitializerRejectsCrossModuleFunctionDependency)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Cross-module initializer test should create an engine")));
		FScopedNativeModule Dependency(*TestRunner, Engine, "DependencyInitializer", "int InitializerValue() { return 9; }");
		if (!Dependency.IsValid()) return;
		FScopedNativeModuleName ModuleScope(Engine, "DependentInitializer");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Cross-module initializer test should create a dependent module")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "DependentInitializer.as", "int Value = InitializerValue();", TEXT("InitializerDependency.Add")), TEXT("Cross-module initializer test should create a builder")));
		asCBuilder* Builder = Module->builder;
		asCScriptFunction* DependencyFunction = static_cast<asCScriptFunction*>(Dependency.Get()->GetFunctionByDecl("int InitializerValue()"));
		ASSERT_THAT(IsNotNull(Builder, TEXT("Cross-module initializer test should expose the builder")));
		ASSERT_THAT(IsNotNull(DependencyFunction, TEXT("Cross-module initializer test should expose the dependency function")));
		Builder->bValueDependenciesAreHard = true;
		Builder->MarkDependency(DependencyFunction, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(static_cast<asCModule*>(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Info, TEXT("Cross-module function use should be tracked before global initialization")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsHardValueDependency, TEXT("Cross-module initializer function dependencies should be hard value dependencies")));
		ASSERT_THAT(IsTrue(Module->Build() < 0, TEXT("A global initializer must reject an unavailable cross-module function")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, TEXT("Rejected cross-module global initialization should report a diagnostic")));
	}

	TEST_METHOD(CrossSectionDependenciesCompileAndKeepSections)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder cross-section dependency test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderCrossSectionDependencies");
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
};

#endif
