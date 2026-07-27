#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

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

	void ReportBuilderFailureDiagnostics(const AngelscriptNativeTestSupport::FNativeTestEngine& Engine) const
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
	TEST_METHOD(ModuleBuildCreatesAndDestroysBuilder)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained successful Module::Build lifecycle smoke; COMPILER-BUILDER-LIFECYCLE owns success, failure, reset, and rebuild phases.");
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder module-lifecycle test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderModuleLifecycle");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder module-lifecycle test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
		)AS");
		PrintGeneratedAsSource(*TestRunner, TEXT("COMPILER-BUILDER-LIFECYCLE-LEGACY-SUCCESS"), TEXT("Lifecycle.as"), UTF8_TO_TCHAR(Source.c_str()));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Lifecycle.as", Source.c_str(), TEXT("ModuleLifecycle.Add")), TEXT("Builder module-lifecycle test should add source")));
		ASSERT_THAT(IsNotNull(Module->builder, TEXT("Adding source should create the module builder")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->Build(), TEXT("Module Build should complete the builder pipeline")));
		ASSERT_THAT(IsNull(Module->builder, TEXT("Successful Module Build should release its transient builder")));
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Entry, TEXT("Successful Module Build should publish Entry")));
		ASSERT_THAT(IsTrue(HasBytecode(Entry), TEXT("Successful Module Build should compile Entry bytecode")));
	}

	TEST_METHOD(BuilderLifecycleClearsTransientStateAndRebuilds)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BUILDER-LIFECYCLE",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder failed-build lifecycle test should create an engine")));
		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
		)AS");
		const std::string BrokenSource = ASTEST_AS_ANSI(R"AS(
			int Entry(
			{
				return 0;
			}
		)AS");
		const TCHAR* Operations[] = { TEXT("module_success"), TEXT("module_failure"), TEXT("standalone_reset") };
		const TCHAR* Phases[] = { TEXT("before"), TEXT("after"), TEXT("rebuild") };
		for (const TCHAR* Operation : Operations)
		{
			for (const TCHAR* Phase : Phases)
			{
				const FString CaseId = MakeNativeCaseId("COMPILER-BUILDER-LIFECYCLE", { Operation, Phase });
				const std::string& ReviewSource = FCString::Strcmp(Operation, TEXT("module_failure")) == 0 ? BrokenSource : GoodSource;
				PrintGeneratedAsSource(*TestRunner, CaseId, TEXT("BuilderLifecycle.as"), UTF8_TO_TCHAR(ReviewSource.c_str()));
			}
		}

		{
			FScopedNativeModuleName SuccessScope(Engine, "BuilderLifecycleSuccessOwner");
			asCModule* SuccessModule = CreateBuilderModule(ScriptEngine, SuccessScope.Get());
			ASSERT_THAT(IsNotNull(SuccessModule, TEXT("Lifecycle owner should create the successful module")));
			ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*SuccessModule, "Success.as", GoodSource.c_str(), TEXT("LifecycleOwner.Success.Before")),
				TEXT("Lifecycle owner should add successful source")));
			ASSERT_THAT(IsNotNull(SuccessModule->builder, TEXT("Successful build should own a transient builder before Build")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SuccessModule->Build(),
				TEXT("Successful lifecycle route should build")));
			ASSERT_THAT(IsNull(SuccessModule->builder, TEXT("Successful build should release its transient builder")));
			asIScriptFunction* SuccessEntry = SuccessModule->GetFunctionByDecl("int Entry()");
			ASSERT_THAT(IsNotNull(SuccessEntry, TEXT("Successful lifecycle route should publish Entry")));
			ASSERT_THAT(IsTrue(SuccessEntry != nullptr && HasBytecode(SuccessEntry),
				TEXT("Successful lifecycle route should publish executable Entry bytecode")));
			SuccessModule = CreateBuilderModule(ScriptEngine, SuccessScope.Get());
			ASSERT_THAT(IsNotNull(SuccessModule, TEXT("Successful lifecycle route should recreate the same-name module")));
			ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*SuccessModule, "SuccessRebuild.as", GoodSource.c_str(), TEXT("LifecycleOwner.Success.Rebuild")),
				TEXT("Successful lifecycle route should add same-name rebuild source")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SuccessModule->Build(),
				TEXT("Successful lifecycle route should rebuild under the same owned name")));
			asIScriptFunction* SuccessRebuiltEntry = SuccessModule->GetFunctionByDecl("int Entry()");
			ASSERT_THAT(IsTrue(SuccessRebuiltEntry != nullptr && HasBytecode(SuccessRebuiltEntry),
				TEXT("Successful same-name rebuild should publish executable Entry bytecode")));
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderFailedLifecycle");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder failed-build lifecycle test should create a module")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Broken.as", BrokenSource.c_str(), TEXT("FailedLifecycle.AddBroken")), TEXT("Builder failed-build lifecycle test should add broken source")));
		ASSERT_THAT(IsTrue(Module->Build() < 0, TEXT("Broken source should fail Module Build")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, TEXT("Failed Module Build should retain a diagnostic")));
		ASSERT_THAT(IsNull(Module->builder, TEXT("Failed Module Build should clear its transient builder")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()), TEXT("Failed Module Build should not publish functions")));
		Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Failed Module Build should allow a replacement module")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "Good.as", GoodSource.c_str(), TEXT("FailedLifecycle.AddGood")), TEXT("Builder failed-build lifecycle test should add replacement source")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->Build(), TEXT("Replacement module should build after a prior failure")));
		asIScriptFunction* RebuiltEntry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(RebuiltEntry, TEXT("Replacement module should publish Entry")));
		ASSERT_THAT(IsTrue(RebuiltEntry != nullptr && HasBytecode(RebuiltEntry), TEXT("Replacement module should publish executable Entry")));

		{
			FScopedNativeModuleName ResetScope(Engine, "BuilderLifecycleResetOwner");
			asCModule* ResetModule = CreateBuilderModule(ScriptEngine, ResetScope.Get());
			ASSERT_THAT(IsNotNull(ResetModule, TEXT("Lifecycle owner should create the standalone-reset module")));
			ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*ResetModule, "Reset.as", GoodSource.c_str(), TEXT("LifecycleOwner.Reset.Before")),
				TEXT("Lifecycle owner should add standalone-reset source")));
			asCBuilder* ResetBuilder = ResetModule->builder;
			ASSERT_THAT(IsNotNull(ResetBuilder, TEXT("Standalone-reset route should create a builder")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ResetBuilder->BuildParallelParseScripts(),
				TEXT("Standalone-reset route should parse before reset")));
			const int32 ParserCount = static_cast<int32>(ResetBuilder->parsers.GetLength());
			ResetBuilder->scriptsParsed = true;
			ResetBuilder->numErrors = 3;
			ResetBuilder->numWarnings = 2;
			ResetBuilder->Reset();
			ASSERT_THAT(IsFalse(ResetBuilder->scriptsParsed, TEXT("Standalone Reset should clear scriptsParsed")));
			ASSERT_THAT(AreEqual(0, ResetBuilder->numErrors, TEXT("Standalone Reset should clear errors")));
			ASSERT_THAT(AreEqual(0, ResetBuilder->numWarnings, TEXT("Standalone Reset should clear warnings")));
			ASSERT_THAT(AreEqual(ParserCount, static_cast<int32>(ResetBuilder->parsers.GetLength()),
				TEXT("Standalone Reset should retain owned parser instances")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ResetModule->Build(),
				TEXT("Module Build should rebuild cleanly after standalone Reset")));
			ASSERT_THAT(IsNull(ResetModule->builder,
				TEXT("Rebuild after standalone Reset should release the transient builder")));
			asIScriptFunction* ResetRebuiltEntry = ResetModule->GetFunctionByDecl("int Entry()");
			ASSERT_THAT(IsTrue(ResetRebuiltEntry != nullptr && HasBytecode(ResetRebuiltEntry),
				TEXT("Rebuild after standalone Reset should publish executable Entry bytecode")));
		}
	}

	TEST_METHOD(StandaloneBuilderResetClearsTransientState)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained focused Reset smoke; COMPILER-BUILDER-LIFECYCLE owns reset state, parser ownership, and module rebuild phases.");
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder reset test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderReset");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder reset test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class Value
			{
				int Number;
			}

			int Entry()
			{
				return 1;
			}
		)AS");
		PrintGeneratedAsSource(*TestRunner, TEXT("COMPILER-BUILDER-LIFECYCLE-LEGACY-RESET"), TEXT("Reset.as"), UTF8_TO_TCHAR(Source.c_str()));
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
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained parse-stage failure publication smoke; COMPILER-BUILDER-LIFECYCLE owns failure teardown and rebuild while COMPILER-BUILDER-PARSE-STAGE owns parse-only publication barriers.");
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
		PrintGeneratedAsSource(*TestRunner, TEXT("COMPILER-BUILDER-LIFECYCLE-LEGACY-STAGE-FAILURE"), TEXT("BuilderStageFailure.as"), UTF8_TO_TCHAR(BrokenStageSource.c_str()));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderStageFailure", BrokenStageSource.c_str(), TEXT("StageFailureStopsBeforeExecutableCode.AddBrokenSection")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder stage failure test should create a builder")));

		Builder->silent = true;
		LogBuilderState(TEXT("StageFailureStopsBeforeExecutableCode.BuildParallelParseScripts.before"), *Builder, Module, true, false);
		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(TEXT("StageFailureStopsBeforeExecutableCode.BuildParallelParseScripts.after"), ParseResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(Engine);
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
