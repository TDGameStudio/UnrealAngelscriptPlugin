#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEnumTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.Enums",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static TArray<void*> CleanedEnumData;

	static void ObserveEnumCleanup(asITypeInfo* Type)
	{
		CleanedEnumData.Add(
			Type != nullptr
				? Type->GetUserData()
				: nullptr);
	}

public:
	TEST_METHOD(EnumTypeEnum)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("TYPE-ENUM-REGISTRATION-RUNTIME",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Runtime
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		CleanedEnumData.Reset();
		ON_SCOPE_EXIT
		{
			CleanedEnumData.Reset();
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Enum type test should create a standalone engine")));

		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("myenum") >= 0, TEXT("Enum type test should register the global enum")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0, TEXT("Enum type test should register the global enum value")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetDefaultNamespace("foo"), TEXT("Enum type test should select a namespace before namespace-local registration")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("myenum") >= 0, TEXT("Enum type test should permit a namespace-local enum with the same short name")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0, TEXT("Enum type test should register the namespace-local enum value")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetDefaultNamespace(""), TEXT("Enum type test should restore the global namespace")));

		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("TEST_ENUM") >= 0, TEXT("Enum type test should register a second enum")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("TEST_ENUM", "ENUM1", 1) >= 0, TEXT("Enum type test should register its value")));

		const FString Source = ASTEST_AS(R"AS(
			enum LocalEnum
			{
				LocalValue = 1
			}

			int ReturnLocalEnumValue()
			{
				LocalEnum Value = LocalEnum::LocalValue;
				return Value == LocalEnum::LocalValue ? 1 : 0;
			}
			)AS");
		const TCHAR* Origins[] =
		{
			TEXT("native-global"),
			TEXT("native-namespace"),
			TEXT("script-local"),
		};
		const TCHAR* Observations[] =
		{
			TEXT("metadata"),
			TEXT("runtime"),
		};
		for (const TCHAR* Origin : Origins)
		{
			for (const TCHAR* Observation : Observations)
			{
				PrintGeneratedAsSource(
					*TestRunner,
					MakeNativeCaseId(
						"TYPE-ENUM-REGISTRATION-RUNTIME",
						{ Origin, Observation }),
					TEXT("TypeEnumRegistrationRuntime"),
					Source);
			}
		}

		const FTCHARToUTF8 SourceUtf8(*Source);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"EnumTypeEnum",
			SourceUtf8.Get());
		if (!Module.IsValid())
		{
			return;
		}

		{
			FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				Module,
				"int ReturnLocalEnumValue()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("Enum type test should resolve its exact script entry declaration")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					1,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Enum type test should execute local-enum equality")));
			}
		}

		const int TypeId = ScriptEngine->GetTypeIdByDecl("TEST_ENUM");
		ASSERT_THAT(IsTrue(TypeId >= 0, TEXT("Enum type test should resolve the registered enum type id")));
		ASSERT_THAT(AreEqual(FString(TEXT("TEST_ENUM")), FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(TypeId))),
			TEXT("Enum type test should preserve its registered declaration")));
		asITypeInfo* const RegisteredType = ScriptEngine->GetTypeInfoById(TypeId);
		ASSERT_THAT(IsNotNull(
			RegisteredType,
			TEXT("Enum registration product should resolve global enum TypeInfo")));
		if (RegisteredType != nullptr)
		{
			ASSERT_THAT(IsTrue(
				(RegisteredType->GetFlags() & asOBJ_ENUM) != 0,
				TEXT("Registered enum should publish the enum type flag")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(1),
				static_cast<int32>(RegisteredType->GetEnumValueCount()),
				TEXT("Registered enum should publish one value")));
			int EnumValue = INDEX_NONE;
			const char* const EnumName =
				RegisteredType->GetEnumValueByIndex(0, &EnumValue);
			ASSERT_THAT(AreEqual(
				FString(TEXT("ENUM1")),
				FString(UTF8_TO_TCHAR(EnumName)),
				TEXT("Registered enum should preserve its exact value name")));
			ASSERT_THAT(AreEqual(
				1,
				EnumValue,
				TEXT("Registered enum should preserve its exact underlying value")));
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetDefaultNamespace("foo"),
			TEXT("Enum registration product should select namespace for metadata lookup")));
		const int NamespacedTypeId = ScriptEngine->GetTypeIdByDecl("myenum");
		ASSERT_THAT(IsTrue(
			NamespacedTypeId >= 0,
			TEXT("Namespace-local enum should resolve inside its owning namespace")));
		asITypeInfo* const NamespacedType =
			ScriptEngine->GetTypeInfoById(NamespacedTypeId);
		ASSERT_THAT(IsNotNull(
			NamespacedType,
			TEXT("Namespace-local enum should publish TypeInfo")));
		if (NamespacedType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("foo")),
				FString(UTF8_TO_TCHAR(NamespacedType->GetNamespace())),
				TEXT("Namespace-local enum should preserve its namespace metadata")));
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetDefaultNamespace(""),
			TEXT("Enum registration product should restore global namespace")));
		const int GlobalTypeId = ScriptEngine->GetTypeIdByDecl("myenum");
		ASSERT_THAT(IsTrue(
			GlobalTypeId >= 0,
			TEXT("Global enum should resolve by declaration in the restored root namespace")));
		asITypeInfo* const GlobalType =
			ScriptEngine->GetTypeInfoById(GlobalTypeId);
		ASSERT_THAT(IsNotNull(
			GlobalType,
			TEXT("Global enum should publish TypeInfo in the restored root namespace")));
		if (GlobalType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(),
				FString(UTF8_TO_TCHAR(GlobalType->GetNamespace())),
				TEXT("Global enum should preserve the empty root namespace")));
		}
		ASSERT_THAT(AreNotEqual(
			GlobalType,
			NamespacedType,
			TEXT("Global and namespace-local enums should publish distinct TypeInfo identities")));

		asITypeInfo* const LocalType =
			Module->GetTypeInfoByName("LocalEnum");
		ASSERT_THAT(IsNotNull(
			LocalType,
			TEXT("Script-local enum should publish module-owned TypeInfo")));
		if (LocalType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				Module.Get(),
				LocalType->GetModule(),
				TEXT("Script-local enum should retain exact module ownership")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(1),
				static_cast<int32>(LocalType->GetEnumValueCount()),
				TEXT("Script-local enum should publish one value")));
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module.Discard(),
			TEXT("Enum registration product should explicitly discard its script-local module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"EnumTypeEnum",
				asGM_ONLY_IF_EXISTS),
			TEXT("Script-local enum module should be absent after cleanup")));

		int32 GlobalCleanupData = 11;
		int32 NamespacedCleanupData = 22;
		int32 RegisteredCleanupData = 33;
		ScriptEngine->SetTypeInfoUserDataCleanupCallback(
			ObserveEnumCleanup);
		if (GlobalType != nullptr)
		{
			ASSERT_THAT(IsNull(
				GlobalType->SetUserData(&GlobalCleanupData),
				TEXT("Global enum cleanup slot should begin empty")));
		}
		if (NamespacedType != nullptr)
		{
			ASSERT_THAT(IsNull(
				NamespacedType->SetUserData(&NamespacedCleanupData),
				TEXT("Namespaced enum cleanup slot should begin empty")));
		}
		if (RegisteredType != nullptr)
		{
			ASSERT_THAT(IsNull(
				RegisteredType->SetUserData(&RegisteredCleanupData),
				TEXT("Second native enum cleanup slot should begin empty")));
		}
		ASSERT_THAT(AreEqual(
			0,
			CleanedEnumData.Num(),
			TEXT("Native enum cleanup callbacks should remain deferred until engine destruction")));

		Engine.Destroy();
		ASSERT_THAT(AreEqual(
			0,
			CleanedEnumData.Num(),
			TEXT("Current fork engine destruction should expose the missing registered-enum userdata cleanup")));
		ASSERT_THAT(IsFalse(
			CleanedEnumData.Contains(&GlobalCleanupData),
			TEXT("Current fork should not report a global-enum cleanup callback that never ran")));
		ASSERT_THAT(IsFalse(
			CleanedEnumData.Contains(&NamespacedCleanupData),
			TEXT("Current fork should not report a namespaced-enum cleanup callback that never ran")));
		ASSERT_THAT(IsFalse(
			CleanedEnumData.Contains(&RegisteredCleanupData),
			TEXT("Current fork should not report a second-enum cleanup callback that never ran")));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] Engine destruction clears registeredEnums without invoking TypeInfo userdata cleanup callbacks; the three enum sentinels remain unobserved"));
	}

	TEST_METHOD(EnumUnderlyingValues)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-ENUM-REGISTRATION-RUNTIME plus LANG-CONV-ENUM-ALIAS and LANG-OP-COMPARISON-ENUM-ALIAS supersede this single flag-composition smoke with native/script origin, metadata, conversion, comparison, runtime, and ownership evidence");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Enum underlying-values test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "EnumUnderlyingValues", ASTEST_AS_ANSI(R"AS(
			enum EFlags
			{
				None = 0, A = 1, B = 2, C = 4, D = 8
			}

			bool CheckEnumUnderlyingValues()
			{
				int composed = int(EFlags::A) | int(EFlags::B);
				return int(EFlags::None) == 0 && int(EFlags::C) == 4 && composed == 3
				&& EFlags(composed) == EFlags(3) && int(EFlags::D) > int(EFlags::C);
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckEnumUnderlyingValues()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Enum underlying-values test should resolve its exact script entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Enum underlying-values test should preserve conversion and flag composition")));
		}
	}
};

#endif
