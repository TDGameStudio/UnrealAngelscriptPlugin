#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptTestEngineHelper.h"

#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerSessionTests,
	"Angelscript.TestModule.Debugger.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsTrue(bActual, Message);
}

static bool CheckFalse(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsFalse(bActual, Message);
}

template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(Expected, Actual, Message);
}

public:
	FDebuggerTestContext Ctx;

	BEFORE_EACH()
	{
		ASSERT_THAT(IsTrue(Ctx.SetUp(*TestRunner)));
	}

	AFTER_EACH()
	{
		Ctx.TearDown();
	}

	TEST_METHOD(DisconnectClearsDebugState)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Session.DisconnectClearsDebugState should compile the breakpoint fixture")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		Breakpoint.Id = 2201;
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint), TEXT("Debugger.Session.DisconnectClearsDebugState should send the target breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Session.DisconnectClearsDebugState should observe the breakpoint registration before disconnecting the last client"))));

		ASSERT_THAT(IsTrue(Ctx.GetDebugServer().bIsDebugging, TEXT("Debugger.Session.DisconnectClearsDebugState should enter debugging before the disconnect")));
		ASSERT_THAT(AreEqual(1, Ctx.GetDebugServer().BreakpointCount, TEXT("Debugger.Session.DisconnectClearsDebugState should hold exactly one authoritative breakpoint before disconnect")));

		ASSERT_THAT(IsTrue(Ctx.Client.SendDisconnect(), TEXT("Debugger.Session.DisconnectClearsDebugState should send Disconnect from the primary client")));

		ASSERT_THAT(IsTrue(
			WaitForDebugServerIdle(Ctx.Session, Ctx.GetDefaultTimeoutSeconds()),
			TEXT("Debugger.Session.DisconnectClearsDebugState should let the server return to an idle state after the last client disconnects")));

		Ctx.Client.Disconnect();

		ASSERT_THAT(IsFalse(Ctx.GetDebugServer().bIsDebugging, TEXT("Debugger.Session.DisconnectClearsDebugState should clear bIsDebugging after the last client disconnects")));
		ASSERT_THAT(IsFalse(Ctx.GetDebugServer().bIsPaused, TEXT("Debugger.Session.DisconnectClearsDebugState should clear bIsPaused after the last client disconnects")));
		ASSERT_THAT(IsFalse(Ctx.GetDebugServer().bPauseRequested, TEXT("Debugger.Session.DisconnectClearsDebugState should clear bPauseRequested after the last client disconnects")));
		ASSERT_THAT(IsFalse(Ctx.GetDebugServer().HasAnyClients(), TEXT("Debugger.Session.DisconnectClearsDebugState should remove the disconnected socket from the client list")));
		ASSERT_THAT(AreEqual(0, Ctx.GetDebugServer().BreakpointCount, TEXT("Debugger.Session.DisconnectClearsDebugState should clear all breakpoints after the last client disconnects")));

		TAtomic<bool> bMonitorReady(false);
		TAtomic<bool> bMonitorShouldStop(false);
		TAtomic<bool> bInvocationCompleted(false);
		TFuture<FLifecycleNoStopMonitorResult> MonitorFuture = StartLifecycleNoStopMonitor(
			Ctx.GetPort(),
			bMonitorReady,
			bMonitorShouldStop,
			bInvocationCompleted,
			Ctx.GetDefaultTimeoutSeconds());
		ON_SCOPE_EXIT
		{
			bMonitorShouldStop = true;
			if (MonitorFuture.IsValid())
			{
				MonitorFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady, TEXT("Debugger.Session.DisconnectClearsDebugState should bring the reconnect monitor up before re-running the test case"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.Session.DisconnectClearsDebugState should complete the follow-up invocation without a lingering breakpoint stop")))
		{
			bMonitorShouldStop = true;
			return;
		}

		bInvocationCompleted = true;
		bMonitorShouldStop = true;
		const FLifecycleNoStopMonitorResult MonitorResult = MonitorFuture.Get();

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Session.DisconnectClearsDebugState should keep the reconnect monitor error-free"), MonitorResult.Error.IsEmpty()))
		{
			TestRunner->AddError(MonitorResult.Error);
			return;
		}

		ASSERT_THAT(IsFalse(MonitorResult.bTimedOut, TEXT("Debugger.Session.DisconnectClearsDebugState should not time out while monitoring the reconnect run")));

		ASSERT_THAT(IsTrue(MonitorResult.bReceivedVersion, TEXT("Debugger.Session.DisconnectClearsDebugState should let the second client receive DebugServerVersion during reconnect")));
		ASSERT_THAT(AreEqual(0, MonitorResult.ResidualMessagesAfterHandshake.Num(), TEXT("Debugger.Session.DisconnectClearsDebugState should not leave residual messages queued after the reconnect handshake")));
		ASSERT_THAT(AreEqual(0, MonitorResult.UnexpectedStopCount, TEXT("Debugger.Session.DisconnectClearsDebugState should not emit any HasStopped during the reconnect run without re-registering breakpoints")));
		ASSERT_THAT(AreEqual(0, MonitorResult.ContinuedCount, TEXT("Debugger.Session.DisconnectClearsDebugState should not need any HasContinued messages during the reconnect run")));
		ASSERT_THAT(AreEqual(0, MonitorResult.ResidualMessagesAfterInvocation.Num(), TEXT("Debugger.Session.DisconnectClearsDebugState should not leave residual debugger messages queued after the reconnect invocation")));
		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Session.DisconnectClearsDebugState should complete the reconnect invocation successfully")));
		ASSERT_THAT(AreEqual(8, InvocationState->Result, TEXT("Debugger.Session.DisconnectClearsDebugState should preserve the reconnect invocation return value")));
	}

	TEST_METHOD(SecondClientStartPreservesBreakpoints)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		TAtomic<bool> bAdditionalMonitorReady(false);
		TAtomic<bool> bAdditionalMonitorHandshakeSucceeded(false);
		TAtomic<bool> bAbortAdditionalMonitor(false);
		TAtomic<bool> bInvocationCompleted(false);
		TAtomic<int32> MinObservedBreakpointCountDuringSecondHandshake(MAX_int32);
		TFuture<FAdditionalDebuggerMonitorResult> AdditionalMonitorFuture;
		ON_SCOPE_EXIT
		{
			bAbortAdditionalMonitor = true;
			if (AdditionalMonitorFuture.IsValid())
			{
				AdditionalMonitorFuture.Wait();
			}

			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger multi-client test should compile the breakpoint fixture")));

		Ctx.Client.DrainPendingMessages();

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		Breakpoint.Id = 421;
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint), TEXT("Debugger multi-client test should register the primary breakpoint before connecting the second client")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger multi-client test should observe one registered breakpoint before the second client starts debugging"))));

		ASSERT_THAT(IsTrue(WaitForSpecificBreakpoint(
			*TestRunner,
			Ctx.Session,
			Fixture.ModuleName.ToString(),
			Fixture.GetLine(TEXT("BreakpointHelperLine")),
			TEXT("Debugger multi-client test should store the helper-line breakpoint before the second client starts debugging"))));

		AdditionalMonitorFuture = StartAdditionalDebuggerClientMonitor(
			Ctx.GetDebugServer(),
			Ctx.GetPort(),
			bAdditionalMonitorReady,
			bAdditionalMonitorHandshakeSucceeded,
			bAbortAdditionalMonitor,
			bInvocationCompleted,
			MinObservedBreakpointCountDuringSecondHandshake,
			Ctx.GetDefaultTimeoutSeconds());

		const bool bAdditionalMonitorReadyReached = Ctx.Session.PumpUntil(
			[&bAdditionalMonitorReady]()
			{
				return bAdditionalMonitorReady.Load();
			},
			Ctx.GetDefaultTimeoutSeconds());
		ASSERT_THAT(IsTrue(bAdditionalMonitorReadyReached, TEXT("Debugger multi-client test should bring the additional debugger client to the post-StartDebugging ready state")));

		if (!bAdditionalMonitorHandshakeSucceeded.Load())
		{
			const FAdditionalDebuggerMonitorResult MonitorResult = AdditionalMonitorFuture.Get();
			if (!MonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(MonitorResult.Error);
			}
			return;
		}

		ASSERT_THAT(AreEqual(
			1,
			MinObservedBreakpointCountDuringSecondHandshake.Load(),
			TEXT("Debugger multi-client test should not let breakpoint count dip during the second client StartDebugging handshake")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger multi-client test should preserve the authoritative breakpoint count after the second client starts debugging"))));

		ASSERT_THAT(IsTrue(WaitForSpecificBreakpoint(
			*TestRunner,
			Ctx.Session,
			Fixture.ModuleName.ToString(),
			Fixture.GetLine(TEXT("BreakpointHelperLine")),
			TEXT("Debugger multi-client test should keep the helper-line breakpoint registered after the second client starts debugging"))));

		TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger multi-client test should complete the invocation after continuing from the preserved breakpoint stop"))));
		bInvocationCompleted = true;

		const FAdditionalDebuggerMonitorResult MonitorResult = AdditionalMonitorFuture.Get();
		if (!CheckTrue(*TestRunner, TEXT("Debugger multi-client test should complete the additional-client monitor without transport errors"), MonitorResult.Error.IsEmpty()))
		{
			if (!MonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(MonitorResult.Error);
			}
			return;
		}

		ASSERT_THAT(IsFalse(MonitorResult.bTimedOut, TEXT("Debugger multi-client test should not time out while waiting for the preserved breakpoint stop")));
		ASSERT_THAT(IsFalse(MonitorResult.bCompletedWithoutStop, TEXT("Debugger multi-client test should not complete the invocation before any additional client observes the preserved breakpoint stop")));
		ASSERT_THAT(AreEqual(1, MonitorResult.StopEnvelopes.Num(), TEXT("Debugger multi-client test should emit exactly one preserved breakpoint stop for the additional debugger client")));
		ASSERT_THAT(IsTrue(MonitorResult.StopMessage.IsSet(), TEXT("Debugger multi-client test should deserialize the preserved HasStopped payload")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), MonitorResult.StopMessage->Reason, TEXT("Debugger multi-client test should stop because of a breakpoint")));

		ASSERT_THAT(IsTrue(MonitorResult.CapturedCallstack.IsSet(), TEXT("Debugger multi-client test should capture a callstack from the additional debugger client after the preserved breakpoint stop")));

		const FAngelscriptCallStack& Callstack = MonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(Callstack.Frames.Num() > 0, TEXT("Debugger multi-client test should return at least one frame after the preserved breakpoint stop")));

		ASSERT_THAT(IsTrue(Callstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger multi-client test should report the fixture filename in the top stack frame")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BreakpointHelperLine")), Callstack.Frames[0].LineNumber, TEXT("Debugger multi-client test should stop at BreakpointHelperLine")));
		ASSERT_THAT(AreEqual(1, MonitorResult.ContinuedCount, TEXT("Debugger multi-client test should observe a single HasContinued after the preserved breakpoint stop")));

		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger multi-client test should execute the breakpoint fixture successfully after continue")));
		ASSERT_THAT(AreEqual(8, InvocationState->Result, TEXT("Debugger multi-client test should preserve the fixture return value")));
	}

	TEST_METHOD(SingleClientBreakpointRoundtrip)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
			const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
			TAtomic<bool> bWorkerReady{false};
			TAtomic<bool> bAbortWorker{false};
			bool bWorkerStarted = false;
			bool bWorkerJoined = false;
			TFuture<FSingleClientDebuggerTranscript> WorkerFuture;

			ON_SCOPE_EXIT
			{
				bAbortWorker = true;
				if (bWorkerStarted && !bWorkerJoined)
				{
					WorkerFuture.Wait();
				}

				Engine.DiscardModule(*Fixture.ModuleName.ToString());
				CollectGarbage(RF_NoFlags, true);
			};

			ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.SingleClient.BreakpointRoundtrip should compile the breakpoint fixture")));

			TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Fixture.Filename, Fixture.ModuleName.ToString());
			ASSERT_THAT(IsTrue(ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr, TEXT("Debugger.SingleClient.BreakpointRoundtrip should resolve the compiled module immediately after compilation")));

			FAngelscriptBreakpoint Breakpoint;
			Breakpoint.Filename = Fixture.Filename;
			Breakpoint.ModuleName = Fixture.ModuleName.ToString();
			Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));

			FSingleClientDebuggerWorkerConfig WorkerConfig;
			WorkerConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();
			WorkerConfig.InitialBreakpoints.Add(Breakpoint);
			WorkerConfig.StopActions.AddDefaulted();

			WorkerFuture = RunSingleClientDebuggerWorker(Ctx.GetPort(), bWorkerReady, bAbortWorker, WorkerConfig);
			bWorkerStarted = true;

			ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bWorkerReady, TEXT("Debugger.SingleClient.BreakpointRoundtrip should finish handshake and breakpoint registration before execution"))));

			ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.SingleClient.BreakpointRoundtrip should observe the breakpoint registration before running the script"))));

			const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
				Engine,
				Fixture.Filename,
				Fixture.ModuleName,
				Fixture.EntryFunctionDeclaration);

			ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.SingleClient.BreakpointRoundtrip should complete script invocation after the same client sends Continue"))));

			FSingleClientDebuggerTranscript Transcript = WorkerFuture.Get();
			bWorkerJoined = true;

			if (!Transcript.Error.IsEmpty())
			{
				TestRunner->AddError(Transcript.Error);
				return;
			}

			ASSERT_THAT(IsFalse(Transcript.bTimedOut, TEXT("Debugger.SingleClient.BreakpointRoundtrip should not time out")));
			ASSERT_THAT(IsTrue(Transcript.DebugServerVersion.IsSet(), TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive DebugServerVersion on the same client")));

			ASSERT_THAT(AreEqual(DEBUG_SERVER_VERSION, Transcript.DebugServerVersion->DebugServerVersion, TEXT("Debugger.SingleClient.BreakpointRoundtrip should keep the adapter handshake on the same socket")));
			ASSERT_THAT(AreEqual(1, CountMessagesByType(Transcript, EDebugMessageType::DebugServerVersion), TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one DebugServerVersion envelope")));
			ASSERT_THAT(AreEqual(1, CountMessagesByType(Transcript, EDebugMessageType::HasStopped), TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one HasStopped envelope")));
			ASSERT_THAT(AreEqual(1, CountMessagesByType(Transcript, EDebugMessageType::CallStack), TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one CallStack envelope")));
			ASSERT_THAT(AreEqual(1, CountMessagesByType(Transcript, EDebugMessageType::HasContinued), TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one HasContinued envelope")));
			const int32 PingAliveCount = CountMessagesByType(Transcript, EDebugMessageType::PingAlive);
			ASSERT_THAT(IsTrue(PingAliveCount <= 1, TEXT("Debugger.SingleClient.BreakpointRoundtrip should observe at most one PingAlive heartbeat during the roundtrip")));
			ASSERT_THAT(AreEqual(4 + PingAliveCount, Transcript.ReceivedMessages.Num(), TEXT("Debugger.SingleClient.BreakpointRoundtrip should only observe handshake, stop, callstack, continue and optional PingAlive")));
			ASSERT_THAT(AreEqual(1, Transcript.StopMessages.Num(), TEXT("Debugger.SingleClient.BreakpointRoundtrip should deserialize exactly one stop message")));
			ASSERT_THAT(AreEqual(1, Transcript.CallStacks.Num(), TEXT("Debugger.SingleClient.BreakpointRoundtrip should deserialize exactly one callstack")));
			ASSERT_THAT(AreEqual(1, Transcript.HasContinuedCount, TEXT("Debugger.SingleClient.BreakpointRoundtrip should count exactly one HasContinued notification")));
			ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.SingleClient.BreakpointRoundtrip should finish the script invocation successfully")));
			ASSERT_THAT(AreEqual(8, InvocationState->Result, TEXT("Debugger.SingleClient.BreakpointRoundtrip should preserve the expected script result")));

			ASSERT_THAT(IsTrue(Transcript.StopMessages.Num() == 1, TEXT("Debugger.SingleClient.BreakpointRoundtrip should deserialize the stop reason")));
			ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), Transcript.StopMessages[0].Reason, TEXT("Debugger.SingleClient.BreakpointRoundtrip should stop because of a breakpoint")));

			ASSERT_THAT(IsTrue(Transcript.CallStacks.Num() == 1, TEXT("Debugger.SingleClient.BreakpointRoundtrip should capture exactly one callstack")));

			const FAngelscriptCallStack& CallStack = Transcript.CallStacks[0];
			ASSERT_THAT(IsTrue(CallStack.Frames.Num() > 0, TEXT("Debugger.SingleClient.BreakpointRoundtrip should return at least one frame")));

			ASSERT_THAT(IsTrue(CallStack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.SingleClient.BreakpointRoundtrip should report the fixture filename in the top stack frame")));
			ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BreakpointHelperLine")), CallStack.Frames[0].LineNumber, TEXT("Debugger.SingleClient.BreakpointRoundtrip should stop at the requested helper line")));
		}
	}
};

#endif
