#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleImportUnbindAllTests,
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

	static bool ExpectUnboundFunctionException(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Function,
		const TCHAR* Scenario)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assert.IsNotNull(
			Context,
			*FString::Printf(
				TEXT("%s should create a case-owned context"),
				Scenario)))
		{
			return false;
		}

		const int32 PrepareResult = Context->Prepare(&Function);
		const int32 ExecuteResult = PrepareResult >= 0
			? Context->Execute()
			: PrepareResult;
		const char* const ExceptionString = Context->GetExceptionString();
		const FString ExceptionText = FString(UTF8_TO_TCHAR(
			ExceptionString != nullptr ? ExceptionString : ""));
		const int32 UnprepareResult = Context->Unprepare();
		const int32 ReleaseResult = Context->Release();
		return Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			PrepareResult,
			*FString::Printf(
				TEXT("%s should prepare after its import is unbound"),
				Scenario))
			&& Assert.AreEqual(
				static_cast<int32>(asEXECUTION_EXCEPTION),
				ExecuteResult,
				*FString::Printf(
					TEXT("%s should fail with a script exception after unbind-all"),
					Scenario))
			&& Assert.IsNotNull(
				ExceptionString,
				*FString::Printf(
					TEXT("%s should publish an exception string"),
					Scenario))
			&& Assert.IsTrue(
				ExceptionText.Contains(TEXT("Unbound function")),
				*FString::Printf(
					TEXT("%s should identify the missing import binding"),
					Scenario))
			&& Assert.AreEqual(
				static_cast<int32>(asSUCCESS),
				UnprepareResult,
				*FString::Printf(
					TEXT("%s should unprepare its exception context"),
					Scenario))
			&& Assert.AreEqual(
				0,
				ReleaseResult,
				*FString::Printf(
					TEXT("%s should release its case-owned context"),
					Scenario));
	}

public:
	TEST_METHOD(UnbindAllImportsThenRebinds)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-IMPORT-UNBIND-ALL",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
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
			TEXT("Module unbind-all contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
			int FirstValue()
			{
				return 83;
			}

			int SecondValue()
			{
				return 89;
			}
		)AS");
		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
			import int FirstValue() from "ModuleApiUnbindProvider";
			import int SecondValue() from "ModuleApiUnbindProvider";

			int Entry()
			{
				return FirstValue() + SecondValue();
			}

			int FirstEntry()
			{
				return FirstValue();
			}

			int SecondEntry()
			{
				return SecondValue();
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-IMPORT-UNBIND-ALL-PROVIDER"),
			TEXT("ModuleApiUnbindProvider"),
			ProviderSource);
		PrintSource(
			*TestRunner,
			TEXT("MOD-IMPORT-UNBIND-ALL-CONSUMER"),
			TEXT("ModuleApiUnbindConsumer"),
			ConsumerSource);
		FScopedNativeModule Provider(
			*TestRunner,
			Engine,
			"ModuleApiUnbindProvider",
			ProviderSource);
		FScopedNativeModule Consumer(
			*TestRunner,
			Engine,
			"ModuleApiUnbindConsumer",
			ConsumerSource);
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->BindAllImportedFunctions(),
			TEXT("Consumer should bind both provider imports")));
		asIScriptFunction* const Entry =
			Consumer->GetFunctionByDecl("int Entry()");
		asIScriptFunction* const FirstEntry =
			Consumer->GetFunctionByDecl("int FirstEntry()");
		asIScriptFunction* const SecondEntry =
			Consumer->GetFunctionByDecl("int SecondEntry()");
		ASSERT_THAT(IsNotNull(
			Entry,
			TEXT("Consumer should expose its import-calling entry")));
		ASSERT_THAT(IsNotNull(
			FirstEntry,
			TEXT("Consumer should expose its first-import entry")));
		ASSERT_THAT(IsNotNull(
			SecondEntry,
			TEXT("Consumer should expose its second-import entry")));
		if (Entry == nullptr || FirstEntry == nullptr || SecondEntry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExecuteFunction(
			*TestRunner,
			*ScriptEngine,
			*Entry,
			172)));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->UnbindAllImportedFunctions(),
			TEXT("Consumer should unbind all imported functions")));
		ASSERT_THAT(IsTrue(ExpectUnboundFunctionException(
			*TestRunner,
			*ScriptEngine,
			*FirstEntry,
			TEXT("First import entry"))));
		ASSERT_THAT(IsTrue(ExpectUnboundFunctionException(
			*TestRunner,
			*ScriptEngine,
			*SecondEntry,
			TEXT("Second import entry"))));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer->BindAllImportedFunctions(),
			TEXT("Consumer should rebind all imports after unbind-all")));
		ASSERT_THAT(IsTrue(ExecuteFunction(
			*TestRunner,
			*ScriptEngine,
			*Entry,
			172)));
		asIScriptContext* const ReusedContext = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(
			ReusedContext,
			TEXT("Rebound import execution should create a clean replacement context")));
		if (ReusedContext != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ReusedContext->Prepare(Entry),
				TEXT("Replacement context should prepare the rebound import entry")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				ReusedContext->Execute(),
				TEXT("Replacement context should execute rebound imports without stale exception state")));
			ASSERT_THAT(AreEqual(
				172,
				static_cast<int32>(ReusedContext->GetReturnDWord()),
				TEXT("Replacement context should return the rebound import result")));
			ASSERT_THAT(AreEqual(
				0,
				ReusedContext->Release(),
				TEXT("Replacement context release should consume the reuse baseline context")));
		}

		const std::string EmptySource = ASTEST_AS_ANSI(R"AS(
			int LocalOnly()
			{
				return 97;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-IMPORT-UNBIND-ALL-EMPTY"),
			TEXT("ModuleApiUnbindEmpty"),
			EmptySource);
		FScopedNativeModule EmptyModule(
			*TestRunner,
			Engine,
			"ModuleApiUnbindEmpty",
			EmptySource);
		if (EmptyModule.IsValid())
		{
			asIScriptFunction* const LocalOnly =
				EmptyModule->GetFunctionByDecl("int LocalOnly()");
			ASSERT_THAT(IsNotNull(
				LocalOnly,
				TEXT("Zero-import module should expose its local control function")));
			if (LocalOnly != nullptr)
			{
				ASSERT_THAT(IsTrue(ExecuteFunction(
					*TestRunner,
					*ScriptEngine,
					*LocalOnly,
					97)));
			}
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				EmptyModule->UnbindAllImportedFunctions(),
				TEXT("Unbind-all should be idempotently successful with zero imports")));
			if (LocalOnly != nullptr)
			{
				ASSERT_THAT(IsTrue(ExecuteFunction(
					*TestRunner,
					*ScriptEngine,
					*LocalOnly,
					97)));
			}
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			EmptyModule.Discard(),
			TEXT("Unbind-all cleanup should discard the empty module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Consumer.Discard(),
			TEXT("Unbind-all cleanup should discard the consumer module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Provider.Discard(),
			TEXT("Unbind-all cleanup should discard the provider module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiUnbindEmpty", asGM_ONLY_IF_EXISTS),
			TEXT("Unbind-all cleanup should remove the empty module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiUnbindConsumer", asGM_ONLY_IF_EXISTS),
			TEXT("Unbind-all cleanup should remove the consumer module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiUnbindProvider", asGM_ONLY_IF_EXISTS),
			TEXT("Unbind-all cleanup should remove the provider module lookup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
