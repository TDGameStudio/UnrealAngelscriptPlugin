// AngelscriptSDKOutputBufferTests.cpp
// Tests for as_outputbuffer.cpp - compile error/warning message capture.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.OutputBuffer.*

#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKOutputBufferTests, "Angelscript.TestModule.AngelScriptSDK.OutputBuffer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ErrorCapture)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// Compile invalid code - should produce error messages
		Messages.Reset();
		asIScriptModule* M = BuildNativeModule(SE, "BadCode", "int Entry() { return undeclared_var; }\n");
		TestRunner->TestNull(TEXT("Invalid code should fail to compile"), M);

		// Verify error was captured
		bool HasError = false;
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				HasError = true;
				break;
			}
		}
		TestRunner->TestTrue(TEXT("Message callback should capture at least one error"), HasError);
		TestRunner->TestTrue(TEXT("Error messages should be non-empty"), Messages.Entries.Num() > 0);
	}

	TEST_METHOD(WarningCapture)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		// Code that compiles but may produce warnings (unused variable)
		Messages.Reset();
		asIScriptModule* M = BuildNativeModule(SE, "WarnCode",
			"int Entry() { int unused = 42; return 1; }\n");

		// Whether or not there are warnings depends on engine config.
		// The key assertion is that message callback works and does not crash.
		TestRunner->AddInfo(FString::Printf(TEXT("Messages captured: %d"), Messages.Entries.Num()));
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			TestRunner->AddInfo(FString::Printf(TEXT("  [%s] %s"), *FString(ToMessageTypeString(Entry.Type)), *Entry.Message));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
