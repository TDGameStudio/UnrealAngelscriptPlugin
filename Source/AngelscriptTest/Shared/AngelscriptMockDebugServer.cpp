#include "AngelscriptMockDebugServer.h"

namespace
{
	int32 AllocateMockPort()
	{
		// Deterministic-ish: mock ports live in a distinct range from real
		// test DebugServer ports (which live in 30000..40000 per
		// AngelscriptDebuggerTestSession.cpp::MakeUniqueDebugServerPort).
		static int32 NextMockPort = 50000;
		return ++NextMockPort;
	}
}

FAngelscriptMockDebugServer::FAngelscriptMockDebugServer()
	: Port(AllocateMockPort())
{
}

void FAngelscriptMockDebugServer::Tick()
{
	FireOnNextTickResponses();
}

void FAngelscriptMockDebugServer::IngestClientMessage(const FAngelscriptDebugMessageEnvelope& Envelope)
{
	++IngestedCount;
	FireScriptedResponsesFor(Envelope.MessageType);
}

TArray<FAngelscriptDebugMessageEnvelope> FAngelscriptMockDebugServer::DrainServerMessages()
{
	TArray<FAngelscriptDebugMessageEnvelope> Drained = MoveTemp(OutboundQueue);
	OutboundQueue.Reset();
	DeliveredCount += Drained.Num();
	return Drained;
}

void FAngelscriptMockDebugServer::AddScriptedResponse(const FMockScriptedResponse& Response)
{
	ScriptedResponses.Add(Response);
}

void FAngelscriptMockDebugServer::EnqueueServerMessage(const FAngelscriptDebugMessageEnvelope& Envelope)
{
	OutboundQueue.Add(Envelope);
}

void FAngelscriptMockDebugServer::SetBreakpoint(const FMockBreakpointRecord& Record)
{
	for (FMockBreakpointRecord& Existing : Breakpoints)
	{
		if (Existing.Matches(Record.SourceFile, Record.Line))
		{
			Existing = Record;
			return;
		}
	}
	Breakpoints.Add(Record);
}

void FAngelscriptMockDebugServer::ClearBreakpoints(const FString& SourceFile)
{
	Breakpoints.RemoveAll([&SourceFile](const FMockBreakpointRecord& Record)
	{
		return Record.SourceFile == SourceFile;
	});
}

void FAngelscriptMockDebugServer::ClearAllBreakpoints()
{
	Breakpoints.Reset();
}

void FAngelscriptMockDebugServer::SimulateBreakpointHit(
	const FString& SourceFile,
	int32 Line,
	const TArray<FMockStackFrame>& InStackFrames)
{
	StackFrames = InStackFrames;
	if (StackFrames.Num() > 0)
	{
		// Ensure the declared hit location matches the top frame.
		StackFrames[0].SourceFile = SourceFile;
		StackFrames[0].Line = Line;
	}
	bIsStopped = true;
}

void FAngelscriptMockDebugServer::SimulateStepTo(int32 NextLine)
{
	if (StackFrames.Num() > 0)
	{
		StackFrames[0].Line = NextLine;
	}
	bIsStopped = true;
}

void FAngelscriptMockDebugServer::SimulateStepOutTopFrame()
{
	if (StackFrames.Num() > 0)
	{
		StackFrames.RemoveAt(0);
	}
	if (StackFrames.Num() == 0)
	{
		bIsStopped = false;
	}
}

void FAngelscriptMockDebugServer::SimulateContinue()
{
	bIsStopped = false;
	StackFrames.Reset();
}

const FMockStackFrame* FAngelscriptMockDebugServer::GetCurrentTopFrame() const
{
	return StackFrames.Num() > 0 ? &StackFrames[0] : nullptr;
}

TArray<FMockScopeEntry> FAngelscriptMockDebugServer::GetLocalScope() const
{
	if (StackFrames.Num() == 0)
	{
		return {};
	}
	return StackFrames[0].Locals;
}

void FAngelscriptMockDebugServer::FireScriptedResponsesFor(EDebugMessageType TriggerType)
{
	for (int32 Index = 0; Index < ScriptedResponses.Num(); )
	{
		FMockScriptedResponse& Response = ScriptedResponses[Index];
		if (Response.bEmitOnNextTick || Response.TriggerType != TriggerType)
		{
			++Index;
			continue;
		}

		OutboundQueue.Add(Response.ResponseEnvelope);

		if (Response.bRepeat)
		{
			++Index;
		}
		else
		{
			ScriptedResponses.RemoveAt(Index);
		}
	}
}

void FAngelscriptMockDebugServer::FireOnNextTickResponses()
{
	for (int32 Index = 0; Index < ScriptedResponses.Num(); )
	{
		FMockScriptedResponse& Response = ScriptedResponses[Index];
		if (!Response.bEmitOnNextTick)
		{
			++Index;
			continue;
		}

		OutboundQueue.Add(Response.ResponseEnvelope);

		if (Response.bRepeat)
		{
			++Index;
		}
		else
		{
			ScriptedResponses.RemoveAt(Index);
		}
	}
}

