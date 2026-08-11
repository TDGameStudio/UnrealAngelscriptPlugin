#include "Core/AngelscriptEngine.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStartupCompileFailurePolicyTests,
	"Angelscript.TestModule.Cache.StartupCompileFailurePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UnattendedProcessRequestsExitEvenWhenInteractiveRetryIsAvailable)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsUnattended = true;

		ASSERT_THAT(AreEqual(
			EAngelscriptStartupCompileFailureResponse::RequestExit,
			ResolveAngelscriptStartupCompileFailureResponse(
				Config,
				true)));
	}

	TEST_METHOD(InteractiveEditorCanKeepRetryingWhenTheRetryWindowIsAvailable)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;

		ASSERT_THAT(AreEqual(
			EAngelscriptStartupCompileFailureResponse::InteractiveRetry,
			ResolveAngelscriptStartupCompileFailureResponse(
				Config,
				true)));
	}

	TEST_METHOD(CommandletExplicitExitAndMissingRetryWindowAllRequestExit)
	{
		FAngelscriptEngineConfig CommandletConfig;
		CommandletConfig.bRunningCommandlet = true;
		ASSERT_THAT(AreEqual(
			EAngelscriptStartupCompileFailureResponse::RequestExit,
			ResolveAngelscriptStartupCompileFailureResponse(
				CommandletConfig,
				true)));

		FAngelscriptEngineConfig ExitOnErrorConfig;
		ExitOnErrorConfig.bExitOnError = true;
		ASSERT_THAT(AreEqual(
			EAngelscriptStartupCompileFailureResponse::RequestExit,
			ResolveAngelscriptStartupCompileFailureResponse(
				ExitOnErrorConfig,
				true)));

		FAngelscriptEngineConfig InteractiveConfig;
		InteractiveConfig.bIsEditor = true;
		ASSERT_THAT(AreEqual(
			EAngelscriptStartupCompileFailureResponse::RequestExit,
			ResolveAngelscriptStartupCompileFailureResponse(
				InteractiveConfig,
				false)));
	}

	TEST_METHOD(UnattendedExitClosesCacheAndWritesDiagnosticsBeforeAStableForcedFailureStatus)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsUnattended = true;

		const FAngelscriptStartupCompileFailureExitRequest ExitRequest =
			ResolveAngelscriptStartupCompileFailureExitRequest(Config);
		ASSERT_THAT(IsTrue(ExitRequest.bBeginCacheShutdownBeforeDiagnosticReport));
		ASSERT_THAT(IsTrue(ExitRequest.bWriteRequestedDiagnosticReportBeforeExit));
		ASSERT_THAT(IsTrue(ExitRequest.bForce));
		ASSERT_THAT(AreEqual(uint8{3}, ExitRequest.Status));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
