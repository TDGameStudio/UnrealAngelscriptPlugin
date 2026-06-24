#include "CQTest.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerTestSession.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerSessionInfraTests,
	"Angelscript.TestModule.Debugger.SessionInfra",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsTrue(bActual, Message);
}

static bool CheckTrue(FAutomationTestBase& Test, const FString& Message, bool bActual)
{
	return CheckTrue(Test, *Message, bActual);
}

static bool CheckFalse(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsFalse(bActual, Message);
}

static bool CheckFalse(FAutomationTestBase& Test, const FString& Message, bool bActual)
{
	return CheckFalse(Test, *Message, bActual);
}

template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(Expected, Actual, Message);
}

template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const FString& Message, const ActualType& Actual, const ExpectedType& Expected)
{
	return CheckEqual(Test, *Message, Actual, Expected);
}

static TSharedRef<FFakeDebuggerClientSocket> MakePendingConnectSocket(
	const FString& Description,
	const TArray<ESocketConnectionState>& ConnectionStates)
{
	TSharedRef<FFakeDebuggerClientSocket> Socket = MakeShared<FFakeDebuggerClientSocket>(Description);
	Socket->SetConnectResult(false, SE_EINPROGRESS);
	Socket->SetConnectionStates(ConnectionStates);
	return Socket;
}

static bool WaitForDebugServerVersion(
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

	if (!CheckTrue(Test, TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should receive DebugServerVersion after StartDebugging"), bReceivedVersion))
	{
		if (!Client.GetLastError().IsEmpty())
		{
			Test.AddError(Client.GetLastError());
		}
		return false;
	}

	const TOptional<FDebugServerVersionMessage> DebugServerVersion =
		FAngelscriptDebuggerTestClient::DeserializeMessage<FDebugServerVersionMessage>(VersionEnvelope.GetValue());
	if (!CheckTrue(Test, TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should deserialize the DebugServerVersion payload"), DebugServerVersion.IsSet()))
	{
		return false;
	}

	return CheckEqual(
		Test,
		TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should report the current debug server version"),
		DebugServerVersion->DebugServerVersion,
		DEBUG_SERVER_VERSION);
}

public:
	TEST_METHOD(InitializeDoesNotMutateAdapterVersion)
	{
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

		ASSERT_THAT(AreEqual(
			InitializeSentinelVersion,
			AdapterVersionSentinel.GetCurrent(),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should start from the sentinel adapter version")
		));

		ASSERT_THAT(IsTrue(Session.Initialize(SessionConfig), TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should initialize the debugger session")));

		ASSERT_THAT(AreEqual(
			InitializeSentinelVersion,
			AdapterVersionSentinel.GetCurrent(),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should preserve the sentinel through Initialize before any handshake")
		));

		ASSERT_THAT(IsTrue(Client.Connect(TEXT("127.0.0.1"), Session.GetPort()), TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should connect the debugger client")));

		ASSERT_THAT(IsTrue(Client.SendStartDebugging(HandshakeAdapterVersion), TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should send StartDebugging")));

		ASSERT_THAT(IsTrue(WaitForDebugServerVersion(*TestRunner, Session, Client)));

		ASSERT_THAT(AreEqual(
			HandshakeAdapterVersion,
			AdapterVersionSentinel.GetCurrent(),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should switch to the requested adapter version after StartDebugging")
		));

		ASSERT_THAT(IsTrue(Client.SendStopDebugging(), TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should send StopDebugging")));

		ASSERT_THAT(IsTrue(
			Session.PumpUntil([&Session]() { return !Session.GetDebugServer().bIsDebugging; }, Session.GetDefaultTimeoutSeconds()),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should leave debugging mode after StopDebugging")));

		Client.SendDisconnect();
		Client.Disconnect();

		Session.Shutdown();
		ASSERT_THAT(AreEqual(
			InitializeSentinelVersion,
			AdapterVersionSentinel.GetCurrent(),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should restore the pre-handshake sentinel during Shutdown")
		));

		AdapterVersionSentinel.SetSentinel(FreshSessionSentinelVersion);

		{
			FAngelscriptDebuggerTestSession FreshSession;
		}

		ASSERT_THAT(AreEqual(
			FreshSessionSentinelVersion,
			AdapterVersionSentinel.GetCurrent(),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should keep the sentinel when a never-initialized session is destroyed")
		));

		FAngelscriptDebuggerTestSession ExplicitShutdownSession;
		ExplicitShutdownSession.Shutdown();
		ASSERT_THAT(AreEqual(
			FreshSessionSentinelVersion,
			AdapterVersionSentinel.GetCurrent(),
			TEXT("Debugger.SessionInfra.InitializeDoesNotMutateAdapterVersion should keep the sentinel when Shutdown is called on a never-initialized session")
		));
	}

	TEST_METHOD(PreservesDebugBreakState)
	{
FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = 45.0f;
		SessionConfig.bDisableDebugBreaks = true;

		TSharedPtr<FAngelscriptMockDebugServer> MockServer = MakeShared<FAngelscriptMockDebugServer>();
		SessionConfig.MockServer = MockServer;

		FScopedDebugBreakStateSentinel DebugBreakStateSentinel;

		auto RunTestCase = [this, &SessionConfig, &DebugBreakStateSentinel](const bool bInitiallyEnabled, const TCHAR* TestCaseLabel) -> bool
		{
			DebugBreakStateSentinel.SetEnabled(bInitiallyEnabled);
			if (!CheckEqual(
					*TestRunner,
					FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should start %s from the requested debug-break state"), TestCaseLabel),
					DebugBreakStateSentinel.IsEnabled(),
					bInitiallyEnabled))
			{
				return false;
			}

			{
				FAngelscriptDebuggerTestSession Session;
				if (!CheckTrue(
						*TestRunner,
						FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should initialize the debugger session for the %s branch"), TestCaseLabel),
						Session.Initialize(SessionConfig)))
				{
					return false;
				}

				if (!CheckFalse(
						*TestRunner,
						FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should disable debug breaks while the %s session is active"), TestCaseLabel),
						DebugBreakStateSentinel.IsEnabled()))
				{
					return false;
				}
			}

			return CheckEqual(
				*TestRunner,
				FString::Printf(TEXT("Debugger.SessionInfra.PreservesDebugBreakState should restore the %s debug-break state after session shutdown"), TestCaseLabel),
				DebugBreakStateSentinel.IsEnabled(),
				bInitiallyEnabled);
		};

		bool bPassed = true;
		bPassed &= RunTestCase(true, TEXT("pre-enabled"));
		bPassed &= RunTestCase(false, TEXT("pre-disabled"));
		ASSERT_THAT(IsTrue(bPassed, TEXT("Debugger.SessionInfra.PreservesDebugBreakState should pass both branches")));
	}

	TEST_METHOD(ClientConnectTimeoutReportsFailure)
	{
constexpr float FailureTimeoutSeconds = 0.01f;
		constexpr float SuccessTimeoutSeconds = 0.05f;

		bool bPassed = true;

		{
			TSharedRef<FFakeDebuggerClientSocket> TimeoutSocket = MakePendingConnectSocket(
				TEXT("TimeoutSocket"),
				{ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_NotConnected});
			FAngelscriptDebuggerTestClient Client(MakeShared<FSingleDebuggerTestSocketFactory>(TimeoutSocket));

			bPassed &= CheckFalse(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should fail the connect attempt when the socket never reaches SCS_Connected"),
				Client.Connect(TEXT("127.0.0.1"), 31337, FailureTimeoutSeconds));
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should mention the host in the connect-timeout error"),
				Client.GetLastError().Contains(TEXT("127.0.0.1")));
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should mention the port in the connect-timeout error"),
				Client.GetLastError().Contains(TEXT("31337")));
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should report a connect-timeout instead of deferring the error to message wait helpers"),
				Client.GetLastError().Contains(TEXT("Timed out")));
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should report the last connection state"),
				Client.GetLastError().Contains(TEXT("SCS_NotConnected")));
			bPassed &= CheckFalse(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should leave the client disconnected after timeout"),
				Client.IsConnected());
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should close the socket after a failed connect attempt"),
				TimeoutSocket->WasClosed());
			bPassed &= CheckFalse(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should not permit StartDebugging after a failed connect attempt"),
				Client.SendStartDebugging(2));
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should clear any half-connected socket before StartDebugging"),
				Client.GetLastError().Contains(TEXT("active socket connection")));
		}

		{
			TSharedRef<FFakeDebuggerClientSocket> SuccessSocket = MakePendingConnectSocket(
				TEXT("SuccessSocket"),
				{ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_NotConnected, ESocketConnectionState::SCS_Connected});
			FAngelscriptDebuggerTestClient Client(MakeShared<FSingleDebuggerTestSocketFactory>(SuccessSocket));

			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should succeed when the same pending connect reaches SCS_Connected before the deadline"),
				Client.Connect(TEXT("127.0.0.1"), 31338, SuccessTimeoutSeconds));
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should report the connected state after the pending connect succeeds"),
				Client.IsConnected());
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should clear LastError after a successful pending connect"),
				Client.GetLastError().IsEmpty());

			Client.Disconnect();
			bPassed &= CheckTrue(
				*TestRunner,
				TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should close the socket when the successfully connected client disconnects"),
				SuccessSocket->WasClosed());
		}

		ASSERT_THAT(IsTrue(bPassed, TEXT("Debugger.SessionInfra.ClientConnectTimeoutReportsFailure should pass all sub-assertions")));
	}
};

#endif
