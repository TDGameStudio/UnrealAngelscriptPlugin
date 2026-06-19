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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK smoke test should create a standalone script engine")));

		ScriptEngine->WriteMessage("SDKSmoke", 0, 0, asMSGTYPE_INFORMATION, "Smoke callback ready");
		ASSERT_THAT(IsTrue(BufferedOutStream.Buffer.find("Smoke callback ready") != std::string::npos,
			TEXT("SDK smoke test should capture engine callback messages")));

		const int ExecuteResult = SDKExecuteString(
			ScriptEngine,
			"bool ExecuteSmoke() { assert(true); return true; }");

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("SDK smoke test should compile and execute a simple snippet")));

		ASSERT_THAT(IsFalse(Engine.HasFailed(), TEXT("SDK smoke test should not latch an engine failure for Assert(true)")));
	}
};

#endif
