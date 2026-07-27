#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeEngineGcCleanupServiceTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.GcCleanupService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr asPWORD EngineDefaultSlot = 0;
	static constexpr asPWORD EngineCustomSlot = 0x45474301;
	static constexpr asPWORD EngineNullSlot = 0x45474302;
	static constexpr asPWORD TypeInfoDefaultSlot = 0;
	static constexpr asPWORD TypeInfoCustomSlot = 0x54474301;
	static constexpr asPWORD TypeInfoNullSlot = 0x54474302;

	struct FForwardGcValue
	{
		int32 EnumCount;
		int32 ReleaseCount;
	};

	struct FEngineCleanupObservation
	{
		int32 PrimaryCount = 0;
		int32 ReplacementCount = 0;
		int32 CustomCount = 0;
		int32 NullCount = 0;
		UPTRINT ReplacementOwnerIdentity = 0;
		UPTRINT CustomOwnerIdentity = 0;
		void* ReplacementData = nullptr;
		void* CustomData = nullptr;

		void Reset()
		{
			*this = {};
		}
	};

	struct FTypeInfoCleanupObservation
	{
		int32 PrimaryCount = 0;
		int32 ReplacementTypeACount = 0;
		int32 ReplacementUnexpectedCount = 0;
		int32 CustomTypeACount = 0;
		int32 CustomTypeBCount = 0;
		int32 CustomUnexpectedCount = 0;
		int32 NullCount = 0;
		void* ReplacementDefaultData = nullptr;

		void Reset()
		{
			*this = {};
		}
	};

	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static bool bForwardTypesRegistered = false;
	inline static FEngineCleanupObservation EngineCleanupObservation;
	inline static FTypeInfoCleanupObservation TypeInfoCleanupObservation;
	inline static asITypeInfo* ExpectedTypeA = nullptr;
	inline static asITypeInfo* ExpectedTypeB = nullptr;

	static FString BuildNativeReviewSource(
		const TCHAR* Service,
		const TCHAR* Operation,
		const TCHAR* Contract)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Service: %s"), Service));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Operation: %s"), Operation));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Contract: %s"), Contract));
		return Source;
	}

	static void PrintNativeReviewSource(
		FAutomationTestBase& Test,
		const TCHAR* CaseId,
		const TCHAR* Service,
		const TCHAR* Operation,
		const TCHAR* Contract)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			CaseId,
			TEXT("NativeEngineGcCleanupServiceReview"),
			BuildNativeReviewSource(Service, Operation, Contract));
	}

	static void ForwardGcEnumReferences(FForwardGcValue* Self, int&)
	{
		if (Self != nullptr)
		{
			++Self->EnumCount;
		}
	}

	static void ForwardGcReleaseReferences(FForwardGcValue* Self, int&)
	{
		if (Self != nullptr)
		{
			++Self->ReleaseCount;
		}
	}

	static bool RegisterForwardTypes(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine)
	{
		FNoDiscardAsserter Assert(Test);
		bool bSuccess = true;

		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"NativeForwardGcValue",
				sizeof(FForwardGcValue),
				asOBJ_VALUE
					| asOBJ_GC
					| asOBJ_POD
					| asGetTypeTraits<FForwardGcValue>()) >= 0,
			TEXT("GC forwarding service should register its value-GC type"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				"NativeForwardGcValue",
				asBEHAVE_ENUMREFS,
				"void f(int&in gcEngine)",
				asFUNCTION(ForwardGcEnumReferences),
				asCALL_CDECL_OBJFIRST) >= 0,
			TEXT("GC forwarding service should register its enum-references behaviour"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				"NativeForwardGcValue",
				asBEHAVE_RELEASEREFS,
				"void f(int&in gcEngine)",
				asFUNCTION(ForwardGcReleaseReferences),
				asCALL_CDECL_OBJFIRST) >= 0,
			TEXT("GC forwarding service should register its release-references behaviour"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"NativeForwardPlainValue",
				sizeof(FForwardGcValue),
				asOBJ_VALUE
					| asOBJ_POD
					| asGetTypeTraits<FForwardGcValue>()) >= 0,
			TEXT("GC forwarding service should register its non-GC control type"));

		return bSuccess;
	}

	static void EnginePrimaryCleanup(asIScriptEngine*)
	{
		++EngineCleanupObservation.PrimaryCount;
	}

	static void EngineReplacementCleanup(asIScriptEngine* ScriptEngine)
	{
		++EngineCleanupObservation.ReplacementCount;
		EngineCleanupObservation.ReplacementOwnerIdentity =
			reinterpret_cast<UPTRINT>(ScriptEngine);
		EngineCleanupObservation.ReplacementData =
			ScriptEngine != nullptr
				? ScriptEngine->GetUserData(EngineDefaultSlot)
				: nullptr;
	}

	static void EngineCustomCleanup(asIScriptEngine* ScriptEngine)
	{
		++EngineCleanupObservation.CustomCount;
		EngineCleanupObservation.CustomOwnerIdentity =
			reinterpret_cast<UPTRINT>(ScriptEngine);
		EngineCleanupObservation.CustomData =
			ScriptEngine != nullptr
				? ScriptEngine->GetUserData(EngineCustomSlot)
				: nullptr;
	}

	static void EngineNullCleanup(asIScriptEngine*)
	{
		++EngineCleanupObservation.NullCount;
	}

	static void TypeInfoPrimaryCleanup(asITypeInfo*)
	{
		++TypeInfoCleanupObservation.PrimaryCount;
	}

	static void TypeInfoReplacementCleanup(asITypeInfo* Type)
	{
		if (Type == ExpectedTypeA)
		{
			++TypeInfoCleanupObservation.ReplacementTypeACount;
			TypeInfoCleanupObservation.ReplacementDefaultData =
				Type != nullptr ? Type->GetUserData() : nullptr;
		}
		else
		{
			++TypeInfoCleanupObservation.ReplacementUnexpectedCount;
		}
	}

	static void TypeInfoCustomCleanup(asITypeInfo* Type)
	{
		if (Type == ExpectedTypeA)
		{
			++TypeInfoCleanupObservation.CustomTypeACount;
		}
		else if (Type == ExpectedTypeB)
		{
			++TypeInfoCleanupObservation.CustomTypeBCount;
		}
		else
		{
			++TypeInfoCleanupObservation.CustomUnexpectedCount;
		}
	}

	static void TypeInfoNullCleanup(asITypeInfo*)
	{
		++TypeInfoCleanupObservation.NullCount;
	}

	static void TranslateApplicationException(asIScriptContext*, void*)
	{
	}

public:
	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine != nullptr)
		{
			bForwardTypesRegistered =
				RegisterForwardTypes(*TestRunner, *ScriptEngine);
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		bForwardTypesRegistered = false;
		ExpectedTypeA = nullptr;
		ExpectedTypeB = nullptr;
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
		EngineCleanupObservation.Reset();
		TypeInfoCleanupObservation.Reset();
		ExpectedTypeA = nullptr;
		ExpectedTypeB = nullptr;
	}

	TEST_METHOD(ForwardsOnlyValueGcReferenceBehaviours)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-GC-FORWARD-VALUE-BEHAVIOURS",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("GC forwarding service should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bForwardTypesRegistered)
		{
			return;
		}

		asITypeInfo* const GcType =
			ScriptEngine->GetTypeInfoByName("NativeForwardGcValue");
		asITypeInfo* const PlainType =
			ScriptEngine->GetTypeInfoByName("NativeForwardPlainValue");
		ASSERT_THAT(IsNotNull(
			GcType,
			TEXT("GC forwarding service should resolve its value-GC type")));
		ASSERT_THAT(IsNotNull(
			PlainType,
			TEXT("GC forwarding service should resolve its non-GC control type")));
		if (GcType == nullptr || PlainType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			(GcType->GetFlags() & (asOBJ_VALUE | asOBJ_GC))
				== (asOBJ_VALUE | asOBJ_GC),
			TEXT("GC forwarding service should retain the required value-GC flags")));
		ASSERT_THAT(IsTrue(
			(PlainType->GetFlags() & asOBJ_VALUE) != 0
				&& (PlainType->GetFlags() & asOBJ_GC) == 0,
			TEXT("GC forwarding service control type should be a non-GC value")));

		FForwardGcValue Probe{};

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-GC-FORWARD-VALUE-BEHAVIOURS-VALUE-GC-ENUM"),
			TEXT("GC forwarding"),
			TEXT("ForwardGCEnumReferences"),
			TEXT("value plus GC flags dispatch exactly one enum-references behaviour"));
		ScriptEngine->ForwardGCEnumReferences(&Probe, GcType);
		ASSERT_THAT(AreEqual(
			1,
			Probe.EnumCount,
			TEXT("ForwardGCEnumReferences should dispatch exactly once for a value-GC type")));
		ASSERT_THAT(AreEqual(
			0,
			Probe.ReleaseCount,
			TEXT("ForwardGCEnumReferences should not dispatch the release behaviour")));

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-GC-FORWARD-VALUE-BEHAVIOURS-VALUE-GC-RELEASE"),
			TEXT("GC forwarding"),
			TEXT("ForwardGCReleaseReferences"),
			TEXT("value plus GC flags dispatch exactly one release-references behaviour"));
		ScriptEngine->ForwardGCReleaseReferences(&Probe, GcType);
		ASSERT_THAT(AreEqual(
			1,
			Probe.EnumCount,
			TEXT("ForwardGCReleaseReferences should not redispatch the enum behaviour")));
		ASSERT_THAT(AreEqual(
			1,
			Probe.ReleaseCount,
			TEXT("ForwardGCReleaseReferences should dispatch exactly once for a value-GC type")));

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-GC-FORWARD-VALUE-BEHAVIOURS-PLAIN-VALUE-ENUM-NOOP"),
			TEXT("GC forwarding"),
			TEXT("ForwardGCEnumReferences"),
			TEXT("a value without the GC flag is an observable no-op"));
		ScriptEngine->ForwardGCEnumReferences(&Probe, PlainType);
		ASSERT_THAT(AreEqual(
			1,
			Probe.EnumCount,
			TEXT("ForwardGCEnumReferences should ignore a non-GC value type")));
		ASSERT_THAT(AreEqual(
			1,
			Probe.ReleaseCount,
			TEXT("A non-GC enum-forward no-op should preserve the release count")));

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-GC-FORWARD-VALUE-BEHAVIOURS-PLAIN-VALUE-RELEASE-NOOP"),
			TEXT("GC forwarding"),
			TEXT("ForwardGCReleaseReferences"),
			TEXT("a value without the GC flag is an observable no-op"));
		ScriptEngine->ForwardGCReleaseReferences(&Probe, PlainType);
		ASSERT_THAT(AreEqual(
			1,
			Probe.EnumCount,
			TEXT("A non-GC release-forward no-op should preserve the enum count")));
		ASSERT_THAT(AreEqual(
			1,
			Probe.ReleaseCount,
			TEXT("ForwardGCReleaseReferences should ignore a non-GC value type")));
	}

	TEST_METHOD(CleansEngineUserDataBySlotAtDestruction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-CLEANUP-ENGINE-USERDATA",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine CleanupEngine;
		CleanupEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { CleanupEngine.Destroy(); };
		asIScriptEngine* const ScriptEngine = CleanupEngine.Get();
		const UPTRINT ExpectedEngineIdentity =
			reinterpret_cast<UPTRINT>(ScriptEngine);
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Engine user-data cleanup service should create an isolated raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 DefaultDataA = 11;
		int32 DefaultDataB = 12;
		int32 CustomDataA = 21;
		int32 CustomDataB = 22;

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-CLEANUP-ENGINE-USERDATA-DEFAULT-SLOT-REPLACE"),
			TEXT("Engine user-data cleanup"),
			TEXT("SetEngineUserDataCleanupCallback"),
			TEXT("a replacement callback owns the final non-null default-slot value"));
		ScriptEngine->SetEngineUserDataCleanupCallback(
			EnginePrimaryCleanup,
			EngineDefaultSlot);
		ASSERT_THAT(IsNull(
			ScriptEngine->SetUserData(&DefaultDataA, EngineDefaultSlot),
			TEXT("Engine default slot should begin empty")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&DefaultDataA),
			ScriptEngine->SetUserData(&DefaultDataB, EngineDefaultSlot),
			TEXT("Engine default-slot replacement should return the prior data")));
		ScriptEngine->SetEngineUserDataCleanupCallback(
			EngineReplacementCleanup,
			EngineDefaultSlot);

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-CLEANUP-ENGINE-USERDATA-CUSTOM-SLOT-ISOLATION"),
			TEXT("Engine user-data cleanup"),
			TEXT("SetEngineUserDataCleanupCallback"),
			TEXT("a distinct typed slot dispatches only its own callback and final value"));
		ScriptEngine->SetEngineUserDataCleanupCallback(
			EngineCustomCleanup,
			EngineCustomSlot);
		ASSERT_THAT(IsNull(
			ScriptEngine->SetUserData(&CustomDataA, EngineCustomSlot),
			TEXT("Engine custom slot should begin empty")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&CustomDataA),
			ScriptEngine->SetUserData(&CustomDataB, EngineCustomSlot),
			TEXT("Engine custom-slot replacement should return the prior data")));

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-CLEANUP-ENGINE-USERDATA-NULL-SLOT-SUPPRESSION"),
			TEXT("Engine user-data cleanup"),
			TEXT("SetEngineUserDataCleanupCallback"),
			TEXT("a registered callback with no slot data is not dispatched"));
		ScriptEngine->SetEngineUserDataCleanupCallback(
			EngineNullCleanup,
			EngineNullSlot);

		ASSERT_THAT(AreEqual(
			0,
			EngineCleanupObservation.PrimaryCount
				+ EngineCleanupObservation.ReplacementCount
				+ EngineCleanupObservation.CustomCount
				+ EngineCleanupObservation.NullCount,
			TEXT("Engine cleanup callbacks should remain deferred until destruction")));

		CleanupEngine.Destroy();

		ASSERT_THAT(AreEqual(
			0,
			EngineCleanupObservation.PrimaryCount,
			TEXT("Replacing an engine cleanup callback should retire the prior callback without dispatch")));
		ASSERT_THAT(AreEqual(
			1,
			EngineCleanupObservation.ReplacementCount,
			TEXT("The replacement engine cleanup callback should run exactly once")));
		ASSERT_THAT(AreEqual(
			1,
			EngineCleanupObservation.CustomCount,
			TEXT("The custom-slot engine cleanup callback should run exactly once")));
		ASSERT_THAT(AreEqual(
			0,
			EngineCleanupObservation.NullCount,
			TEXT("An engine cleanup callback should not run for a slot without data")));
		ASSERT_THAT(AreEqual(
			ExpectedEngineIdentity,
			EngineCleanupObservation.ReplacementOwnerIdentity,
			TEXT("The replacement callback should receive the exact destroyed engine")));
		ASSERT_THAT(AreEqual(
			ExpectedEngineIdentity,
			EngineCleanupObservation.CustomOwnerIdentity,
			TEXT("The custom callback should receive the exact destroyed engine")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&DefaultDataB),
			EngineCleanupObservation.ReplacementData,
			TEXT("The replacement callback should observe the final default-slot data")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&CustomDataB),
			EngineCleanupObservation.CustomData,
			TEXT("The custom callback should observe the final typed-slot data")));
	}

	TEST_METHOD(CleansTypeInfoUserDataBySlotAtDestruction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-CLEANUP-TYPEINFO-USERDATA",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine CleanupEngine;
		CleanupEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { CleanupEngine.Destroy(); };
		asIScriptEngine* const ScriptEngine = CleanupEngine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("TypeInfo user-data cleanup service should create an isolated raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class NativeCleanupTypeA
			{
			}

			class NativeCleanupTypeB
			{
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-CLEANUP-TYPEINFO-USERDATA-DEFAULT-SLOT-REPLACE"),
			TEXT("EngineTypeInfoCleanupTypes"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));

		asIScriptModule* CleanupModule = nullptr;
		const int BuildResult = CompileNativeModule(
			ScriptEngine,
			"EngineTypeInfoCleanupTypes",
			ScriptSource.c_str(),
			CleanupModule);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			BuildResult,
			TEXT("TypeInfo cleanup service should compile its script-owned cleanup types")));
		ASSERT_THAT(IsNotNull(
			CleanupModule,
			TEXT("TypeInfo cleanup service should publish its script-owned cleanup module")));
		if (CleanupModule == nullptr)
		{
			return;
		}

		ExpectedTypeA = CleanupModule->GetTypeInfoByName("NativeCleanupTypeA");
		ExpectedTypeB = CleanupModule->GetTypeInfoByName("NativeCleanupTypeB");
		ASSERT_THAT(IsNotNull(
			ExpectedTypeA,
			TEXT("TypeInfo cleanup service should resolve type A")));
		ASSERT_THAT(IsNotNull(
			ExpectedTypeB,
			TEXT("TypeInfo cleanup service should resolve type B")));
		if (ExpectedTypeA == nullptr || ExpectedTypeB == nullptr)
		{
			return;
		}

		int32 TypeADefaultA = 31;
		int32 TypeADefaultB = 32;
		int32 TypeACustomA = 41;
		int32 TypeACustomB = 42;
		int32 TypeBCustom = 51;

		ScriptEngine->SetTypeInfoUserDataCleanupCallback(
			TypeInfoPrimaryCleanup,
			TypeInfoDefaultSlot);
		ASSERT_THAT(IsNull(
			ExpectedTypeA->SetUserData(&TypeADefaultA, TypeInfoDefaultSlot),
			TEXT("TypeInfo A default slot should begin empty")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&TypeADefaultA),
			ExpectedTypeA->SetUserData(&TypeADefaultB, TypeInfoDefaultSlot),
			TEXT("TypeInfo A default-slot replacement should return the prior data")));
		ScriptEngine->SetTypeInfoUserDataCleanupCallback(
			TypeInfoReplacementCleanup,
			TypeInfoDefaultSlot);

		const std::string ReplacementSource = ASTEST_AS_ANSI(R"AS(
			int CleanupReplacement()
			{
				return 1;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-CLEANUP-TYPEINFO-USERDATA-CUSTOM-SLOT-TYPE-ISOLATION"),
			TEXT("EngineTypeInfoCleanupTypes"),
			UTF8_TO_TCHAR(ReplacementSource.c_str()));
		ScriptEngine->SetTypeInfoUserDataCleanupCallback(
			TypeInfoCustomCleanup,
			TypeInfoCustomSlot);
		ASSERT_THAT(IsNull(
			ExpectedTypeA->SetUserData(&TypeACustomA, TypeInfoCustomSlot),
			TEXT("TypeInfo A custom slot should begin empty")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&TypeACustomA),
			ExpectedTypeA->SetUserData(&TypeACustomB, TypeInfoCustomSlot),
			TEXT("TypeInfo A custom-slot replacement should return the prior data")));
		ASSERT_THAT(IsNull(
			ExpectedTypeB->SetUserData(&TypeBCustom, TypeInfoCustomSlot),
			TEXT("TypeInfo B custom slot should be isolated from type A")));

		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-CLEANUP-TYPEINFO-USERDATA-NULL-SLOT-SUPPRESSION"),
			TEXT("TypeInfo user-data cleanup"),
			TEXT("SetTypeInfoUserDataCleanupCallback"),
			TEXT("a registered callback with no matching type data is not dispatched"));
		ScriptEngine->SetTypeInfoUserDataCleanupCallback(
			TypeInfoNullCleanup,
			TypeInfoNullSlot);

		ASSERT_THAT(AreEqual(
			0,
			TypeInfoCleanupObservation.PrimaryCount
				+ TypeInfoCleanupObservation.ReplacementTypeACount
				+ TypeInfoCleanupObservation.ReplacementUnexpectedCount
				+ TypeInfoCleanupObservation.CustomTypeACount
				+ TypeInfoCleanupObservation.CustomTypeBCount
				+ TypeInfoCleanupObservation.CustomUnexpectedCount
				+ TypeInfoCleanupObservation.NullCount,
			TEXT("TypeInfo cleanup callbacks should remain deferred until their owning types are destroyed")));

		const int AddReplacementSectionResult =
			CleanupModule->AddScriptSection(
				"EngineTypeInfoCleanupTypesReplacement.as",
				ReplacementSource.c_str());
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			AddReplacementSectionResult,
			TEXT("TypeInfo cleanup service should add the printed replacement section")));
		const int ReplacementBuildResult = CleanupModule->Build();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ReplacementBuildResult,
			TEXT("Rebuilding the module should destroy the prior script-owned TypeInfo values")));

		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-TYPEINFO-CLEANUP-COUNTS] Primary=%d ReplacementTypeA=%d ReplacementUnexpected=%d CustomTypeA=%d CustomTypeB=%d CustomUnexpected=%d Null=%d DefaultData=%p"),
			TypeInfoCleanupObservation.PrimaryCount,
			TypeInfoCleanupObservation.ReplacementTypeACount,
			TypeInfoCleanupObservation.ReplacementUnexpectedCount,
			TypeInfoCleanupObservation.CustomTypeACount,
			TypeInfoCleanupObservation.CustomTypeBCount,
			TypeInfoCleanupObservation.CustomUnexpectedCount,
			TypeInfoCleanupObservation.NullCount,
			TypeInfoCleanupObservation.ReplacementDefaultData));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] Engine shutdown clears scriptModules before DeleteDiscardedModules scans them, so this test uses public same-module rebuild to exercise TypeInfo destruction; registered application types likewise do not dispatch TypeInfo cleanup during engine shutdown"));

		ASSERT_THAT(AreEqual(
			0,
			TypeInfoCleanupObservation.PrimaryCount,
			TEXT("Replacing a TypeInfo cleanup callback should retire the prior callback without dispatch")));
		ASSERT_THAT(AreEqual(
			1,
			TypeInfoCleanupObservation.ReplacementTypeACount,
			TEXT("The replacement TypeInfo callback should clean type A exactly once")));
		ASSERT_THAT(AreEqual(
			0,
			TypeInfoCleanupObservation.ReplacementUnexpectedCount,
			TEXT("The default-slot TypeInfo callback should not cross into other types")));
		ASSERT_THAT(AreEqual(
			1,
			TypeInfoCleanupObservation.CustomTypeACount,
			TEXT("The custom-slot callback should clean type A exactly once")));
		ASSERT_THAT(AreEqual(
			1,
			TypeInfoCleanupObservation.CustomTypeBCount,
			TEXT("The custom-slot callback should clean type B exactly once")));
		ASSERT_THAT(AreEqual(
			0,
			TypeInfoCleanupObservation.CustomUnexpectedCount,
			TEXT("The custom-slot callback should not receive unrelated engine types")));
		ASSERT_THAT(AreEqual(
			0,
			TypeInfoCleanupObservation.NullCount,
			TEXT("A TypeInfo cleanup callback should not run for an unused slot")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&TypeADefaultB),
			TypeInfoCleanupObservation.ReplacementDefaultData,
			TEXT("The replacement TypeInfo callback should observe type A's final default-slot data")));

		ExpectedTypeA = nullptr;
		ExpectedTypeB = nullptr;
	}

	TEST_METHOD(RejectsApplicationExceptionTranslationWithoutExceptionSupport)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-APP-EXCEPTION-TRANSLATE-REJECT",
			ENativeEvidence::Runtime
				| ENativeEvidence::Diagnostic);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Application exception translation service should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 CallbackToken = 61;
		PrintNativeReviewSource(
			*TestRunner,
			TEXT("ENG-APP-EXCEPTION-TRANSLATE-REJECT-AS-NO-EXCEPTIONS"),
			TEXT("Application exception translation"),
			TEXT("SetTranslateAppExceptionCallback"),
			TEXT("AS_NO_EXCEPTIONS rejects a well-formed CDECL callback without retaining it"));
		ASSERT_THAT(AreEqual(
			asNOT_SUPPORTED,
			ScriptEngine->SetTranslateAppExceptionCallback(
				asFUNCTION(TranslateApplicationException),
				&CallbackToken,
				asCALL_CDECL),
			TEXT("The current AS_NO_EXCEPTIONS fork should reject application exception translation")));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] SetTranslateAppExceptionCallback returns asNOT_SUPPORTED because this fork compiles the native SDK with AS_NO_EXCEPTIONS"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
