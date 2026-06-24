#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKEngineTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Create)
	{
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

	TEST_METHOD(PropertyGetSet)
	{
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

		// Build two modules.
		asIScriptModule* M1 = BuildNativeModule(Engine, "ModEnumA", "const int a = 1;");
		asIScriptModule* M2 = BuildNativeModule(Engine, "ModEnumB", "const int b = 2;");
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
	}

	TEST_METHOD(GarbageCollectCycle)
	{
		asIScriptEngine* Engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(Engine, TEXT("SDK engine GC test should create an engine")));

		ON_SCOPE_EXIT
		{
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		};

		// GC calls should succeed even with no objects in the engine.
		const int FullResult = Engine->GarbageCollect();
		ASSERT_THAT(IsTrue(FullResult >= 0,
			TEXT("SDK engine GC test should run a full GC cycle without error")));

		// Incremental step (asGC_ONE_STEP) should also succeed.
		const int StepResult = Engine->GarbageCollect(asGC_ONE_STEP);
		ASSERT_THAT(IsTrue(StepResult >= 0,
			TEXT("SDK engine GC test should run an incremental GC step without error")));
	}
};

#endif
