#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptTestEngineHelper.h"

#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerSessionTests,
	"Angelscript.TestModule.Debugger.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should compile the breakpoint fixture"), Fixture.Compile(Engine))));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		Breakpoint.Id = 2201;
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should send the target breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint))));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Session.DisconnectClearsDebugState should observe the breakpoint registration before disconnecting the last client"))));

		TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should enter debugging before the disconnect"), Ctx.GetDebugServer().bIsDebugging);
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should hold exactly one authoritative breakpoint before disconnect"), Ctx.GetDebugServer().BreakpointCount, 1);

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should send Disconnect from the primary client"), Ctx.Client.SendDisconnect())));

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger.Session.DisconnectClearsDebugState should let the server return to an idle state after the last client disconnects"),
			WaitForDebugServerIdle(Ctx.Session, Ctx.GetDefaultTimeoutSeconds()))));

		Ctx.Client.Disconnect();

		TestRunner->TestFalse(TEXT("Debugger.Session.DisconnectClearsDebugState should clear bIsDebugging after the last client disconnects"), Ctx.GetDebugServer().bIsDebugging);
		TestRunner->TestFalse(TEXT("Debugger.Session.DisconnectClearsDebugState should clear bIsPaused after the last client disconnects"), Ctx.GetDebugServer().bIsPaused);
		TestRunner->TestFalse(TEXT("Debugger.Session.DisconnectClearsDebugState should clear bPauseRequested after the last client disconnects"), Ctx.GetDebugServer().bPauseRequested);
		TestRunner->TestFalse(TEXT("Debugger.Session.DisconnectClearsDebugState should remove the disconnected socket from the client list"), Ctx.GetDebugServer().HasAnyClients());
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should clear all breakpoints after the last client disconnects"), Ctx.GetDebugServer().BreakpointCount, 0);

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

		if (!TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should keep the reconnect monitor error-free"), MonitorResult.Error.IsEmpty()))
		{
			TestRunner->AddError(MonitorResult.Error);
			return;
		}

		ASSERT_THAT(IsTrue(TestRunner->TestFalse(TEXT("Debugger.Session.DisconnectClearsDebugState should not time out while monitoring the reconnect run"), MonitorResult.bTimedOut)));

		TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should let the second client receive DebugServerVersion during reconnect"), MonitorResult.bReceivedVersion);
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should not leave residual messages queued after the reconnect handshake"), MonitorResult.ResidualMessagesAfterHandshake.Num(), 0);
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should not emit any HasStopped during the reconnect run without re-registering breakpoints"), MonitorResult.UnexpectedStopCount, 0);
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should not need any HasContinued messages during the reconnect run"), MonitorResult.ContinuedCount, 0);
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should not leave residual debugger messages queued after the reconnect invocation"), MonitorResult.ResidualMessagesAfterInvocation.Num(), 0);
		TestRunner->TestTrue(TEXT("Debugger.Session.DisconnectClearsDebugState should complete the reconnect invocation successfully"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Debugger.Session.DisconnectClearsDebugState should preserve the reconnect invocation return value"), InvocationState->Result, 8);
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

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger multi-client test should compile the breakpoint fixture"), Fixture.Compile(Engine))));

		Ctx.Client.DrainPendingMessages();

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		Breakpoint.Id = 421;
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger multi-client test should register the primary breakpoint before connecting the second client"), Ctx.Client.SendSetBreakpoint(Breakpoint))));

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
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger multi-client test should bring the additional debugger client to the post-StartDebugging ready state"), bAdditionalMonitorReadyReached)));

		if (!bAdditionalMonitorHandshakeSucceeded.Load())
		{
			const FAdditionalDebuggerMonitorResult MonitorResult = AdditionalMonitorFuture.Get();
			if (!MonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(MonitorResult.Error);
			}
			return;
		}

		TestRunner->TestEqual(
			TEXT("Debugger multi-client test should not let breakpoint count dip during the second client StartDebugging handshake"),
			MinObservedBreakpointCountDuringSecondHandshake.Load(),
			1);

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
		if (!TestRunner->TestTrue(TEXT("Debugger multi-client test should complete the additional-client monitor without transport errors"), MonitorResult.Error.IsEmpty()))
		{
			if (!MonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(MonitorResult.Error);
			}
			return;
		}

		ASSERT_THAT(IsTrue(TestRunner->TestFalse(TEXT("Debugger multi-client test should not time out while waiting for the preserved breakpoint stop"), MonitorResult.bTimedOut)));
		ASSERT_THAT(IsTrue(TestRunner->TestFalse(TEXT("Debugger multi-client test should not complete the invocation before any additional client observes the preserved breakpoint stop"), MonitorResult.bCompletedWithoutStop)));
		ASSERT_THAT(IsTrue(TestRunner->TestEqual(TEXT("Debugger multi-client test should emit exactly one preserved breakpoint stop for the additional debugger client"), MonitorResult.StopEnvelopes.Num(), 1)));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger multi-client test should deserialize the preserved HasStopped payload"), MonitorResult.StopMessage.IsSet())));

		TestRunner->TestEqual(TEXT("Debugger multi-client test should stop because of a breakpoint"), MonitorResult.StopMessage->Reason, FString(TEXT("breakpoint")));

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger multi-client test should capture a callstack from the additional debugger client after the preserved breakpoint stop"), MonitorResult.CapturedCallstack.IsSet())));

		const FAngelscriptCallStack& Callstack = MonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger multi-client test should return at least one frame after the preserved breakpoint stop"), Callstack.Frames.Num() > 0)));

		TestRunner->TestTrue(TEXT("Debugger multi-client test should report the fixture filename in the top stack frame"), Callstack.Frames[0].Source.EndsWith(Fixture.Filename));
		TestRunner->TestEqual(TEXT("Debugger multi-client test should stop at BreakpointHelperLine"), Callstack.Frames[0].LineNumber, Fixture.GetLine(TEXT("BreakpointHelperLine")));
		TestRunner->TestEqual(TEXT("Debugger multi-client test should observe a single HasContinued after the preserved breakpoint stop"), MonitorResult.ContinuedCount, 1);

		TestRunner->TestTrue(TEXT("Debugger multi-client test should execute the breakpoint fixture successfully after continue"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Debugger multi-client test should preserve the fixture return value"), InvocationState->Result, 8);
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

			ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should compile the breakpoint fixture"), Fixture.Compile(Engine))));

			TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Fixture.Filename, Fixture.ModuleName.ToString());
			ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should resolve the compiled module immediately after compilation"), ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr)));

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

			TestRunner->TestFalse(TEXT("Debugger.SingleClient.BreakpointRoundtrip should not time out"), Transcript.bTimedOut);
			ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive DebugServerVersion on the same client"), Transcript.DebugServerVersion.IsSet())));

			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should keep the adapter handshake on the same socket"), Transcript.DebugServerVersion->DebugServerVersion, DEBUG_SERVER_VERSION);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one DebugServerVersion envelope"), CountMessagesByType(Transcript, EDebugMessageType::DebugServerVersion), 1);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one HasStopped envelope"), CountMessagesByType(Transcript, EDebugMessageType::HasStopped), 1);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one CallStack envelope"), CountMessagesByType(Transcript, EDebugMessageType::CallStack), 1);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should receive exactly one HasContinued envelope"), CountMessagesByType(Transcript, EDebugMessageType::HasContinued), 1);
			const int32 PingAliveCount = CountMessagesByType(Transcript, EDebugMessageType::PingAlive);
			TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should observe at most one PingAlive heartbeat during the roundtrip"), PingAliveCount <= 1);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should only observe handshake, stop, callstack, continue and optional PingAlive"), Transcript.ReceivedMessages.Num(), 4 + PingAliveCount);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should deserialize exactly one stop message"), Transcript.StopMessages.Num(), 1);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should deserialize exactly one callstack"), Transcript.CallStacks.Num(), 1);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should count exactly one HasContinued notification"), Transcript.HasContinuedCount, 1);
			TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should finish the script invocation successfully"), InvocationState->bSucceeded);
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should preserve the expected script result"), InvocationState->Result, 8);

			ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should deserialize the stop reason"), Transcript.StopMessages.Num() == 1)));
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should stop because of a breakpoint"), Transcript.StopMessages[0].Reason, FString(TEXT("breakpoint")));

			ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should capture exactly one callstack"), Transcript.CallStacks.Num() == 1)));

			const FAngelscriptCallStack& CallStack = Transcript.CallStacks[0];
			ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should return at least one frame"), CallStack.Frames.Num() > 0)));

			TestRunner->TestTrue(TEXT("Debugger.SingleClient.BreakpointRoundtrip should report the fixture filename in the top stack frame"), CallStack.Frames[0].Source.EndsWith(Fixture.Filename));
			TestRunner->TestEqual(TEXT("Debugger.SingleClient.BreakpointRoundtrip should stop at the requested helper line"), CallStack.Frames[0].LineNumber, Fixture.GetLine(TEXT("BreakpointHelperLine")));
		}
	}
};

#endif
