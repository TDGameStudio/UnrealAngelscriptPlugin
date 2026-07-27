// Raw SDK output-buffer coverage.
// Tests for as_outputbuffer.cpp - compile error/warning message capture.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.Compiler.OutputBuffer.*

#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FOutputBufferTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.OutputBuffer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OutputBufferErrorCapture)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained public message-collector integration smoke; COMPILER-BUILDER-SHAPE-FAILURE owns exact compile rejection, diagnostics, publication exclusion, cleanup, and isolation.");

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Compile invalid code - should produce error messages
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BadCode");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return undeclared_var;
			}
			)AS");
		asIScriptModule* M = BuildNativeModule(SE, "BadCode", ScriptSource);
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

	TEST_METHOD(OutputBufferWarningCapture)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"Infrastructure",
			"Retained non-warning public-build control for message logging; COMPILER-DIAGNOSTIC-WARNING-POLICY owns deterministic warning generation, severity policy, diagnostics, publication, and cleanup.");

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// This source is a non-warning control. Warning generation is owned by
		// COMPILER-DIAGNOSTIC-WARNING-POLICY with deterministic overload input.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "WarnCode");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int unused = 42;
				return 1;
			}
			)AS");
		asIScriptModule* M = BuildNativeModule(SE, "WarnCode", ScriptSource);
		ASSERT_THAT(IsNotNull(M, TEXT("Non-warning output-buffer control should compile")));
		ASSERT_THAT(IsNotNull(
			M != nullptr ? M->GetFunctionByDecl("int Entry()") : nullptr,
			TEXT("Non-warning output-buffer control should publish its exact entry declaration")));

		bool bHasError = false;
		TestRunner->AddInfo(FString::Printf(TEXT("Messages captured: %d"), Engine.GetMessages().Entries.Num()));
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			TestRunner->AddInfo(FString::Printf(TEXT("  [%s] %s"), *FString(ToMessageTypeString(Entry.Type)), *Entry.Message));
			bHasError |= Entry.Type == asMSGTYPE_ERROR;
		}
		ASSERT_THAT(IsFalse(
			bHasError,
			TEXT("Non-warning output-buffer control should not publish an error diagnostic")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
