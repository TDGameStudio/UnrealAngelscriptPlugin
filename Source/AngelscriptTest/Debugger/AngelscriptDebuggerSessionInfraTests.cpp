#include "CQTest.h"
#include "Shared/AngelscriptDebuggerTestClient.h"
#include "Shared/AngelscriptDebuggerTestSession.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptDebuggerSessionInfraTests_Private
{
	TSharedRef<FFakeDebuggerClientSocket> MakePendingConnectSocket(
		const FString& Description,
		const TArray<ESocketConnectionState>& ConnectionStates)
	{
		TSharedRef<FFakeDebuggerClientSocket> Socket = MakeShared<FFakeDebuggerClientSocket>(Description);
		Socket->SetConnectResult(false, SE_EINPROGRESS);
		Socket->SetConnectionStates(ConnectionStates);
		return Socket;
	}

	bool WaitForDebugServerVersion(
		FAutomationTestBase& Test,
		FAngelscriptDebuggerTestSession& Session,
		FAngelscriptDebuggerTestClient& Client)
	{
		TOptional<FAngelscriptDebugMessageEnvelope> VersionEnvelope;
		const bool bReceivedVersion = Session.PumpUntil(
			[&Client, &VersionEnvelope]()
			{
				if (VersionEnvelope.IsSet())
				{
					return true;
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
				if (Envelope.IsSet() && Envelope->MessageType == EDebugMessageType::DebugServerVersion)
				{
					VersionEnvelope = MoveTemp(Envelope);
					return true;
				}

				return false;
			},
			Session.GetDefaultTimeoutSeconds());

		if (!Test.TestTrue(TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should receive DebugServerVersion after StartDebugging"), bReceivedVersion))
		{
			if (!Client.GetLastError().IsEmpty())
			{
				Test.AddError(Client.GetLastError());
			}
			return false;
		}

		const TOptional<FDebugServerVersionMessage> DebugServerVersion =
			FAngelscriptDebuggerTestClient::DeserializeMessage<FDebugServerVersionMessage>(VersionEnvelope.GetValue());
		if (!Test.TestTrue(TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should deserialize the DebugServerVersion payload"), DebugServerVersion.IsSet()))
		{
			return false;
		}

		return Test.TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should report the current debug server version"),
			DebugServerVersion->DebugServerVersion,
			DEBUG_SERVER_VERSION);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerSessionInfraTests,
	"Angelscript.TestModule.Debugger.SessionInfra",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InitializeDoesNotMutateAdapterVersion)
	{
		using namespace AngelscriptDebuggerSessionInfraTests_Private;
		constexpr int32 InitializeSentinelVersion = 7;
		constexpr int32 HandshakeAdapterVersion = 2;
		constexpr int32 FreshSessionSentinelVersion = 11;

		FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = 45.0f;

		FScopedDebugAdapterVersionSentinel AdapterVersionSentinel(InitializeSentinelVersion);
		FAngelscriptDebuggerTestSession Session;
		FAngelscriptDebuggerTestClient Client;
		ON_SCOPE_EXIT
		{
			if (Client.IsConnected())
			{
				if (Session.IsInitialized() && Session.GetDebugServer().bIsDebugging)
				{
					Client.SendStopDebugging();
					Session.PumpUntil([&Session]() { return !Session.GetDebugServer().bIsDebugging; }, 1.0f);
				}

				Client.SendDisconnect();
				Client.Disconnect();
			}

			Session.Shutdown();
		};

		TestRunner->TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should start from the sentinel adapter version"),
			AdapterVersionSentinel.GetCurrent(),
			InitializeSentinelVersion);

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should initialize the debugger session"), Session.Initialize(SessionConfig))));

		TestRunner->TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should preserve the sentinel through Initialize before any handshake"),
			AdapterVersionSentinel.GetCurrent(),
			InitializeSentinelVersion);

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should connect the debugger client"), Client.Connect(TEXT("127.0.0.1"), Session.GetPort()))));

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should send StartDebugging"), Client.SendStartDebugging(HandshakeAdapterVersion))));

		ASSERT_THAT(IsTrue(WaitForDebugServerVersion(*TestRunner, Session, Client)));

		TestRunner->TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should switch to the requested adapter version after StartDebugging"),
			AdapterVersionSentinel.GetCurrent(),
			HandshakeAdapterVersion);

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should send StopDebugging"), Client.SendStopDebugging())));

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should leave debugging mode after StopDebugging"),
			Session.PumpUntil([&Session]() { return !Session.GetDebugServer().bIsDebugging; }, Session.GetDefaultTimeoutSeconds()))));

		Client.SendDisconnect();
		Client.Disconnect();

		Session.Shutdown();
		TestRunner->TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should restore the pre-handshake sentinel during Shutdown"),
			AdapterVersionSentinel.GetCurrent(),
			InitializeSentinelVersion);

		AdapterVersionSentinel.SetSentinel(FreshSessionSentinelVersion);

		{
			FAngelscriptDebuggerTestSession FreshSession;
		}

		TestRunner->TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should keep the sentinel when a never-initialized session is destroyed"),
			AdapterVersionSentinel.GetCurrent(),
			FreshSessionSentinelVersion);

		FAngelscriptDebuggerTestSession ExplicitShutdownSession;
		ExplicitShutdownSession.Shutdown();
		TestRunner->TestEqual(
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should keep the sentinel when Shutdown is called on a never-initialized session"),
			AdapterVersionSentinel.GetCurrent(),
			FreshSessionSentinelVersion);
	}

	TEST_METHOD(PreservesDebugBreakState)
	{
		using namespace AngelscriptDebuggerSessionInfraTests_Private;
		FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = 45.0f;
		SessionConfig.bDisableDebugBreaks = true;

		TSharedPtr<FAngelscriptMockDebugServer> MockServer = MakeShared<FAngelscriptMockDebugServer>();
		SessionConfig.MockServer = MockServer;

		FScopedDebugBreakStateSentinel DebugBreakStateSentinel;

		auto RunTestCase = [this, &SessionConfig, &DebugBreakStateSentinel](const bool bInitiallyEnabled, const TCHAR* TestCaseLabel) -> bool
		{
			DebugBreakStateSentinel.SetEnabled(bInitiallyEnabled);
			if (!TestRunner->TestEqual(
					*FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should start %s from the requested debug-break state"), TestCaseLabel),
					DebugBreakStateSentinel.IsEnabled(),
					bInitiallyEnabled))
			{
				return false;
			}

			{
				FAngelscriptDebuggerTestSession Session;
				if (!TestRunner->TestTrue(
						*FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should initialize the debugger session for the %s branch"), TestCaseLabel),
						Session.Initialize(SessionConfig)))
				{
					return false;
				}

				if (!TestRunner->TestFalse(
						*FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should disable debug breaks while the %s session is active"), TestCaseLabel),
						DebugBreakStateSentinel.IsEnabled()))
				{
					return false;
				}
			}

			return TestRunner->TestEqual(
				*FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should restore the %s debug-break state after session shutdown"), TestCaseLabel),
				DebugBreakStateSentinel.IsEnabled(),
				bInitiallyEnabled);
		};

		bool bPassed = true;
		bPassed &= RunTestCase(true, TEXT("pre-enabled"));
		bPassed &= RunTestCase(false, TEXT("pre-disabled"));
		TestRunner->TestTrue(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should pass both branches"), bPassed);
	}

	TEST_METHOD(ClientConnectTimeoutReportsFailure)
	{
		using namespace AngelscriptDebuggerSessionInfraTests_Private;
		constexpr float FailureTimeoutSeconds = 0.01f;
		constexpr float SuccessTimeoutSeconds = 0.05f;

		bool bPassed = true;

		{
			TSharedRef<FFakeDebuggerClientSocket> TimeoutSocket = MakePendingConnectSocket(
				TEXT("TimeoutSocket"),
				{ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_NotConnected});
			FAngelscriptDebuggerTestClient Client(MakeShared<FSingleDebuggerTestSocketFactory>(TimeoutSocket));

			bPassed &= TestRunner->TestFalse(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should fail the connect attempt when the socket never reaches SCS_Connected"),
				Client.Connect(TEXT("127.0.0.1"), 31337, FailureTimeoutSeconds));
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should mention the host in the connect-timeout error"),
				Client.GetLastError().Contains(TEXT("127.0.0.1")));
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should mention the port in the connect-timeout error"),
				Client.GetLastError().Contains(TEXT("31337")));
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should report a connect-timeout instead of deferring the error to message wait helpers"),
				Client.GetLastError().Contains(TEXT("Timed out")));
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should report the last connection state"),
				Client.GetLastError().Contains(TEXT("SCS_NotConnected")));
			bPassed &= TestRunner->TestFalse(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should leave the client disconnected after timeout"),
				Client.IsConnected());
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should close the socket after a failed connect attempt"),
				TimeoutSocket->WasClosed());
			bPassed &= TestRunner->TestFalse(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should not permit StartDebugging after a failed connect attempt"),
				Client.SendStartDebugging(2));
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should clear any half-connected socket before StartDebugging"),
				Client.GetLastError().Contains(TEXT("active socket connection")));
		}

		{
			TSharedRef<FFakeDebuggerClientSocket> SuccessSocket = MakePendingConnectSocket(
				TEXT("SuccessSocket"),
				{ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_Connected});
			FAngelscriptDebuggerTestClient Client(MakeShared<FSingleDebuggerTestSocketFactory>(SuccessSocket));

			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should succeed when the same pending connect reaches SCS_Connected before the deadline"),
				Client.Connect(TEXT("127.0.0.1"), 31338, SuccessTimeoutSeconds));
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should report the connected state after the pending connect succeeds"),
				Client.IsConnected());
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should clear LastError after a successful pending connect"),
				Client.GetLastError().IsEmpty());

			Client.Disconnect();
			bPassed &= TestRunner->TestTrue(
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should close the socket when the successfully connected client disconnects"),
				SuccessSocket->WasClosed());
		}

		TestRunner->TestTrue(TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should pass all sub-assertions"), bPassed);
	}
};

#endif
