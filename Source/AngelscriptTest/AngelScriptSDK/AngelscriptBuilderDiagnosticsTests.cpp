#include "AngelscriptBuilderTestSupport.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderDiagnosticsTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(CompilerMessageCollectorCanMatchWarnings)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		AngelscriptNativeTestSupport::FNativeMessageEntry Warning;
		Warning.Section = TEXT("BuilderDiagSyntheticWarning.as");
		Warning.Row = 7;
		Warning.Column = 3;
		Warning.Type = asMSGTYPE_WARNING;
		Warning.Message = TEXT("Synthetic builder warning");
		Messages.Entries.Add(Warning);

		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Messages,
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Warning(TEXT("BuilderDiagSyntheticWarning.as"), 7, TEXT("builder warning"), 3),
			TEXT("synthetic warning diagnostic should match type, section, row, column and message"))));
	}

	TEST_METHOD(ParseErrorReportsSectionAndDoesNotPublishDeclarations)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagParseError");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry(
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagParseError.as", Source.c_str(), TEXT("ParseError.AddSection")),
			TEXT("Builder diagnostic test should add the broken parse section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder diagnostic test should create a builder")));

		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(*TestRunner, TEXT("ParseError.BuildParallelParseScripts"), ParseResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Builder diagnostic parse error should fail during parse")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder diagnostic parse error should increment builder errors")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->classDeclarations.GetLength()),
			TEXT("Builder diagnostic parse error should not publish class declarations")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder diagnostic parse error should not publish function descriptions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder diagnostic parse error should not publish module functions")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDiagParseError.as"), 2, TEXT("Expected")),
			TEXT("parse diagnostic should carry section, row and message keyword"))));
	}

	TEST_METHOD(DuplicateClassFailsDuringTypeGenerationWithoutFunctionLeak)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder duplicate diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagDuplicateClass");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder duplicate diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
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
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagDuplicateClass.as", Source.c_str(), TEXT("DuplicateClass.AddSection")),
			TEXT("Builder duplicate diagnostic test should add the duplicate declaration section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder duplicate diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DuplicateClass.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder duplicate diagnostic test should parse syntactically valid duplicate declarations")));

		const int GenerateTypesResult = Builder->BuildGenerateTypes();
		LogBuilderStageResult(*TestRunner, TEXT("DuplicateClass.BuildGenerateTypes"), GenerateTypesResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(GenerateTypesResult < 0, TEXT("Builder duplicate diagnostic test should fail during type generation")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder duplicate diagnostic test should increment builder errors")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder duplicate diagnostic test should not register function descriptions after type failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder duplicate diagnostic test should not register globals after type failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder duplicate diagnostic test should not publish Entry after type failure")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"),
			TEXT("Builder duplicate diagnostic test should not expose Entry after type failure")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDiagDuplicateClass.as"), INDEX_NONE, TEXT("DuplicateType")),
			TEXT("duplicate diagnostic should mention the duplicate class"))));
	}

	TEST_METHOD(UnknownTypeReportsFunctionSectionAndKeepsBytecodeEmpty)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder unknown-type diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagUnknownType");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder unknown-type diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				GhostType Value;
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagUnknownType.as", Source.c_str(), TEXT("UnknownType.AddSection")),
			TEXT("Builder unknown-type diagnostic test should add the broken function section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder unknown-type diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module),
			TEXT("Builder unknown-type diagnostic test should build through layout before codegen catches function body errors")));

		const int CompileResult = Builder->BuildCompileCode();
		LogBuilderStageResult(*TestRunner, TEXT("UnknownType.BuildCompileCode"), CompileResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Builder unknown-type diagnostic test should fail during code compilation")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder unknown-type diagnostic test should increment builder errors")));
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder unknown-type diagnostic test should have registered Entry before codegen")));
		ASSERT_THAT(IsFalse(HasBytecode(EntryFunction), TEXT("Builder unknown-type diagnostic test should not produce Entry bytecode")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDiagUnknownType.as"), 3, TEXT("GhostType")),
			TEXT("unknown-type diagnostic should carry section, row and symbol keyword"))));
	}

	TEST_METHOD(WarningReportsSectionRowAndDoesNotFailByDefault)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder warning diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagWarningDefault");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder warning diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class WarningCarrier
			{
				int Pick(int Value)
				{
					return Value;
				}

				int Pick(int Value, int Extra = 1)
				{
					return Value + Extra;
				}
			}

			int Entry()
			{
				WarningCarrier Carrier;
				return Carrier.Pick(41, 1);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagWarningDefault.as", Source.c_str(), TEXT("WarningDefault.AddSection")),
			TEXT("Builder warning diagnostic test should add the warning section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder warning diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module),
			TEXT("Builder warning diagnostic test should build through layout with a non-fatal warning")));

		const int CompileResult = Builder->BuildCompileCode();
		LogBuilderStageResult(*TestRunner, TEXT("WarningDefault.BuildCompileCode"), CompileResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), CompileResult,
			TEXT("Builder warning diagnostic test should not fail default warning mode")));
		ASSERT_THAT(AreEqual(0, Builder->numErrors, TEXT("Builder warning diagnostic test should not convert warnings to errors by default")));
		ASSERT_THAT(IsTrue(Builder->numWarnings > 0, TEXT("Builder warning diagnostic test should increment warning count")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Warning(TEXT("BuilderDiagWarningDefault.as"), 8, TEXT("overload")),
			TEXT("default-arg overload warning should carry section, row and message keyword"))));

		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder warning diagnostic test should expose Entry metadata")));
		ASSERT_THAT(IsTrue(HasBytecode(EntryFunction), TEXT("Builder warning diagnostic test should produce Entry bytecode despite the warning")));
	}

	TEST_METHOD(WarningsAsErrorsFailBuildAndPreserveWarningDiagnostic)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder warnings-as-errors diagnostic test should create a standalone SDK engine")));

		const asPWORD PreviousCompilerWarnings = ScriptEngine->GetEngineProperty(asEP_COMPILER_WARNINGS);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->SetEngineProperty(asEP_COMPILER_WARNINGS, 2),
			TEXT("Builder warnings-as-errors diagnostic test should enable warnings-as-errors mode")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_COMPILER_WARNINGS, PreviousCompilerWarnings);
		};

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagWarningsAsErrors");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder warnings-as-errors diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class WarningCarrier
			{
				int Pick(int Value)
				{
					return Value;
				}

				int Pick(int Value, int Extra = 1)
				{
					return Value + Extra;
				}
			}

			int Entry()
			{
				WarningCarrier Carrier;
				return Carrier.Pick(41, 1);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagWarningsAsErrors.as", Source.c_str(), TEXT("WarningsAsErrors.AddSection")),
			TEXT("Builder warnings-as-errors diagnostic test should add the warning section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder warnings-as-errors diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module),
			TEXT("Builder warnings-as-errors diagnostic test should build through layout before final warning promotion")));

		const int CompileResult = Builder->BuildCompileCode();
		LogBuilderStageResult(*TestRunner, TEXT("WarningsAsErrors.BuildCompileCode"), CompileResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Builder warnings-as-errors diagnostic test should fail the final compile stage")));
		ASSERT_THAT(IsTrue(Builder->numWarnings > 0, TEXT("Builder warnings-as-errors diagnostic test should keep the original warning count")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder warnings-as-errors diagnostic test should add an error for warning promotion")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Warning(TEXT("BuilderDiagWarningsAsErrors.as"), 8, TEXT("overload")),
			TEXT("warnings-as-errors should preserve the original warning diagnostic"))));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT(""), 0, TEXT("Warnings are treated as errors")),
			TEXT("warnings-as-errors should add the promotion error"))));
	}

	TEST_METHOD(MultiSectionErrorReportsOwningSectionOnly)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder multi-section diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagMultiSection");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder multi-section diagnostic test should create a module")));

		const std::string GoodSource = ASTEST_AS_ANSI(R"AS(
			int Helper()
			{
				return 40;
			}
			)AS");
		const std::string BadSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				GhostType Value;
				return Helper() + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagMultiSection_Good.as", GoodSource.c_str(), TEXT("MultiSection.AddGoodSection")),
			TEXT("Builder multi-section diagnostic test should add the good section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagMultiSection_Bad.as", BadSource.c_str(), TEXT("MultiSection.AddBadSection")),
			TEXT("Builder multi-section diagnostic test should add the bad section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder multi-section diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module),
			TEXT("Builder multi-section diagnostic test should build through layout before codegen catches the bad section")));

		const int CompileResult = Builder->BuildCompileCode();
		LogBuilderStageResult(*TestRunner, TEXT("MultiSection.BuildCompileCode"), CompileResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Builder multi-section diagnostic test should fail during code compilation")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDiagMultiSection_Bad.as"), 3, TEXT("GhostType")),
			TEXT("multi-section diagnostic should report the owning bad section"))));

		bool bGoodSectionReportedError = false;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR && Entry.Section == TEXT("BuilderDiagMultiSection_Good.as"))
			{
				bGoodSectionReportedError = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bGoodSectionReportedError,
			TEXT("Builder multi-section diagnostic test should not attribute the bad-section error to the good section")));

		asIScriptFunction* HelperFunction = Module->GetFunctionByDecl("int Helper()");
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(HelperFunction, TEXT("Builder multi-section diagnostic test should keep Helper metadata")));
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder multi-section diagnostic test should keep Entry metadata")));
		ASSERT_THAT(IsTrue(HasBytecode(HelperFunction), TEXT("Builder multi-section diagnostic test should compile the good-section Helper bytecode")));
		ASSERT_THAT(IsFalse(HasBytecode(EntryFunction), TEXT("Builder multi-section diagnostic test should not compile the bad-section Entry bytecode")));
	}

	TEST_METHOD(GlobalInitializerErrorDoesNotMarkGlobalCompiled)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global-initializer diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagGlobalInitializer");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder global-initializer diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const int BrokenGlobal = MissingFunction();

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagGlobalInitializer.as", Source.c_str(), TEXT("GlobalInitializer.AddSection")),
			TEXT("Builder global-initializer diagnostic test should add the broken global section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder global-initializer diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalInitializer.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder global-initializer diagnostic test should parse the section")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalInitializer.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder global-initializer diagnostic test should generate types")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalInitializer.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module),
			TEXT("Builder global-initializer diagnostic test should register functions and globals")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalInitializer.BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, Module),
			TEXT("Builder global-initializer diagnostic test should layout classes before globals")));
		LogBuilderState(*TestRunner, TEXT("GlobalInitializer.BuildAllocateGlobalVariables.before"), *Builder, Module, true, false);
		Builder->BuildAllocateGlobalVariables();
		LogBuilderState(*TestRunner, TEXT("GlobalInitializer.BuildAllocateGlobalVariables.after"), *Builder, Module);

		const int LayoutFunctionsResult = Builder->BuildLayoutFunctions();
		LogBuilderStageResult(*TestRunner, TEXT("GlobalInitializer.BuildLayoutFunctions"), LayoutFunctionsResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(LayoutFunctionsResult < 0, TEXT("Builder global-initializer diagnostic test should fail during global initialization compilation")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder global-initializer diagnostic test should increment builder errors")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDiagGlobalInitializer.as"), 1, TEXT("MissingFunction")),
			TEXT("global initializer diagnostic should carry section, row and symbol keyword"))));

		sGlobalVariableDescription* BrokenGlobal = FindGlobalVariableDescriptionByName(*Builder, "BrokenGlobal");
		ASSERT_THAT(IsNotNull(BrokenGlobal, TEXT("Builder global-initializer diagnostic test should retain the failed global descriptor")));
		ASSERT_THAT(IsFalse(BrokenGlobal != nullptr && BrokenGlobal->isCompiled,
			TEXT("Builder global-initializer diagnostic test should not mark the failed global compiled")));
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder global-initializer diagnostic test should expose Entry metadata before codegen")));
		ASSERT_THAT(IsFalse(HasBytecode(EntryFunction), TEXT("Builder global-initializer diagnostic test should not compile Entry bytecode after layout failure")));
	}

	TEST_METHOD(CompileFunctionWarningUsesLineOffset)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction warning diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagCompileFunctionWarning");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder CompileFunction warning diagnostic test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		LogBuilderState(*TestRunner, TEXT("CompileFunctionWarning.initial"), Builder, Module, true, false);
		asCScriptFunction* Function = nullptr;
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int Value = 1.5;
				return Value;
			}
			)AS");
		LogBuilderSectionInput(*TestRunner, TEXT("CompileFunctionWarning.input"), "BuilderDiagCompileFunctionWarning.as", Source.c_str());
		const int CompileResult = Builder.CompileFunction("BuilderDiagCompileFunctionWarning.as", Source.c_str(), 20, asCOMP_ADD_TO_MODULE, &Function);
		LogBuilderStageResult(*TestRunner, TEXT("CompileFunctionWarning.CompileFunction"), CompileResult, Builder, Module, false);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), CompileResult,
			TEXT("Builder CompileFunction warning diagnostic test should compile despite the warning")));
		ASSERT_THAT(IsNotNull(Function, TEXT("Builder CompileFunction warning diagnostic test should return the compiled function")));
		ASSERT_THAT(IsTrue(Builder.numWarnings > 0, TEXT("Builder CompileFunction warning diagnostic test should increment warning count")));
		bool bMatchedOffsetWarning = false;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			if (Entry.Type == asMSGTYPE_WARNING &&
				Entry.Section == TEXT("BuilderDiagCompileFunctionWarning.as") &&
				Entry.Row > 20 &&
				Entry.Message.Contains(TEXT("exact")))
			{
				bMatchedOffsetWarning = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bMatchedOffsetWarning,
			TEXT("Builder CompileFunction warning diagnostic test should apply the provided line offset to the warning row")));
	}

	TEST_METHOD(DuplicateFunctionDiagnosticReportsFunctionSection)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder duplicate-function diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDiagDuplicateFunction");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder duplicate-function diagnostic test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}

			int Entry()
			{
				return 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDiagDuplicateFunction.as", Source.c_str(), TEXT("DuplicateFunction.AddSection")),
			TEXT("Builder duplicate-function diagnostic test should add the duplicate function section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder duplicate-function diagnostic test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DuplicateFunction.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder duplicate-function diagnostic test should parse duplicate function declarations")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DuplicateFunction.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder duplicate-function diagnostic test should generate types before function registration")));

		const int GenerateFunctionsResult = Builder->BuildGenerateFunctions();
		LogBuilderStageResult(*TestRunner, TEXT("DuplicateFunction.BuildGenerateFunctions"), GenerateFunctionsResult, *Builder, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(GenerateFunctionsResult < 0, TEXT("Builder duplicate-function diagnostic test should fail during function registration")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder duplicate-function diagnostic test should increment builder errors")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDiagDuplicateFunction.as"), 6, TEXT("same name and parameters")),
			TEXT("duplicate function diagnostic should carry section, row and duplicate-signature message"))));
		ASSERT_THAT(IsTrue(CountGlobalFunctionDescriptions(*Builder, "Entry") >= 2,
			TEXT("Builder duplicate-function diagnostic test should retain the duplicate Entry descriptions for diagnostics")));
		for (asUINT Index = 0; Index < Module->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(Index);
			if (Function != nullptr && FCStringAnsi::Strcmp(Function->GetName(), "Entry") == 0)
			{
				ASSERT_THAT(IsFalse(HasBytecode(Function),
					TEXT("Builder duplicate-function diagnostic test should not compile duplicate Entry bytecode after registration failure")));
			}
		}
	}
};

#endif
