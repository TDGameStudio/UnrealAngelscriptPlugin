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
};

#endif
