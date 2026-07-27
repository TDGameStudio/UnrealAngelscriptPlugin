#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleRenameReindexTests,
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
	TEST_METHOD(RenameReindexesEngineLookup)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-API-RENAME-REINDEX",
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
			TEXT("Module rename contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const char* const OldName = "ModuleApiRenameOld";
		const char* const NewName = "ModuleApiRenameNew";
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int RenamedValue()
			{
				return 17;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-API-RENAME-REINDEX-OLD-TO-NEW"),
			TEXT("ModuleApiRenameOld"),
			Source);

		FScopedNativeModuleName OldScope(Engine, OldName);
		FScopedNativeModuleName NewScope(Engine, NewName);
		asIScriptModule* const Module =
			BuildNativeModule(ScriptEngine, OldName, Source);
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Module rename contract should build its source")));
		if (Module == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		Module->SetName(NewName);
		ASSERT_THAT(AreEqual(
			FString(TEXT("ModuleApiRenameNew")),
			FString(UTF8_TO_TCHAR(Module->GetName())),
			TEXT("Module rename should publish the new name")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(OldName, asGM_ONLY_IF_EXISTS),
			TEXT("Module rename should remove the old engine lookup")));
		ASSERT_THAT(AreEqual(
			Module,
			ScriptEngine->GetModule(NewName, asGM_ONLY_IF_EXISTS),
			TEXT("Module rename should reindex the same module under the new name")));

		asIScriptFunction* const Function =
			Module->GetFunctionByDecl("int RenamedValue()");
		ASSERT_THAT(IsNotNull(
			Function,
			TEXT("Renamed module should retain its compiled function")));
		if (Function != nullptr)
		{
			ASSERT_THAT(IsTrue(ExecuteFunction(
				*TestRunner,
				*ScriptEngine,
				*Function,
				17)));
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(NewName),
			TEXT("Module rename cleanup should discard the module by its new name")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(NewName, asGM_ONLY_IF_EXISTS),
			TEXT("Discarding by the new name should remove the renamed engine lookup")));

		PrintSource(
			*TestRunner,
			TEXT("MOD-API-RENAME-REINDEX-NEW-NAME-REBUILD"),
			TEXT("ModuleApiRenameNew"),
			Source);
		asIScriptModule* const ReusedModule =
			BuildNativeModule(ScriptEngine, NewName, Source);
		ASSERT_THAT(IsNotNull(
			ReusedModule,
			TEXT("Discarded renamed module name should support a clean rebuild")));
		if (ReusedModule != nullptr)
		{
			asIScriptFunction* const ReusedFunction =
				ReusedModule->GetFunctionByDecl("int RenamedValue()");
			ASSERT_THAT(IsNotNull(
				ReusedFunction,
				TEXT("Clean renamed module should restore its exact function declaration")));
			if (ReusedFunction != nullptr)
			{
				ASSERT_THAT(IsTrue(ExecuteFunction(
					*TestRunner,
					*ScriptEngine,
					*ReusedFunction,
					17)));
			}
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(NewName),
				TEXT("Rebuilt renamed module should discard cleanly")));
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(NewName, asGM_ONLY_IF_EXISTS),
			TEXT("Renamed module cleanup should leave no stale new-name lookup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
