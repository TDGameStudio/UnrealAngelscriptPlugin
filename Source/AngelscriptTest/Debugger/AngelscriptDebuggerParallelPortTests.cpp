#include "CQTest.h"

#include "AngelscriptDebuggerTestSession.h"
#include "Common/TcpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/ScopeExit.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerParallelPortTests,
	"Angelscript.TestModule.Debugger.ParallelPort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExplicitOccupiedPortFailsFast)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		ASSERT_THAT(IsNotNull(SocketSubsystem, TEXT("Debugger parallel-port test should resolve the platform socket subsystem")));

		FSocket* OccupiedSocket = FTcpSocketBuilder(TEXT("AngelscriptDebuggerOccupiedPortTest"))
			.AsReusable(false)
			.BoundToEndpoint(FIPv4Endpoint(FIPv4Address::Any, 0))
			.Listening(1);
		ASSERT_THAT(IsNotNull(OccupiedSocket, TEXT("Debugger parallel-port test should reserve an ephemeral TCP port")));
		ON_SCOPE_EXIT
		{
			if (OccupiedSocket != nullptr)
			{
				OccupiedSocket->Close();
				SocketSubsystem->DestroySocket(OccupiedSocket);
			}
		};

		TSharedRef<FInternetAddr> OccupiedAddress = SocketSubsystem->CreateInternetAddr();
		OccupiedSocket->GetAddress(*OccupiedAddress);
		ASSERT_THAT(IsTrue(
			OccupiedAddress->GetPort() > 0,
			TEXT("Debugger parallel-port test should receive a concrete ephemeral port")));

		FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DebugServerPort = OccupiedAddress->GetPort();
		SessionConfig.DefaultTimeoutSeconds = 0.1f;

		FAngelscriptDebuggerTestSession Session;
		ASSERT_THAT(IsFalse(
			Session.Initialize(SessionConfig),
			TEXT("Debugger session should reject an explicitly occupied port during initialization")));
		ASSERT_THAT(IsFalse(
			Session.IsInitialized(),
			TEXT("Debugger session should remain uninitialized after an occupied-port failure")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
