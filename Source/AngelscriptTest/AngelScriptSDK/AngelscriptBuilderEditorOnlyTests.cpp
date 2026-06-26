#include "AngelscriptBuilderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptBuilderEditorOnlyTest
{
	static sClassDeclaration* FindClassDeclarationByNameInAnyList(asCBuilder& Builder, const char* Name)
	{
		if (sClassDeclaration* ClassDeclaration = AngelscriptBuilderTestSupport::FindClassDeclarationByName(Builder, Name))
		{
			return ClassDeclaration;
		}
		return AngelscriptBuilderTestSupport::FindNamedTypeDeclarationByName(Builder, Name);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderEditorOnlyTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.EditorOnly",
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

	TEST_METHOD(LineBlocksClassifyTopLevelDeclarations)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

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
		sClassDeclaration* RuntimeClass = AngelscriptBuilderEditorOnlyTest::FindClassDeclarationByNameInAnyList(*Builder, "RuntimeCarrier");
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Builder editor-only line block test should find RuntimeCarrier declaration")));
		sClassDeclaration* EditorClass = AngelscriptBuilderEditorOnlyTest::FindClassDeclarationByNameInAnyList(*Builder, "EditorCarrier");
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
		ASSERT_THAT(IsTrue(true, TEXT("Builder editor-only line block test is editor-only")));
#endif
	}

	TEST_METHOD(EditorOnlyModuleClassifiesEveryParsedNodeAsEditorOnly)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

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
		sClassDeclaration* RuntimeClass = AngelscriptBuilderEditorOnlyTest::FindClassDeclarationByNameInAnyList(*Builder, "RuntimeNamedCarrier");
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Builder editor-only module test should find RuntimeNamedCarrier declaration")));
		sFunctionDescription* RuntimeFunction = FindFunctionDescriptionByName(*Builder, "RuntimeNamedEntry");
		ASSERT_THAT(IsNotNull(RuntimeFunction, TEXT("Builder editor-only module test should find RuntimeNamedEntry declaration")));

		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(Script, RuntimeClass->node),
			TEXT("Builder editor-only module test should classify class node as editor-only when module flag is set")));
		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(Script, RuntimeFunction->node),
			TEXT("Builder editor-only module test should classify function node as editor-only when module flag is set")));
#else
		ASSERT_THAT(IsTrue(true, TEXT("Builder editor-only whole-module test is editor-only")));
#endif
	}

	TEST_METHOD(LineBlocksIgnoreNodesFromOtherScriptSections)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

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
)AS");
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
		ASSERT_THAT(IsTrue(Builder->scripts.GetLength() >= 2, TEXT("Builder editor-only section isolation test should retain both script sections")));

		Builder->SetEditorOnlyBlockLinePositions({ TPair<int32, int32>(1, 3) });

		asCScriptCode* FirstScript = Builder->scripts[0];
		asCScriptCode* SecondScript = Builder->scripts[1];
		sClassDeclaration* FirstClass = AngelscriptBuilderEditorOnlyTest::FindClassDeclarationByNameInAnyList(*Builder, "FirstSectionEditorCarrier");
		ASSERT_THAT(IsNotNull(FirstClass, TEXT("Builder editor-only section isolation test should find first-section class")));
		sClassDeclaration* SecondClass = AngelscriptBuilderEditorOnlyTest::FindClassDeclarationByNameInAnyList(*Builder, "SecondSectionCarrier");
		ASSERT_THAT(IsNotNull(SecondClass, TEXT("Builder editor-only section isolation test should find second-section class")));

		ASSERT_THAT(IsTrue(Builder->IsNodeInEditorOnlyCode(FirstScript, FirstClass->node),
			TEXT("Builder editor-only section isolation test should classify first-section node inside editor-only block")));
		ASSERT_THAT(IsFalse(Builder->IsNodeInEditorOnlyCode(SecondScript, SecondClass->node),
			TEXT("Builder editor-only section isolation test should ignore matching lines from non-primary sections")));
#else
		ASSERT_THAT(IsTrue(true, TEXT("Builder editor-only section isolation test is editor-only")));
#endif
	}
};

#endif
