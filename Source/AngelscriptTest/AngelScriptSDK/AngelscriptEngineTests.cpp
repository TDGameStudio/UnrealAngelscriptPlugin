#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKEngineTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Create)
	{
		FSDKBufferedOutStream BufferedOutStream;
		asIScriptEngine* PrimaryEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		if (!TestRunner->TestNotNull(TEXT("SDK engine-create test should create the primary engine"), PrimaryEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			if (PrimaryEngine != nullptr)
			{
				PrimaryEngine->ShutDownAndRelease();
			}
		};

		const int PrimaryCallbackResult = PrimaryEngine->SetMessageCallback(
			asMETHODPR(FSDKBufferedOutStream, Callback, (asSMessageInfo*), void),
			&BufferedOutStream,
			asCALL_THISCALL);
		if (!TestRunner->TestEqual(TEXT("SDK engine-create test should install the primary engine callback"), PrimaryCallbackResult, static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		asIScriptEngine* SecondaryEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		if (!TestRunner->TestNotNull(TEXT("SDK engine-create test should create the secondary engine"), SecondaryEngine))
		{
			return;
		}

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
		if (!TestRunner->TestEqual(TEXT("SDK engine-create test should read back the primary callback"), GetCallbackResult, static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		const int ReuseCallbackResult = SecondaryEngine->SetMessageCallback(MessageCallback, CallbackObject, CallConv);
		if (!TestRunner->TestEqual(TEXT("SDK engine-create test should reuse the primary callback on the secondary engine"), ReuseCallbackResult, static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		const int WriteMessageResult = SecondaryEngine->WriteMessage("test", 0, 0, asMSGTYPE_INFORMATION, "Hello from engine2");
		if (!TestRunner->TestEqual(TEXT("SDK engine-create test should emit a callback message from the secondary engine"), WriteMessageResult, static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK engine-create test should preserve the upstream callback payload"), BufferedOutStream.Buffer.find("Hello from engine2") != std::string::npos);
		TestRunner->TestTrue(TEXT("SDK engine-create test should preserve the upstream callback section"), BufferedOutStream.Buffer.find("test (0, 0)") != std::string::npos);
	}

	TEST_METHOD(PropertyGetSet)
	{
		asIScriptEngine* Engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		if (!TestRunner->TestNotNull(TEXT("SDK engine property test should create an engine"), Engine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		};

		// Set a property and read it back.
		const int SetResult = Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
		if (!TestRunner->TestEqual(TEXT("SDK engine property test should set asEP_ALLOW_UNSAFE_REFERENCES"), SetResult, static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		const asPWORD Value = Engine->GetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES);
		TestRunner->TestEqual(TEXT("SDK engine property test should verify the round-trip value"), static_cast<int32>(Value), 1);
	}

	TEST_METHOD(ModuleEnumeration)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* Engine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK engine module-enumeration test should create an engine"), Engine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(Engine);
		};

		// Before building any modules.
		const asUINT InitialCount = Engine->GetModuleCount();
		if (!TestRunner->TestEqual(TEXT("SDK engine module-enumeration test should start with zero modules"), static_cast<int32>(InitialCount), 0))
		{
			return;
		}

		// Build two modules.
		asIScriptModule* M1 = BuildNativeModule(Engine, "ModEnumA", "const int a = 1;");
		asIScriptModule* M2 = BuildNativeModule(Engine, "ModEnumB", "const int b = 2;");
		if (!TestRunner->TestNotNull(TEXT("SDK engine module-enumeration test should compile ModEnumA"), M1) ||
			!TestRunner->TestNotNull(TEXT("SDK engine module-enumeration test should compile ModEnumB"), M2))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		const asUINT PostBuildCount = Engine->GetModuleCount();
		if (!TestRunner->TestEqual(TEXT("SDK engine module-enumeration test should have 2 modules after build"), static_cast<int32>(PostBuildCount), 2))
		{
			return;
		}

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

		TestRunner->TestTrue(TEXT("SDK engine module-enumeration test should find ModEnumA via GetModuleByIndex"), bFoundA);
		TestRunner->TestTrue(TEXT("SDK engine module-enumeration test should find ModEnumB via GetModuleByIndex"), bFoundB);
	}

	TEST_METHOD(GarbageCollectCycle)
	{
		asIScriptEngine* Engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		if (!TestRunner->TestNotNull(TEXT("SDK engine GC test should create an engine"), Engine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		};

		// GC calls should succeed even with no objects in the engine.
		const int FullResult = Engine->GarbageCollect();
		if (!TestRunner->TestTrue(TEXT("SDK engine GC test should run a full GC cycle without error"), FullResult >= 0))
		{
			return;
		}

		// Incremental step (asGC_ONE_STEP) should also succeed.
		const int StepResult = Engine->GarbageCollect(asGC_ONE_STEP);
		TestRunner->TestTrue(TEXT("SDK engine GC test should run an incremental GC step without error"), StepResult >= 0);
	}
};

#endif
