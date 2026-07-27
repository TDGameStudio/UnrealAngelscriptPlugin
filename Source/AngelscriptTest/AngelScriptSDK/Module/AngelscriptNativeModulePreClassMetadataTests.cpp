#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModulePreClassMetadataTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.ApiContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FUserDataToken
	{
		int32 Value = 0;
	};

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
	TEST_METHOD(PreClassMetadataAppliesOnlyToExactDeclaration)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-PRECLASS-METADATA-APPLICATION",
			ENativeEvidence::Compile
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

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Pre-class metadata contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class NativePreClassExact
			{
				int Value;
			}

			class NativePreClassControl
			{
				int Value;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-PRECLASS-METADATA-APPLICATION-EXACT"),
			TEXT("ModuleApiPreClassMetadata"),
			ScriptSource);
		PrintSource(
			*TestRunner,
			TEXT("MOD-PRECLASS-METADATA-APPLICATION-CONTROL"),
			TEXT("ModuleApiPreClassMetadata"),
			ScriptSource);

		FScopedNativeModuleName ModuleScope(
			Engine,
			"ModuleApiPreClassMetadata");
		asIScriptModule* const Module =
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Pre-class metadata contract should create its owner module")));
		if (Module == nullptr)
		{
			return;
		}

		FUserDataToken InitialUserData{ 101 };
		asPreClassData ExactData;
		ExactData.PropertyOffset = 32;
		ExactData.InitialUserData = &InitialUserData;
		Module->AddPreClassData("NativePreClassExact", ExactData);

		asPreClassData UnmatchedData;
		UnmatchedData.PropertyOffset = 64;
		UnmatchedData.InitialUserData =
			reinterpret_cast<void*>(static_cast<UPTRINT>(1));
		Module->AddPreClassData("NativePreClassMissing", UnmatchedData);

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module->AddScriptSection(
				"ModuleApiPreClassMetadata.as",
				ScriptSource.c_str()),
			TEXT("Pre-class metadata contract should add its printed source")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module->Build(),
			TEXT("Pre-class metadata contract should build both declarations")));

		asITypeInfo* const ExactType =
			Module->GetTypeInfoByName("NativePreClassExact");
		asITypeInfo* const ControlType =
			Module->GetTypeInfoByName("NativePreClassControl");
		ASSERT_THAT(IsNotNull(
			ExactType,
			TEXT("Pre-class metadata contract should publish the exact declaration")));
		ASSERT_THAT(IsNotNull(
			ControlType,
			TEXT("Pre-class metadata contract should publish the control declaration")));
		if (ExactType == nullptr || ControlType == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<void*>(&InitialUserData),
			ExactType->GetUserData(),
			TEXT("Exact pre-class metadata should publish its initial user data")));
		ASSERT_THAT(IsNull(
			ControlType->GetUserData(),
			TEXT("An unmatched pre-class entry should not affect another declaration")));

		const char* ExactPropertyName = nullptr;
		int ExactPropertyTypeId = asINVALID_TYPE;
		int ExactPropertyOffset = -1;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ExactType->GetProperty(
				0,
				&ExactPropertyName,
				&ExactPropertyTypeId,
				nullptr,
				nullptr,
				&ExactPropertyOffset),
			TEXT("Exact pre-class declaration should expose its property metadata")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Value")),
			FString(UTF8_TO_TCHAR(
				ExactPropertyName != nullptr ? ExactPropertyName : "")),
			TEXT("Exact pre-class declaration should retain its property name")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asTYPEID_INT32),
			ExactPropertyTypeId,
			TEXT("Exact pre-class declaration should retain its property type")));
		ASSERT_THAT(AreEqual(
			32,
			ExactPropertyOffset,
			TEXT("Exact pre-class declaration should begin properties at the supplied offset")));
		ASSERT_THAT(IsTrue(
			ExactType->GetSize() >= 32 + sizeof(int32),
			TEXT("Exact pre-class declaration size should contain its supplied base offset and property")));

		const char* ControlPropertyName = nullptr;
		int ControlPropertyTypeId = asINVALID_TYPE;
		int ControlPropertyOffset = -1;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ControlType->GetProperty(
				0,
				&ControlPropertyName,
				&ControlPropertyTypeId,
				nullptr,
				nullptr,
				&ControlPropertyOffset),
			TEXT("Control declaration should expose its ordinary property metadata")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Value")),
			FString(UTF8_TO_TCHAR(
				ControlPropertyName != nullptr ? ControlPropertyName : "")),
			TEXT("Control declaration should retain its property name")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asTYPEID_INT32),
			ControlPropertyTypeId,
			TEXT("Control declaration should retain its property type")));
		ASSERT_THAT(AreEqual(
			0,
			ControlPropertyOffset,
			TEXT("Control declaration should retain its ordinary zero-based layout")));
		ASSERT_THAT(IsTrue(
			ControlType->GetSize() < 32,
			TEXT("Unmatched pre-class metadata should not inflate the control declaration")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("Pre-class metadata cleanup should discard its owner module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Pre-class metadata cleanup should remove the owner module lookup")));
		ASSERT_THAT(AreEqual(
			101,
			InitialUserData.Value,
			TEXT("Module discard should not consume application-owned pre-class user data")));
		asIScriptModule* const CleanModule =
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			CleanModule,
			TEXT("Pre-class metadata cleanup should create a clean same-name module")));
		if (CleanModule != nullptr)
		{
			ASSERT_THAT(IsNull(
				CleanModule->GetTypeInfoByName("NativePreClassExact"),
				TEXT("Clean module should not retain the exact pre-class type")));
			ASSERT_THAT(IsNull(
				CleanModule->GetTypeInfoByName("NativePreClassControl"),
				TEXT("Clean module should not retain the control pre-class type")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(ModuleScope.Get()),
				TEXT("Clean pre-class metadata module should discard after type cleanup inspection")));
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Pre-class metadata cleanup should leave no recreated module lookup")));
		ASSERT_THAT(AreEqual(
			101,
			InitialUserData.Value,
			TEXT("Same-name clean rebuild should preserve application-owned pre-class user data")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
