#pragma once

// =============================================================================
// AngelscriptDebuggerTestContext.h
//
// Unified per-test lifecycle container for Debugger automation tests.
// Wraps FAngelscriptDebuggerTestSession + FAngelscriptDebuggerTestClient into
// a single struct with SetUp / TearDown semantics suitable for CQTest
// BEFORE_EACH / AFTER_EACH hooks.
//
// Usage:
//   TEST_CLASS_WITH_FLAGS(...)
//   {
//       FDebuggerTestContext Ctx;
//       BEFORE_EACH() { ASSERT_THAT(IsTrue(Ctx.SetUp(*TestRunner))); }
//       AFTER_EACH()  { Ctx.TearDown(); }
//       TEST_METHOD(X) { auto& Engine = Ctx.GetEngine(); ... }
//   };
// =============================================================================

#include "CoreMinimal.h"
#include "Assert/NoDiscardAsserter.h"
#include "Misc/AutomationTest.h"

#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptDebuggerTestSession.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FDebuggerTestContext
{
	FAngelscriptDebuggerTestSession Session;
	FAngelscriptDebuggerTestClient Client;
	TAtomic<bool> bMonitorShouldStop{false};

	// ---- SetUp variants ----

	/**
	 * Full setup: Initialize session → connect client → StartDebugging →
	 * receive DebugServerVersion handshake.
	 *
	 * @param AdapterVersion  Protocol version sent in StartDebugging (default 2).
	 *                        Pass 1 for legacy v1 payload tests.
	 */
	bool SetUp(FAutomationTestBase& Test, int32 AdapterVersion = 2, float TimeoutSeconds = kDefaultDebuggerTestTimeoutSeconds)
	{
		bMonitorShouldStop = false;

		FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = TimeoutSeconds;

		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsTrue(
				Session.Initialize(SessionConfig),
				TEXT("Debugger context should initialize the session")))
		{
			return false;
		}

		if (!Assert.IsTrue(
				Client.Connect(TEXT("127.0.0.1"), Session.GetPort()),
				TEXT("Debugger context should connect the primary client")))
		{
			Test.AddError(Client.GetLastError());
			return false;
		}

		bool bSentStartDebugging = false;
		const bool bStartMessageSent = Session.PumpUntil(
			[this, &bSentStartDebugging, AdapterVersion]()
			{
				if (bSentStartDebugging)
				{
					return true;
				}

				bSentStartDebugging = Client.SendStartDebugging(AdapterVersion);
				return bSentStartDebugging;
			},
			Session.GetDefaultTimeoutSeconds());

		if (!Assert.IsTrue(
				bStartMessageSent,
				TEXT("Debugger context should send StartDebugging")))
		{
			Test.AddError(Client.GetLastError());
			return false;
		}

		TOptional<FAngelscriptDebugMessageEnvelope> VersionEnvelope;
		const bool bReceivedVersion = Session.PumpUntil(
			[this, &VersionEnvelope]()
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

		if (!Assert.IsTrue(
				bReceivedVersion,
				TEXT("Debugger context should receive the DebugServerVersion response")))
		{
			Test.AddError(Client.GetLastError());
			return false;
		}

		return true;
	}

	/**
	 * Session-only setup: Initialize session but do NOT connect a client.
	 * Used by SessionInfra tests that test the infrastructure itself.
	 */
	bool SetUpSessionOnly(FAutomationTestBase& Test, float TimeoutSeconds = kDefaultDebuggerTestTimeoutSeconds)
	{
		bMonitorShouldStop = false;

		FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = TimeoutSeconds;

		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(
			Session.Initialize(SessionConfig),
			TEXT("Debugger context should initialize a session-only setup"));
	}

	// ---- TearDown ----

	/**
	 * Clean up all resources. Signals monitors to stop, disconnects the
	 * client, and optionally discards the script module + collects garbage.
	 */
	void TearDown(const FAngelscriptDebuggerScriptFixture* Fixture = nullptr)
	{
		bMonitorShouldStop = true;

		if (Client.IsConnected())
		{
			Client.SendStopDebugging();
			Client.SendDisconnect();
			Client.Disconnect();
		}

		if (Fixture != nullptr && Session.IsInitialized() && !Session.IsMockMode())
		{
			Session.GetEngine().DiscardModule(*Fixture->ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		}

		Session.Shutdown();
	}

	// ---- Accessors ----

	FAngelscriptEngine& GetEngine() const { return Session.GetEngine(); }
	FAngelscriptDebugServer& GetDebugServer() const { return Session.GetDebugServer(); }
	int32 GetPort() const { return Session.GetPort(); }
	float GetDefaultTimeoutSeconds() const { return Session.GetDefaultTimeoutSeconds(); }
};


#endif // WITH_DEV_AUTOMATION_TESTS
