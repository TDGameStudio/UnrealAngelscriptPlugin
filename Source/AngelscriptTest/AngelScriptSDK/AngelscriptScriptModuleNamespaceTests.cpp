#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptModuleNamespaceTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptModule.Namespace",
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

	TEST_METHOD(DefaultNamespaceDoesNotRehomeDeclarations)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace default test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceDefault");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace default test should create a module"), Module))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("ScriptModule namespace default test should set the default namespace"), Module->SetDefaultNamespace("Gameplay"), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		const char* Source = R"(
const int Value = 42;

int Entry()
{
	return Value;
}

class Agent
{
}
)";
		if (!TestRunner->TestTrue(TEXT("ScriptModule namespace default test should add script source"), Module->AddScriptSection("ScriptModuleNamespaceDefault", Source, std::strlen(Source), 0) >= 0))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ScriptModule namespace default test should build the module"), Module->Build(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = Module->GetFunctionByDecl("int Entry()");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace default test should resolve Entry through parent namespace fallback"), Function))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("ScriptModule namespace default test should keep unqualified declarations in the global namespace"), FString(UTF8_TO_TCHAR(Function->GetNamespace())), FString(TEXT("")));
		TestRunner->TestTrue(TEXT("ScriptModule namespace default test should resolve the global through parent namespace fallback"), static_cast<asCModule*>(Module)->GetGlobalVarIndexByDecl("const int Value") >= 0);
		TestRunner->TestNotNull(TEXT("ScriptModule namespace default test should expose the class through parent namespace fallback"), Module->GetTypeInfoByDecl("Agent"));
		TestRunner->TestEqual(TEXT("ScriptModule namespace default test should keep the module default namespace setting"), FString(UTF8_TO_TCHAR(Module->GetDefaultNamespace())), FString(TEXT("Gameplay")));
	}

	TEST_METHOD(ExplicitNamespaceOverridesDefaultLookup)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace explicit test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceExplicit");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace explicit test should create a module"), Module))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ScriptModule namespace explicit test should set the default namespace"), Module->SetDefaultNamespace("Gameplay"), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		const char* Source = R"(
int DefaultEntry()
{
	return 1;
}

namespace Tools
{
	int ExplicitEntry()
	{
		return 2;
	}

	class ToolState
	{
	}
}
)";
		if (!TestRunner->TestTrue(TEXT("ScriptModule namespace explicit test should add script source"), Module->AddScriptSection("ScriptModuleNamespaceExplicit", Source, std::strlen(Source), 0) >= 0))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ScriptModule namespace explicit test should build the module"), Module->Build(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* DefaultFunction = Module->GetFunctionByDecl("int DefaultEntry()");
		asIScriptFunction* ExplicitFunction = Module->GetFunctionByDecl("int Tools::ExplicitEntry()");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace explicit test should resolve the default namespace function"), DefaultFunction) ||
			!TestRunner->TestNotNull(TEXT("ScriptModule namespace explicit test should resolve the explicit namespace function"), ExplicitFunction))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule namespace explicit test should keep unqualified default function in the global namespace"), FString(UTF8_TO_TCHAR(DefaultFunction->GetNamespace())), FString(TEXT("")));
		TestRunner->TestEqual(TEXT("ScriptModule namespace explicit test should store explicit function in Tools"), FString(UTF8_TO_TCHAR(ExplicitFunction->GetNamespace())), FString(TEXT("Tools")));
		TestRunner->TestNull(TEXT("ScriptModule namespace explicit test should not resolve an explicit namespace function as default"), Module->GetFunctionByDecl("int ExplicitEntry()"));
		TestRunner->TestNotNull(TEXT("ScriptModule namespace explicit test should expose the explicit namespace type"), Module->GetTypeInfoByDecl("Tools::ToolState"));
	}

	TEST_METHOD(InvalidDefaultNamespaceRejected)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace invalid test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceInvalid");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("ScriptModule namespace invalid test should create a module"), Module))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("ScriptModule namespace invalid test should set a known valid namespace"), Module->SetDefaultNamespace("Valid"), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule namespace invalid test should reject malformed namespace syntax"), Module->SetDefaultNamespace("Invalid::123"), static_cast<int32>(asINVALID_DECLARATION));
		TestRunner->TestEqual(TEXT("ScriptModule namespace invalid test should preserve the previous namespace"), FString(UTF8_TO_TCHAR(Module->GetDefaultNamespace())), FString(TEXT("Valid")));
	}
};

#endif
