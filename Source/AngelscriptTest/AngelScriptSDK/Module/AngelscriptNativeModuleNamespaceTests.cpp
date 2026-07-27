#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

// Raw SDK module-namespace coverage.

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FModuleNamespaceTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.Namespaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

	TEST_METHOD(DefaultNamespaceDoesNotRehomeDeclarations)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-NAMESPACE-LOOKUP-CONTRACT",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule namespace default test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceDefault");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule namespace default test should create a module")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SetDefaultNamespace("Gameplay"),
			TEXT("ScriptModule namespace default test should set the default namespace")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int Value = 42;

			int Entry()
			{
				return Value;
			}

			class Agent
			{
			}
			)AS");
		ASSERT_THAT(IsTrue(Module->AddScriptSection("ScriptModuleNamespaceDefault", ScriptSource.c_str(), ScriptSource.length(), 0) >= 0,
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

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("ScriptModule namespace default test should explicitly discard the owning module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule namespace default test should remove all declarations with the discarded module")));

		AngelscriptNativeTestSupport::FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(),
			TEXT("ScriptModule namespace default test should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != ScriptEngine,
			TEXT("ScriptModule namespace default test should isolate namespace state by engine")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule namespace default test should not publish its module into an independent engine")));
	}

	TEST_METHOD(ExplicitNamespaceOverridesDefaultLookup)
	{
		AS_NATIVE_PRODUCT_PART("MOD-NAMESPACE-LOOKUP-CONTRACT", "explicit_namespace_qualified_lookup");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule namespace explicit test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceExplicit");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule namespace explicit test should create a module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SetDefaultNamespace("Gameplay"),
			TEXT("ScriptModule namespace explicit test should set the default namespace")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
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
			)AS");
		ASSERT_THAT(IsTrue(Module->AddScriptSection("ScriptModuleNamespaceExplicit", ScriptSource.c_str(), ScriptSource.length(), 0) >= 0,
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
		AS_NATIVE_PRODUCT_PART("MOD-NAMESPACE-LOOKUP-CONTRACT", "invalid_default_preserves_previous");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule namespace invalid test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleNamespaceInvalid");
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
