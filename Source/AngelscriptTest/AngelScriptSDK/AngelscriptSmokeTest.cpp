#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKSmokeTests, "Angelscript.TestModule.AngelScriptSDK", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FSDKBufferedOutStream BufferedOutStream;
	inline static FNativeSdkAdapterEngineFixture EngineFixture;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner, &BufferedOutStream);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
	}

	BEFORE_EACH()
	{
		EngineFixture.Reset(*TestRunner);
	}

	TEST_METHOD(Smoke)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK smoke test should create a standalone script engine"), ScriptEngine))
		{
			return;
		}

		ScriptEngine->WriteMessage("SDKSmoke", 0, 0, asMSGTYPE_INFORMATION, "Smoke callback ready");
		if (!TestRunner->TestTrue(TEXT("SDK smoke test should capture engine callback messages"), BufferedOutStream.Buffer.find("Smoke callback ready") != std::string::npos))
		{
			return;
		}

		const int ExecuteResult = SDKExecuteString(
			ScriptEngine,
			"bool ExecuteSmoke() { assert(true); return true; }");

		if (!TestRunner->TestEqual(TEXT("SDK smoke test should compile and execute a simple snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return;
		}

		const FAngelscriptSDKTestAdapter* const Adapter = EngineFixture.GetAdapter();
		if (!TestRunner->TestNotNull(TEXT("SDK smoke test should expose its adapter"), Adapter))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("SDK smoke test should not latch an adapter failure for Assert(true)"), Adapter->bFailed);
	}
};

#endif
