#pragma once

// =============================================================================
// AngelscriptDebuggerTestMonitor.h
//
// Shared monitor patterns extracted from Debugger test _Private namespaces.
// Each monitor runs on a ThreadPool thread, connects as a second DAP client,
// and interacts with the DebugServer while the GameThread is blocked in
// PauseExecution().
//
// Contents:
//   - Async invocation: dispatch script execution on GameThread + wait
//   - Monitor handshake: shared connect + StartDebugging + version exchange
//   - Breakpoint monitor: wait for HasStopped, optionally request callstack
//   - Step monitor: phased StepIn/Over/Out sequences with callstack capture
// =============================================================================

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Misc/AutomationTest.h"

#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerTestSession.h"
#include "AngelscriptTestEngineHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

// =========================================================================
// Async Module Invocation
// =========================================================================

struct FAsyncModuleInvocationState : public TSharedFromThis<FAsyncModuleInvocationState>
{
	TAtomic<bool> bCompleted{false};
	bool bSucceeded = false;
	int32 Result = 0;
	FString Error;
};

ANGELSCRIPTTEST_API TSharedRef<FAsyncModuleInvocationState> DispatchModuleInvocation(
	FAngelscriptEngine& Engine,
	const FString& Filename,
	FName ModuleName,
	const FString& Declaration);

/** Overload with explicit world context for FAngelscriptEngineScope. */
ANGELSCRIPTTEST_API TSharedRef<FAsyncModuleInvocationState> DispatchModuleInvocation(
	FAngelscriptEngine& Engine,
	UObject* WorldContext,
	const FString& Filename,
	FName ModuleName,
	const FString& Declaration);

/** Dispatch a script function that takes a single int32 argument. */
ANGELSCRIPTTEST_API TSharedRef<FAsyncModuleInvocationState> DispatchModuleInvocationWithIntArg(
	FAngelscriptEngine& Engine,
	const FString& Filename,
	FName ModuleName,
	const FString& Declaration,
	int32 ArgumentValue);

ANGELSCRIPTTEST_API bool WaitForInvocationCompletion(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	const TSharedRef<FAsyncModuleInvocationState>& InvocationState,
	const TCHAR* Context);

// =========================================================================
// Monitor Handshake Helper
// =========================================================================

/**
 * Shared handshake logic for all monitor threads: connect, send
 * StartDebugging, wait for DebugServerVersion.
 *
 * @return true if handshake completed successfully.
 */
ANGELSCRIPTTEST_API bool HandshakeMonitorClient(
	FAngelscriptDebuggerTestClient& Client,
	TAtomic<bool>& bShouldStop,
	int32 AdapterVersion,
	float TimeoutSeconds,
	FString& OutError);

/** Convenience overload defaulting to AdapterVersion=2. */
inline bool HandshakeMonitorClient(
	FAngelscriptDebuggerTestClient& Client,
	TAtomic<bool>& bShouldStop,
	float TimeoutSeconds,
	FString& OutError)
{
	return HandshakeMonitorClient(Client, bShouldStop, 2, TimeoutSeconds, OutError);
}

/**
 * Pump the session until bMonitorReady becomes true (or timeout).
 */
ANGELSCRIPTTEST_API bool WaitForMonitorReady(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	TAtomic<bool>& bMonitorReady,
	const TCHAR* Context);

// =========================================================================
// Breakpoint Monitor
// =========================================================================

struct FBreakpointMonitorConfig
{
	bool bRequestCallstack = false;
	bool bSendContinueOnStop = true;
	int32 MaxStopsToHandle = 1;
	float TimeoutSeconds = kDefaultDebuggerTestTimeoutSeconds;
};

struct FBreakpointMonitorResult
{
	TArray<FAngelscriptDebugMessageEnvelope> StopEnvelopes;
	TOptional<FAngelscriptCallStack> CapturedCallstack;
	bool bTimedOut = false;
	FString Error;
};

ANGELSCRIPTTEST_API TFuture<FBreakpointMonitorResult> StartBreakpointMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	const FBreakpointMonitorConfig& Config);

ANGELSCRIPTTEST_API bool StartAndWaitForBreakpointMonitorReady(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	const FBreakpointMonitorConfig& Config,
	TFuture<FBreakpointMonitorResult>& OutFuture);

// =========================================================================
// Step Monitor
// =========================================================================

enum class EStepMonitorAction : uint8
{
	Continue,
	StepIn,
	StepOver,
	StepOut,
};

struct FStepMonitorPhase
{
	EStepMonitorAction Action = EStepMonitorAction::Continue;
	bool bRequestCallstack = true;
};

struct FStepMonitorStop
{
	FAngelscriptDebugMessageEnvelope StopEnvelope;
	TOptional<FAngelscriptCallStack> Callstack;
};

struct FStepMonitorResult
{
	TArray<FStepMonitorStop> Stops;
	bool bTimedOut = false;
	FString Error;
};

ANGELSCRIPTTEST_API TFuture<FStepMonitorResult> StartStepMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TArray<FStepMonitorPhase> Phases,
	float TimeoutSeconds);

ANGELSCRIPTTEST_API bool StartAndWaitForStepMonitorReady(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TArray<FStepMonitorPhase> Phases,
	TFuture<FStepMonitorResult>& OutFuture);

// =========================================================================
// Lifecycle No-Stop Monitor
// =========================================================================

struct FLifecycleNoStopMonitorResult
{
	bool bReceivedVersion = false;
	TArray<FAngelscriptDebugMessageEnvelope> ResidualMessagesAfterHandshake;
	TArray<FAngelscriptDebugMessageEnvelope> ResidualMessagesAfterInvocation;
	int32 UnexpectedStopCount = 0;
	int32 ContinuedCount = 0;
	bool bTimedOut = false;
	FString Error;
};

ANGELSCRIPTTEST_API TFuture<FLifecycleNoStopMonitorResult> StartLifecycleNoStopMonitor(
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bShouldStop,
	TAtomic<bool>& bInvocationCompleted,
	float TimeoutSeconds);

// =========================================================================
// Additional Debugger Client Monitor
// =========================================================================

struct FAdditionalDebuggerMonitorResult
{
	TArray<FAngelscriptDebugMessageEnvelope> StopEnvelopes;
	TOptional<FStoppedMessage> StopMessage;
	TOptional<FAngelscriptCallStack> CapturedCallstack;
	int32 ContinuedCount = 0;
	bool bTimedOut = false;
	bool bCompletedWithoutStop = false;
	FString Error;
};

ANGELSCRIPTTEST_API TFuture<FAdditionalDebuggerMonitorResult> StartAdditionalDebuggerClientMonitor(
	FAngelscriptDebugServer& DebugServer,
	int32 Port,
	TAtomic<bool>& bMonitorReady,
	TAtomic<bool>& bHandshakeSucceeded,
	TAtomic<bool>& bAbortMonitor,
	TAtomic<bool>& bInvocationCompleted,
	TAtomic<int32>& OutMinObservedBreakpointCount,
	float TimeoutSeconds);

// =========================================================================
// Protocol Message Utilities
// =========================================================================

ANGELSCRIPTTEST_API void AppendNonPingMessages(
	TArray<FAngelscriptDebugMessageEnvelope>& OutMessages,
	const TArray<FAngelscriptDebugMessageEnvelope>& InMessages);

ANGELSCRIPTTEST_API int32 CountMessagesByType(
	const FSingleClientDebuggerTranscript& Transcript,
	EDebugMessageType Type);


#endif // WITH_DEV_AUTOMATION_TESTS
