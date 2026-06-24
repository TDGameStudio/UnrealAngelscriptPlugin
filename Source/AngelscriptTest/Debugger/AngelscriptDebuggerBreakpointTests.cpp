#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptNativeScriptTestObject.h"

#include "Algo/AllOf.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS


// =============================================================================
// Helpers specific to break-options gating (BreakOptionsGateStop test)
// =============================================================================

// =============================================================================
// CQTest class merging breakpoint, protocol, break-options, and conditional tests
// =============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerBreakpointTests,
	"Angelscript.TestModule.Debugger.Breakpoint",
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

template <typename ValueType>
static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}

class FScopedDebugBreakOptionsBinding
{
public:
	FScopedDebugBreakOptionsBinding(FAngelscriptEngine& Engine, TFunction<bool(const FAngelscriptDebugBreakOptions&, UObject*)> InShouldBreak)
		: TargetDelegate(Engine.GetDebugCheckBreakOptions())
		, PreviousDelegate(TargetDelegate)
		, ShouldBreak(MoveTemp(InShouldBreak))
	{
		TargetDelegate.BindLambda([this](const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
		{
			return ShouldBreak(BreakOptions, WorldContext);
		});
	}

	~FScopedDebugBreakOptionsBinding()
	{
		TargetDelegate = MoveTemp(PreviousDelegate);
	}

private:
	FAngelscriptDebugCheckBreakOptions& TargetDelegate;
	FAngelscriptDebugCheckBreakOptions PreviousDelegate;
	TFunction<bool(const FAngelscriptDebugBreakOptions&, UObject*)> ShouldBreak;
};

static bool WaitForBreakOptionsState(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	const TArray<FName>& ExpectedFilters,
	const TArray<FName>& RejectedFilters,
	const TCHAR* Context)
{
	const bool bObservedExpectedState = Session.PumpUntil(
		[&Session, &ExpectedFilters, &RejectedFilters]()
		{
			const TArray<FName>& ActiveBreakOptions = Session.GetDebugServer().BreakOptions;
			for (const FName& ExpectedFilter : ExpectedFilters)
			{
				if (!ActiveBreakOptions.Contains(ExpectedFilter))
				{
					return false;
				}
			}

			for (const FName& RejectedFilter : RejectedFilters)
			{
				if (ActiveBreakOptions.Contains(RejectedFilter))
				{
					return false;
				}
			}

			return true;
		},
		Session.GetDefaultTimeoutSeconds());

	return CheckTrue(Test, Context, bObservedExpectedState);
}

// Protocol tests need to wait for breakpoint ack envelopes
static bool WaitForBreakpointAck(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	FAngelscriptDebuggerTestClient& Client,
	TOptional<FAngelscriptBreakpoint>& OutBreakpoint)
{
	const bool bReceivedAck = Session.PumpUntil(
		[&Client, &OutBreakpoint]()
		{
			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
			if (!Envelope.IsSet() || Envelope->MessageType != EDebugMessageType::SetBreakpoint)
			{
				return false;
			}

			OutBreakpoint = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptBreakpoint>(Envelope.GetValue());
			return OutBreakpoint.IsSet();
		},
		Session.GetDefaultTimeoutSeconds());

	if (!bReceivedAck)
	{
		Test.AddError(FString::Printf(TEXT("Timed out waiting for SetBreakpoint ack: %s"), *Client.GetLastError()));
	}

	return bReceivedAck;
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

	// =========================================================================
	// HitLine �?sets a breakpoint and verifies the stop event + callstack
	// =========================================================================
	TEST_METHOD(HitLine)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.HitLine should compile the breakpoint fixture")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Fixture.Filename, Fixture.ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr, TEXT("Debugger.Breakpoint.HitLine should resolve the compiled module immediately after compilation")));

		TAtomic<bool> bMonitorReady{false};
		FBreakpointMonitorConfig MonitorConfig;
		MonitorConfig.bRequestCallstack = true;
		MonitorConfig.bSendContinueOnStop = true;
		MonitorConfig.MaxStopsToHandle = 1;
		MonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MonitorConfig, MonitorFuture)));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.HitLine should send the target breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Breakpoint.HitLine should observe the breakpoint registration before running the script"))));

		TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.Breakpoint.HitLine should complete script invocation after monitor sends Continue")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult MonitorResult = MonitorFuture.Get();

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.HitLine should receive at least one HasStopped via the monitor"), MonitorResult.StopEnvelopes.Num() > 0))
		{
			if (!MonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(MonitorResult.Error);
			}
			return;
		}

		const TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.StopEnvelopes[0]);
		ASSERT_THAT(IsTrue(StopMessage.IsSet(), TEXT("Debugger.Breakpoint.HitLine should deserialize the stop payload")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), StopMessage->Reason, TEXT("Debugger.Breakpoint.HitLine should stop because of a breakpoint")));

		ASSERT_THAT(IsTrue(MonitorResult.CapturedCallstack.IsSet(), TEXT("Debugger.Breakpoint.HitLine should capture a callstack via the monitor")));

		const FAngelscriptCallStack& Callstack = MonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(Callstack.Frames.Num() > 0, TEXT("Debugger.Breakpoint.HitLine should return at least one frame")));

		ASSERT_THAT(IsTrue(Callstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Breakpoint.HitLine should report the fixture filename in the top stack frame")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BreakpointHelperLine")), Callstack.Frames[0].LineNumber, TEXT("Debugger.Breakpoint.HitLine should stop at the requested helper line")));
		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Breakpoint.HitLine should execute the script function successfully after resume")));
		ASSERT_THAT(AreEqual(8, InvocationState->Result, TEXT("Debugger.Breakpoint.HitLine should keep the module function return value")));
	}

	// =========================================================================
	// ClearThenResume �?sets a breakpoint, hits it, clears it, runs again
	// =========================================================================
	TEST_METHOD(ClearThenResume)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.ClearThenResume should compile the breakpoint fixture")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Fixture.Filename, Fixture.ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr, TEXT("Debugger.Breakpoint.ClearThenResume should resolve the compiled module immediately after compilation")));

		// First run: breakpoint should fire
		TAtomic<bool> bFirstMonitorReady{false};
		FBreakpointMonitorConfig FirstMonitorConfig;
		FirstMonitorConfig.bRequestCallstack = false;
		FirstMonitorConfig.bSendContinueOnStop = true;
		FirstMonitorConfig.MaxStopsToHandle = 1;
		FirstMonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> FirstMonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bFirstMonitorReady, Ctx.bMonitorShouldStop, FirstMonitorConfig, FirstMonitorFuture)));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.ClearThenResume should send the target breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Breakpoint.ClearThenResume should observe the breakpoint registration before the first run"))));

		TSharedRef<FAsyncModuleInvocationState> FirstInvocation = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, FirstInvocation, TEXT("Debugger.Breakpoint.ClearThenResume should finish the first invocation after monitor sends Continue")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult FirstMonitorResult = FirstMonitorFuture.Get();

		ASSERT_THAT(IsTrue(FirstMonitorResult.StopEnvelopes.Num() > 0, TEXT("Debugger.Breakpoint.ClearThenResume should receive a stop event on the first invocation")));
		ASSERT_THAT(IsTrue(FirstInvocation->bSucceeded, TEXT("Debugger.Breakpoint.ClearThenResume should finish the first invocation successfully")));

		// Clear breakpoints
		FAngelscriptClearBreakpoints ClearBreakpoints;
		ClearBreakpoints.Filename = Fixture.Filename;
		ClearBreakpoints.ModuleName = Fixture.ModuleName.ToString();
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.ClearThenResume should clear the active breakpoints"), Ctx.Client.SendClearBreakpoints(ClearBreakpoints)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 0, TEXT("Debugger.Breakpoint.ClearThenResume should observe the breakpoint removal before the second run"))));

		Ctx.Client.DrainPendingMessages();

		// Second run: breakpoint should NOT fire
		Ctx.bMonitorShouldStop = false;
		TAtomic<bool> bSecondMonitorReady{false};
		FBreakpointMonitorConfig SecondMonitorConfig;
		SecondMonitorConfig.bRequestCallstack = false;
		SecondMonitorConfig.bSendContinueOnStop = true;
		SecondMonitorConfig.MaxStopsToHandle = 1;
		SecondMonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> SecondMonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bSecondMonitorReady, Ctx.bMonitorShouldStop, SecondMonitorConfig, SecondMonitorFuture)));

		TSharedRef<FAsyncModuleInvocationState> SecondInvocation = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, SecondInvocation, TEXT("Debugger.Breakpoint.ClearThenResume should complete the second invocation after breakpoints are cleared")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult SecondMonitorResult = SecondMonitorFuture.Get();

		ASSERT_THAT(AreEqual(0, SecondMonitorResult.StopEnvelopes.Num(), TEXT("Cleared breakpoints should not stop the second invocation.")));
		ASSERT_THAT(IsTrue(SecondInvocation->bSucceeded, TEXT("Debugger.Breakpoint.ClearThenResume should still execute the script successfully after clearing breakpoints")));
	}

	// =========================================================================
	// IgnoreInactiveBranch �?breakpoint in dead branch should not fire
	// =========================================================================
	TEST_METHOD(IgnoreInactiveBranch)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.IgnoreInactiveBranch should compile the breakpoint fixture")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Fixture.Filename, Fixture.ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr, TEXT("Debugger.Breakpoint.IgnoreInactiveBranch should resolve the compiled module immediately after compilation")));

		TAtomic<bool> bMonitorReady{false};
		FBreakpointMonitorConfig MonitorConfig;
		MonitorConfig.bRequestCallstack = false;
		MonitorConfig.bSendContinueOnStop = true;
		MonitorConfig.MaxStopsToHandle = 1;
		MonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MonitorConfig, MonitorFuture)));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointInactiveBranchLine"));
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.IgnoreInactiveBranch should send the inactive-branch breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Breakpoint.IgnoreInactiveBranch should observe the inactive-branch breakpoint registration before running the script"))));

		TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.Breakpoint.IgnoreInactiveBranch should complete script invocation")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult MonitorResult = MonitorFuture.Get();

		ASSERT_THAT(AreEqual(0, MonitorResult.StopEnvelopes.Num(), TEXT("Breakpoint in the inactive branch should not have triggered a stop event.")));
		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Breakpoint.IgnoreInactiveBranch should execute the function successfully when the inactive branch breakpoint never hits")));
	}

	// =========================================================================
	// NearestExecutableLineAck �?non-executable line snaps to nearest executable
	// =========================================================================
	TEST_METHOD(NearestExecutableLineAck)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.NearestExecutableLineAck should compile the breakpoint fixture")));

		const int32 RequestedLine = Fixture.GetLine(TEXT("BreakpointHelperLine")) - 1;
		const int32 ExpectedLine = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		ASSERT_THAT(IsTrue(RequestedLine < ExpectedLine, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should target a non-executable line before the helper marker")));

		TAtomic<bool> bMonitorReady{false};
		FBreakpointMonitorConfig MonitorConfig;
		MonitorConfig.bRequestCallstack = true;
		MonitorConfig.bSendContinueOnStop = true;
		MonitorConfig.MaxStopsToHandle = 1;
		MonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MonitorConfig, MonitorFuture)));

		Ctx.Client.DrainPendingMessages();

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = RequestedLine;
		Breakpoint.Id = 101;
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should send the non-executable-line breakpoint request"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		TOptional<FAngelscriptBreakpoint> BreakpointAck;
		if (!WaitForBreakpointAck(*TestRunner, Ctx.Session, Ctx.Client, BreakpointAck))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should deserialize the SetBreakpoint ack payload"), BreakpointAck.IsSet()))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		ASSERT_THAT(AreEqual(101, BreakpointAck->Id, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should preserve the explicit breakpoint id in the ack")));
		ASSERT_THAT(AreEqual(Fixture.Filename, BreakpointAck->Filename, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should keep the original fixture filename in the ack")));
		ASSERT_THAT(AreEqual(ExpectedLine, BreakpointAck->LineNumber, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should snap the ack line to the nearest executable helper line")));

		if (!WaitForSpecificBreakpoint(
			*TestRunner,
			Ctx.Session,
			Fixture.ModuleName.ToString(),
			ExpectedLine,
			TEXT("Debugger.Breakpoint.NearestExecutableLineAck should register the adjusted executable line on the server before running the script")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		ASSERT_THAT(AreEqual(1, Ctx.GetDebugServer().BreakpointCount, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should keep exactly one authoritative breakpoint registered")));

		TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should complete script invocation after monitor sends Continue")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult MonitorResult = MonitorFuture.Get();

		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
			return;
		}

		if (!CheckEqual(*TestRunner, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should stop exactly once after the adjusted breakpoint is hit"), MonitorResult.StopEnvelopes.Num(), 1))
		{
			return;
		}

		const TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.StopEnvelopes[0]);
		ASSERT_THAT(IsTrue(StopMessage.IsSet(), TEXT("Debugger.Breakpoint.NearestExecutableLineAck should deserialize the stop payload")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), StopMessage->Reason, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should stop because of a breakpoint")));

		ASSERT_THAT(IsTrue(MonitorResult.CapturedCallstack.IsSet(), TEXT("Debugger.Breakpoint.NearestExecutableLineAck should capture a callstack after the stop")));

		const FAngelscriptCallStack& Callstack = MonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(Callstack.Frames.Num() > 0, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should return at least one frame")));

		ASSERT_THAT(IsTrue(Callstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Breakpoint.NearestExecutableLineAck should report the fixture filename in the top stack frame")));
		ASSERT_THAT(AreEqual(BreakpointAck->LineNumber, Callstack.Frames[0].LineNumber, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should stop at the adjusted executable line")));
		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should execute the script successfully after resume")));
		ASSERT_THAT(AreEqual(8, InvocationState->Result, TEXT("Debugger.Breakpoint.NearestExecutableLineAck should preserve the script return value")));
	}

	// =========================================================================
	// DuplicateSetReturnsRemovalAck �?second set on same line returns removal
	// =========================================================================
	TEST_METHOD(DuplicateSetReturnsRemovalAck)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should compile the breakpoint fixture")));

		const int32 BreakpointLine = Fixture.GetLine(TEXT("BreakpointHelperLine"));

		TAtomic<bool> bMonitorReady{false};
		FBreakpointMonitorConfig MonitorConfig;
		MonitorConfig.bRequestCallstack = true;
		MonitorConfig.bSendContinueOnStop = true;
		MonitorConfig.MaxStopsToHandle = 1;
		MonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MonitorConfig, MonitorFuture)));

		Ctx.Client.DrainPendingMessages();

		FAngelscriptBreakpoint InitialBreakpoint;
		InitialBreakpoint.Filename = Fixture.Filename;
		InitialBreakpoint.ModuleName = Fixture.ModuleName.ToString();
		InitialBreakpoint.LineNumber = BreakpointLine;
		InitialBreakpoint.Id = 201;
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should send the initial breakpoint request"), Ctx.Client.SendSetBreakpoint(InitialBreakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(
			*TestRunner,
			Ctx.Session,
			1,
			TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should register exactly one breakpoint after the first request"))));

		ASSERT_THAT(IsTrue(WaitForSpecificBreakpoint(
			*TestRunner,
			Ctx.Session,
			Fixture.ModuleName.ToString(),
			BreakpointLine,
			TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should store the first breakpoint on the executable helper line"))));

		FAngelscriptBreakpoint DuplicateBreakpoint = InitialBreakpoint;
		DuplicateBreakpoint.Id = 202;
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should send the duplicate breakpoint request"), Ctx.Client.SendSetBreakpoint(DuplicateBreakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		TOptional<FAngelscriptBreakpoint> DuplicateAck;
		if (!WaitForBreakpointAck(*TestRunner, Ctx.Session, Ctx.Client, DuplicateAck))
		{
			return;
		}

		ASSERT_THAT(IsTrue(DuplicateAck.IsSet(), TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should deserialize the duplicate-breakpoint ack")));

		ASSERT_THAT(AreEqual(202, DuplicateAck->Id, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should preserve the duplicate request id in the rejection ack")));
		ASSERT_THAT(AreEqual(Fixture.Filename, DuplicateAck->Filename, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should preserve the original fixture filename in the rejection ack")));
		ASSERT_THAT(AreEqual(-1, DuplicateAck->LineNumber, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should ask the frontend to remove the duplicate breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(
			*TestRunner,
			Ctx.Session,
			1,
			TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should keep the authoritative breakpoint count unchanged after a duplicate request"))));

		TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should complete script invocation after monitor sends Continue")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult MonitorResult = MonitorFuture.Get();

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should not report monitor-side transport errors"), MonitorResult.Error.IsEmpty()))
		{
			if (!MonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(MonitorResult.Error);
			}
			return;
		}

		if (!CheckFalse(*TestRunner, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should not time out while waiting for the breakpoint hit"), MonitorResult.bTimedOut))
		{
			return;
		}

		if (!CheckEqual(*TestRunner, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should stop exactly once even after a duplicate request"), MonitorResult.StopEnvelopes.Num(), 1))
		{
			return;
		}

		const TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.StopEnvelopes[0]);
		ASSERT_THAT(IsTrue(StopMessage.IsSet(), TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should deserialize the stop payload")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), StopMessage->Reason, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should stop because of a breakpoint")));

		ASSERT_THAT(IsTrue(MonitorResult.CapturedCallstack.IsSet(), TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should capture a callstack after the stop")));

		const FAngelscriptCallStack& Callstack = MonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(Callstack.Frames.Num() > 0, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should return at least one frame")));

		ASSERT_THAT(IsTrue(Callstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should report the fixture filename in the top stack frame")));
		ASSERT_THAT(AreEqual(BreakpointLine, Callstack.Frames[0].LineNumber, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should stop at the helper line")));
		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should execute the script successfully after resume")));
		ASSERT_THAT(AreEqual(8, InvocationState->Result, TEXT("Debugger.Breakpoint.DuplicateSetReturnsRemovalAck should preserve the script return value")));
	}

	// =========================================================================
	// BreakOptionsGateStop �?break-options delegate gates breakpoint hits
	// =========================================================================
	TEST_METHOD(BreakOptionsGateStop)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		UObject* BreakOptionsWorldContext = NewObject<UAngelscriptNativeScriptTestObject>();
		ASSERT_THAT(IsNotNull(BreakOptionsWorldContext, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should create a non-null world context for break-option gating")));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should compile the breakpoint fixture")));

		struct FObservedBreakOptionsState
		{
			mutable FCriticalSection Mutex;
			TArray<TArray<FName>> Calls;
			TArray<TWeakObjectPtr<UObject>> WorldContexts;

			bool Record(const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
			{
				FScopeLock Lock(&Mutex);
				Calls.Add(BreakOptions);
				WorldContexts.Add(WorldContext);
				return BreakOptions.Contains(FName(TEXT("break:test")));
			}
		};

		FObservedBreakOptionsState ObservedBreakOptions;
		FScopedDebugBreakOptionsBinding ScopedBreakOptionsBinding(
			Engine,
			[&ObservedBreakOptions](const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
			{
				return ObservedBreakOptions.Record(BreakOptions, WorldContext);
			});

		auto RunBreakpointRound =
			[this, &Engine, &Fixture, BreakOptionsWorldContext](
				int32 BreakpointId,
				const TArray<FString>& Filters,
				const TArray<FName>& ExpectedFilters,
				const TArray<FName>& RejectedFilters,
				FBreakpointMonitorResult& OutMonitorResult,
				TSharedRef<FAsyncModuleInvocationState>& OutInvocationState,
				const TCHAR* SendContext,
				const TCHAR* StateContext,
				const TCHAR* InvocationContext) -> bool
			{
				Ctx.Client.DrainPendingMessages();

				TAtomic<bool> bMonitorReady{false};
				TAtomic<bool> bMonitorShouldStop{false};
				FBreakpointMonitorConfig MonitorConfig;
				MonitorConfig.bRequestCallstack = true;
				MonitorConfig.bSendContinueOnStop = true;
				MonitorConfig.MaxStopsToHandle = 1;
				MonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

				TFuture<FBreakpointMonitorResult> MonitorFuture;
				if (!StartAndWaitForBreakpointMonitorReady(*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, bMonitorShouldStop, MonitorConfig, MonitorFuture))
				{
					return false;
				}

				FAngelscriptBreakpoint Breakpoint;
				Breakpoint.Id = BreakpointId;
				Breakpoint.Filename = Fixture.Filename;
				Breakpoint.ModuleName = Fixture.ModuleName.ToString();
				Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
				if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should register the round-specific breakpoint after the monitor starts debugging"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
				{
					bMonitorShouldStop = true;
					TestRunner->AddError(Ctx.Client.GetLastError());
					return false;
				}

				if (!WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should observe exactly one registered breakpoint after the round-specific registration")))
				{
					bMonitorShouldStop = true;
					return false;
				}

				if (!WaitForSpecificBreakpoint(
					*TestRunner,
					Ctx.Session,
					Fixture.ModuleName.ToString(),
					Fixture.GetLine(TEXT("BreakpointHelperLine")),
					TEXT("Debugger.Breakpoint.BreakOptionsGateStop should resolve the requested helper-line breakpoint before running the script")))
				{
					bMonitorShouldStop = true;
					return false;
				}

				if (!CheckTrue(*TestRunner, SendContext, Ctx.Client.SendBreakOptions(Filters)))
				{
					bMonitorShouldStop = true;
					TestRunner->AddError(Ctx.Client.GetLastError());
					return false;
				}

				if (!WaitForBreakOptionsState(*TestRunner, Ctx.Session, ExpectedFilters, RejectedFilters, StateContext))
				{
					bMonitorShouldStop = true;
					return false;
				}

				OutInvocationState = DispatchModuleInvocation(
					Engine,
					BreakOptionsWorldContext,
					Fixture.Filename,
					Fixture.ModuleName,
					Fixture.EntryFunctionDeclaration);

				if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, OutInvocationState, InvocationContext))
				{
					bMonitorShouldStop = true;
					return false;
				}

				bMonitorShouldStop = true;
				OutMonitorResult = MonitorFuture.Get();
				return true;
			};

		FBreakpointMonitorResult FirstRoundMonitorResult;
		TSharedRef<FAsyncModuleInvocationState> FirstInvocationState = MakeShared<FAsyncModuleInvocationState>();
		if (!RunBreakpointRound(
			151,
			{TEXT("break:other")},
			{FName(TEXT("break:other")), FName(TEXT("break:any"))},
			{FName(TEXT("break:test"))},
			FirstRoundMonitorResult,
			FirstInvocationState,
			TEXT("Debugger.Breakpoint.BreakOptionsGateStop should send the first break-options filter set"),
			TEXT("Debugger.Breakpoint.BreakOptionsGateStop should update the server break-options state for the rejected round"),
			TEXT("Debugger.Breakpoint.BreakOptionsGateStop should complete the first invocation without stopping")))
		{
			return;
		}

		FBreakpointMonitorResult SecondRoundMonitorResult;
		TSharedRef<FAsyncModuleInvocationState> SecondInvocationState = MakeShared<FAsyncModuleInvocationState>();
		if (!RunBreakpointRound(
			152,
			{TEXT("break:test")},
			{FName(TEXT("break:test")), FName(TEXT("break:any"))},
			{FName(TEXT("break:other"))},
			SecondRoundMonitorResult,
			SecondInvocationState,
			TEXT("Debugger.Breakpoint.BreakOptionsGateStop should send the second break-options filter set"),
			TEXT("Debugger.Breakpoint.BreakOptionsGateStop should update the server break-options state for the accepted round"),
			TEXT("Debugger.Breakpoint.BreakOptionsGateStop should complete the second invocation after the monitor resumes from the breakpoint")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(FirstRoundMonitorResult.Error.IsEmpty(), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should keep the rejected-round monitor error empty")));
		ASSERT_THAT(IsFalse(FirstRoundMonitorResult.bTimedOut, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should not time out while waiting for a rejected breakpoint stop")));
		ASSERT_THAT(AreEqual(0, FirstRoundMonitorResult.StopEnvelopes.Num(), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should not emit any HasStopped message in the rejected round")));
		ASSERT_THAT(IsTrue(FirstInvocationState->bSucceeded, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should still execute the rejected-round invocation successfully")));
		ASSERT_THAT(AreEqual(8, FirstInvocationState->Result, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should preserve the rejected-round script return value")));

		ASSERT_THAT(IsTrue(SecondRoundMonitorResult.Error.IsEmpty(), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should keep the accepted-round monitor error empty")));
		if (!CheckEqual(*TestRunner, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should emit exactly one HasStopped message in the accepted round"), SecondRoundMonitorResult.StopEnvelopes.Num(), 1))
		{
			return;
		}

		const TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(SecondRoundMonitorResult.StopEnvelopes[0]);
		ASSERT_THAT(IsTrue(StopMessage.IsSet(), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should deserialize the accepted-round stop payload")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), StopMessage->Reason, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should stop because of a breakpoint in the accepted round")));
		ASSERT_THAT(IsTrue(SecondRoundMonitorResult.CapturedCallstack.IsSet(), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should capture a callstack for the accepted round")));

		const FAngelscriptCallStack& Callstack = SecondRoundMonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(Callstack.Frames.Num() > 0, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should return at least one frame in the accepted-round callstack")));

		ASSERT_THAT(IsTrue(Callstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should report the fixture filename in the accepted-round top stack frame")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BreakpointHelperLine")), Callstack.Frames[0].LineNumber, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should stop at the breakpoint helper line")));
		ASSERT_THAT(IsTrue(SecondInvocationState->bSucceeded, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should execute the accepted-round invocation successfully")));
		ASSERT_THAT(AreEqual(8, SecondInvocationState->Result, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should preserve the accepted-round script return value")));

		FScopeLock Lock(&ObservedBreakOptions.Mutex);
		if (!CheckEqual(*TestRunner, TEXT("Debugger.Breakpoint.BreakOptionsGateStop should invoke the break-options delegate exactly once per round"), ObservedBreakOptions.Calls.Num(), 2))
		{
			return;
		}

		ASSERT_THAT(IsTrue(ObservedBreakOptions.Calls[0].Contains(FName(TEXT("break:other"))), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should pass break:other to the first delegate invocation")));
		ASSERT_THAT(IsTrue(ObservedBreakOptions.Calls[0].Contains(FName(TEXT("break:any"))), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should append break:any to the first delegate invocation")));
		ASSERT_THAT(IsFalse(ObservedBreakOptions.Calls[0].Contains(FName(TEXT("break:test"))), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should not leak break:test into the first delegate invocation")));
		ASSERT_THAT(IsTrue(ObservedBreakOptions.Calls[1].Contains(FName(TEXT("break:test"))), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should pass break:test to the second delegate invocation")));
		ASSERT_THAT(IsTrue(ObservedBreakOptions.Calls[1].Contains(FName(TEXT("break:any"))), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should append break:any to the second delegate invocation")));
		ASSERT_THAT(IsFalse(ObservedBreakOptions.Calls[1].Contains(FName(TEXT("break:other"))), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should not leak break:other into the second delegate invocation")));
		ASSERT_THAT(IsTrue(Algo::AllOf(ObservedBreakOptions.WorldContexts, [](const TWeakObjectPtr<UObject>& WorldContext) { return WorldContext.IsValid(); }), TEXT("Debugger.Breakpoint.BreakOptionsGateStop should provide a valid world context to every delegate invocation")));
	}

	// =========================================================================
	// Expression �?conditional breakpoint fires only when expression is true
	// =========================================================================
	TEST_METHOD(Expression)
	{
FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Breakpoint.ConditionExpression should compile the breakpoint fixture")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByFilenameOrModuleName(Fixture.Filename, Fixture.ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr, TEXT("Debugger.Breakpoint.ConditionExpression should resolve the compiled module immediately after compilation")));

		// Positive run: condition "Input > 0" with Input=3 should trigger stop
		TAtomic<bool> bPositiveMonitorReady{false};
		FBreakpointMonitorConfig PositiveMonitorConfig;
		PositiveMonitorConfig.bRequestCallstack = true;
		PositiveMonitorConfig.bSendContinueOnStop = true;
		PositiveMonitorConfig.MaxStopsToHandle = 1;
		PositiveMonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> PositiveMonitorFuture = StartBreakpointMonitor(
			Ctx.GetPort(),
			bPositiveMonitorReady,
			Ctx.bMonitorShouldStop,
			PositiveMonitorConfig);

		ASSERT_THAT(IsTrue(
			Ctx.Session.PumpUntil([&bPositiveMonitorReady]() { return bPositiveMonitorReady.Load(); }, Ctx.GetDefaultTimeoutSeconds()),
			TEXT("Debugger.Breakpoint.ConditionExpression should start the positive-run monitor")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BreakpointHelperLine"));
		Breakpoint.Condition = TEXT("Input > 0");
		if (!CheckTrue(*TestRunner, TEXT("Debugger.Breakpoint.ConditionExpression should send the conditional breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.Breakpoint.ConditionExpression should observe the conditional breakpoint registration before running the positive case"))));

		TSharedRef<FAsyncModuleInvocationState> PositiveInvocation = DispatchModuleInvocationWithIntArg(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			TEXT("int Helper(int Input)"),
			3);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, PositiveInvocation, TEXT("Debugger.Breakpoint.ConditionExpression should finish the positive invocation after the monitor resumes execution")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult PositiveMonitorResult = PositiveMonitorFuture.Get();
		if (!CheckEqual(*TestRunner, TEXT("Debugger.Breakpoint.ConditionExpression should stop exactly once when the condition is true"), PositiveMonitorResult.StopEnvelopes.Num(), 1))
		{
			if (!PositiveMonitorResult.Error.IsEmpty())
			{
				TestRunner->AddError(PositiveMonitorResult.Error);
			}
			return;
		}

		const TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(PositiveMonitorResult.StopEnvelopes[0]);
		ASSERT_THAT(IsTrue(StopMessage.IsSet(), TEXT("Debugger.Breakpoint.ConditionExpression should deserialize the positive stop payload")));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), StopMessage->Reason, TEXT("Debugger.Breakpoint.ConditionExpression should stop because of a breakpoint when the condition is true")));
		ASSERT_THAT(IsTrue(PositiveMonitorResult.CapturedCallstack.IsSet(), TEXT("Debugger.Breakpoint.ConditionExpression should capture a callstack when the condition is true")));

		const FAngelscriptCallStack& PositiveCallstack = PositiveMonitorResult.CapturedCallstack.GetValue();
		ASSERT_THAT(IsTrue(PositiveCallstack.Frames.Num() > 0, TEXT("Debugger.Breakpoint.ConditionExpression should return at least one stack frame for the positive stop")));

		ASSERT_THAT(IsTrue(PositiveCallstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Breakpoint.ConditionExpression should report the fixture filename in the top stack frame")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BreakpointHelperLine")), PositiveCallstack.Frames[0].LineNumber, TEXT("Debugger.Breakpoint.ConditionExpression should stop at the requested helper line when the condition is true")));
		ASSERT_THAT(IsTrue(PositiveInvocation->bSucceeded, TEXT("Debugger.Breakpoint.ConditionExpression should execute the positive case successfully")));
		ASSERT_THAT(AreEqual(8, PositiveInvocation->Result, TEXT("Debugger.Breakpoint.ConditionExpression should keep the positive helper return value")));

		// Negative run: condition "Input > 0" with Input=-1 should NOT trigger stop
		Ctx.bMonitorShouldStop = false;
		TAtomic<bool> bNegativeMonitorReady{false};
		FBreakpointMonitorConfig NegativeMonitorConfig;
		NegativeMonitorConfig.bRequestCallstack = false;
		NegativeMonitorConfig.bSendContinueOnStop = true;
		NegativeMonitorConfig.MaxStopsToHandle = 1;
		NegativeMonitorConfig.TimeoutSeconds = Ctx.GetDefaultTimeoutSeconds();

		TFuture<FBreakpointMonitorResult> NegativeMonitorFuture = StartBreakpointMonitor(
			Ctx.GetPort(),
			bNegativeMonitorReady,
			Ctx.bMonitorShouldStop,
			NegativeMonitorConfig);

		ASSERT_THAT(IsTrue(
			Ctx.Session.PumpUntil([&bNegativeMonitorReady]() { return bNegativeMonitorReady.Load(); }, Ctx.GetDefaultTimeoutSeconds()),
			TEXT("Debugger.Breakpoint.ConditionExpression should start the negative-run monitor")));

		TSharedRef<FAsyncModuleInvocationState> NegativeInvocation = DispatchModuleInvocationWithIntArg(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			TEXT("int Helper(int Input)"),
			-1);

		if (!WaitForInvocationCompletion(*TestRunner, Ctx.Session, NegativeInvocation, TEXT("Debugger.Breakpoint.ConditionExpression should finish the negative invocation without stopping")))
		{
			Ctx.bMonitorShouldStop = true;
			return;
		}

		Ctx.bMonitorShouldStop = true;
		FBreakpointMonitorResult NegativeMonitorResult = NegativeMonitorFuture.Get();

		ASSERT_THAT(AreEqual(0, NegativeMonitorResult.StopEnvelopes.Num(), TEXT("Debugger.Breakpoint.ConditionExpression should not stop when the condition is false")));
		ASSERT_THAT(IsTrue(NegativeMonitorResult.Error.IsEmpty(), TEXT("Debugger.Breakpoint.ConditionExpression should keep the negative monitor error empty")));
		ASSERT_THAT(IsFalse(NegativeMonitorResult.bTimedOut, TEXT("Debugger.Breakpoint.ConditionExpression should not time out while waiting for a false-condition run to complete")));
		ASSERT_THAT(IsTrue(NegativeInvocation->bSucceeded, TEXT("Debugger.Breakpoint.ConditionExpression should execute the negative case successfully")));
		ASSERT_THAT(AreEqual(4, NegativeInvocation->Result, TEXT("Debugger.Breakpoint.ConditionExpression should keep the negative helper return value")));
	}
};

#endif
