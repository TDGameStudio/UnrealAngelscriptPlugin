#include "Support/AngelscriptNativeBuilderTestSupport.h"

// Builder editor-only coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderEditorOnlyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.EditorOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static sClassDeclaration* FindClassDeclarationByNameInAnyList(
		asCBuilder& Builder,
		const char* Name)
	{
		if (sClassDeclaration* const ClassDeclaration =
			AngelscriptBuilderTestSupport::FindClassDeclarationByName(Builder, Name))
		{
			return ClassDeclaration;
		}
		return AngelscriptBuilderTestSupport::FindNamedTypeDeclarationByName(
			Builder,
			Name);
	}

public:
	TEST_METHOD(EditorOnlyModesClassifyDeclarationsAndIsolateSections)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

#if WITH_EDITOR
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder editor-only line block test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderEditorOnlyLineBlocks");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder editor-only line block test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class RuntimeCarrier
			{
			}

			class EditorCarrier
			{
			}

			int RuntimeEntry()
			{
				return 1;
			}

			int EditorEntry()
			{
				return 2;
			}
		)AS");
		const FString ReviewSource(UTF8_TO_TCHAR(Source.c_str()));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-CLASS-LINE-BLOCK-OWNING"),
			TEXT("BuilderEditorOnlyLineBlock"),
			ReviewSource);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-FUNCTION-LINE-BLOCK-OWNING"),
			TEXT("BuilderEditorOnlyLineBlock"),
			ReviewSource);
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderEditorOnlyLineBlocks.as", Source.c_str(), TEXT("EditorOnlyLineBlocks.AddSection")),
			TEXT("Builder editor-only line block test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder editor-only line block test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlyLineBlocks.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder editor-only line block test should parse declarations")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlyLineBlocks.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder editor-only line block test should generate type declarations")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlyLineBlocks.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module),
			TEXT("Builder editor-only line block test should generate function declarations")));
		ASSERT_THAT(IsTrue(Builder->scripts.GetLength() > 0, TEXT("Builder editor-only line block test should retain script code")));

		Builder->SetEditorOnlyBlockLinePositions({
			TPair<int32, int32>(5, 7),
			TPair<int32, int32>(14, 17)
		});

		asCScriptCode* Script = Builder->scripts[0];
		sClassDeclaration* RuntimeClass = FindClassDeclarationByNameInAnyList(*Builder, "RuntimeCarrier");
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Builder editor-only line block test should find RuntimeCarrier declaration")));
		sClassDeclaration* EditorClass = FindClassDeclarationByNameInAnyList(*Builder, "EditorCarrier");
		ASSERT_THAT(IsNotNull(EditorClass, TEXT("Builder editor-only line block test should find EditorCarrier declaration")));
		sFunctionDescription* RuntimeFunction = FindFunctionDescriptionByName(*Builder, "RuntimeEntry");
		ASSERT_THAT(IsNotNull(RuntimeFunction, TEXT("Builder editor-only line block test should find RuntimeEntry declaration")));
		sFunctionDescription* EditorFunction = FindFunctionDescriptionByName(*Builder, "EditorEntry");
		ASSERT_THAT(IsNotNull(EditorFunction, TEXT("Builder editor-only line block test should find EditorEntry declaration")));

		ASSERT_THAT(IsFalse(Builder->IsNodeInEditorOnlyCode(Script, RuntimeClass->node),
			TEXT("Builder editor-only line block test should classify RuntimeCarrier outside editor-only code")));
		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(Script, EditorClass->node),
			TEXT("Builder editor-only line block test should classify EditorCarrier inside editor-only code")));
		ASSERT_THAT(IsFalse(Builder->IsNodeInEditorOnlyCode(Script, RuntimeFunction->node),
			TEXT("Builder editor-only line block test should classify RuntimeEntry outside editor-only code")));
		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(Script, EditorFunction->node),
			TEXT("Builder editor-only line block test should classify EditorEntry inside editor-only code")));
#else
		TestRunner->AddError(
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION requires WITH_EDITOR"));
#endif
	}

	TEST_METHOD(EditorOnlyModuleClassifiesEveryParsedNodeAsEditorOnly)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"Whole-module class/function classification supplies cells owned by COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION.");

#if WITH_EDITOR
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder editor-only module test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderEditorOnlyWholeModule");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder editor-only module test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class RuntimeNamedCarrier
			{
			}

			int RuntimeNamedEntry()
			{
				return 3;
			}
		)AS");
		const FString ReviewSource(UTF8_TO_TCHAR(Source.c_str()));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-CLASS-WHOLE-MODULE-OWNING"),
			TEXT("BuilderEditorOnlyWholeModule"),
			ReviewSource);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-FUNCTION-WHOLE-MODULE-OWNING"),
			TEXT("BuilderEditorOnlyWholeModule"),
			ReviewSource);
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderEditorOnlyWholeModule.as", Source.c_str(), TEXT("EditorOnlyWholeModule.AddSection")),
			TEXT("Builder editor-only module test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder editor-only module test should create a builder")));
		Builder->isEditorOnlyModule = true;

		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlyWholeModule.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder editor-only module test should parse declarations")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlyWholeModule.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder editor-only module test should generate types")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlyWholeModule.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module),
			TEXT("Builder editor-only module test should generate functions")));
		ASSERT_THAT(IsTrue(Builder->scripts.GetLength() > 0, TEXT("Builder editor-only module test should retain script code")));

		asCScriptCode* Script = Builder->scripts[0];
		sClassDeclaration* RuntimeClass = FindClassDeclarationByNameInAnyList(*Builder, "RuntimeNamedCarrier");
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Builder editor-only module test should find RuntimeNamedCarrier declaration")));
		sFunctionDescription* RuntimeFunction = FindFunctionDescriptionByName(*Builder, "RuntimeNamedEntry");
		ASSERT_THAT(IsNotNull(RuntimeFunction, TEXT("Builder editor-only module test should find RuntimeNamedEntry declaration")));

		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(Script, RuntimeClass->node),
			TEXT("Builder editor-only module test should classify class node as editor-only when module flag is set")));
		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(Script, RuntimeFunction->node),
			TEXT("Builder editor-only module test should classify function node as editor-only when module flag is set")));
#else
		TestRunner->AddError(
			TEXT("Whole-module editor-only classification requires WITH_EDITOR"));
#endif
	}

	TEST_METHOD(LineBlocksIgnoreNodesFromOtherScriptSections)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"Cross-section line-range isolation supplies cells owned by COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION.");

#if WITH_EDITOR
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder editor-only section isolation test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderEditorOnlySectionIsolation");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder editor-only section isolation test should create a module")));

		const std::string FirstSource = ASTEST_AS_ANSI(R"AS(
			class FirstSectionEditorCarrier
			{
			}
		)AS");
		const std::string SecondSource = ASTEST_AS_ANSI(R"AS(
			class SecondSectionCarrier
			{
			}

			int SecondSectionEntry()
			{
				return 2;
			}
		)AS");
		const FString FirstReviewSource(UTF8_TO_TCHAR(FirstSource.c_str()));
		const FString SecondReviewSource(UTF8_TO_TCHAR(SecondSource.c_str()));
		FString CombinedReviewSource;
		AppendGeneratedAsLine(CombinedReviewSource, FirstReviewSource);
		AppendGeneratedAsLine(CombinedReviewSource, SecondReviewSource);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-CLASS-LINE-BLOCK-OTHER"),
			TEXT("BuilderEditorOnlyOtherSection"),
			SecondReviewSource);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-FUNCTION-LINE-BLOCK-OTHER"),
			TEXT("BuilderEditorOnlyOtherSection"),
			SecondReviewSource);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-CLASS-WHOLE-MODULE-OTHER"),
			TEXT("BuilderEditorOnlyWholeModuleRelation"),
			CombinedReviewSource);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-EDITOR-ONLY-CLASSIFICATION-FUNCTION-WHOLE-MODULE-OTHER"),
			TEXT("BuilderEditorOnlyWholeModuleRelation"),
			CombinedReviewSource);
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderEditorOnlyFirst.as", FirstSource.c_str(), TEXT("EditorOnlySectionIsolation.AddFirstSection")),
			TEXT("Builder editor-only section isolation test should add first section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderEditorOnlySecond.as", SecondSource.c_str(), TEXT("EditorOnlySectionIsolation.AddSecondSection")),
			TEXT("Builder editor-only section isolation test should add second section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder editor-only section isolation test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlySectionIsolation.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder editor-only section isolation test should parse both sections")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("EditorOnlySectionIsolation.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder editor-only section isolation test should generate type declarations")));
		ASSERT_THAT(IsTrue(
			RunBuilderStage(
				*TestRunner,
				*Builder,
				TEXT("EditorOnlySectionIsolation.BuildGenerateFunctions"),
				&asCBuilder::BuildGenerateFunctions,
				Module),
			TEXT("Builder editor-only section isolation test should generate function descriptions")));
		ASSERT_THAT(IsTrue(Builder->scripts.GetLength() >= 2, TEXT("Builder editor-only section isolation test should retain both script sections")));

		Builder->SetEditorOnlyBlockLinePositions({ TPair<int32, int32>(1, 3) });

		asCScriptCode* FirstScript = Builder->scripts[0];
		asCScriptCode* SecondScript = Builder->scripts[1];
		sClassDeclaration* FirstClass = FindClassDeclarationByNameInAnyList(*Builder, "FirstSectionEditorCarrier");
		ASSERT_THAT(IsNotNull(FirstClass, TEXT("Builder editor-only section isolation test should find first-section class")));
		sClassDeclaration* SecondClass = FindClassDeclarationByNameInAnyList(*Builder, "SecondSectionCarrier");
		ASSERT_THAT(IsNotNull(SecondClass, TEXT("Builder editor-only section isolation test should find second-section class")));
		sFunctionDescription* SecondFunction =
			FindFunctionDescriptionByName(*Builder, "SecondSectionEntry");
		ASSERT_THAT(IsNotNull(
			SecondFunction,
			TEXT("Builder editor-only section isolation test should find second-section function")));

		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(FirstScript, FirstClass->node),
			TEXT("Builder editor-only section isolation test should classify first-section node inside editor-only block")));
		ASSERT_THAT(IsFalse(Builder->IsNodeInEditorOnlyCode(SecondScript, SecondClass->node),
			TEXT("Builder editor-only section isolation test should ignore matching lines from non-primary sections")));
		ASSERT_THAT(IsFalse(
			Builder->IsNodeInEditorOnlyCode(SecondScript, SecondFunction->node),
			TEXT("Builder editor-only section isolation test should isolate function rows from non-primary sections")));

		Builder->isEditorOnlyModule = true;
		ASSERT_THAT(IsTrue(
			Builder->IsNodeInEditorOnlyCode(SecondScript, SecondClass->node),
			TEXT("Whole-module mode should classify a class from the other retained section")));
		ASSERT_THAT(IsTrue(
			Builder->IsNodeInEditorOnlyCode(SecondScript, SecondFunction->node),
			TEXT("Whole-module mode should classify a function from the other retained section")));
#else
		TestRunner->AddError(
			TEXT("Editor-only section isolation requires WITH_EDITOR"));
#endif
	}
};

#endif
