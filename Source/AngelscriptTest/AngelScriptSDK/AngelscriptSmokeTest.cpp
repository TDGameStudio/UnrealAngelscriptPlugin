#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKSmokeTests, "Angelscript.TestModule.AngelScriptSDK", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FSDKBufferedOutStream BufferedOutStream;
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner, &BufferedOutStream);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
	}

	TEST_METHOD(Smoke)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
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

		TestRunner->TestFalse(TEXT("SDK smoke test should not latch an engine failure for Assert(true)"), Engine.HasFailed());
	}
};

#endif
