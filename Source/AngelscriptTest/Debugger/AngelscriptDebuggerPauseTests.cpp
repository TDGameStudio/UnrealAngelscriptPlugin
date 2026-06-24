#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestHelpers.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerPauseTests,
	"Angelscript.TestModule.Debugger.Pause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
template <typename T>
static bool WaitForTypedMessageUntil(
	FAngelscriptDebuggerTestClient& Client,
	TAtomic<bool>& bShouldStop,
	EDebugMessageType ExpectedType,
	float TimeoutSeconds,
	TOptional<T>& OutValue,
	FString& OutError,
	const TCHAR* Context)
{
	const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformTime::Seconds() < EndTime && !bShouldStop.Load())
	{
		TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
		if (Envelope.IsSet())
		{
			if (Envelope->MessageType == ExpectedType)
			{
				OutValue = FAngelscriptDebuggerTestClient::DeserializeMessage<T>(Envelope.GetValue());
				if (!OutValue.IsSet())
				{
					OutError = FString::Printf(TEXT("%s: failed to deserialize debugger message type %d."), Context, static_cast<int32>(ExpectedType));
					return false;
				}

				return true;
			}
		}
		else if (!Client.GetLastError().IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s: %s"), Context, *Client.GetLastError());
			return false;
		}

		FPlatformProcess::Sleep(0.001f);
	}

	OutError = bShouldStop.Load()
		? FString::Printf(TEXT("%s: aborted while waiting for debugger message type %d."), Context, static_cast<int32>(ExpectedType))
		: FString::Printf(TEXT("%s: timed out waiting for debugger message type %d."), Context, static_cast<int32>(ExpectedType));
	return false;
}

struct FPauseMonitorResult
{
	TOptional<FStoppedMessage> FirstStopMessage;
	TOptional<FAngelscriptCallStack> FirstCallstack;
	TOptional<FStoppedMessage> SecondStopMessage;
	TOptional<FAngelscriptCallStack> SecondCallstack;
	int32 ContinuedCount = 0;
	int32 UnexpectedStopCount = 0;
	bool bTimedOut = false;
	FString Error;
};

struct FControlPauseResult
{
	bool bObservedContinued = false;
	bool bSentPause = false;
	bool bTimedOut = false;
	FString Error;
};

static TFuture<FPauseMonitorResult> StartPauseMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TAtomic<bool>& bInvocationCompleted,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bMonitorReady, &bShouldStop, &bInvocationCompleted, TimeoutSeconds]() -> FPauseMonitorResult
		{
			FPauseMonitorResult Result;
			FAngelscriptDebuggerTestClient MonitorClient;
			ON_SCOPE_EXIT
			{
				MonitorClient.SendStopDebugging();
				MonitorClient.SendDisconnect();
				MonitorClient.Disconnect();
			};

			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Pause monitor failed to connect: %s"), *MonitorClient.GetLastError());
				bMonitorReady = true;
				return Result;
			}

			if (!HandshakeMonitorClient(MonitorClient, bShouldStop, TimeoutSeconds, Result.Error))
			{
				Result.bTimedOut = !bShouldStop.Load();
				bMonitorReady = true;
				return Result;
			}

			bMonitorReady = true;

			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::HasStopped, TimeoutSeconds, Result.FirstStopMessage, Result.Error, TEXT("Pause monitor failed to receive the initial stop")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (!MonitorClient.SendRequestCallStack() ||
				!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::CallStack, TimeoutSeconds, Result.FirstCallstack, Result.Error, TEXT("Pause monitor failed to receive the initial callstack")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (!MonitorClient.SendContinue())
			{
				Result.Error = FString::Printf(TEXT("Pause monitor failed to send Continue after the breakpoint stop: %s"), *MonitorClient.GetLastError());
				return Result;
			}

			TOptional<FEmptyMessage> ContinueMessage;
			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::HasContinued, TimeoutSeconds, ContinueMessage, Result.Error, TEXT("Pause monitor failed to receive HasContinued after resuming from the breakpoint stop")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}
			++Result.ContinuedCount;

			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::HasStopped, TimeoutSeconds, Result.SecondStopMessage, Result.Error, TEXT("Pause monitor failed to receive the pause stop")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (!MonitorClient.SendRequestCallStack() ||
				!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::CallStack, TimeoutSeconds, Result.SecondCallstack, Result.Error, TEXT("Pause monitor failed to receive the pause callstack")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (!MonitorClient.SendContinue())
			{
				Result.Error = FString::Printf(TEXT("Pause monitor failed to send Continue after the pause stop: %s"), *MonitorClient.GetLastError());
				return Result;
			}

			const double CompletionEnd = FPlatformTime::Seconds() + TimeoutSeconds;
			while (FPlatformTime::Seconds() < CompletionEnd && !bShouldStop.Load())
			{
				if (bInvocationCompleted.Load())
				{
					return Result;
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (Envelope.IsSet())
				{
					if (Envelope->MessageType == EDebugMessageType::HasStopped)
					{
						++Result.UnexpectedStopCount;
						MonitorClient.SendContinue();
					}
				}
				else if (!MonitorClient.GetLastError().IsEmpty())
				{
					Result.Error = FString::Printf(TEXT("Pause monitor failed while waiting for invocation completion: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			Result.bTimedOut = !bInvocationCompleted.Load();
			if (Result.bTimedOut && Result.Error.IsEmpty())
			{
				Result.Error = TEXT("Pause monitor timed out waiting for invocation completion.");
			}
			return Result;
		});
}

static TFuture<FControlPauseResult> StartControlPauseRequest(
	FAngelscriptDebuggerTestClient& Client,
	TAtomic<bool>& bShouldStop,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[&Client, &bShouldStop, TimeoutSeconds]() -> FControlPauseResult
		{
			FControlPauseResult Result;
			const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
			while (FPlatformTime::Seconds() < EndTime && !bShouldStop.Load())
			{
				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
				if (Envelope.IsSet())
				{
					if (Envelope->MessageType == EDebugMessageType::HasContinued)
					{
						Result.bObservedContinued = true;
						Result.bSentPause = Client.SendPause();
						if (!Result.bSentPause)
						{
							Result.Error = FString::Printf(TEXT("Control client failed to send Pause after receiving HasContinued: %s"), *Client.GetLastError());
						}
						return Result;
					}
				}
				else if (!Client.GetLastError().IsEmpty())
				{
					Result.Error = FString::Printf(TEXT("Control client failed while waiting for HasContinued: %s"), *Client.GetLastError());
					return Result;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			Result.bTimedOut = !bShouldStop.Load();
			if (Result.bTimedOut)
			{
				Result.Error = TEXT("Control client timed out waiting for HasContinued before sending Pause.");
			}
			return Result;
		});
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

	TEST_METHOD(PauseStopsAtNextScriptLine)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreatePauseFixture();
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(
			Fixture.Compile(Engine),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should compile the pause fixture")));

		TAtomic<bool> bMonitorReady(false);
		TAtomic<bool> bInvocationCompleted(false);
		TFuture<FPauseMonitorResult> MonitorFuture = StartPauseMonitor(
			Ctx.GetPort(),
			bMonitorReady,
			Ctx.bMonitorShouldStop,
			bInvocationCompleted,
			Ctx.GetDefaultTimeoutSeconds());
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (MonitorFuture.IsValid())
			{
				MonitorFuture.Wait();
			}
		};

		TAtomic<bool> bControlShouldStop(false);
		TFuture<FControlPauseResult> ControlPauseFuture;
		ON_SCOPE_EXIT
		{
			bControlShouldStop = true;
			if (ControlPauseFuture.IsValid())
			{
				ControlPauseFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(
			*TestRunner,
			Ctx.Session,
			bMonitorReady,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should bring the pause monitor up before execution"))));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("PauseReadyLine"));
		ASSERT_THAT(IsTrue(
			Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should set the pause-ready breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(
			*TestRunner,
			Ctx.Session,
			1,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should observe the breakpoint registration"))));

		ControlPauseFuture = StartControlPauseRequest(
			Ctx.Client,
			bControlShouldStop,
			Ctx.GetDefaultTimeoutSeconds());

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(
			*TestRunner,
			Ctx.Session,
			InvocationState,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should finish the invocation after resuming from the pause stop"))));

		bInvocationCompleted = true;
		Ctx.bMonitorShouldStop = true;
		bControlShouldStop = true;

		const FPauseMonitorResult MonitorResult = MonitorFuture.Get();
		const FControlPauseResult ControlPauseResult = ControlPauseFuture.Get();

		ASSERT_THAT(IsTrue(
			ControlPauseResult.Error.IsEmpty(),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should let the control client observe HasContinued without errors")));
		if (!ControlPauseResult.Error.IsEmpty())
		{
			TestRunner->AddError(ControlPauseResult.Error);
		}

		ASSERT_THAT(IsFalse(
			ControlPauseResult.bTimedOut,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should not time out while waiting for the control-client HasContinued")));

		ASSERT_THAT(IsTrue(
			ControlPauseResult.bObservedContinued,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should observe HasContinued on the control client before sending Pause")));

		ASSERT_THAT(IsTrue(
			ControlPauseResult.bSentPause,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should send Pause from the control client after HasContinued")));

		ASSERT_THAT(IsTrue(
			MonitorResult.Error.IsEmpty(),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should keep the pause monitor error-free")));
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(
			MonitorResult.bTimedOut,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should not time out while driving the pause test case")));

		ASSERT_THAT(IsTrue(
			MonitorResult.FirstStopMessage.IsSet(),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should capture the initial breakpoint stop")));

		ASSERT_THAT(IsTrue(
			MonitorResult.FirstCallstack.IsSet(),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should capture the initial callstack")));

		ASSERT_THAT(IsTrue(
			MonitorResult.SecondStopMessage.IsSet(),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should capture the pause stop")));

		ASSERT_THAT(IsTrue(
			MonitorResult.SecondCallstack.IsSet(),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should capture the pause callstack")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), MonitorResult.FirstStopMessage->Reason, TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should stop first because of the breakpoint")));
		ASSERT_THAT(IsTrue(
			MonitorResult.FirstCallstack->Frames.Num() > 0 && MonitorResult.FirstCallstack->Frames[0].Source.EndsWith(Fixture.Filename),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should report the fixture filename on the first stop")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("PauseReadyLine")), MonitorResult.FirstCallstack->Frames[0].LineNumber, TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should stop first at PauseReadyLine")));
		ASSERT_THAT(AreEqual(1, MonitorResult.ContinuedCount, TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should observe exactly one HasContinued after the initial resume")));
		ASSERT_THAT(AreEqual(FString(TEXT("pause")), MonitorResult.SecondStopMessage->Reason, TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should stop the second time because of Pause")));
		ASSERT_THAT(IsTrue(
			MonitorResult.SecondCallstack->Frames.Num() > 0 && MonitorResult.SecondCallstack->Frames[0].Source.EndsWith(Fixture.Filename),
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should report the fixture filename on the pause stop")));

		// UE 5.7: VM line-event granularity may pause at either the for-header (PauseForHeaderLine)
		// or the loop body (PauseLoopLine) depending on engine state and test ordering.
		const int32 ActualPauseLine = MonitorResult.SecondCallstack->Frames[0].LineNumber;
		const int32 ForHeaderLine = Fixture.GetLine(TEXT("PauseForHeaderLine"));
		const int32 LoopBodyLine = Fixture.GetLine(TEXT("PauseLoopLine"));
		ASSERT_THAT(IsTrue(
			ActualPauseLine == ForHeaderLine || ActualPauseLine == LoopBodyLine,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should stop the second time at either PauseForHeaderLine or PauseLoopLine")));

		ASSERT_THAT(AreEqual(
			0,
			MonitorResult.UnexpectedStopCount,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should not receive any unexpected extra HasStopped messages after resuming from Pause")));
		ASSERT_THAT(IsTrue(
			InvocationState->bSucceeded,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should finish the invocation successfully")));
		ASSERT_THAT(AreEqual(
			5001,
			InvocationState->Result,
			TEXT("Debugger.Pause.PauseStopsAtNextScriptLine should preserve the pause fixture return value")));
	}
};

#endif
