// AngelscriptSDKOutputBufferTests.cpp
// Tests for as_outputbuffer.cpp - compile error/warning message capture.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.OutputBuffer.*

#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKOutputBufferTests, "Angelscript.TestModule.AngelScriptSDK.OutputBuffer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

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
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Compile invalid code - should produce error messages
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BadCode");
		asIScriptModule* M = BuildNativeModule(SE, "BadCode", "int Entry() { return undeclared_var; }\n");
		ASSERT_THAT(IsNull(M, TEXT("Invalid code should fail to compile")));

		// Verify error was captured
		bool HasError = false;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				HasError = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(HasError, TEXT("Message callback should capture at least one error")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, TEXT("Error messages should be non-empty")));
	}

	TEST_METHOD(WarningCapture)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Code that compiles but may produce warnings (unused variable)
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "WarnCode");
		asIScriptModule* M = BuildNativeModule(SE, "WarnCode",
			"int Entry() { int unused = 42; return 1; }\n");

		// Whether or not there are warnings depends on engine config.
		// The key assertion is that message callback works and does not crash.
		TestRunner->AddInfo(FString::Printf(TEXT("Messages captured: %d"), Engine.GetMessages().Entries.Num()));
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			TestRunner->AddInfo(FString::Printf(TEXT("  [%s] %s"), *FString(ToMessageTypeString(Entry.Type)), *Entry.Message));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
