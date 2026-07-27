#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

// Raw SDK module-import coverage.
#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FModuleImportTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.Imports",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

	TEST_METHOD(ImportMetadataBeforeBinding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-IMPORT-BINDING-CONTRACT",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
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
			TEXT("ScriptModule import metadata test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportMetadataConsumer", ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportMetadataProvider";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(1, static_cast<int32>(Consumer->GetImportedFunctionCount()),
			TEXT("ScriptModule import metadata should expose one imported function")));
		ASSERT_THAT(AreEqual(0, Consumer->GetImportedFunctionIndexByDecl("int SharedValue()"),
			TEXT("ScriptModule import metadata should resolve the import index by declaration")));
		ASSERT_THAT(AreEqual(FString(TEXT("int SharedValue()")), FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionDeclaration(0))),
			TEXT("ScriptModule import metadata should preserve the imported declaration")));
		ASSERT_THAT(AreEqual(FString(TEXT("ScriptModuleImportMetadataProvider")), FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionSourceModule(0))),
			TEXT("ScriptModule import metadata should preserve the source module name")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(Consumer.Discard()),
			TEXT("ScriptModule import metadata should explicitly discard the consumer")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("ScriptModuleImportMetadataConsumer", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule import metadata should remove the consumer and its import table from name lookup")));

		AngelscriptNativeTestSupport::FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("ScriptModule import metadata should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != ScriptEngine, TEXT("ScriptModule import metadata should isolate import tables by engine")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule("ScriptModuleImportMetadataConsumer", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule import metadata should not publish its consumer into an independent engine")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule("ScriptModuleImportMetadataProvider", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule import metadata should not materialize an unbound provider in any engine")));
	}

	TEST_METHOD(BindImportedFunctionExecutesProvider)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-IMPORT-BINDING-CONTRACT", "manual_bind_execute");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule import bind test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportBindProvider", ASTEST_AS_ANSI(R"AS(
			int SharedValue()
			{
				return 77;
			}
		)AS"));
		AngelscriptNativeTestSupport::FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportBindConsumer", ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportBindProvider";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* SourceFunction = Provider->GetFunctionByDecl("int SharedValue()");
		ASSERT_THAT(IsNotNull(SourceFunction,
			TEXT("ScriptModule import bind test should expose the provider function")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindImportedFunction(0, SourceFunction),
			TEXT("ScriptModule import bind test should bind the imported function")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(77, Result,
			TEXT("ScriptModule import bind test should execute the provider function through the consumer")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Consumer->GetImportedFunctionCount()),
			TEXT("ScriptModule import bind test should preserve the import inventory after binding")));
	}

	TEST_METHOD(BindImportedFunctionRejectsSignatureMismatch)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-IMPORT-BINDING-CONTRACT", "signature_mismatch_rejected");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule import mismatch test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportMismatchProvider", ASTEST_AS_ANSI(R"AS(
			int SharedValue(int Value)
			{
				return Value;
			}
		)AS"));
		AngelscriptNativeTestSupport::FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportMismatchConsumer", ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportMismatchProvider";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* SourceFunction = GetNativeFunctionByDecl(Provider, "int SharedValue(const int)");
		ASSERT_THAT(IsNotNull(SourceFunction,
			TEXT("ScriptModule import mismatch test should expose the mismatched provider function")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(SourceFunction->GetParamCount()),
			TEXT("ScriptModule import mismatch test should use a one-parameter provider")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asINVALID_INTERFACE), Consumer->BindImportedFunction(0, SourceFunction),
			TEXT("ScriptModule import mismatch test should reject incompatible signatures")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Consumer->GetImportedFunctionCount()),
			TEXT("ScriptModule import mismatch test should preserve the unbound import after rejection")));
	}

	TEST_METHOD(BindImportedFunctionRejectsInvalidIndex)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-IMPORT-BINDING-CONTRACT", "invalid_index_rejected");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule invalid-index test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportInvalidIndexProvider", ASTEST_AS_ANSI(R"AS(
			int SharedValue()
			{
				return 17;
			}
		)AS"));
		AngelscriptNativeTestSupport::FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportInvalidIndexConsumer", ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportInvalidIndexProvider";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* SourceFunction = Provider->GetFunctionByDecl("int SharedValue()");
		ASSERT_THAT(IsNotNull(SourceFunction,
			TEXT("ScriptModule invalid-index test should expose the compatible provider function")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asINVALID_ARG), Consumer->BindImportedFunction(7, SourceFunction),
			TEXT("ScriptModule invalid-index test should reject an out-of-range import index")));
		ASSERT_THAT(AreEqual(0, Consumer->GetImportedFunctionIndexByDecl("int SharedValue()"),
			TEXT("ScriptModule invalid-index test should preserve the valid import slot after rejection")));
	}

	TEST_METHOD(UnbindImportedFunctionAllowsRebind)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-IMPORT-BINDING-CONTRACT", "unbind_rebind_execute");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule import rebind test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule ProviderA(*TestRunner, Engine, "ScriptModuleImportRebindProviderA", ASTEST_AS_ANSI(R"AS(
			int SharedValue()
			{
				return 11;
			}
		)AS"));
		AngelscriptNativeTestSupport::FScopedNativeModule ProviderB(*TestRunner, Engine, "ScriptModuleImportRebindProviderB", ASTEST_AS_ANSI(R"AS(
			int SharedValue()
			{
				return 29;
			}
		)AS"));
		AngelscriptNativeTestSupport::FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportRebindConsumer", ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportRebindProviderA";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!ProviderA.IsValid() || !ProviderB.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* FirstFunction = ProviderA->GetFunctionByDecl("int SharedValue()");
		asIScriptFunction* SecondFunction = ProviderB->GetFunctionByDecl("int SharedValue()");
		ASSERT_THAT(IsNotNull(FirstFunction,
			TEXT("ScriptModule import rebind test should expose the first provider function")));
		ASSERT_THAT(IsNotNull(SecondFunction,
			TEXT("ScriptModule import rebind test should expose the second provider function")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindImportedFunction(0, FirstFunction),
			TEXT("ScriptModule import rebind test should bind the first provider")));

		int32 FirstResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", FirstResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(11, FirstResult,
			TEXT("ScriptModule import rebind test should execute the first provider")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->UnbindImportedFunction(0),
			TEXT("ScriptModule import rebind test should unbind the import")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindImportedFunction(0, SecondFunction),
			TEXT("ScriptModule import rebind test should bind the second provider")));

		int32 SecondResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", SecondResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(29, SecondResult,
			TEXT("ScriptModule import rebind test should execute the rebound provider")));
		ASSERT_THAT(AreEqual(FString(TEXT("int SharedValue()")), FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionDeclaration(0))),
			TEXT("ScriptModule import rebind test should preserve import metadata across provider replacement")));
	}

	TEST_METHOD(BindAllImportedFunctionsRejectsMissingProvider)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-IMPORT-BINDING-CONTRACT", "bind_all_missing_rejected");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule import bind-all test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName MissingScope(Engine, "ScriptModuleImportBindAllMissingConsumer");
		asIScriptModule* MissingConsumer = BuildNativeModule(ScriptEngine, MissingScope.Get(), ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportBindAllMissingProvider";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!this->Assert.IsNotNull(MissingConsumer,
			TEXT("ScriptModule import bind-all test should compile the missing-provider consumer")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asCANT_BIND_ALL_FUNCTIONS), MissingConsumer->BindAllImportedFunctions(),
			TEXT("ScriptModule import bind-all test should fail when provider is missing")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(MissingConsumer->GetImportedFunctionCount()),
			TEXT("ScriptModule import bind-all test should preserve the missing import after rejection")));
	}

	TEST_METHOD(BindAllImportedFunctionsExecutesProvider)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-IMPORT-BINDING-CONTRACT", "bind_all_execute");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule import bind-all execution test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportBindAllProvider", ASTEST_AS_ANSI(R"AS(
			int SharedValue()
			{
				return 42;
			}
		)AS"));
		AngelscriptNativeTestSupport::FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportBindAllConsumer", ASTEST_AS_ANSI(R"AS(
			import int SharedValue() from "ScriptModuleImportBindAllProvider";
			int Entry()
			{
				return SharedValue();
			}
		)AS"));
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindAllImportedFunctions(),
			TEXT("ScriptModule import bind-all test should bind all matching imports")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, Result,
			TEXT("ScriptModule import bind-all test should execute after automatic binding")));
	}
};

#endif
