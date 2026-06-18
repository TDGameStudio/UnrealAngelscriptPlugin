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
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(ErrorCapture)
	{
		asIScriptEngine* const SE = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		// Compile invalid code - should produce error messages
		Engine.ResetMessages();
		FScopedNativeModuleName ModuleScope(Engine, "BadCode");
		asIScriptModule* M = BuildNativeModule(SE, "BadCode", "int Entry() { return undeclared_var; }\n");
		TestRunner->TestNull(TEXT("Invalid code should fail to compile"), M);

		// Verify error was captured
		bool HasError = false;
		for (const FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				HasError = true;
				break;
			}
		}
		TestRunner->TestTrue(TEXT("Message callback should capture at least one error"), HasError);
		TestRunner->TestTrue(TEXT("Error messages should be non-empty"), Engine.GetMessages().Entries.Num() > 0);
	}

	TEST_METHOD(WarningCapture)
	{
		asIScriptEngine* const SE = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		// Code that compiles but may produce warnings (unused variable)
		Engine.ResetMessages();
		FScopedNativeModuleName ModuleScope(Engine, "WarnCode");
		asIScriptModule* M = BuildNativeModule(SE, "WarnCode",
			"int Entry() { int unused = 42; return 1; }\n");

		// Whether or not there are warnings depends on engine config.
		// The key assertion is that message callback works and does not crash.
		TestRunner->AddInfo(FString::Printf(TEXT("Messages captured: %d"), Engine.GetMessages().Entries.Num()));
		for (const FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			TestRunner->AddInfo(FString::Printf(TEXT("  [%s] %s"), *FString(ToMessageTypeString(Entry.Type)), *Entry.Message));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
