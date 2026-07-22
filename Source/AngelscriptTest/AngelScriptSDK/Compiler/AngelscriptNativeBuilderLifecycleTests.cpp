#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderLifecycleTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(ModuleBuildCreatesAndDestroysBuilder)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder module-lifecycle test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderModuleLifecycle");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder module-lifecycle test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry() { return 42; }
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Lifecycle.as", Source.c_str(), TEXT("ModuleLifecycle.Add")), TEXT("Builder module-lifecycle test should add source")));
		ASSERT_THAT(IsNotNull(Module->builder, TEXT("Adding source should create the module builder")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->Build(), TEXT("Module Build should complete the builder pipeline")));
		ASSERT_THAT(IsNull(Module->builder, TEXT("Successful Module Build should release its transient builder")));
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Entry, TEXT("Successful Module Build should publish Entry")));
		ASSERT_THAT(IsTrue(HasBytecode(Entry), TEXT("Successful Module Build should compile Entry bytecode")));
	}

	TEST_METHOD(FailedBuildClearsBuilderAndAllowsCleanRebuild)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder failed-build lifecycle test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderFailedLifecycle");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder failed-build lifecycle test should create a module")));
		const std::string BrokenSource = ASTEST_AS_ANSI(R"AS(
			int Entry( { return 0; }
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Broken.as", BrokenSource.c_str(), TEXT("FailedLifecycle.AddBroken")), TEXT("Builder failed-build lifecycle test should add broken source")));
		ASSERT_THAT(IsTrue(Module->Build() < 0, TEXT("Broken source should fail Module Build")));
		ASSERT_THAT(IsNull(Module->builder, TEXT("Failed Module Build should clear its transient builder")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()), TEXT("Failed Module Build should not publish functions")));
		Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Failed Module Build should allow a replacement module")));
		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Entry() { return 42; }
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Good.as", GoodSource.c_str(), TEXT("FailedLifecycle.AddGood")), TEXT("Builder failed-build lifecycle test should add replacement source")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->Build(), TEXT("Replacement module should build after a prior failure")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Replacement module should publish Entry")));
	}

	TEST_METHOD(StandaloneBuilderResetClearsTransientState)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder reset test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderReset");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder reset test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class Value { int Number; }
			int Entry() { return 1; }
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Reset.as", Source.c_str(), TEXT("BuilderReset.Add")), TEXT("Builder reset test should add source")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder reset test should create a builder")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildParallelParseScripts(), TEXT("Builder reset test should parse source")));
		ASSERT_THAT(IsTrue(Builder->parsers.GetLength() > 0, TEXT("Builder reset test should have parser state before reset")));
		Builder->scriptsParsed = true;
		Builder->numErrors = 3;
		Builder->numWarnings = 2;
		Builder->Reset();
		ASSERT_THAT(IsFalse(Builder->scriptsParsed, TEXT("Builder Reset should clear the parsed-state flag")));
		ASSERT_THAT(AreEqual(0, Builder->numErrors, TEXT("Builder Reset should clear transient error count")));
		ASSERT_THAT(AreEqual(0, Builder->numWarnings, TEXT("Builder Reset should clear transient warning count")));
		ASSERT_THAT(IsTrue(Builder->parsers.GetLength() > 0, TEXT("Builder Reset should retain parser ownership until builder destruction")));
	}

	TEST_METHOD(StageFailureStopsBeforeExecutableCode)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder stage failure test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderStageFailure");
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
};

#endif
