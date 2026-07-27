#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleNestedImportVisibilityTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.ApiContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* Id,
		const TCHAR* ModuleName,
		const char* Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			Id,
			ModuleName,
			FString(UTF8_TO_TCHAR(Source != nullptr ? Source : "")));
	}

	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* Id,
		const TCHAR* ModuleName,
		const std::string& Source)
	{
		PrintSource(Test, Id, ModuleName, Source.c_str());
	}

	static bool ExecuteFunction(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Function,
		int32 ExpectedValue)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assert.IsNotNull(
			Context,
			TEXT("Module API function execution should create a context")))
		{
			return false;
		}

		const int32 ExecuteResult = PrepareAndExecute(Context, &Function);
		const int32 ActualValue = static_cast<int32>(Context->GetReturnDWord());
		const int ReleaseResult = Context->Release();
		return Assert.AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			TEXT("Module API function should finish execution"))
			&& Assert.AreEqual(
				ExpectedValue,
				ActualValue,
				TEXT("Module API function should return the exact expected value"))
			&& Assert.AreEqual(
				0,
				ReleaseResult,
				TEXT("Module API function execution should release its case-owned context"));
	}

public:
	TEST_METHOD(NestedImportModuleVisibilityAndDeduplication)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-NESTED-IMPORT-VISIBILITY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Nested module visibility contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string BaseSource = ASTEST_AS_ANSI(R"AS(
			int BaseValue()
			{
				return 101;
			}
		)AS");
		const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
			int ProviderValue()
			{
				return 103;
			}
		)AS");
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			int LocalValue()
			{
				return 107;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-NESTED-IMPORT-VISIBILITY-BASE"),
			TEXT("ModuleApiNestedBase"),
			BaseSource);
		PrintSource(
			*TestRunner,
			TEXT("MOD-NESTED-IMPORT-VISIBILITY-PROVIDER"),
			TEXT("ModuleApiNestedProvider"),
			ProviderSource);
		PrintSource(
			*TestRunner,
			TEXT("MOD-NESTED-IMPORT-VISIBILITY-CONSUMER"),
			TEXT("ModuleApiNestedConsumer"),
			ConsumerSource);
		FScopedNativeModule Base(
			*TestRunner,
			Engine,
			"ModuleApiNestedBase",
			BaseSource);
		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ModuleApiNestedProvider",
			ProviderSource);
		FScopedNativeModule Consumer(
			*TestRunner,
			Engine,
			"ModuleApiNestedConsumer",
			ConsumerSource);
		if (!Base.IsValid() || !Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		Provider->ImportModule(Base.Get());
		Consumer->ImportModule(Provider.Get());
		asIScriptFunction* const BaseFunction =
			Base->GetFunctionByDecl("int BaseValue()");
		asIScriptFunction* const ProviderFunction =
			Provider->GetFunctionByDecl("int ProviderValue()");
		ASSERT_THAT(IsNotNull(
			BaseFunction,
			TEXT("Nested import visibility should resolve the base function")));
		ASSERT_THAT(IsNotNull(
			ProviderFunction,
			TEXT("Nested import visibility should resolve the provider function")));
		if (BaseFunction == nullptr || ProviderFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			ProviderFunction,
			Consumer->GetFunctionByDecl("int ProviderValue()"),
			TEXT("Consumer should see the directly imported provider function")));
		ASSERT_THAT(AreEqual(
			BaseFunction,
			Consumer->GetFunctionByDecl("int BaseValue()"),
			TEXT("Consumer should see the provider's flattened base import")));

		Consumer->ImportModule(Provider.Get());
		Consumer->ImportModule(Base.Get());
		ASSERT_THAT(AreEqual(
			ProviderFunction,
			Consumer->GetFunctionByDecl("int ProviderValue()"),
			TEXT("Repeated direct import should retain one stable provider lookup")));
		ASSERT_THAT(AreEqual(
			BaseFunction,
			Consumer->GetFunctionByDecl("int BaseValue()"),
			TEXT("Repeated flattened import should retain one stable base lookup")));
		ASSERT_THAT(IsTrue(ExecuteFunction(
			*TestRunner,
			*ScriptEngine,
			*ProviderFunction,
			103)));
		ASSERT_THAT(IsTrue(ExecuteFunction(
			*TestRunner,
			*ScriptEngine,
			*BaseFunction,
			101)));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer.Discard(),
			TEXT("Nested import cleanup should discard the consumer module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Provider.Discard(),
			TEXT("Nested import cleanup should discard the provider module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Base.Discard(),
			TEXT("Nested import cleanup should discard the base module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiNestedConsumer", asGM_ONLY_IF_EXISTS),
			TEXT("Nested import cleanup should remove the consumer module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiNestedProvider", asGM_ONLY_IF_EXISTS),
			TEXT("Nested import cleanup should remove the provider module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiNestedBase", asGM_ONLY_IF_EXISTS),
			TEXT("Nested import cleanup should remove the base module lookup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
