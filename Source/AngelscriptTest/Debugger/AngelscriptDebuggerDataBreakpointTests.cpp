#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptTestEngineHelper.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerDataBreakpointTests,
	"Angelscript.TestModule.Debugger.DataBreakpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FAngelscriptDebuggerScriptFixture CreateDataBreakpointFixture()
{
	FAngelscriptDebuggerScriptFixture Fixture;
	Fixture.ModuleName = TEXT("DebuggerDataBreakpointFixture");
	Fixture.GeneratedClassName = NAME_None;
	Fixture.EntryFunctionName = NAME_None;
	Fixture.EntryFunctionDeclaration = TEXT("int RunTestCase()");
	Fixture.Filename = TEXT("DebuggerDataBreakpointFixture.as");
	Fixture.ScriptSource = TEXT(R"AS(int Inner(int Value)
{
int StoredValue = 9;

int InnerValue = Value + StoredValue;
return InnerValue;
}

int RunTestCase()
{
int Result = Inner(4);
/*MARK:StepAfterCallLine*/ Result += 1;
/*MARK:PostMutationLine*/ int Snapshot = Result;
return Snapshot;
}
)AS");
	Fixture.LineMarkers.Add(TEXT("StepAfterCallLine"), 12);
	Fixture.LineMarkers.Add(TEXT("PostMutationLine"), 13);
	Fixture.EvalPaths.Add(TEXT("ResultPath"), TEXT("0:Result"));
	Fixture.bUseAnnotatedCompilation = false;
	return Fixture;
}

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

struct FDataBreakpointMonitorResult
{
	TOptional<FStoppedMessage> InitialStopMessage;
	TOptional<FAngelscriptVariable> ResultVariable;
	TOptional<FStoppedMessage> DataBreakpointStopMessage;
	TOptional<FAngelscriptClearDataBreakpoints> ClearDataBreakpoints;
	bool bTimedOut = false;
	FString Error;
};

static TFuture<FDataBreakpointMonitorResult> StartDataBreakpointMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	const FAngelscriptDebuggerScriptFixture& Fixture,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bMonitorReady, &bShouldStop, Fixture, TimeoutSeconds]() -> FDataBreakpointMonitorResult
		{
			FDataBreakpointMonitorResult Result;
			FAngelscriptDebuggerTestClient MonitorClient;
			ON_SCOPE_EXIT
			{
				MonitorClient.SendStopDebugging();
				MonitorClient.SendDisconnect();
				MonitorClient.Disconnect();
			};

			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Data breakpoint monitor failed to connect: %s"), *MonitorClient.GetLastError());
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

			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::HasStopped, TimeoutSeconds, Result.InitialStopMessage, Result.Error, TEXT("Data breakpoint monitor failed to receive the initial stop")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (!MonitorClient.SendRequestEvaluate(Fixture.GetEvalPath(TEXT("ResultPath"))))
			{
				Result.Error = FString::Printf(TEXT("Data breakpoint monitor failed to request evaluate for Result: %s"), *MonitorClient.GetLastError());
				return Result;
			}

			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::Evaluate, TimeoutSeconds, Result.ResultVariable, Result.Error, TEXT("Data breakpoint monitor failed to receive evaluate reply for Result")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (Result.ResultVariable->ValueAddress == 0 || Result.ResultVariable->ValueSize == 0)
			{
				Result.Error = FString::Printf(
					TEXT("Data breakpoint monitor received a non-monitorable evaluate reply for Result (Address=%llu, Size=%u)."),
					static_cast<unsigned long long>(Result.ResultVariable->ValueAddress),
					static_cast<uint32>(Result.ResultVariable->ValueSize));
				return Result;
			}

			FAngelscriptDataBreakpoints DataBreakpoints;
			FAngelscriptDataBreakpoint DataBreakpoint;
			DataBreakpoint.Id = 11;
			DataBreakpoint.Address = Result.ResultVariable->ValueAddress;
			DataBreakpoint.AddressSize = Result.ResultVariable->ValueSize;
			DataBreakpoint.HitCount = 1;
			DataBreakpoint.Name = TEXT("Result");
			DataBreakpoints.Breakpoints.Add(DataBreakpoint);
			if (!MonitorClient.SendTypedMessage(EDebugMessageType::SetDataBreakpoints, DataBreakpoints))
			{
				Result.Error = FString::Printf(TEXT("Data breakpoint monitor failed to send SetDataBreakpoints: %s"), *MonitorClient.GetLastError());
				return Result;
			}

			if (!MonitorClient.SendContinue())
			{
				Result.Error = FString::Printf(TEXT("Data breakpoint monitor failed to continue from the initial breakpoint stop: %s"), *MonitorClient.GetLastError());
				return Result;
			}

			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::HasStopped, TimeoutSeconds, Result.DataBreakpointStopMessage, Result.Error, TEXT("Data breakpoint monitor failed to receive the data breakpoint stop")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			if (!MonitorClient.SendContinue())
			{
				Result.Error = FString::Printf(TEXT("Data breakpoint monitor failed to continue from the data breakpoint stop: %s"), *MonitorClient.GetLastError());
				return Result;
			}

			if (!WaitForTypedMessageUntil(MonitorClient, bShouldStop, EDebugMessageType::ClearDataBreakpoints, TimeoutSeconds, Result.ClearDataBreakpoints, Result.Error, TEXT("Data breakpoint monitor failed to receive ClearDataBreakpoints")))
			{
				Result.bTimedOut = !bShouldStop.Load();
				return Result;
			}

			return Result;
		});
}

struct FPassiveBreakpointMonitorResult
{
	TArray<FStoppedMessage> StopMessages;
	bool bTimedOut = false;
	FString Error;
};

static TFuture<FPassiveBreakpointMonitorResult> StartPassiveBreakpointMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bMonitorReady, &bShouldStop, TimeoutSeconds]() -> FPassiveBreakpointMonitorResult
		{
			FPassiveBreakpointMonitorResult Result;
			FAngelscriptDebuggerTestClient MonitorClient;
			ON_SCOPE_EXIT
			{
				MonitorClient.SendStopDebugging();
				MonitorClient.SendDisconnect();
				MonitorClient.Disconnect();
			};

			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Passive breakpoint monitor failed to connect: %s"), *MonitorClient.GetLastError());
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

			const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
			while (FPlatformTime::Seconds() < EndTime && !bShouldStop.Load())
			{
				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (Envelope.IsSet())
				{
					if (Envelope->MessageType == EDebugMessageType::HasStopped)
					{
						TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(Envelope.GetValue());
						if (!StopMessage.IsSet())
						{
							Result.Error = TEXT("Passive breakpoint monitor failed to deserialize a HasStopped payload.");
							return Result;
						}

						Result.StopMessages.Add(StopMessage.GetValue());
						MonitorClient.SendContinue();
					}
				}
				else if (!MonitorClient.GetLastError().IsEmpty())
				{
					Result.Error = FString::Printf(TEXT("Passive breakpoint monitor failed while polling: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			if (!bShouldStop.Load() && FPlatformTime::Seconds() >= EndTime)
			{
				Result.bTimedOut = true;
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

	TEST_METHOD(LocalValueHitCount)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = CreateDataBreakpointFixture();
		FAngelscriptClearBreakpoints ClearBreakpoints;
		ClearBreakpoints.Filename = Fixture.Filename;
		ClearBreakpoints.ModuleName = Fixture.ModuleName.ToString();

		ON_SCOPE_EXIT
		{
			Ctx.Client.SendClearBreakpoints(ClearBreakpoints);
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should compile the data breakpoint fixture")));

		TAtomic<bool> bDataMonitorReady(false);
		TAtomic<bool> bDataMonitorShouldStop(false);
		TFuture<FDataBreakpointMonitorResult> DataMonitorFuture = StartDataBreakpointMonitor(
			Ctx.GetPort(),
			bDataMonitorReady,
			bDataMonitorShouldStop,
			Fixture,
			Ctx.GetDefaultTimeoutSeconds());
		ON_SCOPE_EXIT
		{
			bDataMonitorShouldStop = true;
			if (DataMonitorFuture.IsValid())
			{
				DataMonitorFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bDataMonitorReady, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should bring the data breakpoint monitor up before execution"))));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepAfterCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should set the step-after-call breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should observe the line breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> FirstInvocation = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);
		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, FirstInvocation, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should finish the first invocation after the monitor resumes execution"))));

		const FDataBreakpointMonitorResult DataMonitorResult = DataMonitorFuture.Get();

		ASSERT_THAT(IsTrue(DataMonitorResult.Error.IsEmpty(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should finish the data breakpoint monitor without transport errors")));
		if (!DataMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(DataMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(DataMonitorResult.bTimedOut, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should not time out while driving the data breakpoint sequence")));
		ASSERT_THAT(IsTrue(DataMonitorResult.InitialStopMessage.IsSet(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should capture the initial breakpoint stop")));
		ASSERT_THAT(IsTrue(DataMonitorResult.ResultVariable.IsSet(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should capture the evaluate reply for Result")));
		ASSERT_THAT(IsTrue(DataMonitorResult.DataBreakpointStopMessage.IsSet(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should capture the data breakpoint stop")));
		ASSERT_THAT(IsTrue(DataMonitorResult.ClearDataBreakpoints.IsSet(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should capture the ClearDataBreakpoints reply")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), DataMonitorResult.InitialStopMessage->Reason, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should stop initially because of the explicit line breakpoint")));
		ASSERT_THAT(AreEqual(FString(TEXT("13")), DataMonitorResult.ResultVariable->Value, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should evaluate Result before mutation")));
		ASSERT_THAT(IsTrue(DataMonitorResult.ResultVariable->ValueAddress != 0, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should expose a non-zero ValueAddress for Result")));
		ASSERT_THAT(IsTrue(DataMonitorResult.ResultVariable->ValueSize > 0, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should expose a positive ValueSize for Result")));
		ASSERT_THAT(AreEqual(FString(TEXT("exception")), DataMonitorResult.DataBreakpointStopMessage->Reason, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should stop because of an exception when the data breakpoint fires")));
		ASSERT_THAT(IsTrue(DataMonitorResult.DataBreakpointStopMessage->Text.Contains(TEXT("Data breakpoint (Result) triggered!")), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should report the data breakpoint name in the stop text")));
		ASSERT_THAT(AreEqual(1, DataMonitorResult.ClearDataBreakpoints->Ids.Num(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should clear exactly one data breakpoint")));
		ASSERT_THAT(AreEqual(11, DataMonitorResult.ClearDataBreakpoints->Ids[0], TEXT("Debugger.DataBreakpoint.LocalValueHitCount should clear the hit-count-limited data breakpoint id")));
		ASSERT_THAT(IsTrue(FirstInvocation->bSucceeded, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should complete the first invocation successfully")));
		ASSERT_THAT(AreEqual(14, FirstInvocation->Result, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should preserve the first invocation result")));

		ASSERT_THAT(IsTrue(Ctx.Client.SendClearBreakpoints(ClearBreakpoints), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should clear the line breakpoint before the second invocation")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 0, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should observe the breakpoint removal before the second invocation"))));

		Ctx.Client.DrainPendingMessages();

		TAtomic<bool> bPassiveMonitorReady(false);
		TAtomic<bool> bPassiveMonitorShouldStop(false);
		TFuture<FPassiveBreakpointMonitorResult> PassiveMonitorFuture = StartPassiveBreakpointMonitor(
			Ctx.GetPort(),
			bPassiveMonitorReady,
			bPassiveMonitorShouldStop,
			Ctx.GetDefaultTimeoutSeconds());
		ON_SCOPE_EXIT
		{
			bPassiveMonitorShouldStop = true;
			if (PassiveMonitorFuture.IsValid())
			{
				PassiveMonitorFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bPassiveMonitorReady, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should bring the passive monitor up before the second invocation"))));

		const TSharedRef<FAsyncModuleInvocationState> SecondInvocation = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);
		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, SecondInvocation, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should finish the second invocation without any debugger stop"))));

		bPassiveMonitorShouldStop = true;
		const FPassiveBreakpointMonitorResult PassiveMonitorResult = PassiveMonitorFuture.Get();

		ASSERT_THAT(IsTrue(PassiveMonitorResult.Error.IsEmpty(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should keep the passive monitor error-free on the second invocation")));
		if (!PassiveMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(PassiveMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(PassiveMonitorResult.bTimedOut, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should not time out while passively watching the second invocation")));

		ASSERT_THAT(AreEqual(0, PassiveMonitorResult.StopMessages.Num(), TEXT("Debugger.DataBreakpoint.LocalValueHitCount should not stop again after the hit-count-limited data breakpoint auto-clears")));
		ASSERT_THAT(IsTrue(SecondInvocation->bSucceeded, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should complete the second invocation successfully")));
		ASSERT_THAT(AreEqual(14, SecondInvocation->Result, TEXT("Debugger.DataBreakpoint.LocalValueHitCount should preserve the second invocation result")));
	}
};

#endif
