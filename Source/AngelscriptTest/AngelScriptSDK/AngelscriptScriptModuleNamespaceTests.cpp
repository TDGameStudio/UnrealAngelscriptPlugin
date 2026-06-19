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
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule namespace default test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceDefault");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule namespace default test should create a module")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SetDefaultNamespace("Gameplay"),
			TEXT("ScriptModule namespace default test should set the default namespace")));

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
		ASSERT_THAT(IsTrue(Module->AddScriptSection("ScriptModuleNamespaceDefault", Source, std::strlen(Source), 0) >= 0,
			TEXT("ScriptModule namespace default test should add script source")));
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Module->Build(),
			TEXT("ScriptModule namespace default test should build the module")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("ScriptModule namespace default test should resolve Entry through parent namespace fallback")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), FString(UTF8_TO_TCHAR(Function->GetNamespace())),
			TEXT("ScriptModule namespace default test should keep unqualified declarations in the global namespace")));
		ASSERT_THAT(IsTrue(static_cast<asCModule*>(Module)->GetGlobalVarIndexByDecl("const int Value") >= 0,
			TEXT("ScriptModule namespace default test should resolve the global through parent namespace fallback")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("Agent"),
			TEXT("ScriptModule namespace default test should expose the class through parent namespace fallback")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay")), FString(UTF8_TO_TCHAR(Module->GetDefaultNamespace())),
			TEXT("ScriptModule namespace default test should keep the module default namespace setting")));
	}

	TEST_METHOD(ExplicitNamespaceOverridesDefaultLookup)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule namespace explicit test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceExplicit");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule namespace explicit test should create a module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SetDefaultNamespace("Gameplay"),
			TEXT("ScriptModule namespace explicit test should set the default namespace")));

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
		ASSERT_THAT(IsTrue(Module->AddScriptSection("ScriptModuleNamespaceExplicit", Source, std::strlen(Source), 0) >= 0,
			TEXT("ScriptModule namespace explicit test should add script source")));
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Module->Build(),
			TEXT("ScriptModule namespace explicit test should build the module")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* DefaultFunction = Module->GetFunctionByDecl("int DefaultEntry()");
		asIScriptFunction* ExplicitFunction = Module->GetFunctionByDecl("int Tools::ExplicitEntry()");
		ASSERT_THAT(IsNotNull(DefaultFunction,
			TEXT("ScriptModule namespace explicit test should resolve the default namespace function")));
		ASSERT_THAT(IsNotNull(ExplicitFunction,
			TEXT("ScriptModule namespace explicit test should resolve the explicit namespace function")));

		ASSERT_THAT(AreEqual(FString(TEXT("")), FString(UTF8_TO_TCHAR(DefaultFunction->GetNamespace())),
			TEXT("ScriptModule namespace explicit test should keep unqualified default function in the global namespace")));
		ASSERT_THAT(AreEqual(FString(TEXT("Tools")), FString(UTF8_TO_TCHAR(ExplicitFunction->GetNamespace())),
			TEXT("ScriptModule namespace explicit test should store explicit function in Tools")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int ExplicitEntry()"),
			TEXT("ScriptModule namespace explicit test should not resolve an explicit namespace function as default")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("Tools::ToolState"),
			TEXT("ScriptModule namespace explicit test should expose the explicit namespace type")));
	}

	TEST_METHOD(InvalidDefaultNamespaceRejected)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule namespace invalid test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceInvalid");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule namespace invalid test should create a module")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SetDefaultNamespace("Valid"),
			TEXT("ScriptModule namespace invalid test should set a known valid namespace")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asINVALID_DECLARATION), Module->SetDefaultNamespace("Invalid::123"),
			TEXT("ScriptModule namespace invalid test should reject malformed namespace syntax")));
		ASSERT_THAT(AreEqual(FString(TEXT("Valid")), FString(UTF8_TO_TCHAR(Module->GetDefaultNamespace())),
			TEXT("ScriptModule namespace invalid test should preserve the previous namespace")));
	}
};

#endif
