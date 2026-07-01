#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerTestSession.h"
#include "Debugging/AngelscriptDebugServer.h"

/**
 * AngelscriptDebuggerTestHelpers — shared session/breakpoint pump helpers
 * used by every Debugger/*Tests.cpp file. These were previously duplicated
 * (with minor wording variations) as file-local helpers, causing unity-build
 * name collisions.
 *
 * All helpers preserve the original behaviour:
 *   - 45 second default session timeout (UE 5.7 headless default).
 *   - Session.Initialize creates an isolated FAngelscriptEngine + DebugServer.
 *   - Client connects to 127.0.0.1:Session.GetPort().
 *   - StartDebugging(2) is sent through Session.PumpUntil so it cooperates
 *     with the asynchronous session pump.
 *
 * Each helper accepts a `Context` label so the originating test theme can
 * stamp its identity into any failure messages.
 */

#if WITH_ANGELSCRIPT_UNITTESTS

/**
 * Initialize a debugger session, connect a primary client, and send
 * StartDebugging(2). Reports a failure with the supplied context label if
 * any step does not complete inside the session pump timeout.
 */
inline bool StartDebuggerSession(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	FAngelscriptDebuggerTestClient& Client,
	const TCHAR* Context = TEXT("Debugger session"))
{
	FAngelscriptDebuggerSessionConfig SessionConfig;
	// UE 5.7: headless has no production subsystem. Let Initialize() create
	// an isolated FAngelscriptEngine with its own FAngelscriptDebugServer.
	SessionConfig.DefaultTimeoutSeconds = 45.0f;

	if (!Test.TestTrue(
			*FString::Printf(TEXT("%s should initialize against the debuggable production engine"), Context),
			Session.Initialize(SessionConfig)))
	{
		return false;
	}

	if (!Test.TestTrue(
			*FString::Printf(TEXT("%s should connect the primary debugger client"), Context),
			Client.Connect(TEXT("127.0.0.1"), Session.GetPort())))
	{
		Test.AddError(Client.GetLastError());
		return false;
	}

	bool bSentStartDebugging = false;
	const bool bStartMessageSent = Session.PumpUntil(
		[&Client, &bSentStartDebugging]()
		{
			if (bSentStartDebugging)
			{
				return true;
			}

			bSentStartDebugging = Client.SendStartDebugging(2);
			return bSentStartDebugging;
		},
		Session.GetDefaultTimeoutSeconds());

	if (!Test.TestTrue(
			*FString::Printf(TEXT("%s should send StartDebugging before exercising the test"), Context),
			bStartMessageSent))
	{
		Test.AddError(Client.GetLastError());
		return false;
	}

	return true;
}

/**
 * Start a debugger session and additionally pump until the
 * DebugServerVersion envelope is observed by the client. Used by the
 * breakpoint family of tests that want to validate the protocol
 * handshake before firing additional traffic.
 */
inline bool StartDebuggerSessionWithVersionHandshake(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	FAngelscriptDebuggerTestClient& Client,
	const TCHAR* Context = TEXT("Debugger session"))
{
	if (!StartDebuggerSession(Test, Session, Client, Context))
	{
		return false;
	}

	TOptional<FAngelscriptDebugMessageEnvelope> VersionEnvelope;
	const bool bReceivedVersion = Session.PumpUntil(
		[&Client, &VersionEnvelope]()
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
			if (Envelope.IsSet() && Envelope->MessageType == EDebugMessageType::DebugServerVersion)
			{
				VersionEnvelope = MoveTemp(Envelope);
				return true;
			}
			return false;
		},
		Session.GetDefaultTimeoutSeconds());

	if (!Test.TestTrue(
			*FString::Printf(TEXT("%s should receive the DebugServerVersion response"), Context),
			bReceivedVersion))
	{
		Test.AddError(Client.GetLastError());
		return false;
	}

	return true;
}

/** Full overload with caller-supplied context label. */
inline bool WaitForBreakpointCount(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	int32 ExpectedCount,
	const TCHAR* Context)
{
	const bool bReachedCount = Session.PumpUntil(
		[&Session, ExpectedCount]()
		{
			return Session.GetDebugServer().BreakpointCount == ExpectedCount;
		},
		Session.GetDefaultTimeoutSeconds());

	if (!bReachedCount)
	{
		Test.AddError(FString::Printf(
			TEXT("%s (actual breakpoint count: %d)."),
			Context,
			Session.GetDebugServer().BreakpointCount));
	}

	return bReachedCount;
}

/**
 * Pump the session until DebugServer.BreakpointCount equals the expected
 * value (or the session timeout elapses). Reports the actual count on
 * failure for debugging.
 */
inline bool WaitForBreakpointCount(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	int32 ExpectedCount)
{
	const FString AutoContext = FString::Printf(
		TEXT("Should observe breakpoint count reaching %d"), ExpectedCount);
	return WaitForBreakpointCount(Test, Session, ExpectedCount, *AutoContext);
}

/**
 * Pump the session until the given module/line shows up in DebugServer's
 * active breakpoint table (or the session timeout elapses). The Context
 * label is forwarded to TestTrue.
 */
inline bool WaitForSpecificBreakpoint(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	const FString& ModuleKey,
	int32 ExpectedLine,
	const TCHAR* Context)
{
	const bool bFoundBreakpoint = Session.PumpUntil(
		[&Session, &ModuleKey, ExpectedLine]()
		{
			const TSharedPtr<FAngelscriptDebugServer::FFileBreakpoints>* ActiveBreakpoints =
				Session.GetDebugServer().Breakpoints.Find(ModuleKey);
			return ActiveBreakpoints != nullptr
				&& ActiveBreakpoints->IsValid()
				&& (*ActiveBreakpoints)->Lines.Contains(ExpectedLine);
		},
		Session.GetDefaultTimeoutSeconds());

	return Test.TestTrue(Context, bFoundBreakpoint);
}


#endif // WITH_ANGELSCRIPT_UNITTESTS
