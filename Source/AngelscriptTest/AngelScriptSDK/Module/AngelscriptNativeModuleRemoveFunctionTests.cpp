#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleRemoveFunctionTests,
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
	TEST_METHOD(RemoveFunctionPreservesExternalOwnership)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-API-REMOVE-FUNCTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Module RemoveFunction contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string OwnerSource = ASTEST_AS_ANSI(R"AS(
			int RemovedValue()
			{
				return 59;
			}

			int RetainedValue()
			{
				return 61;
			}
		)AS");
		const std::string ForeignSource = ASTEST_AS_ANSI(R"AS(
			int ForeignValue()
			{
				return 71;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-API-REMOVE-FUNCTION-OWNED"),
			TEXT("ModuleApiRemoveOwner"),
			OwnerSource);
		PrintSource(
			*TestRunner,
			TEXT("MOD-API-REMOVE-FUNCTION-FOREIGN"),
			TEXT("ModuleApiRemoveForeign"),
			ForeignSource);
		FScopedNativeModule Owner(
			*TestRunner,
			Engine,
			"ModuleApiRemoveOwner",
			OwnerSource);
		FScopedNativeModule Foreign(
			*TestRunner,
			Engine,
			"ModuleApiRemoveForeign",
			ForeignSource);
		if (!Owner.IsValid() || !Foreign.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* const RemovedFunction =
			Owner->GetFunctionByDecl("int RemovedValue()");
		asIScriptFunction* const RetainedFunction =
			Owner->GetFunctionByDecl("int RetainedValue()");
		asIScriptFunction* const ForeignFunction =
			Foreign->GetFunctionByDecl("int ForeignValue()");
		ASSERT_THAT(IsNotNull(RemovedFunction,
			TEXT("RemoveFunction contract should find its target")));
		ASSERT_THAT(IsNotNull(RetainedFunction,
			TEXT("RemoveFunction contract should find its control")));
		ASSERT_THAT(IsNotNull(ForeignFunction,
			TEXT("RemoveFunction contract should find its foreign control")));
		if (RemovedFunction == nullptr
			|| RetainedFunction == nullptr
			|| ForeignFunction == nullptr)
		{
			return;
		}

		RemovedFunction->AddRef();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asNO_FUNCTION),
			Owner->RemoveFunction(ForeignFunction),
			TEXT("RemoveFunction should reject a function owned by another module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Owner->RemoveFunction(RemovedFunction),
			TEXT("RemoveFunction should remove an owned global function")));
		ASSERT_THAT(IsNull(
			Owner->GetFunctionByDecl("int RemovedValue()"),
			TEXT("Removed function should disappear from declaration lookup")));
		ASSERT_THAT(AreEqual(
			RetainedFunction,
			Owner->GetFunctionByDecl("int RetainedValue()"),
			TEXT("Removing one function should preserve its sibling")));
		ASSERT_THAT(IsTrue(ExecuteFunction(
			*TestRunner,
			*ScriptEngine,
			*RemovedFunction,
			59)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asNO_FUNCTION),
			Owner->RemoveFunction(RemovedFunction),
			TEXT("Repeated removal should report that the function is no longer owned")));
		ASSERT_THAT(AreEqual(
			0,
			RemovedFunction->Release(),
			TEXT("Removed function release should consume the final external reference")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Owner.Discard(),
			TEXT("RemoveFunction cleanup should discard the owner module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Foreign.Discard(),
			TEXT("RemoveFunction cleanup should discard the foreign module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiRemoveOwner", asGM_ONLY_IF_EXISTS),
			TEXT("RemoveFunction cleanup should remove the owner module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ModuleApiRemoveForeign", asGM_ONLY_IF_EXISTS),
			TEXT("RemoveFunction cleanup should remove the foreign module lookup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
