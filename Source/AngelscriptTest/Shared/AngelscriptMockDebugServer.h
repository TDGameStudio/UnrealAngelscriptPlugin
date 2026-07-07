#pragma once

// Mock debug server test infrastructure for Angelscript automation tests.
//
// Scope (Phase 1 A.1 of the UE 5.7 disabled-test restart plan):
//   * Declares IAngelscriptDebugServerTestInterface: the minimal abstraction that
//     debugger automation tests can target instead of FAngelscriptDebugServer when
//     running headless (no production UAngelscriptSubsystem available).
//   * Declares FAngelscriptMockDebugServer: a scripted-response implementation of
//     that interface with a simplified stack-frame / local-scope model, so that
//     stepping / evaluation group tests can exercise realistic transitions without
//     standing up a real DebugServer or socket pair.
//
// This file intentionally adds NO test enablement and does NOT modify any
// production code. It is the pure-additive infrastructure commit that A.2-A.5
// build on top of.

#include "CoreMinimal.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Templates/SharedPointer.h"

// ---- Simplified stack-frame / scope model used by the mock server ----

struct FMockScopeEntry
{
	FString Name;
	FString TypeName;
	FString Value;
};

struct FMockStackFrame
{
	FString FunctionName;
	FString SourceFile;
	int32 Line = 0;

	// Locals / arguments for this frame, queried by tests as "local scope".
	TArray<FMockScopeEntry> Locals;
};

struct FMockBreakpointRecord
{
	FString SourceFile;
	int32 Line = 0;
	FString Condition;
	bool bDataBreakpoint = false;

	bool Matches(const FString& InFile, int32 InLine) const
	{
		return Line == InLine && SourceFile == InFile;
	}
};

// ---- Scripted response primitive ----
//
// The mock stores a queue of outgoing envelopes to emit in response to a
// particular incoming client message type. Tests pre-seed this queue so they
// can deterministically drive protocol handshakes and break sequences
// without relying on a real server's threading or socket stack.

struct FMockScriptedResponse
{
	EDebugMessageType TriggerType = EDebugMessageType::PingAlive;
	FAngelscriptDebugMessageEnvelope ResponseEnvelope;

	// If true, the response fires whenever Tick() runs with nothing else
	// pending, rather than waiting for a matching ingest. Useful for
	// auto-delivered version/handshake packets.
	bool bEmitOnNextTick = false;

	// One-shot by default; set to true to re-arm after firing.
	bool bRepeat = false;
};

// ---- Interface targeted by tests ----

class IAngelscriptDebugServerTestInterface
{
public:
	virtual ~IAngelscriptDebugServerTestInterface() = default;

	// --- Lifecycle / transport substitute ---

	// Simulated server port for tests that log/assert it. The real server
	// binds to a TCP port; the mock just returns a stable numeric id.
	virtual int32 GetPort() const = 0;

	// Runs one tick of the scripted response pipeline. Tests call this from
	// FAngelscriptDebuggerTestSession::PumpOneTick() when in mock mode.
	virtual void Tick() = 0;

	// True if the mock is currently in "stopped" state (simulating a
	// breakpoint hit / step completion). Drives PumpUntil() predicates.
	virtual bool IsStopped() const = 0;

	// --- Protocol plumbing (scripted) ---

	// Called by a test (directly or via a fake client) to feed a client->server
	// envelope into the mock. The mock may immediately push scripted replies
	// into its outbound queue based on TriggerType matches.
	virtual void IngestClientMessage(const FAngelscriptDebugMessageEnvelope& Envelope) = 0;

	// Drains all pending server->client envelopes produced so far by Tick() or
	// IngestClientMessage(). After this call, the outbound queue is empty.
	virtual TArray<FAngelscriptDebugMessageEnvelope> DrainServerMessages() = 0;

	// Pre-seed a response mapping. Tests call this during setup.
	virtual void AddScriptedResponse(const FMockScriptedResponse& Response) = 0;

	// --- Breakpoint table view ---

	virtual const TArray<FMockBreakpointRecord>& GetBreakpoints() const = 0;
	virtual void SetBreakpoint(const FMockBreakpointRecord& Record) = 0;
	virtual void ClearBreakpoints(const FString& SourceFile) = 0;
	virtual void ClearAllBreakpoints() = 0;

	// --- Simplified stepping model ---

	// Trigger the "stopped at breakpoint" state with an explicit stack and
	// local scope. Typically invoked by tests at the point they would have
	// expected the real engine to hit a script breakpoint.
	virtual void SimulateBreakpointHit(
		const FString& SourceFile,
		int32 Line,
		const TArray<FMockStackFrame>& StackFrames) = 0;

	// Advance the top frame to a new line. Does not change frame count.
	virtual void SimulateStepTo(int32 NextLine) = 0;

	// Pop the top frame; if no frames remain, transition out of stopped state.
	virtual void SimulateStepOutTopFrame() = 0;

	// Clear stopped state (emulating Continue).
	virtual void SimulateContinue() = 0;

	virtual TArray<FMockStackFrame> GetCurrentStack() const = 0;
	virtual const FMockStackFrame* GetCurrentTopFrame() const = 0;
	virtual TArray<FMockScopeEntry> GetLocalScope() const = 0;
};

// ---- Concrete implementation ----

class FAngelscriptMockDebugServer final : public IAngelscriptDebugServerTestInterface
{
public:
	FAngelscriptMockDebugServer();
	virtual ~FAngelscriptMockDebugServer() override = default;

	// --- IAngelscriptDebugServerTestInterface ---

	virtual int32 GetPort() const override { return Port; }
	virtual void Tick() override;
	virtual bool IsStopped() const override { return bIsStopped; }

	virtual void IngestClientMessage(const FAngelscriptDebugMessageEnvelope& Envelope) override;
	virtual TArray<FAngelscriptDebugMessageEnvelope> DrainServerMessages() override;
	virtual void AddScriptedResponse(const FMockScriptedResponse& Response) override;

	virtual const TArray<FMockBreakpointRecord>& GetBreakpoints() const override { return Breakpoints; }
	virtual void SetBreakpoint(const FMockBreakpointRecord& Record) override;
	virtual void ClearBreakpoints(const FString& SourceFile) override;
	virtual void ClearAllBreakpoints() override;

	virtual void SimulateBreakpointHit(
		const FString& SourceFile,
		int32 Line,
		const TArray<FMockStackFrame>& StackFrames) override;
	virtual void SimulateStepTo(int32 NextLine) override;
	virtual void SimulateStepOutTopFrame() override;
	virtual void SimulateContinue() override;

	virtual TArray<FMockStackFrame> GetCurrentStack() const override { return StackFrames; }
	virtual const FMockStackFrame* GetCurrentTopFrame() const override;
	virtual TArray<FMockScopeEntry> GetLocalScope() const override;

	// --- Non-virtual mock-only helpers (exposed for direct test introspection) ---

	// Assign the simulated port (used by session initialization).
	void SetPort(int32 InPort) { Port = InPort; }

	// Number of incoming envelopes received via IngestClientMessage so far.
	int32 GetIngestedMessageCount() const { return IngestedCount; }

	// Number of outbound envelopes delivered via DrainServerMessages so far.
	int32 GetDeliveredMessageCount() const { return DeliveredCount; }

	// For tests that need to push an arbitrary server->client envelope without
	// a trigger (e.g. simulating an EngineBreak notification).
	void EnqueueServerMessage(const FAngelscriptDebugMessageEnvelope& Envelope);

private:
	void FireScriptedResponsesFor(EDebugMessageType TriggerType);
	void FireOnNextTickResponses();

	int32 Port = 0;
	bool bIsStopped = false;

	TArray<FMockBreakpointRecord> Breakpoints;
	TArray<FMockStackFrame> StackFrames;
	TArray<FMockScriptedResponse> ScriptedResponses;
	TArray<FAngelscriptDebugMessageEnvelope> OutboundQueue;

	int32 IngestedCount = 0;
	int32 DeliveredCount = 0;
};

// Convenience typedef used by session config below.
using FMockDebugServerPtr = TSharedPtr<IAngelscriptDebugServerTestInterface>;

