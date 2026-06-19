#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	asCModule* CreateBuilderModule(asIScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return ScriptEngine != nullptr
			? static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE))
			: nullptr;
	}

	bool AddBuilderSection(asCModule& Module, const char* SectionName, const char* Source)
	{
		const int Result = Module.AddScriptSection(SectionName, Source, std::strlen(Source), 0);
		return Result >= 0;
	}

	bool RunBuilderPipelineThroughLayout(asCBuilder& Builder)
	{
		if (Builder.BuildParallelParseScripts() != asSUCCESS)
		{
			return false;
		}
		if (Builder.BuildGenerateTypes() != asSUCCESS)
		{
			return false;
		}
		if (Builder.BuildGenerateFunctions() != asSUCCESS)
		{
			return false;
		}
		if (Builder.BuildLayoutClasses() != asSUCCESS)
		{
			return false;
		}
		Builder.BuildAllocateGlobalVariables();
		return Builder.BuildLayoutFunctions() == asSUCCESS;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(CompileFunctionUsesProvidedSectionName)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction section test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionSection");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder CompileFunction section test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asCScriptFunction* Function = nullptr;
		const std::string EntryFunctionSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		const int CompileResult = Builder.CompileFunction("BuilderCompileFunctionSection_A", EntryFunctionSource.c_str(), 20, asCOMP_ADD_TO_MODULE, &Function);
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), CompileResult, TEXT("Builder CompileFunction section test should compile one function")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		ASSERT_THAT(IsNotNull(Function, TEXT("Builder CompileFunction section test should return the compiled function")));

		ASSERT_THAT(AreEqual(FString(TEXT("BuilderCompileFunctionSection_A")), FString(UTF8_TO_TCHAR(Function->GetScriptSectionName())),
			TEXT("Builder CompileFunction section test should preserve the provided section name")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder CompileFunction section test should add the function to the module")));
	}

	TEST_METHOD(CompileFunctionFailureDoesNotLeakFunction)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder CompileFunction failure test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionFailure");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder CompileFunction failure test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		Builder.silent = true;
		asCScriptFunction* Function = nullptr;
		const std::string BrokenEntryFunctionSource = ASTEST_AS_ANSI(R"AS(
			int Entry(
			{
				return 42;
			}
			)AS");
		const int CompileResult = Builder.CompileFunction("BuilderCompileFunctionFailure_A", BrokenEntryFunctionSource.c_str(), 0, asCOMP_ADD_TO_MODULE, &Function);
		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Builder CompileFunction failure test should fail the invalid function")));
		ASSERT_THAT(IsNull(Function, TEXT("Builder CompileFunction failure test should not return a function")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder CompileFunction failure test should not leak a module function")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Builder CompileFunction failure test should not expose Entry")));
	}

	TEST_METHOD(ParseScriptsCreatesParserNodes)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder parse test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderParseScripts");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder parse test should create a backing module")));

		const std::string ClassSectionSource = ASTEST_AS_ANSI(R"AS(
			class A
			{
			}
			)AS");
		const std::string EnumSectionSource = ASTEST_AS_ANSI(R"AS(
			enum EParseState
			{
				Idle,
				Busy
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(*Module, "BuilderParse_A", ClassSectionSource.c_str()),
			TEXT("Builder test should add a script section")));
		ASSERT_THAT(IsTrue(AddBuilderSection(*Module, "BuilderParse_B", EnumSectionSource.c_str()),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Adding script sections should create a builder")));

		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildParallelParseScripts(), TEXT("Builder parse should parse both sections")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Builder->parsers.GetLength()),
			TEXT("Builder parse should create one parser per section")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder parse should not register global functions before function generation")));
	}

	TEST_METHOD(GenerateTypesRegistersDeclarations)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder type-generation test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateTypes");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder type-generation test should create a backing module")));

		const std::string TypeDeclarationsSource = ASTEST_AS_ANSI(R"AS(
			enum EState
			{
				Idle,
				Busy
			}

			class ActorState
			{
			}

			namespace Types
			{
				class NestedState
				{
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(*Module, "BuilderTypes", TypeDeclarationsSource.c_str()),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder type-generation test should create a builder")));

		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildParallelParseScripts(), TEXT("Builder type-generation test should parse scripts")) ||
			!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildGenerateTypes(), TEXT("Builder type-generation test should generate types")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(IsTrue(Builder->classDeclarations.GetLength() >= 2,
			TEXT("Builder type-generation should discover class declarations")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->interfaceDeclarations.GetLength()),
			TEXT("Builder type-generation should not require rejected interface syntax")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("ActorState"),
			TEXT("Builder type-generation should register the class type on the module")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("Types::NestedState"),
			TEXT("Builder type-generation should register the namespaced class type on the module")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("EState"),
			TEXT("Builder type-generation should register the enum type on the module")));
	}

	TEST_METHOD(GenerateFunctionsRegistersGlobalsAndFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder function-generation test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateFunctions");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder function-generation test should create a backing module")));

		const std::string BuilderFunctionsSource = ASTEST_AS_ANSI(R"AS(
			const int Base = 40;

			int AddTwo()
			{
				return Base + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(*Module, "BuilderFunctions", BuilderFunctionsSource.c_str()),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder function-generation test should create a builder")));

		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildParallelParseScripts(), TEXT("Builder function-generation test should parse scripts")) ||
			!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildGenerateTypes(), TEXT("Builder function-generation test should generate types")) ||
			!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildGenerateFunctions(), TEXT("Builder function-generation test should generate functions")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder function-generation should register one global function")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder function-generation should register one global variable")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int AddTwo()"),
			TEXT("Builder function-generation should expose AddTwo by declaration")));
	}

	TEST_METHOD(LayoutAndCompileProduceExecutableBytecode)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder compile test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileCode");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder compile test should create a backing module")));

		const std::string CompilePipelineSource = ASTEST_AS_ANSI(R"AS(
			int Helper(int Value)
			{
				return Value * 2;
			}

			int Entry()
			{
				return Helper(21);
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(*Module, "BuilderCompile", CompilePipelineSource.c_str()),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder compile test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder), TEXT("Builder should build through layout")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildCompileCode(), TEXT("Builder compile test should compile function bytecode")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Builder compile should expose Entry after code generation")));

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Builder compile should emit bytecode for Entry")));
		ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Builder compile should emit at least one bytecode dword")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder compile should execute compiled bytecode")));
	}

	TEST_METHOD(StageFailureStopsBeforeExecutableCode)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder stage failure test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderStageFailure");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder stage failure test should create a backing module")));

		const std::string BrokenStageSource = ASTEST_AS_ANSI(R"AS(
			int Entry(
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSection(*Module, "BuilderStageFailure", BrokenStageSource.c_str()),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder stage failure test should create a builder")));

		Builder->silent = true;
		const int ParseResult = Builder->BuildParallelParseScripts();
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Builder stage failure should fail during parse")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0, TEXT("Builder stage failure should record at least one builder error")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder stage failure should not register executable functions")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"), TEXT("Builder stage failure should not expose Entry")));
	}
};

#endif
