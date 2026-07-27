#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

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
	TEST_METHOD(ParseStageRetainsRootsAndBlocksDownstreamPublication)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BUILDER-PARSE-STAGE",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder parse test should create a standalone SDK engine")));

		const std::string SingleSectionSource = ASTEST_AS_ANSI(R"AS(
			namespace SingleSection
			{
				class Value
				{
					int Number;
				}

				enum EState
				{
					Ready
				}
			}

			const int BaseValue = 40;

			int Entry()
			{
				return BaseValue + 2;
			}
		)AS");
		const TCHAR* Families[] = { TEXT("namespace"), TEXT("class"), TEXT("enum"), TEXT("function"), TEXT("global") };
		for (const TCHAR* Family : Families)
		{
			const FString CaseId = MakeNativeCaseId("COMPILER-BUILDER-PARSE-STAGE", { TEXT("one"), Family });
			PrintGeneratedAsSource(*TestRunner, CaseId, TEXT("BuilderParse_Single.as"), UTF8_TO_TCHAR(SingleSectionSource.c_str()));
		}

		{
			FScopedNativeModuleName SingleModuleScope(Engine, "BuilderParseSingleSection");
			asCModule* SingleModule = CreateBuilderModule(ScriptEngine, SingleModuleScope.Get());
			ASSERT_THAT(IsNotNull(SingleModule, TEXT("Single-section parse case should create a module")));
			ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*SingleModule, "BuilderParse_Single", SingleSectionSource.c_str(), TEXT("ParseStage.Single.Add")),
				TEXT("Single-section parse case should add its source")));
			asCBuilder* SingleBuilder = SingleModule->builder;
			ASSERT_THAT(IsNotNull(SingleBuilder, TEXT("Single-section parse case should create a builder")));
			if (!RunBuilderStage(*SingleBuilder, TEXT("ParseStage.Single.Parse"), &asCBuilder::BuildParallelParseScripts, SingleModule))
			{
				ReportBuilderFailureDiagnostics(Engine);
				return;
			}
			ASSERT_THAT(AreEqual(1, static_cast<int32>(SingleBuilder->parsers.GetLength()),
				TEXT("Single-section parse case should retain one parser")));
			asCScriptNode* Root = SingleBuilder->parsers.GetLength() == 1
				? SingleBuilder->parsers[0]->GetScriptNode()
				: nullptr;
			ASSERT_THAT(IsNotNull(Root, TEXT("Single-section parse case should retain its AST root")));
			ASSERT_THAT(AreEqual(1, CountNodesOfType(Root, snNamespace), TEXT("Single section should retain its namespace")));
			ASSERT_THAT(AreEqual(1, CountNodesOfType(Root, snClass), TEXT("Single section should retain its class")));
			ASSERT_THAT(AreEqual(1, CountNodesOfType(Root, snEnum), TEXT("Single section should retain its enum")));
			ASSERT_THAT(AreEqual(1, CountNodesOfType(Root, snFunction), TEXT("Single section should retain its function")));
			ASSERT_THAT(IsTrue(CountNodesOfType(Root, snDeclaration) >= 1, TEXT("Single section should retain a global declaration")));
			ASSERT_THAT(AreEqual(0, static_cast<int32>(SingleBuilder->classDeclarations.GetLength()),
				TEXT("Parse-only single section should not publish class descriptions")));
			ASSERT_THAT(AreEqual(0, static_cast<int32>(SingleBuilder->functions.GetLength()),
				TEXT("Parse-only single section should not publish function descriptions")));
			ASSERT_THAT(AreEqual(0, static_cast<int32>(SingleModule->GetObjectTypeCount()),
				TEXT("Parse-only single section should not publish module types")));
			ASSERT_THAT(AreEqual(0, static_cast<int32>(SingleModule->GetFunctionCount()),
				TEXT("Parse-only single section should not publish module functions")));
			ASSERT_THAT(AreEqual(0, static_cast<int32>(SingleModule->GetGlobalVarCount()),
				TEXT("Parse-only single section should not publish module globals")));

			Root = nullptr;
			if (SingleModule->builder != nullptr)
			{
				asDELETE(SingleModule->builder, asCBuilder);
			}
			SingleModule->builder = nullptr;
			SingleBuilder = nullptr;
			ASSERT_THAT(IsNull(
				SingleModule->builder,
				TEXT("Single-section parse cleanup should release its builder, parser, and AST tree")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(SingleModuleScope.Get()),
				TEXT("Single-section parse cleanup should explicitly discard its exact module")));
			SingleModule = nullptr;
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(SingleModuleScope.Get(), asGM_ONLY_IF_EXISTS),
				TEXT("Single-section parse module should be absent after explicit discard")));
		}

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
		const FString TwoSectionReviewSource = FString::Printf(
			TEXT("%s\n\n%s"),
			UTF8_TO_TCHAR(TypeSectionSource.c_str()),
			UTF8_TO_TCHAR(FunctionSectionSource.c_str()));
		for (const TCHAR* Family : Families)
		{
			const FString CaseId = MakeNativeCaseId("COMPILER-BUILDER-PARSE-STAGE", { TEXT("two"), Family });
			PrintGeneratedAsSource(*TestRunner, CaseId, TEXT("BuilderParse_TwoSections.as"), TwoSectionReviewSource);
		}
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderParse_Types", TypeSectionSource.c_str(), TEXT("ParseScriptsCreatesParserNodes.AddTypes")),
			TEXT("Builder test should add a script section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderParse_Functions", FunctionSectionSource.c_str(), TEXT("ParseScriptsCreatesParserNodes.AddFunctions")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Adding script sections should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("ParseScriptsCreatesParserNodes.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module), TEXT("Builder parse should parse both sections")))
		{
			ReportBuilderFailureDiagnostics(Engine);
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

		if (Module->builder != nullptr)
		{
			asDELETE(Module->builder, asCBuilder);
		}
		Module->builder = nullptr;
		Builder = nullptr;
		ASSERT_THAT(IsNull(
			Module->builder,
			TEXT("Two-section parse cleanup should release its builder, parsers, and AST trees")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("Two-section parse cleanup should explicitly discard its exact module")));
		Module = nullptr;
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Two-section parse module should be absent after explicit discard")));
		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Builder parse isolation should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				IndependentEngine.Get()->GetModuleCount(),
				TEXT("Builder parse stages should not create modules in an independent engine")));
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl("BuilderParse::ActorState"),
				TEXT("Parse-only AST state should not leak types into an independent engine")));
		}
	}
};

#endif
