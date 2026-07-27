#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleUserDataLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.ApiContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FUserDataToken
	{
		int32 Value = 0;
	};

	inline static constexpr asPWORD PrimaryUserDataSlot =
		static_cast<asPWORD>(0x4D4F445052494D31ull);
	inline static constexpr asPWORD CleanupUserDataSlot =
		static_cast<asPWORD>(0x4D4F44434C45414Eull);

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

	static void ModuleUserDataCleanup(asIScriptModule* Module)
	{
		++CleanupCallCount;
		CleanupModule = Module;
		CleanupData = Module != nullptr
			? Module->GetUserData(CleanupUserDataSlot)
			: nullptr;
	}

public:
	inline static FUserDataToken UserDataA{ 11 };
	inline static FUserDataToken UserDataB{ 22 };
	inline static FUserDataToken CleanupToken{ 33 };
	inline static int32 CleanupCallCount = 0;
	inline static asIScriptModule* CleanupModule = nullptr;
	inline static void* CleanupData = nullptr;

	TEST_METHOD(UserDataTransitionsAndCleanup)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-USERDATA-LIFECYCLE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		CleanupCallCount = 0;
		CleanupModule = nullptr;
		CleanupData = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupCallCount = 0;
			CleanupModule = nullptr;
			CleanupData = nullptr;
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Module user-data contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const char* const ModuleName = "ModuleApiUserData";
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int UserDataOwner()
			{
				return 79;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-USERDATA-LIFECYCLE-INSTALL-REPLACE-CLEAR"),
			TEXT("ModuleApiUserData"),
			Source);
		FScopedNativeModuleName ModuleScope(Engine, ModuleName);
		asIScriptModule* const Module =
			BuildNativeModule(ScriptEngine, ModuleName, Source);
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Module user-data contract should build its owner")));
		if (Module == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(IsNull(
			Module->GetUserData(PrimaryUserDataSlot),
			TEXT("Module user-data slot should start empty")));
		ASSERT_THAT(IsNull(
			Module->SetUserData(&UserDataA, PrimaryUserDataSlot),
			TEXT("First module user-data install should return null")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataA),
			Module->GetUserData(PrimaryUserDataSlot),
			TEXT("Module user-data install should be observable")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataA),
			Module->SetUserData(&UserDataB, PrimaryUserDataSlot),
			TEXT("Module user-data replacement should return the prior pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataB),
			Module->GetUserData(PrimaryUserDataSlot),
			TEXT("Module user-data replacement should publish the new pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataB),
			Module->SetUserData(nullptr, PrimaryUserDataSlot),
			TEXT("Module user-data clear should return the replaced pointer")));
		ASSERT_THAT(IsNull(
			Module->GetUserData(PrimaryUserDataSlot),
			TEXT("Cleared module user-data slot should return null")));

		PrintSource(
			*TestRunner,
			TEXT("MOD-USERDATA-LIFECYCLE-DISCARD-CLEANUP"),
			TEXT("ModuleApiUserData"),
			ASTEST_AS_ANSI(R"AS(
				// Discard defers cleanup; successful engine GC retires the module exactly once.
			)AS"));
		ScriptEngine->SetModuleUserDataCleanupCallback(
			ModuleUserDataCleanup,
			CleanupUserDataSlot);
		ASSERT_THAT(IsNull(
			Module->SetUserData(&CleanupToken, CleanupUserDataSlot),
			TEXT("Cleanup slot should accept its sentinel")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleName),
			TEXT("Discard should hide the module user-data owner")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			TEXT("Discarded module should no longer be visible by name")));
		ASSERT_THAT(AreEqual(
			0,
			CleanupCallCount,
			TEXT("Discard should defer module cleanup until actual destruction")));

		Engine.Destroy();
		ASSERT_THAT(AreEqual(
			1,
			CleanupCallCount,
			TEXT("Engine shutdown should retire the discarded module exactly once")));
		ASSERT_THAT(AreEqual(
			static_cast<asIScriptModule*>(Module),
			CleanupModule,
			TEXT("Module cleanup callback should receive the exact discarded owner")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&CleanupToken),
			CleanupData,
			TEXT("Module cleanup callback should observe the exact stored sentinel")));
		Engine.Create(*TestRunner);
		ASSERT_THAT(AreEqual(
			1,
			CleanupCallCount,
			TEXT("Independent engine creation should not repeat the predecessor module cleanup callback")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
