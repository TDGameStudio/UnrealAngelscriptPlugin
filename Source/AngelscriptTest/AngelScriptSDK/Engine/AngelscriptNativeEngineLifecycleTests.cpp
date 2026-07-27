#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Raw SDK engine lifecycle coverage.

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FEngineLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CreatesAndShutsDownRawEngine)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-LIFECYCLE-CROSS-ENGINE-MESSAGE-CALLBACK",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FSDKBufferedOutStream BufferedOutStream;
		asIScriptEngine* PrimaryEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(PrimaryEngine, TEXT("SDK engine-create test should create the primary engine")));

		ON_SCOPE_EXIT
		{
			if (PrimaryEngine != nullptr)
			{
				PrimaryEngine->ShutDownAndRelease();
			}
		};

		const int PrimaryCallbackResult = PrimaryEngine->SetMessageCallback(
			asMETHODPR(AngelscriptNativeTestSupport::FSDKBufferedOutStream, Callback, (asSMessageInfo*), void),
			&BufferedOutStream,
			asCALL_THISCALL);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrimaryCallbackResult,
			TEXT("SDK engine-create test should install the primary engine callback")));

		asIScriptEngine* SecondaryEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(SecondaryEngine, TEXT("SDK engine-create test should create the secondary engine")));

		ON_SCOPE_EXIT
		{
			if (SecondaryEngine != nullptr)
			{
				SecondaryEngine->ShutDownAndRelease();
			}
		};

		asSFuncPtr MessageCallback;
		void* CallbackObject = nullptr;
		asDWORD CallConv = 0;
		const int GetCallbackResult = PrimaryEngine->GetMessageCallback(&MessageCallback, &CallbackObject, &CallConv);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), GetCallbackResult,
			TEXT("SDK engine-create test should read back the primary callback")));

		const int ReuseCallbackResult = SecondaryEngine->SetMessageCallback(MessageCallback, CallbackObject, CallConv);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ReuseCallbackResult,
			TEXT("SDK engine-create test should reuse the primary callback on the secondary engine")));

		const int WriteMessageResult = SecondaryEngine->WriteMessage("test", 0, 0, asMSGTYPE_INFORMATION, "Hello from engine2");
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), WriteMessageResult,
			TEXT("SDK engine-create test should emit a callback message from the secondary engine")));

		ASSERT_THAT(IsTrue(BufferedOutStream.Buffer.find("Hello from engine2") != std::string::npos,
			TEXT("SDK engine-create test should preserve the upstream callback payload")));
		ASSERT_THAT(IsTrue(BufferedOutStream.Buffer.find("test (0, 0)") != std::string::npos,
			TEXT("SDK engine-create test should preserve the upstream callback section")));
	}

	TEST_METHOD(EngineLifecyclePropertyGetSet)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"ENG-PROPERTY-ISOLATION and ENG-PROPERTY-PROFILE supersede this single-property round trip with defaults, profile application, restoration, and cross-engine isolation");

		asIScriptEngine* Engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(Engine, TEXT("SDK engine property test should create an engine")));

		ON_SCOPE_EXIT
		{
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		};

		// Set a property and read it back.
		const int SetResult = Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SetResult,
			TEXT("SDK engine property test should set asEP_ALLOW_UNSAFE_REFERENCES")));

		const asPWORD Value = Engine->GetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES);
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Value),
			TEXT("SDK engine property test should verify the round-trip value")));
	}

	TEST_METHOD(ModuleEnumeration)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("ENG-LIFECYCLE-MODULE-ENUMERATION",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* Engine = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(Engine, TEXT("SDK engine module-enumeration test should create an engine")));

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(Engine);
		};

		// Before building any modules.
		const asUINT InitialCount = Engine->GetModuleCount();
		ASSERT_THAT(AreEqual(0, static_cast<int32>(InitialCount),
			TEXT("SDK engine module-enumeration test should start with zero modules")));

		const std::string ModuleASource = ASTEST_AS_ANSI(R"AS(
			const int A = 1;
			)AS");
		const std::string ModuleBSource = ASTEST_AS_ANSI(R"AS(
			const int B = 2;
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-LIFECYCLE-MODULE-ENUMERATION-FIRST"),
			TEXT("ModEnumA"),
			UTF8_TO_TCHAR(ModuleASource.c_str()));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-LIFECYCLE-MODULE-ENUMERATION-SECOND"),
			TEXT("ModEnumB"),
			UTF8_TO_TCHAR(ModuleBSource.c_str()));

		// Build two modules.
		asIScriptModule* M1 =
			BuildNativeModule(Engine, "ModEnumA", ModuleASource);
		asIScriptModule* M2 =
			BuildNativeModule(Engine, "ModEnumB", ModuleBSource);
		if (!this->Assert.IsNotNull(M1, TEXT("SDK engine module-enumeration test should compile ModEnumA")) ||
			!this->Assert.IsNotNull(M2, TEXT("SDK engine module-enumeration test should compile ModEnumB")))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		const asUINT PostBuildCount = Engine->GetModuleCount();
		ASSERT_THAT(AreEqual(2, static_cast<int32>(PostBuildCount),
			TEXT("SDK engine module-enumeration test should have 2 modules after build")));

		// Enumerate and verify both modules are present.
		bool bFoundA = false, bFoundB = false;
		for (asUINT i = 0; i < PostBuildCount; ++i)
		{
			asIScriptModule* M = Engine->GetModuleByIndex(i);
			if (M != nullptr && M->GetName() != nullptr)
			{
				const FString Name = UTF8_TO_TCHAR(M->GetName());
				if (Name == TEXT("ModEnumA")) bFoundA = true;
				if (Name == TEXT("ModEnumB")) bFoundB = true;
			}
		}

		ASSERT_THAT(IsTrue(bFoundA, TEXT("SDK engine module-enumeration test should find ModEnumA via GetModuleByIndex")));
		ASSERT_THAT(IsTrue(bFoundB, TEXT("SDK engine module-enumeration test should find ModEnumB via GetModuleByIndex")));
		ASSERT_THAT(IsNull(
			Engine->GetModuleByIndex(PostBuildCount),
			TEXT("Module enumeration should return null one index past the published inventory")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->DiscardModule("ModEnumA"),
			TEXT("Module enumeration should explicitly discard its first module")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->DiscardModule("ModEnumB"),
			TEXT("Module enumeration should explicitly discard its second module")));
		ASSERT_THAT(AreEqual(
			PostBuildCount,
			Engine->GetModuleCount(),
			TEXT("Current fork should retain discarded modules in the indexed inventory until engine shutdown")));
		ASSERT_THAT(IsNotNull(
			Engine->GetModuleByIndex(0),
			TEXT("Current fork should retain the first discarded module in indexed storage")));
		ASSERT_THAT(IsNotNull(
			Engine->GetModuleByIndex(1),
			TEXT("Current fork should retain the second discarded module in indexed storage")));
		ASSERT_THAT(IsNull(
			Engine->GetModule("ModEnumA", asGM_ONLY_IF_EXISTS),
			TEXT("First enumerated module should be absent after cleanup")));
		ASSERT_THAT(IsNull(
			Engine->GetModule("ModEnumB", asGM_ONLY_IF_EXISTS),
			TEXT("Second enumerated module should be absent after cleanup")));
		TestRunner->AddInfo(TEXT(
			"[AS-FORK-LIMITATION] DiscardModule removes name lookup publication but "
			"GetModuleCount/GetModuleByIndex retain discarded modules until shutdown; "
			"DeleteDiscardedModules is not reachable from the ordinary public lifecycle."));
	}

	TEST_METHOD(GarbageCollectCycle)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-LIFECYCLE-EMPTY-GC-MODES",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* Engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(Engine, TEXT("SDK engine GC test should create an engine")));

		ON_SCOPE_EXIT
		{
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		};

		asUINT CurrentSize = 1;
		asUINT TotalDestroyed = 1;
		asUINT TotalDetected = 1;
		Engine->GetGCStatistics(
			&CurrentSize,
			&TotalDestroyed,
			&TotalDetected);
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(CurrentSize),
			TEXT("Fresh raw engine should start with an empty GC inventory")));

		ASSERT_THAT(AreEqual(
			0,
			Engine->GarbageCollect(),
			TEXT("SDK engine GC test should run the default full cycle")));
		ASSERT_THAT(AreEqual(
			0,
			Engine->GarbageCollect(
				asGC_FULL_CYCLE | asGC_DETECT_GARBAGE,
				1),
			TEXT("SDK engine GC test should run a detecting full cycle")));

		// Incremental step (asGC_ONE_STEP) should also succeed.
		ASSERT_THAT(AreEqual(
			1,
			Engine->GarbageCollect(asGC_ONE_STEP),
			TEXT("SDK engine GC test should report that one incremental step does not finish a full cycle")));

		Engine->GetGCStatistics(
			&CurrentSize,
			&TotalDestroyed,
			&TotalDetected);
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(CurrentSize),
			TEXT("Empty GC modes should leave the current inventory empty")));
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(TotalDestroyed),
			TEXT("Empty GC modes should destroy no objects")));
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(TotalDetected),
			TEXT("Empty GC modes should detect no garbage")));
	}
};

#endif
