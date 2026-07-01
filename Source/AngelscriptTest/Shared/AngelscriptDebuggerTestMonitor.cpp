#include "AngelscriptDebuggerTestMonitor.h"

#include "Core/AngelscriptEngine.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

// =========================================================================
// Async Module Invocation
// =========================================================================

TSharedRef<FAsyncModuleInvocationState> DispatchModuleInvocation(
	FAngelscriptEngine& Engine,
	const FString& Filename,
	FName ModuleName,
	const FString& Declaration)
{
	TSharedRef<FAsyncModuleInvocationState> State = MakeShared<FAsyncModuleInvocationState>();

	AsyncTask(ENamedThreads::GameThread, [&Engine, Filename, ModuleName, Declaration, State]()
	{
		int32 InvocationResult = 0;
		State->bSucceeded = ExecuteIntFunction(&Engine, Filename, ModuleName, Declaration, InvocationResult);
		State->Result = InvocationResult;
		State->bCompleted = true;
	});

	return State;
}

TSharedRef<FAsyncModuleInvocationState> DispatchModuleInvocation(
	FAngelscriptEngine& Engine,
	UObject* WorldContext,
	const FString& Filename,
	FName ModuleName,
	const FString& Declaration)
{
	TSharedRef<FAsyncModuleInvocationState> State = MakeShared<FAsyncModuleInvocationState>();

	AsyncTask(ENamedThreads::GameThread, [&Engine, WorldContext, Filename, ModuleName, Declaration, State]()
	{
		if (WorldContext == nullptr)
		{
			State->Error = TEXT("WorldContext was null.");
			State->bCompleted = true;
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Filename, ModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			State->Error = FString::Printf(TEXT("Module lookup failed for '%s'."), *ModuleName.ToString());
			State->bCompleted = true;
			return;
		}

		asIScriptModule* Module = ModuleDesc->ScriptModule;
		FTCHARToUTF8 DeclUtf8(*Declaration);
		asIScriptFunction* Function = Module->GetFunctionByDecl(DeclUtf8.Get());
		if (Function == nullptr)
		{
			State->Error = FString::Printf(TEXT("Function '%s' not found in module '%s'."), *Declaration, *ModuleName.ToString());
			State->bCompleted = true;
			return;
		}

		FAngelscriptEngineScope EngineScope(Engine, WorldContext);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			State->Error = TEXT("Failed to create script context.");
			State->bCompleted = true;
			return;
		}

		ON_SCOPE_EXIT { Context->Release(); };

		if (Context->Prepare(Function) != asSUCCESS)
		{
			State->Error = FString::Printf(TEXT("Prepare failed for '%s'."), *Declaration);
			State->bCompleted = true;
			return;
		}

		if (Context->Execute() != asEXECUTION_FINISHED)
		{
			State->Error = FString::Printf(TEXT("Execute failed for '%s'."), *Declaration);
			State->bCompleted = true;
			return;
		}

		State->Result = static_cast<int32>(Context->GetReturnDWord());
		State->bSucceeded = true;
		State->bCompleted = true;
	});

	return State;
}

TSharedRef<FAsyncModuleInvocationState> DispatchModuleInvocationWithIntArg(
	FAngelscriptEngine& Engine,
	const FString& Filename,
	FName ModuleName,
	const FString& Declaration,
	int32 ArgumentValue)
{
	TSharedRef<FAsyncModuleInvocationState> State = MakeShared<FAsyncModuleInvocationState>();

	AsyncTask(ENamedThreads::GameThread, [&Engine, Filename, ModuleName, Declaration, ArgumentValue, State]()
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Filename, ModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			State->Error = FString::Printf(TEXT("Module lookup failed for '%s'."), *ModuleName.ToString());
			State->bCompleted = true;
			return;
		}

		asIScriptModule* Module = ModuleDesc->ScriptModule;
		FTCHARToUTF8 DeclUtf8(*Declaration);
		asIScriptFunction* Function = Module->GetFunctionByDecl(DeclUtf8.Get());
		if (Function == nullptr)
		{
			State->Error = FString::Printf(TEXT("Function '%s' not found in module '%s'."), *Declaration, *ModuleName.ToString());
			State->bCompleted = true;
			return;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			State->Error = TEXT("Failed to create script context.");
			State->bCompleted = true;
			return;
		}

		ON_SCOPE_EXIT { Context->Release(); };

		if (Context->Prepare(Function) != asSUCCESS)
		{
			State->Error = FString::Printf(TEXT("Prepare failed for '%s'."), *Declaration);
			State->bCompleted = true;
			return;
		}

		Context->SetArgDWord(0, static_cast<asDWORD>(ArgumentValue));

		if (Context->Execute() != asEXECUTION_FINISHED)
		{
			State->Error = FString::Printf(TEXT("Execute failed for '%s'."), *Declaration);
			State->bCompleted = true;
			return;
		}

		State->bSucceeded = true;
		State->Result = static_cast<int32>(Context->GetReturnDWord());
		State->bCompleted = true;
	});

	return State;
}

bool WaitForInvocationCompletion(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	const TSharedRef<FAsyncModuleInvocationState>& InvocationState,
	const TCHAR* Context)
{
	const bool bCompleted = Session.PumpUntil(
		[&InvocationState]()
		{
			return InvocationState->bCompleted.Load();
		},
		Session.GetDefaultTimeoutSeconds());

	return Test.TestTrue(Context, bCompleted);
}

// =========================================================================
// Monitor Handshake Helper
// =========================================================================

bool HandshakeMonitorClient(
	FAngelscriptDebuggerTestClient& Client,
	TAtomic<bool>& bShouldStop,
	int32 AdapterVersion,
	float TimeoutSeconds,
	FString& OutError)
{
	const double HandshakeEnd = FPlatformTime::Seconds() + TimeoutSeconds;
	bool bSentStart = false;
	bool bReceivedVersion = false;

	while (FPlatformTime::Seconds() < HandshakeEnd && !bShouldStop.Load())
	{
		if (!bSentStart)
		{
			bSentStart = Client.SendStartDebugging(AdapterVersion);
		}

		if (bSentStart && !bReceivedVersion)
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
			if (Envelope.IsSet() && Envelope->MessageType == EDebugMessageType::DebugServerVersion)
			{
				bReceivedVersion = true;
				break;
			}
		}

		FPlatformProcess::Sleep(0.001f);
	}

	if (!bReceivedVersion)
	{
		OutError = bShouldStop.Load()
			? TEXT("Monitor handshake aborted before receiving DebugServerVersion.")
			: TEXT("Monitor timed out waiting for DebugServerVersion handshake.");
		return false;
	}

	return true;
}

bool WaitForMonitorReady(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	TAtomic<bool>& bMonitorReady,
	const TCHAR* Context)
{
	const bool bReady = Session.PumpUntil(
		[&bMonitorReady]()
		{
			return bMonitorReady.Load();
		},
		Session.GetDefaultTimeoutSeconds());

	return Test.TestTrue(Context, bReady);
}

// =========================================================================
// Breakpoint Monitor
// =========================================================================

TFuture<FBreakpointMonitorResult> StartBreakpointMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	const FBreakpointMonitorConfig& Config)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bMonitorReady, &bShouldStop, Config]() -> FBreakpointMonitorResult
		{
			FBreakpointMonitorResult Result;

			FAngelscriptDebuggerTestClient MonitorClient;
			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Breakpoint monitor failed to connect: %s"), *MonitorClient.GetLastError());
				bMonitorReady = true;
				return Result;
			}

			if (!HandshakeMonitorClient(MonitorClient, bShouldStop, Config.TimeoutSeconds, Result.Error))
			{
				bMonitorReady = true;
				return Result;
			}

			bMonitorReady = true;

			int32 StopsHandled = 0;
			const double MonitorEnd = FPlatformTime::Seconds() + Config.TimeoutSeconds;

			while (FPlatformTime::Seconds() < MonitorEnd && !bShouldStop.Load())
			{
				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (!Envelope.IsSet())
				{
					FPlatformProcess::Sleep(0.001f);
					continue;
				}

				if (Envelope->MessageType == EDebugMessageType::HasStopped)
				{
					Result.StopEnvelopes.Add(Envelope.GetValue());

					if (Config.bRequestCallstack)
					{
						if (MonitorClient.SendRequestCallStack())
						{
							const double CallstackEnd = FPlatformTime::Seconds() + 10.0;
							while (FPlatformTime::Seconds() < CallstackEnd)
							{
								TOptional<FAngelscriptDebugMessageEnvelope> CsEnvelope = MonitorClient.ReceiveEnvelope();
								if (CsEnvelope.IsSet() && CsEnvelope->MessageType == EDebugMessageType::CallStack)
								{
									Result.CapturedCallstack = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptCallStack>(CsEnvelope.GetValue());
									break;
								}
								FPlatformProcess::Sleep(0.001f);
							}
						}
					}

					if (Config.bSendContinueOnStop)
					{
						MonitorClient.SendContinue();
					}

					StopsHandled++;
					if (StopsHandled >= Config.MaxStopsToHandle)
					{
						break;
					}
				}
			}

			if (StopsHandled == 0 && !bShouldStop.Load() && FPlatformTime::Seconds() >= MonitorEnd)
			{
				Result.bTimedOut = true;
			}

			MonitorClient.SendStopDebugging();
			MonitorClient.SendDisconnect();
			MonitorClient.Disconnect();
			return Result;
		});
}

bool StartAndWaitForBreakpointMonitorReady(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	const FBreakpointMonitorConfig& Config,
	TFuture<FBreakpointMonitorResult>& OutFuture)
{
	OutFuture = StartBreakpointMonitor(Port, bMonitorReady, bShouldStop, Config);

	const bool bReady = Session.PumpUntil(
		[&bMonitorReady]()
		{
			return bMonitorReady.Load();
		},
		Session.GetDefaultTimeoutSeconds());

	if (!Test.TestTrue(TEXT("Breakpoint monitor should become ready within the timeout"), bReady))
	{
		bShouldStop = true;
		return false;
	}

	return true;
}

// =========================================================================
// Step Monitor
// =========================================================================

TFuture<FStepMonitorResult> StartStepMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TArray<FStepMonitorPhase> Phases,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bMonitorReady, &bShouldStop, Phases = MoveTemp(Phases), TimeoutSeconds]() -> FStepMonitorResult
		{
			FStepMonitorResult Result;
			FAngelscriptDebuggerTestClient MonitorClient;
			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Step monitor failed to connect: %s"), *MonitorClient.GetLastError());
				bMonitorReady = true;
				return Result;
			}

			if (!HandshakeMonitorClient(MonitorClient, bShouldStop, TimeoutSeconds, Result.Error))
			{
				bMonitorReady = true;
				MonitorClient.SendStopDebugging();
				MonitorClient.SendDisconnect();
				MonitorClient.Disconnect();
				return Result;
			}

			bMonitorReady = true;

			int32 StopsHandled = 0;
			const double MonitorEnd = FPlatformTime::Seconds() + TimeoutSeconds;
			while (FPlatformTime::Seconds() < MonitorEnd && !bShouldStop.Load())
			{
				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (!Envelope.IsSet())
				{
					FPlatformProcess::Sleep(0.001f);
					continue;
				}

				if (Envelope->MessageType != EDebugMessageType::HasStopped)
				{
					continue;
				}

				if (StopsHandled >= Phases.Num())
				{
					MonitorClient.SendContinue();
					Result.Error = FString::Printf(TEXT("Received unexpected stop #%d beyond configured phases."), StopsHandled + 1);
					break;
				}

				FStepMonitorStop Stop;
				Stop.StopEnvelope = Envelope.GetValue();

				const FStepMonitorPhase& Phase = Phases[StopsHandled];
				if (Phase.bRequestCallstack && MonitorClient.SendRequestCallStack())
				{
					const double CallstackEnd = FPlatformTime::Seconds() + 10.0;
					while (FPlatformTime::Seconds() < CallstackEnd)
					{
						TOptional<FAngelscriptDebugMessageEnvelope> CallstackEnvelope = MonitorClient.ReceiveEnvelope();
						if (CallstackEnvelope.IsSet() && CallstackEnvelope->MessageType == EDebugMessageType::CallStack)
						{
							Stop.Callstack = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptCallStack>(CallstackEnvelope.GetValue());
							break;
						}
						FPlatformProcess::Sleep(0.001f);
					}
				}

				Result.Stops.Add(MoveTemp(Stop));

				switch (Phase.Action)
				{
				case EStepMonitorAction::Continue:
					MonitorClient.SendContinue();
					break;
				case EStepMonitorAction::StepIn:
					MonitorClient.SendStepIn();
					break;
				case EStepMonitorAction::StepOver:
					MonitorClient.SendStepOver();
					break;
				case EStepMonitorAction::StepOut:
					MonitorClient.SendStepOut();
					break;
				}

				++StopsHandled;
				if (StopsHandled >= Phases.Num())
				{
					break;
				}
			}

			if (StopsHandled == 0 && !bShouldStop.Load() && FPlatformTime::Seconds() >= MonitorEnd)
			{
				Result.bTimedOut = true;
			}

			MonitorClient.SendStopDebugging();
			MonitorClient.SendDisconnect();
			MonitorClient.Disconnect();
			return Result;
		});
}

bool StartAndWaitForStepMonitorReady(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TArray<FStepMonitorPhase> Phases,
	TFuture<FStepMonitorResult>& OutFuture)
{
	OutFuture = StartStepMonitor(Port, bMonitorReady, bShouldStop, MoveTemp(Phases), Session.GetDefaultTimeoutSeconds());

	const bool bReady = Session.PumpUntil(
		[&bMonitorReady]()
		{
			return bMonitorReady.Load();
		},
		Session.GetDefaultTimeoutSeconds());

	if (!Test.TestTrue(TEXT("Step monitor should become ready within the timeout"), bReady))
	{
		bShouldStop = true;
		return false;
	}

	return true;
}

// =========================================================================
// Lifecycle No-Stop Monitor
// =========================================================================

TFuture<FLifecycleNoStopMonitorResult> StartLifecycleNoStopMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TAtomic<bool>& bInvocationCompleted,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bMonitorReady, &bShouldStop, &bInvocationCompleted, TimeoutSeconds]() -> FLifecycleNoStopMonitorResult
		{
			FLifecycleNoStopMonitorResult Result;
			FAngelscriptDebuggerTestClient MonitorClient;
			ON_SCOPE_EXIT
			{
				MonitorClient.SendStopDebugging();
				MonitorClient.SendDisconnect();
				MonitorClient.Disconnect();
			};

			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Lifecycle monitor failed to connect: %s"), *MonitorClient.GetLastError());
				bMonitorReady = true;
				return Result;
			}

			const double HandshakeEnd = FPlatformTime::Seconds() + TimeoutSeconds;
			bool bSentStart = false;
			while (FPlatformTime::Seconds() < HandshakeEnd && !bShouldStop.Load())
			{
				if (!bSentStart)
				{
					bSentStart = MonitorClient.SendStartDebugging(2);
					if (!bSentStart)
					{
						Result.Error = FString::Printf(TEXT("Lifecycle monitor failed to send StartDebugging: %s"), *MonitorClient.GetLastError());
						bMonitorReady = true;
						return Result;
					}
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (Envelope.IsSet())
				{
					if (Envelope->MessageType == EDebugMessageType::DebugServerVersion)
					{
						Result.bReceivedVersion = true;
						break;
					}

					if (Envelope->MessageType != EDebugMessageType::PingAlive)
					{
						Result.ResidualMessagesAfterHandshake.Add(Envelope.GetValue());
					}
				}
				else if (!MonitorClient.GetLastError().IsEmpty())
				{
					Result.Error = FString::Printf(TEXT("Lifecycle monitor failed during handshake: %s"), *MonitorClient.GetLastError());
					bMonitorReady = true;
					return Result;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			if (!Result.bReceivedVersion)
			{
				Result.bTimedOut = !bShouldStop.Load();
				Result.Error = Result.bTimedOut
					? TEXT("Lifecycle monitor timed out waiting for DebugServerVersion.")
					: TEXT("Lifecycle monitor handshake aborted before receiving DebugServerVersion.");
				bMonitorReady = true;
				return Result;
			}

			AppendNonPingMessages(Result.ResidualMessagesAfterHandshake, MonitorClient.DrainPendingMessages());
			bMonitorReady = true;

			const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
			while (FPlatformTime::Seconds() < EndTime && !bShouldStop.Load())
			{
				if (bInvocationCompleted.Load())
				{
					AppendNonPingMessages(Result.ResidualMessagesAfterInvocation, MonitorClient.DrainPendingMessages());
					return Result;
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (Envelope.IsSet())
				{
					if (Envelope->MessageType == EDebugMessageType::HasStopped)
					{
						++Result.UnexpectedStopCount;
						if (!MonitorClient.SendContinue())
						{
							Result.Error = FString::Printf(TEXT("Lifecycle monitor failed to send Continue after an unexpected stop: %s"), *MonitorClient.GetLastError());
							return Result;
						}

						TOptional<FEmptyMessage> ContinuedMessage = MonitorClient.WaitForTypedMessage<FEmptyMessage>(EDebugMessageType::HasContinued, TimeoutSeconds);
						if (!ContinuedMessage.IsSet())
						{
							Result.Error = FString::Printf(TEXT("Lifecycle monitor failed to receive HasContinued after an unexpected stop: %s"), *MonitorClient.GetLastError());
							Result.bTimedOut = !bShouldStop.Load();
							return Result;
						}

						++Result.ContinuedCount;
					}
					else if (Envelope->MessageType != EDebugMessageType::PingAlive)
					{
						Result.ResidualMessagesAfterInvocation.Add(Envelope.GetValue());
					}
				}
				else if (!MonitorClient.GetLastError().IsEmpty())
				{
					Result.Error = FString::Printf(TEXT("Lifecycle monitor failed while waiting for invocation completion: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			Result.bTimedOut = !bInvocationCompleted.Load() && !bShouldStop.Load();
			if (Result.bTimedOut && Result.Error.IsEmpty())
			{
				Result.Error = TEXT("Lifecycle monitor timed out waiting for invocation completion.");
			}
			AppendNonPingMessages(Result.ResidualMessagesAfterInvocation, MonitorClient.DrainPendingMessages());
			return Result;
		});
}

// =========================================================================
// Additional Debugger Client Monitor
// =========================================================================

TFuture<FAdditionalDebuggerMonitorResult> StartAdditionalDebuggerClientMonitor(
	FAngelscriptDebugServer& DebugServer,
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bHandshakeSucceeded,
	TAtomic<bool>& bAbortMonitor,
	TAtomic<bool>& bInvocationCompleted,
	TAtomic<int32>& OutMinObservedBreakpointCount,
	float TimeoutSeconds)
{
	return Async(EAsyncExecution::ThreadPool,
		[&DebugServer, Port, &bMonitorReady, &bHandshakeSucceeded, &bAbortMonitor, &bInvocationCompleted, &OutMinObservedBreakpointCount, TimeoutSeconds]() -> FAdditionalDebuggerMonitorResult
		{
			FAdditionalDebuggerMonitorResult Result;
			FAngelscriptDebuggerTestClient MonitorClient;
			ON_SCOPE_EXIT
			{
				MonitorClient.SendDisconnect();
				MonitorClient.Disconnect();
			};

			if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
			{
				Result.Error = FString::Printf(TEXT("Additional debugger client failed to connect: %s"), *MonitorClient.GetLastError());
				bMonitorReady = true;
				return Result;
			}

			const double HandshakeEnd = FPlatformTime::Seconds() + TimeoutSeconds;
			bool bSentStartDebugging = false;
			while (FPlatformTime::Seconds() < HandshakeEnd && !bAbortMonitor.Load())
			{
				OutMinObservedBreakpointCount.Store(FMath::Min(OutMinObservedBreakpointCount.Load(), DebugServer.BreakpointCount));

				if (!bSentStartDebugging)
				{
					bSentStartDebugging = MonitorClient.SendStartDebugging(2);
					if (!bSentStartDebugging)
					{
						Result.Error = FString::Printf(TEXT("Additional debugger client failed to send StartDebugging: %s"), *MonitorClient.GetLastError());
						bMonitorReady = true;
						return Result;
					}
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (Envelope.IsSet() && Envelope->MessageType == EDebugMessageType::DebugServerVersion)
				{
					bHandshakeSucceeded = true;
					bMonitorReady = true;
					break;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			if (!bHandshakeSucceeded.Load())
			{
				Result.Error = bAbortMonitor.Load()
					? TEXT("Additional debugger client monitor aborted before completing StartDebugging.")
					: FString::Printf(TEXT("Additional debugger client timed out waiting for DebugServerVersion after %.3f seconds."), TimeoutSeconds);
				Result.bTimedOut = !bAbortMonitor.Load();
				bMonitorReady = true;
				return Result;
			}

			const double MonitorEnd = FPlatformTime::Seconds() + TimeoutSeconds;
			while (FPlatformTime::Seconds() < MonitorEnd && !bAbortMonitor.Load())
			{
				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
				if (Envelope.IsSet())
				{
					if (Envelope->MessageType == EDebugMessageType::HasStopped)
					{
						Result.StopEnvelopes.Add(Envelope.GetValue());
						Result.StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(Envelope.GetValue());
						if (!Result.StopMessage.IsSet())
						{
							Result.Error = TEXT("Additional debugger client failed to deserialize the HasStopped payload.");
							return Result;
						}

						if (!MonitorClient.SendRequestCallStack())
						{
							Result.Error = FString::Printf(TEXT("Additional debugger client failed to request callstack: %s"), *MonitorClient.GetLastError());
							return Result;
						}

						Result.CapturedCallstack = MonitorClient.WaitForTypedMessage<FAngelscriptCallStack>(EDebugMessageType::CallStack, TimeoutSeconds);
						if (!Result.CapturedCallstack.IsSet())
						{
							Result.Error = FString::Printf(TEXT("Additional debugger client failed to receive callstack: %s"), *MonitorClient.GetLastError());
							Result.bTimedOut = true;
							return Result;
						}

						if (!MonitorClient.SendContinue())
						{
							Result.Error = FString::Printf(TEXT("Additional debugger client failed to send Continue: %s"), *MonitorClient.GetLastError());
							return Result;
						}

						const TOptional<FEmptyMessage> ContinuedMessage = MonitorClient.WaitForTypedMessage<FEmptyMessage>(EDebugMessageType::HasContinued, TimeoutSeconds);
						if (!ContinuedMessage.IsSet())
						{
							Result.Error = FString::Printf(TEXT("Additional debugger client failed to receive HasContinued: %s"), *MonitorClient.GetLastError());
							Result.bTimedOut = true;
							return Result;
						}

						Result.ContinuedCount = 1;
						return Result;
					}
				}
				else if (!MonitorClient.GetLastError().IsEmpty())
				{
					Result.Error = FString::Printf(TEXT("Additional debugger client monitor failed while waiting for HasStopped: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				if (bInvocationCompleted.Load())
				{
					Result.bCompletedWithoutStop = true;
					return Result;
				}

				FPlatformProcess::Sleep(0.001f);
			}

			if (!bAbortMonitor.Load())
			{
				Result.bTimedOut = true;
				Result.Error = FString::Printf(TEXT("Additional debugger client monitor timed out after %.3f seconds waiting for HasStopped."), TimeoutSeconds);
			}

			return Result;
		});
}

// =========================================================================
// Protocol Message Utilities
// =========================================================================

void AppendNonPingMessages(
	TArray<FAngelscriptDebugMessageEnvelope>& OutMessages,
	const TArray<FAngelscriptDebugMessageEnvelope>& InMessages)
{
	for (const FAngelscriptDebugMessageEnvelope& Envelope : InMessages)
	{
		if (Envelope.MessageType != EDebugMessageType::PingAlive)
		{
			OutMessages.Add(Envelope);
		}
	}
}

int32 CountMessagesByType(const FSingleClientDebuggerTranscript& Transcript, EDebugMessageType Type)
{
	int32 Count = 0;
	for (const FAngelscriptDebugMessageEnvelope& Envelope : Transcript.ReceivedMessages)
	{
		if (Envelope.MessageType == Type)
		{
			++Count;
		}
	}
	return Count;
}


#endif // WITH_ANGELSCRIPT_UNITTESTS
