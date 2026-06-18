#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	asIScriptModule* CreateScriptModule(asIScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return ScriptEngine != nullptr
			? ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE)
			: nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptModuleTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptModule",
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

	TEST_METHOD(SingleModulePipeline)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule single-module test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleSinglePipeline", R"(
int Entry()
{
	return 42;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule single-module pipeline should execute the compiled function"), Result, 42);
	}

	TEST_METHOD(RebuildModule)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule rebuild test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleRebuild");
		asIScriptModule* ModuleV1 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), R"(
int Entry()
{
	return 1;
}
)");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule rebuild test should create the initial backing module"), ModuleV1))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 FirstResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV1, "int Entry()", FirstResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Initial script module rebuild function should return the first version"), FirstResult, 1);

		asIScriptModule* ModuleV2 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), R"(
int Entry()
{
	return 2;
}
)");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule rebuild test should create the rebuilt module"), ModuleV2))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 SecondResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV2, "int Entry()", SecondResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Rebuilt script module should execute the latest function body"), SecondResult, 2);
	}

	TEST_METHOD(MultiSectionBuild)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule multi-section test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleMultiSection");
		asIScriptModule* Module = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		if (!TestRunner->TestNotNull(TEXT("ScriptModule multi-section test should create a backing module"), Module))
		{
			return;
		}

		const char* Section1 = R"(
int Add(int A, int B)
{
	return A + B;
}
)";
		const char* Section2 = R"(
int Entry()
{
	return Add(10, 20);
}
)";
		const int AddFirstResult = Module->AddScriptSection("ScriptModuleMultiSection_A", Section1, std::strlen(Section1), 0);
		const int AddSecondResult = Module->AddScriptSection("ScriptModuleMultiSection_B", Section2, std::strlen(Section2), 0);
		if (!TestRunner->TestTrue(TEXT("ScriptModule multi-section test should add both script sections"), AddFirstResult >= 0 && AddSecondResult >= 0))
		{
			return;
		}

		const int BuildResult = Module->Build();
		if (!TestRunner->TestEqual(TEXT("ScriptModule multi-section test should compile both sections"), BuildResult, static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule multi-section test should execute cross-section call Add(10,20)=30"), Result, 30);
	}
};

#endif
