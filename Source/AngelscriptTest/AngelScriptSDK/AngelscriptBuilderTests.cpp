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

	bool AddBuilderSection(FAutomationTestBase& Test, asCModule& Module, const char* SectionName, const char* Source)
	{
		const int Result = Module.AddScriptSection(SectionName, Source, std::strlen(Source), 0);
		return Test.TestTrue(TEXT("Builder test should add a script section"), Result >= 0);
	}

	bool RunBuilderPipelineThroughLayout(FAutomationTestBase& Test, asCBuilder& Builder)
	{
		if (!Test.TestEqual(TEXT("Builder should parse scripts"), Builder.BuildParallelParseScripts(), static_cast<int32>(asSUCCESS)))
		{
			return false;
		}
		if (!Test.TestEqual(TEXT("Builder should generate types"), Builder.BuildGenerateTypes(), static_cast<int32>(asSUCCESS)))
		{
			return false;
		}
		if (!Test.TestEqual(TEXT("Builder should generate functions"), Builder.BuildGenerateFunctions(), static_cast<int32>(asSUCCESS)))
		{
			return false;
		}
		if (!Test.TestEqual(TEXT("Builder should layout classes"), Builder.BuildLayoutClasses(), static_cast<int32>(asSUCCESS)))
		{
			return false;
		}
		Builder.BuildAllocateGlobalVariables();
		return Test.TestEqual(TEXT("Builder should layout functions"), Builder.BuildLayoutFunctions(), static_cast<int32>(asSUCCESS));
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
		if (!TestRunner->TestNotNull(TEXT("Builder CompileFunction section test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionSection");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder CompileFunction section test should create a module"), Module))
		{
			return;
		}

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asCScriptFunction* Function = nullptr;
		const int CompileResult = Builder.CompileFunction("BuilderCompileFunctionSection_A", R"(
int Entry()
{
	return 42;
}
)", 20, asCOMP_ADD_TO_MODULE, &Function);
		if (!TestRunner->TestEqual(TEXT("Builder CompileFunction section test should compile one function"), CompileResult, static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		if (!TestRunner->TestNotNull(TEXT("Builder CompileFunction section test should return the compiled function"), Function))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Builder CompileFunction section test should preserve the provided section name"), FString(UTF8_TO_TCHAR(Function->GetScriptSectionName())), FString(TEXT("BuilderCompileFunctionSection_A")));
		TestRunner->TestEqual(TEXT("Builder CompileFunction section test should add the function to the module"), static_cast<int32>(Module->GetFunctionCount()), 1);
	}

	TEST_METHOD(CompileFunctionFailureDoesNotLeakFunction)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Builder CompileFunction failure test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileFunctionFailure");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder CompileFunction failure test should create a module"), Module))
		{
			return;
		}

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		Builder.silent = true;
		asCScriptFunction* Function = nullptr;
		const int CompileResult = Builder.CompileFunction("BuilderCompileFunctionFailure_A", R"(
int Entry(
{
	return 42;
}
)", 0, asCOMP_ADD_TO_MODULE, &Function);
		TestRunner->TestTrue(TEXT("Builder CompileFunction failure test should fail the invalid function"), CompileResult < 0);
		TestRunner->TestEqual(TEXT("Builder CompileFunction failure test should not return a function"), Function, static_cast<asCScriptFunction*>(nullptr));
		TestRunner->TestEqual(TEXT("Builder CompileFunction failure test should not leak a module function"), static_cast<int32>(Module->GetFunctionCount()), 0);
		TestRunner->TestNull(TEXT("Builder CompileFunction failure test should not expose Entry"), Module->GetFunctionByDecl("int Entry()"));
	}

	TEST_METHOD(ParseScriptsCreatesParserNodes)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Builder parse test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderParseScripts");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder parse test should create a backing module"), Module) ||
			!AddBuilderSection(*TestRunner, *Module, "BuilderParse_A", R"(
class A
{
}
)") ||
			!AddBuilderSection(*TestRunner, *Module, "BuilderParse_B", R"(
enum EParseState
{
	Idle,
	Busy
}
)"))
		{
			return;
		}

		asCBuilder* Builder = Module->builder;
		if (!TestRunner->TestNotNull(TEXT("Adding script sections should create a builder"), Builder))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Builder parse should parse both sections"), Builder->BuildParallelParseScripts(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		TestRunner->TestEqual(TEXT("Builder parse should create one parser per section"), static_cast<int32>(Builder->parsers.GetLength()), 2);
		TestRunner->TestEqual(TEXT("Builder parse should not register global functions before function generation"), static_cast<int32>(Module->GetFunctionCount()), 0);
	}

	TEST_METHOD(GenerateTypesRegistersDeclarations)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Builder type-generation test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateTypes");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder type-generation test should create a backing module"), Module) ||
			!AddBuilderSection(*TestRunner, *Module, "BuilderTypes", R"(
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
)"))
		{
			return;
		}

		asCBuilder* Builder = Module->builder;
		if (!TestRunner->TestNotNull(TEXT("Builder type-generation test should create a builder"), Builder))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Builder type-generation test should parse scripts"), Builder->BuildParallelParseScripts(), static_cast<int32>(asSUCCESS)) ||
			!TestRunner->TestEqual(TEXT("Builder type-generation test should generate types"), Builder->BuildGenerateTypes(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		TestRunner->TestTrue(TEXT("Builder type-generation should discover class declarations"), Builder->classDeclarations.GetLength() >= 2);
		TestRunner->TestEqual(TEXT("Builder type-generation should not require rejected interface syntax"), static_cast<int32>(Builder->interfaceDeclarations.GetLength()), 0);
		TestRunner->TestNotNull(TEXT("Builder type-generation should register the class type on the module"), Module->GetTypeInfoByDecl("ActorState"));
		TestRunner->TestNotNull(TEXT("Builder type-generation should register the namespaced class type on the module"), Module->GetTypeInfoByDecl("Types::NestedState"));
		TestRunner->TestNotNull(TEXT("Builder type-generation should register the enum type on the module"), Module->GetTypeInfoByDecl("EState"));
	}

	TEST_METHOD(GenerateFunctionsRegistersGlobalsAndFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Builder function-generation test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateFunctions");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder function-generation test should create a backing module"), Module) ||
			!AddBuilderSection(*TestRunner, *Module, "BuilderFunctions", R"(
const int Base = 40;

int AddTwo()
{
	return Base + 2;
}
)"))
		{
			return;
		}

		asCBuilder* Builder = Module->builder;
		if (!TestRunner->TestNotNull(TEXT("Builder function-generation test should create a builder"), Builder))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Builder function-generation test should parse scripts"), Builder->BuildParallelParseScripts(), static_cast<int32>(asSUCCESS)) ||
			!TestRunner->TestEqual(TEXT("Builder function-generation test should generate types"), Builder->BuildGenerateTypes(), static_cast<int32>(asSUCCESS)) ||
			!TestRunner->TestEqual(TEXT("Builder function-generation test should generate functions"), Builder->BuildGenerateFunctions(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		TestRunner->TestEqual(TEXT("Builder function-generation should register one global function"), static_cast<int32>(Module->GetFunctionCount()), 1);
		TestRunner->TestEqual(TEXT("Builder function-generation should register one global variable"), static_cast<int32>(Module->GetGlobalVarCount()), 1);
		TestRunner->TestNotNull(TEXT("Builder function-generation should expose AddTwo by declaration"), Module->GetFunctionByDecl("int AddTwo()"));
	}

	TEST_METHOD(LayoutAndCompileProduceExecutableBytecode)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Builder compile test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileCode");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder compile test should create a backing module"), Module) ||
			!AddBuilderSection(*TestRunner, *Module, "BuilderCompile", R"(
int Helper(int Value)
{
	return Value * 2;
}

int Entry()
{
	return Helper(21);
}
)"))
		{
			return;
		}

		asCBuilder* Builder = Module->builder;
		if (!TestRunner->TestNotNull(TEXT("Builder compile test should create a builder"), Builder))
		{
			return;
		}

		if (!RunBuilderPipelineThroughLayout(*TestRunner, *Builder))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Builder compile test should compile function bytecode"), Builder->BuildCompileCode(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = Module->GetFunctionByDecl("int Entry()");
		if (!TestRunner->TestNotNull(TEXT("Builder compile should expose Entry after code generation"), Function))
		{
			return;
		}

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		TestRunner->TestNotNull(TEXT("Builder compile should emit bytecode for Entry"), Bytecode);
		TestRunner->TestTrue(TEXT("Builder compile should emit at least one bytecode dword"), BytecodeLength > 0);

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Builder compile should execute compiled bytecode"), Result, 42);
	}

	TEST_METHOD(StageFailureStopsBeforeExecutableCode)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Builder stage failure test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "BuilderStageFailure");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("Builder stage failure test should create a backing module"), Module) ||
			!AddBuilderSection(*TestRunner, *Module, "BuilderStageFailure", R"(
int Entry(
{
	return 42;
}
)"))
		{
			return;
		}

		asCBuilder* Builder = Module->builder;
		if (!TestRunner->TestNotNull(TEXT("Builder stage failure test should create a builder"), Builder))
		{
			return;
		}

		Builder->silent = true;
		const int ParseResult = Builder->BuildParallelParseScripts();
		TestRunner->TestTrue(TEXT("Builder stage failure should fail during parse"), ParseResult < 0);
		TestRunner->TestTrue(TEXT("Builder stage failure should record at least one builder error"), Builder->numErrors > 0);
		TestRunner->TestEqual(TEXT("Builder stage failure should not register executable functions"), static_cast<int32>(Module->GetFunctionCount()), 0);
		TestRunner->TestNull(TEXT("Builder stage failure should not expose Entry"), Module->GetFunctionByDecl("int Entry()"));
	}
};

#endif
