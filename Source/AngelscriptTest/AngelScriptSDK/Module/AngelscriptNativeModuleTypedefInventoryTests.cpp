#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleTypedefInventoryTests,
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

public:
	TEST_METHOD(TypedefInventoryUsesForkEmptyBoundary)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-TYPEDEF-INVENTORY-BOUNDS",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
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
			TEXT("Module typedef inventory contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string RejectedSource = ASTEST_AS_ANSI(R"AS(
			typedef int ModuleAlias;
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-TYPEDEF-INVENTORY-BOUNDS-FORK-REJECTION"),
			TEXT("ModuleApiTypedefRejected"),
			RejectedSource);
		FScopedNativeModuleName RejectedScope(Engine, "ModuleApiTypedefRejected");
		asIScriptModule* const RejectedModule =
			ScriptEngine->GetModule(RejectedScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			RejectedModule,
			TEXT("Typedef rejection should create an isolated module")));
		if (RejectedModule == nullptr)
		{
			return;
		}

		Engine.ResetMessages();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			RejectedModule->AddScriptSection(
				"ModuleApiTypedefRejected.as",
				RejectedSource.c_str()),
			TEXT("Typedef rejection source should be accepted as a script section")));
		ASSERT_THAT(IsTrue(
			RejectedModule->Build() < 0,
			TEXT("Current fork should reject script-level typedef syntax")));
		ASSERT_THAT(IsTrue(
			Engine.GetMessagesText().Contains(TEXT("Expected identifier")),
			TEXT("Typedef rejection should retain the parser-boundary diagnostic")));
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(RejectedModule->GetTypedefCount()),
			TEXT("Rejected typedef build should not publish a partial typedef inventory")));
		ASSERT_THAT(IsNull(
			RejectedModule->GetTypedefByIndex(0),
			TEXT("Rejected typedef build should have no typedef at index zero")));

		PrintSource(
			*TestRunner,
			TEXT("MOD-TYPEDEF-INVENTORY-BOUNDS-EMPTY-ZERO"),
			TEXT("ModuleApiTypedefEmpty"),
			ASTEST_AS_ANSI(R"AS(
				// Script typedef is fork-rejected, so the public module inventory remains empty.
			)AS"));
		FScopedNativeModuleName EmptyScope(Engine, "ModuleApiTypedefEmpty");
		asIScriptModule* const EmptyModule =
			ScriptEngine->GetModule(EmptyScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			EmptyModule,
			TEXT("Empty typedef inventory should create an isolated module")));
		if (EmptyModule == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(EmptyModule->GetTypedefCount()),
			TEXT("Fork-rejected script typedef should leave the module inventory empty")));
		ASSERT_THAT(IsNull(
			EmptyModule->GetTypedefByIndex(0),
			TEXT("Empty typedef inventory should return null at index zero")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(RejectedScope.Get()),
			TEXT("Typedef rejection cleanup should discard the rejected module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(EmptyScope.Get()),
			TEXT("Typedef inventory cleanup should discard the empty module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(RejectedScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Typedef rejection cleanup should remove the rejected module lookup")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(EmptyScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Typedef inventory cleanup should remove the empty module lookup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
