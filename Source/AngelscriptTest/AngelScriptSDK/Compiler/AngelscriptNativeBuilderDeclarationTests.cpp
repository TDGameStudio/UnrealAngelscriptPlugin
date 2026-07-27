#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"

// Builder declaration coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FBuilderDeclarationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asIScriptFunction* FindExactFunction(
		asIScriptModule& Module,
		const char* Namespace,
		const char* Declaration)
	{
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetNamespace(), Namespace) == 0
				&& FCStringAnsi::Strcmp(
					Function->GetDeclaration(false, false, false),
					Declaration) == 0)
			{
				return Function;
			}
		}
		return nullptr;
	}

public:
	TEST_METHOD(StagesPublishDeclarationFamiliesWithoutEarlyLeak)
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

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-DECLARATION-PUBLICATION",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Runtime
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder declaration test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDeclTypes");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder declaration test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			enum GlobalChoice
			{
				GlobalFirst = 1,
				GlobalSecond = 2
			}

			class GlobalCarrier
			{
				int Value;
			}

			import int GlobalExternal() from "Provider";

			const int GlobalAnswer = 40;

			int Pick(int Value)
			{
				return Value;
			}

			int Pick(bool Value)
			{
				return Value ? 1 : 0;
			}

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

				import int External() from "Provider";

				const int Answer = 2;

				int Pick(int Value)
				{
					return Value;
				}

				int Pick(bool Value)
				{
					return Value ? 1 : 0;
				}
			}

			int Entry()
			{
				return GlobalAnswer + BuilderDecl::Answer;
			}
			)AS");
		const FString ReviewSource(UTF8_TO_TCHAR(Source.c_str()));
		const TCHAR* Families[] =
		{
			TEXT("class"),
			TEXT("enum"),
			TEXT("function"),
			TEXT("import"),
			TEXT("const_global"),
		};
		const TCHAR* Scopes[] = { TEXT("global"), TEXT("namespace") };
		const TCHAR* Stages[] =
		{
			TEXT("parse"),
			TEXT("type"),
			TEXT("function"),
			TEXT("layout"),
			TEXT("code"),
		};
		for (const TCHAR* Family : Families)
		{
			for (const TCHAR* Scope : Scopes)
			{
				for (const TCHAR* Stage : Stages)
				{
					PrintGeneratedAsSource(
						*TestRunner,
						MakeNativeCaseId(
							"COMPILER-BUILDER-DECLARATION-PUBLICATION",
							{ Family, Scope, Stage }),
						TEXT("BuilderDeclarationPublication"),
						ReviewSource);
				}
			}
		}
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderDeclTypes.as", Source.c_str(), TEXT("DeclTypes.AddSection")),
			TEXT("Builder declaration test should add the declaration section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder declaration test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder declaration test should parse declarations")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->classDeclarations.GetLength()),
			TEXT("Parse stage should not publish class declarations early")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->namedTypeDeclarations.GetLength()),
			TEXT("Parse stage should not publish enum declarations early")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Parse stage should not publish function descriptions early")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Parse stage should not publish global descriptions early")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()),
			TEXT("Parse stage should leave the module type table empty")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Parse stage should leave the module function table empty")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Parse stage should leave the module global table empty")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder declaration test should generate types")));

		ASSERT_THAT(IsNotNull(FindClassDeclarationByName(*Builder, "GlobalCarrier"), TEXT("Type stage should track the global class declaration")));
		ASSERT_THAT(IsNotNull(FindNamedTypeDeclarationByName(*Builder, "GlobalChoice"), TEXT("Type stage should track the global enum declaration")));
		ASSERT_THAT(IsNotNull(FindClassDeclarationByName(*Builder, "Carrier"), TEXT("Builder declaration test should track Carrier class declaration")));
		ASSERT_THAT(IsNotNull(FindNamedTypeDeclarationByName(*Builder, "Choice"), TEXT("Builder declaration test should track Choice named type declaration")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->funcDefs.GetLength()), TEXT("Builder declaration test should not require rejected script-level funcdefs")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("GlobalCarrier"), TEXT("Type stage should publish the global class type")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("GlobalChoice"), TEXT("Type stage should publish the global enum type")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("BuilderDecl::Carrier"), TEXT("Builder declaration test should publish namespaced class type")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("BuilderDecl::Choice"), TEXT("Builder declaration test should publish namespaced enum type")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Type stage should not publish global functions early")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetImportedFunctionCount()),
			TEXT("Type stage should not publish imported functions early")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Type stage should not publish const globals early")));

		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module),
			TEXT("Builder declaration test should generate functions, imports, and globals")));
		asIScriptFunction* const GlobalPickInt =
			FindExactFunction(*Module, "", "int Pick(const int)");
		asIScriptFunction* const GlobalPickBool =
			FindExactFunction(*Module, "", "int Pick(const bool)");
		asIScriptFunction* const NamespacedPickInt =
			FindExactFunction(*Module, "BuilderDecl", "int Pick(const int)");
		asIScriptFunction* const NamespacedPickBool =
			FindExactFunction(*Module, "BuilderDecl", "int Pick(const bool)");
		ASSERT_THAT(IsNotNull(GlobalPickInt, TEXT("Function stage should publish the global int overload")));
		ASSERT_THAT(IsNotNull(GlobalPickBool, TEXT("Function stage should publish the global bool overload")));
		ASSERT_THAT(IsNotNull(NamespacedPickInt, TEXT("Function stage should publish the namespaced int overload")));
		ASSERT_THAT(IsNotNull(NamespacedPickBool, TEXT("Function stage should publish the namespaced bool overload")));
		ASSERT_THAT(AreNotEqual(
			GlobalPickInt != nullptr ? GlobalPickInt->GetId() : asNO_FUNCTION,
			GlobalPickBool != nullptr ? GlobalPickBool->GetId() : asNO_FUNCTION,
			TEXT("Global overloads should retain distinct function identities")));
		ASSERT_THAT(AreNotEqual(
			NamespacedPickInt != nullptr ? NamespacedPickInt->GetId() : asNO_FUNCTION,
			NamespacedPickBool != nullptr ? NamespacedPickBool->GetId() : asNO_FUNCTION,
			TEXT("Namespaced overloads should retain distinct function identities")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetImportedFunctionCount()),
			TEXT("Function stage should publish both scoped imports")));
		ASSERT_THAT(IsTrue(FindGlobalVarIndexByNameAndNamespace(Module, "GlobalAnswer") >= 0,
			TEXT("Function stage should publish the global const global")));
		ASSERT_THAT(IsTrue(FindGlobalVarIndexByNameAndNamespace(Module, "Answer", "BuilderDecl") >= 0,
			TEXT("Function stage should publish the namespaced const global")));

		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, Module),
			TEXT("Builder declaration test should layout classes")));
		Builder->BuildAllocateGlobalVariables();
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildLayoutFunctions"), &asCBuilder::BuildLayoutFunctions, Module),
			TEXT("Builder declaration test should layout functions")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DeclTypes.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module),
			TEXT("Builder declaration test should compile declaration bytecode")));
		asIScriptFunction* const EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Code stage should retain exact Entry metadata")));
		ASSERT_THAT(IsTrue(HasBytecode(EntryFunction), TEXT("Code stage should publish executable Entry bytecode")));
		int32 Result = 0;
		ASSERT_THAT(IsTrue(
			ExecuteScriptFunction<int32>(*TestRunner, ScriptEngine, Module, "int Entry()", Result),
			TEXT("Declaration publication product should execute final code")));
		ASSERT_THAT(AreEqual(42, Result, TEXT("Final declaration code should resolve both const-global owners")));

		if (Module->builder != nullptr)
		{
			asDELETE(Module->builder, asCBuilder);
		}
		Module->builder = nullptr;
		Builder = nullptr;
		ASSERT_THAT(IsNull(
			Module->builder,
			TEXT("Declaration publication cleanup should release its transient builder and parser trees")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("Declaration publication cleanup should explicitly discard its exact module")));
		Module = nullptr;
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Declaration publication module should be absent after explicit discard")));
		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Declaration publication isolation should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				IndependentEngine.Get()->GetModuleCount(),
				TEXT("Declaration publication should not create modules in an independent engine")));
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl("BuilderDecl::Carrier"),
				TEXT("Declaration publication should not leak namespaced types into an independent engine")));
		}
	}

	TEST_METHOD(FunctionGenerationRegistersFunctionsImportsAndConstGlobals)
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
			"LegacyCompatibility",
			"COMPILER-BUILDER-DECLARATION-PUBLICATION owns staged declaration-family publication; this retained function/import/global stage probe supplies compatibility detail.");

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
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-DECLARATION-PUBLICATION-LEGACY-FUNCTION-IMPORT-GLOBAL"),
			TEXT("BuilderDeclarationFunctionCompatibility"),
			FString(UTF8_TO_TCHAR(Source.c_str())));
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
