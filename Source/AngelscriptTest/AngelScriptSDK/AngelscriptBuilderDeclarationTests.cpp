#include "AngelscriptBuilderTestSupport.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderDeclarationTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.Declarations",
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

	TEST_METHOD(TypeGenerationRegistersNamespacedClassAndEnum)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder declaration test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDeclTypes");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder declaration test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			namespace BuilderDecl
			{
				enum Choice
				{
					First = 1,
					Second = 2
				}
				class Carrier
				{
					int Value;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDeclTypes.as", Source.c_str(), TEXT("DeclTypes.AddSection")),
			TEXT("Builder declaration test should add the declaration section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder declaration test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder declaration test should parse declarations")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder declaration test should generate types")));

		ASSERT_THAT(IsNotNull(FindClassDeclarationByName(*Builder, "Carrier"), TEXT("Builder declaration test should track Carrier class declaration")));
		ASSERT_THAT(IsNotNull(FindNamedTypeDeclarationByName(*Builder, "Choice"), TEXT("Builder declaration test should track Choice named type declaration")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->funcDefs.GetLength()), TEXT("Builder declaration test should not require rejected script-level funcdefs")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("BuilderDecl::Carrier"), TEXT("Builder declaration test should publish namespaced class type")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("BuilderDecl::Choice"), TEXT("Builder declaration test should publish namespaced enum type")));
	}

	TEST_METHOD(FunctionGenerationRegistersFunctionsImportsAndConstGlobals)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder declaration function test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDeclFunctions");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder declaration function test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			import int ExternalValue() from "Provider";

			const int GlobalAnswer = 42;

			int Entry()
			{
				return GlobalAnswer;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDeclFunctions.as", Source.c_str(), TEXT("DeclFunctions.AddSection")),
			TEXT("Builder declaration function test should add the declaration section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder declaration function test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclFunctions.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder declaration function test should parse declarations")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclFunctions.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder declaration function test should generate types")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclFunctions.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module),
			TEXT("Builder declaration function test should generate functions")));

		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "Entry"), TEXT("Builder declaration function test should track Entry function description")));
		ASSERT_THAT(IsNotNull(FindGlobalVariableDescriptionByName(*Builder, "GlobalAnswer"), TEXT("Builder declaration function test should track GlobalAnswer description")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Builder declaration function test should publish Entry function metadata")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetImportedFunctionCount()), TEXT("Builder declaration function test should publish imported function metadata")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Builder declaration function test should publish one const global")));
		const int32 GlobalIndex = FindGlobalVarIndexByNameAndNamespace(Module, "GlobalAnswer");
		ASSERT_THAT(IsTrue(GlobalIndex >= 0, TEXT("Builder declaration function test should expose GlobalAnswer by name")));
	}
};

#endif
