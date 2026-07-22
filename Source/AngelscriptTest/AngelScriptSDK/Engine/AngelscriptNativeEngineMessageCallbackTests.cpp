#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FEngineMessageCallbackTests, "Angelscript.TestModule.AngelScriptSDK.Engine.MessageCallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EngineMessageCallbackCapturesInfoAndStatementExecutionSucceeds)
	{
		using namespace AngelscriptSDKTestSupport;

		AngelscriptNativeTestSupport::FSDKBufferedOutStream BufferedOutStream;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner, &BufferedOutStream);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK smoke test should create a standalone script engine")));

		ScriptEngine->WriteMessage("SDKSmoke", 0, 0, asMSGTYPE_INFORMATION, "Smoke callback ready");
		ASSERT_THAT(IsTrue(BufferedOutStream.Buffer.find("Smoke callback ready") != std::string::npos,
			TEXT("SDK smoke test should capture engine callback messages")));

		const int ExecuteResult = SDKExecuteString(
			ScriptEngine,
			"assert(true);");

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("SDK smoke test should compile and execute a simple snippet")));

		ASSERT_THAT(IsFalse(Engine.HasFailed(), TEXT("SDK smoke test should not latch an engine failure for Assert(true)")));
	}
};

#endif
