#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderParsingTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderParsing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(ParseScriptsCreatesParserNodes)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder parse test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderParseScripts");
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
};

#endif
