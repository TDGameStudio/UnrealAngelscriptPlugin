#include "AngelscriptBuilderTestSupport.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptBuilderTestSupport;
using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(ModuleBuildCreatesAndDestroysBuilder)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder lifecycle test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderLifecycleModuleBuild");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder lifecycle test should create a module")));
		ASSERT_THAT(IsNull(Module->builder, TEXT("Builder lifecycle test should start without a builder before adding sections")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderLifecycleModuleBuild.as", Source.c_str(), TEXT("ModuleBuild.AddSection")),
			TEXT("Builder lifecycle test should add the script section")));
		asCBuilder* InitialBuilder = Module->builder;
		ASSERT_THAT(IsNotNull(InitialBuilder, TEXT("Builder lifecycle test should create a builder after adding a section")));

		const int BuildResult = Module->Build();
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), BuildResult, TEXT("Builder lifecycle test should build the module successfully")));
		ASSERT_THAT(IsNull(Module->builder, TEXT("Builder lifecycle test should destroy and clear module->builder after Build")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Builder lifecycle test should publish Entry after Build")));
	}

	TEST_METHOD(FailedBuildClearsBuilderAndAllowsCleanRebuild)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder failed-build lifecycle test should create a standalone SDK engine")));

		{
			FScopedNativeModuleName BadModuleScope(Engine, "BuilderLifecycleFailedBuild");
			asCModule* BadModule = CreateBuilderModule(ScriptEngine, BadModuleScope.Get());
			ASSERT_THAT(IsNotNull(BadModule, TEXT("Builder failed-build lifecycle test should create the broken module")));

			const std::string BrokenSource = ASTEST_AS_ANSI(R"AS(
				int Entry(
				{
					return 1;
				}
				)AS");
			ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *BadModule, "BuilderLifecycleFailedBuild.as", BrokenSource.c_str(), TEXT("FailedBuild.AddBrokenSection")),
				TEXT("Builder failed-build lifecycle test should add the broken section")));
			ASSERT_THAT(IsNotNull(BadModule->builder, TEXT("Builder failed-build lifecycle test should create a builder for the broken module")));

			const int BadBuildResult = BadModule->Build();
			ReportBuilderFailureDiagnostics(*TestRunner, Engine);
			ASSERT_THAT(IsTrue(BadBuildResult < 0, TEXT("Builder failed-build lifecycle test should fail the broken build")));
			ASSERT_THAT(IsNull(BadModule->builder, TEXT("Builder failed-build lifecycle test should clear module->builder after failure")));
			ASSERT_THAT(AreEqual(0, static_cast<int32>(BadModule->GetFunctionCount()), TEXT("Builder failed-build lifecycle test should reset failed module functions")));
		}

		Engine.ResetMessages();
		FScopedNativeModuleName GoodModuleScope(Engine, "BuilderLifecycleFailedBuild");
		asCModule* GoodModule = CreateBuilderModule(ScriptEngine, GoodModuleScope.Get());
		ASSERT_THAT(IsNotNull(GoodModule, TEXT("Builder failed-build lifecycle test should recreate the module name")));

		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *GoodModule, "BuilderLifecycleRecovery.as", GoodSource.c_str(), TEXT("FailedBuild.AddRecoverySection")),
			TEXT("Builder failed-build lifecycle test should add the recovery section")));
		const int GoodBuildResult = GoodModule->Build();
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), GoodBuildResult, TEXT("Builder failed-build lifecycle test should rebuild cleanly with the same module name")));
		ASSERT_THAT(IsNull(GoodModule->builder, TEXT("Builder failed-build lifecycle test should clear builder after recovery build")));
		ASSERT_THAT(IsNotNull(GoodModule->GetFunctionByDecl("int Entry()"), TEXT("Builder failed-build lifecycle test should publish recovery Entry")));
	}

	TEST_METHOD(StandaloneBuilderResetClearsTransientState)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Standalone builder lifecycle test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderLifecycleStandaloneReset");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Standalone builder lifecycle test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class ResetCarrier
			{
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderLifecycleStandaloneReset.as", Source.c_str(), TEXT("StandaloneReset.AddSection")),
			TEXT("Standalone builder lifecycle test should add the script section")));
		asCBuilder* ModuleBuilder = Module->builder;
		ASSERT_THAT(IsNotNull(ModuleBuilder, TEXT("Standalone builder lifecycle test should create a module builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *ModuleBuilder, TEXT("StandaloneReset.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Standalone builder lifecycle test should parse the section")));
		ASSERT_THAT(IsTrue(ModuleBuilder->parsers.GetLength() > 0, TEXT("Standalone builder lifecycle test should have parser state before reset")));

		ModuleBuilder->numErrors = 7;
		ModuleBuilder->numWarnings = 3;
		ModuleBuilder->Reset();
		LogBuilderState(*TestRunner, TEXT("StandaloneReset.AfterReset"), *ModuleBuilder, Module);
		ASSERT_THAT(AreEqual(0, ModuleBuilder->numErrors, TEXT("Standalone builder lifecycle test should reset error count")));
		ASSERT_THAT(AreEqual(0, ModuleBuilder->numWarnings, TEXT("Standalone builder lifecycle test should reset warning count")));
		ASSERT_THAT(IsTrue(ModuleBuilder->parsers.GetLength() > 0, TEXT("Standalone builder lifecycle test should keep parsed AST ownership outside Reset")));
		ASSERT_THAT(IsFalse(ModuleBuilder->scriptsParsed, TEXT("Standalone builder lifecycle test should reset scriptsParsed")));

		asCBuilder TemporaryBuilder(static_cast<asCScriptEngine*>(ScriptEngine), nullptr);
		ASSERT_THAT(IsNull(TemporaryBuilder.module, TEXT("Standalone builder lifecycle test should allow a null module builder")));
		ASSERT_THAT(AreEqual(ScriptEngine, static_cast<asIScriptEngine*>(TemporaryBuilder.engine),
			TEXT("Standalone builder lifecycle test should keep the owning script engine")));
		TemporaryBuilder.Reset();
		ASSERT_THAT(AreEqual(0, TemporaryBuilder.numErrors, TEXT("Standalone builder lifecycle test should reset null-module builder errors")));
	}
};

#endif
