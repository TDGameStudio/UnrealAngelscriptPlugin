#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_AngelscriptBuilderTests_Private
{
	asCModule* CreateBuilderModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SingleModulePipeline)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptBuilderTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"BuilderSinglePipeline",
			TEXT("int Entry() { return 42; }"));
		if (!TestRunner->TestNotNull(TEXT("Builder single-module test should create a backing module"), Module))
		{
			return;
		}
		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		if (!TestRunner->TestNotNull(TEXT("Builder single-module pipeline should expose the compiled function"), Function))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Builder single-module pipeline should execute the compiled function"), Result, 42);
		}
	}

	TEST_METHOD(CompileErrors)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptBuilderTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		asCModule* Module = CreateBuilderModule(ScriptEngine, "BuilderCompileErrors");
		if (!TestRunner->TestNotNull(TEXT("Builder compile-error test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptFunction* Function = nullptr;
		const int32 BuildResult = Builder.CompileFunction("BuilderCompileErrors", "int Entry( { return 42; }", 0, 0, &Function);
		TestRunner->TestTrue(TEXT("Builder should report invalid syntax as a build failure"), BuildResult < 0);
		TestRunner->TestEqual(TEXT("Builder compile-error test should not return a compiled function on failure"), Function, static_cast<asCScriptFunction*>(nullptr));
		}
	}

	TEST_METHOD(RebuildModule)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptBuilderTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* ModuleV1 = BuildModule(
			*TestRunner,
			Engine,
			"BuilderRebuild",
			TEXT("int Entry() { return 1; }"));
		if (!TestRunner->TestNotNull(TEXT("Builder rebuild test should create the initial backing module"), ModuleV1))
		{
			return;
		}
		asIScriptFunction* FunctionV1 = GetFunctionByDecl(*TestRunner, *ModuleV1, TEXT("int Entry()"));
		if (!TestRunner->TestNotNull(TEXT("Initial builder rebuild compile should expose Entry()"), FunctionV1))
		{
			return;
		}

		int32 FirstResult = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *FunctionV1, FirstResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Initial builder rebuild function should return the first version"), FirstResult, 1);

		asIScriptModule* ModuleV2 = BuildModule(
			*TestRunner,
			Engine,
			"BuilderRebuild",
			TEXT("int Entry() { return 2; }"));
		if (!TestRunner->TestNotNull(TEXT("Builder rebuild test should create the rebuilt module"), ModuleV2))
		{
			return;
		}
		asIScriptFunction* FunctionV2 = GetFunctionByDecl(*TestRunner, *ModuleV2, TEXT("int Entry()"));
		if (!TestRunner->TestNotNull(TEXT("Rebuilt builder module should expose Entry()"), FunctionV2))
		{
			return;
		}

		int32 SecondResult = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *FunctionV2, SecondResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Rebuilt builder module should execute the latest function body"), SecondResult, 2);
		}
	}

	TEST_METHOD(ImportBinding)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptBuilderTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* SourceModule = BuildModule(
			*TestRunner,
			Engine,
			"BuilderImportSource",
			TEXT("int SharedValue() { return 77; }"));
		asIScriptModule* ConsumerModule = BuildModule(
			*TestRunner,
			Engine,
			"BuilderImportConsumer",
			TEXT("import int SharedValue() from \"BuilderImportSource\"; int Entry() { return SharedValue(); }"));
		if (!TestRunner->TestNotNull(TEXT("Builder import test should create the source module"), SourceModule) ||
			!TestRunner->TestNotNull(TEXT("Builder import test should create the consumer module"), ConsumerModule))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Consumer module should expose one imported function before binding"), static_cast<int32>(ConsumerModule->GetImportedFunctionCount()), 1);
		TestRunner->TestEqual(TEXT("Imported function should point to the expected source module"), FString(UTF8_TO_TCHAR(ConsumerModule->GetImportedFunctionSourceModule(0))), FString(TEXT("BuilderImportSource")));

		asIScriptFunction* SourceFunction = SourceModule->GetFunctionByDecl("int SharedValue()");
		if (!TestRunner->TestNotNull(TEXT("Builder import test should expose the source function for binding"), SourceFunction))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("BindImportedFunction should resolve the imported function"), ConsumerModule->BindImportedFunction(0, SourceFunction), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		asIScriptFunction* EntryFunction = GetFunctionByDecl(*TestRunner, *ConsumerModule, TEXT("int Entry()"));
		if (!TestRunner->TestNotNull(TEXT("Consumer module should expose Entry() after import binding"), EntryFunction))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *EntryFunction, Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Imported function binding should let the consumer execute the source function"), Result, 77);
		}
	}
};

#endif
