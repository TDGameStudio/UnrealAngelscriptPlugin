#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestHelpers.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptDebuggerBindingTests_Private
{
	static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(bActual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.AreEqual(Expected, Actual, Message);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsNotNull(Value, Message);
	}

	template <typename TInvocationState>
	bool WaitForBindingInvocationCompletion(
		FAutomationTestBase& Test,
		FAngelscriptDebuggerTestSession& Session,
		const TSharedRef<TInvocationState>& InvocationState,
		const TCHAR* Context)
	{
		const bool bCompleted = Session.PumpUntil(
			[&InvocationState]()
			{
				return InvocationState->bCompleted.Load();
			},
			Session.GetDefaultTimeoutSeconds());

		return CheckTrue(Test, Context, bCompleted);
	}

	template <typename T>
	bool WaitForTypedBindingMessage(
		FAngelscriptDebuggerTestClient& Client,
		EDebugMessageType ExpectedType,
		float TimeoutSeconds,
		TOptional<T>& OutValue,
		FString& OutError,
		const TCHAR* Context)
	{
		OutValue = Client.WaitForTypedMessage<T>(ExpectedType, TimeoutSeconds);
		if (!OutValue.IsSet())
		{
			OutError = FString::Printf(TEXT("%s: %s"), Context, *Client.GetLastError());
			return false;
		}

		return true;
	}

	struct FBindingStopMonitorResult
	{
		TArray<FAngelscriptDebugMessageEnvelope> StopEnvelopes;
		TOptional<FStoppedMessage> StopMessage;
		TOptional<FAngelscriptCallStack> Callstack;
		int32 ContinuedCount = 0;
		bool bTimedOut = false;
		FString Error;
	};

	struct FBindingNoStopMonitorResult
	{
		int32 UnexpectedStopCount = 0;
		int32 ContinuedCount = 0;
		bool bTimedOut = false;
		FString Error;
	};

	TFuture<FBindingStopMonitorResult> StartBindingStopMonitor(
		int32 Port,
		TAtomic<bool>& bMonitorReady,
		TAtomic<bool>& bShouldStop,
		float TimeoutSeconds)
	{
		return Async(EAsyncExecution::ThreadPool,
			[Port, &bMonitorReady, &bShouldStop, TimeoutSeconds]() -> FBindingStopMonitorResult
			{
				FBindingStopMonitorResult Result;
				FAngelscriptDebuggerTestClient MonitorClient;
				ON_SCOPE_EXIT
				{
					MonitorClient.SendStopDebugging();
					MonitorClient.SendDisconnect();
					MonitorClient.Disconnect();
				};

				if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
				{
					Result.Error = FString::Printf(TEXT("Binding monitor failed to connect: %s"), *MonitorClient.GetLastError());
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

				TOptional<FAngelscriptDebugMessageEnvelope> StopEnvelope = MonitorClient.WaitForMessageType(EDebugMessageType::HasStopped, TimeoutSeconds);
				if (!StopEnvelope.IsSet())
				{
					Result.Error = FString::Printf(TEXT("Binding monitor timed out waiting for HasStopped: %s"), *MonitorClient.GetLastError());
					Result.bTimedOut = !bShouldStop.Load();
					return Result;
				}

				Result.StopEnvelopes.Add(StopEnvelope.GetValue());
				Result.StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(StopEnvelope.GetValue());
				if (!Result.StopMessage.IsSet())
				{
					Result.Error = TEXT("Binding monitor failed to deserialize the HasStopped payload.");
					return Result;
				}

				if (!MonitorClient.SendRequestCallStack() ||
					!WaitForTypedBindingMessage(MonitorClient, EDebugMessageType::CallStack, TimeoutSeconds, Result.Callstack, Result.Error, TEXT("Binding monitor failed to receive CallStack")))
				{
					Result.bTimedOut = !bShouldStop.Load();
					return Result;
				}

				if (!MonitorClient.SendContinue())
				{
					Result.Error = FString::Printf(TEXT("Binding monitor failed to send Continue: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				TOptional<FEmptyMessage> ContinuedMessage;
				if (!WaitForTypedBindingMessage(MonitorClient, EDebugMessageType::HasContinued, TimeoutSeconds, ContinuedMessage, Result.Error, TEXT("Binding monitor failed to receive HasContinued")))
				{
					Result.bTimedOut = !bShouldStop.Load();
					return Result;
				}

				Result.ContinuedCount = 1;
				return Result;
			});
	}

	TFuture<FBindingNoStopMonitorResult> StartBindingNoStopMonitor(
		int32 Port,
		TAtomic<bool>& bMonitorReady,
		TAtomic<bool>& bShouldStop,
		TAtomic<bool>& bInvocationCompleted,
		float TimeoutSeconds)
	{
		return Async(EAsyncExecution::ThreadPool,
			[Port, &bMonitorReady, &bShouldStop, &bInvocationCompleted, TimeoutSeconds]() -> FBindingNoStopMonitorResult
			{
				FBindingNoStopMonitorResult Result;
				FAngelscriptDebuggerTestClient MonitorClient;
				ON_SCOPE_EXIT
				{
					MonitorClient.SendStopDebugging();
					MonitorClient.SendDisconnect();
					MonitorClient.Disconnect();
				};

				if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
				{
					Result.Error = FString::Printf(TEXT("Binding no-stop monitor failed to connect: %s"), *MonitorClient.GetLastError());
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
							if (!MonitorClient.SendContinue())
							{
								Result.Error = FString::Printf(TEXT("Binding no-stop monitor failed to send Continue after an unexpected stop: %s"), *MonitorClient.GetLastError());
								return Result;
							}

							TOptional<FEmptyMessage> ContinuedMessage;
							if (!WaitForTypedBindingMessage(MonitorClient, EDebugMessageType::HasContinued, TimeoutSeconds, ContinuedMessage, Result.Error, TEXT("Binding no-stop monitor failed to receive HasContinued after an unexpected stop")))
							{
								Result.bTimedOut = !bShouldStop.Load();
								return Result;
							}

							++Result.ContinuedCount;
						}
					}
					else if (!MonitorClient.GetLastError().IsEmpty())
					{
						Result.Error = FString::Printf(TEXT("Binding no-stop monitor failed while waiting for invocation completion: %s"), *MonitorClient.GetLastError());
						return Result;
					}

					FPlatformProcess::Sleep(0.001f);
				}

				if (!bInvocationCompleted.Load() && !bShouldStop.Load())
				{
					Result.bTimedOut = true;
					Result.Error = TEXT("Binding no-stop monitor timed out waiting for invocation completion.");
				}

				return Result;
			});
	}

	struct FBindingFixtureRuntime
	{
		UClass* GeneratedClass = nullptr;
		UFunction* TriggerDebugBreakFunction = nullptr;
		UFunction* TriggerEnsureFunction = nullptr;
		UObject* Object = nullptr;
	};

	bool ResolveBindingFixtureRuntime(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FAngelscriptDebuggerScriptFixture& Fixture,
		FBindingFixtureRuntime& OutRuntime)
	{
		OutRuntime.GeneratedClass = Fixture.FindGeneratedClass(Engine);
		if (!CheckNotNull(Test, TEXT("Debugger.Binding.DebugBreakAndEnsure should resolve the generated binding fixture class"), OutRuntime.GeneratedClass))
		{
			return false;
		}

		OutRuntime.TriggerDebugBreakFunction = Fixture.FindGeneratedFunction(Engine, TEXT("TriggerDebugBreak"));
		if (!CheckNotNull(Test, TEXT("Debugger.Binding.DebugBreakAndEnsure should resolve TriggerDebugBreak on the generated binding fixture"), OutRuntime.TriggerDebugBreakFunction))
		{
			return false;
		}

		OutRuntime.TriggerEnsureFunction = Fixture.FindGeneratedFunction(Engine, TEXT("TriggerEnsure"));
		if (!CheckNotNull(Test, TEXT("Debugger.Binding.DebugBreakAndEnsure should resolve TriggerEnsure on the generated binding fixture"), OutRuntime.TriggerEnsureFunction))
		{
			return false;
		}

		OutRuntime.Object = NewObject<UObject>(GetTransientPackage(), OutRuntime.GeneratedClass);
		return CheckNotNull(Test, TEXT("Debugger.Binding.DebugBreakAndEnsure should create a runtime UObject from the generated binding fixture class"), OutRuntime.Object);
	}

	struct FBindingCheckFixtureRuntime
	{
		UFunction* TriggerCheckFunction = nullptr;
		UObject* Object = nullptr;
	};

	bool ResolveBindingCheckFixtureRuntime(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FAngelscriptDebuggerScriptFixture& Fixture,
		FBindingCheckFixtureRuntime& OutRuntime)
	{
		UClass* GeneratedClass = Fixture.FindGeneratedClass(Engine);
		if (!CheckNotNull(Test, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should resolve the generated binding fixture class"), GeneratedClass))
		{
			return false;
		}

		OutRuntime.TriggerCheckFunction = Fixture.FindGeneratedFunction(Engine, TEXT("TriggerCheck"));
		if (!CheckNotNull(Test, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should resolve TriggerCheck on the generated binding fixture"), OutRuntime.TriggerCheckFunction))
		{
			return false;
		}

		OutRuntime.Object = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		return CheckNotNull(Test, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should create a runtime UObject from the generated binding fixture class"), OutRuntime.Object);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerBindingTests,
	"Angelscript.TestModule.Debugger.Binding",
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

	TEST_METHOD(DebugBreakAndEnsure)
	{
		using namespace AngelscriptDebuggerBindingTests_Private;

		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBindingFixture();
		FAngelscriptEngine& Engine = Ctx.GetEngine();

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine)));

		FBindingFixtureRuntime Runtime;
		ASSERT_THAT(IsTrue(ResolveBindingFixtureRuntime(*TestRunner, Engine, Fixture, Runtime)));

		// --- Debug break phase ---
		FBindingStopMonitorResult DebugBreakMonitorResult;
		bool bDebugBreakInvocationSucceeded = false;
		{
			Ctx.Client.DrainPendingMessages();

			TAtomic<bool> bMonitorReady(false);
			TAtomic<bool> bMonitorShouldStop(false);
			TFuture<FBindingStopMonitorResult> MonitorFuture = StartBindingStopMonitor(
				Ctx.GetPort(),
				bMonitorReady,
				bMonitorShouldStop,
				Ctx.GetDefaultTimeoutSeconds());
			ON_SCOPE_EXIT
			{
				bMonitorShouldStop = true;
				if (MonitorFuture.IsValid())
				{
					MonitorFuture.Wait();
				}
			};

			ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
				TEXT("Debugger.Binding.DebugBreakAndEnsure should bring the debug-break monitor up before invoking TriggerDebugBreak"))));

			const TSharedRef<FAsyncGeneratedVoidInvocationState> InvocationState = DispatchGeneratedVoidInvocation(
				Engine,
				Runtime.Object,
				Runtime.TriggerDebugBreakFunction);
			ASSERT_THAT(IsTrue(WaitForBindingInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
				TEXT("Debugger.Binding.DebugBreakAndEnsure should finish TriggerDebugBreak after the monitor resumes execution"))));

			bDebugBreakInvocationSucceeded = InvocationState->bSucceeded;
			bMonitorShouldStop = true;
			DebugBreakMonitorResult = MonitorFuture.Get();
		}

		ASSERT_THAT(IsTrue(DebugBreakMonitorResult.Error.IsEmpty()));
		if (!DebugBreakMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(DebugBreakMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(DebugBreakMonitorResult.bTimedOut));

		ASSERT_THAT(IsTrue(DebugBreakMonitorResult.StopEnvelopes.Num() == 1));

		ASSERT_THAT(IsTrue(DebugBreakMonitorResult.StopMessage.IsSet()));

		ASSERT_THAT(IsTrue(DebugBreakMonitorResult.Callstack.IsSet()));

		const FAngelscriptCallStack& DebugBreakCallstack = DebugBreakMonitorResult.Callstack.GetValue();
		ASSERT_THAT(IsTrue(DebugBreakCallstack.Frames.Num() > 0));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), DebugBreakMonitorResult.StopMessage->Reason, TEXT("Debugger.Binding.DebugBreakAndEnsure should report a breakpoint stop for TriggerDebugBreak")));
		ASSERT_THAT(IsTrue(DebugBreakCallstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Binding.DebugBreakAndEnsure should report the binding fixture filename for TriggerDebugBreak")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BindingDebugBreakLine")), DebugBreakCallstack.Frames[0].LineNumber, TEXT("Debugger.Binding.DebugBreakAndEnsure should stop TriggerDebugBreak at BindingDebugBreakLine")));
		ASSERT_THAT(AreEqual(1, DebugBreakMonitorResult.ContinuedCount, TEXT("Debugger.Binding.DebugBreakAndEnsure should observe a single HasContinued after TriggerDebugBreak")));
		ASSERT_THAT(IsTrue(bDebugBreakInvocationSucceeded, TEXT("Debugger.Binding.DebugBreakAndEnsure should execute TriggerDebugBreak successfully after resume")));

		// --- Ensure phase (first invocation) ---
		TestRunner->AddExpectedError(TEXT("Ensure condition failed: Once"), EAutomationExpectedErrorFlags::Contains, 1);

		FBindingStopMonitorResult EnsureMonitorResult;
		bool bEnsureInvocationSucceeded = false;
		bool bEnsureReturnValue = true;
		{
			Ctx.Client.DrainPendingMessages();

			TAtomic<bool> bMonitorReady(false);
			TAtomic<bool> bMonitorShouldStop(false);
			TFuture<FBindingStopMonitorResult> MonitorFuture = StartBindingStopMonitor(
				Ctx.GetPort(),
				bMonitorReady,
				bMonitorShouldStop,
				Ctx.GetDefaultTimeoutSeconds());
			ON_SCOPE_EXIT
			{
				bMonitorShouldStop = true;
				if (MonitorFuture.IsValid())
				{
					MonitorFuture.Wait();
				}
			};

			ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
				TEXT("Debugger.Binding.DebugBreakAndEnsure should bring the ensure monitor up before invoking TriggerEnsure(false, Once)"))));

			const TSharedRef<FAsyncGeneratedBoolInvocationState> InvocationState = DispatchGeneratedBoolInvocation(
				Engine,
				Runtime.Object,
				Runtime.TriggerEnsureFunction,
				false,
				TEXT("Once"));
			ASSERT_THAT(IsTrue(WaitForBindingInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
				TEXT("Debugger.Binding.DebugBreakAndEnsure should finish TriggerEnsure(false, Once) after the monitor resumes execution"))));

			bEnsureInvocationSucceeded = InvocationState->bSucceeded;
			bEnsureReturnValue = InvocationState->bReturnValue;
			bMonitorShouldStop = true;
			EnsureMonitorResult = MonitorFuture.Get();
		}

		ASSERT_THAT(IsTrue(EnsureMonitorResult.Error.IsEmpty()));
		if (!EnsureMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(EnsureMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(EnsureMonitorResult.bTimedOut));

		ASSERT_THAT(IsTrue(EnsureMonitorResult.StopEnvelopes.Num() == 1));

		ASSERT_THAT(IsTrue(EnsureMonitorResult.StopMessage.IsSet()));

		ASSERT_THAT(IsTrue(EnsureMonitorResult.Callstack.IsSet()));

		const FAngelscriptCallStack& EnsureCallstack = EnsureMonitorResult.Callstack.GetValue();
		ASSERT_THAT(IsTrue(EnsureCallstack.Frames.Num() > 0));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), EnsureMonitorResult.StopMessage->Reason, TEXT("Debugger.Binding.DebugBreakAndEnsure should report a breakpoint stop for TriggerEnsure(false, Once)")));
		ASSERT_THAT(IsTrue(EnsureCallstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Binding.DebugBreakAndEnsure should report the binding fixture filename for TriggerEnsure(false, Once)")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BindingEnsureLine")), EnsureCallstack.Frames[0].LineNumber, TEXT("Debugger.Binding.DebugBreakAndEnsure should stop TriggerEnsure(false, Once) at BindingEnsureLine")));
		ASSERT_THAT(AreEqual(1, EnsureMonitorResult.ContinuedCount, TEXT("Debugger.Binding.DebugBreakAndEnsure should observe a single HasContinued after TriggerEnsure(false, Once)")));
		ASSERT_THAT(IsTrue(bEnsureInvocationSucceeded, TEXT("Debugger.Binding.DebugBreakAndEnsure should execute TriggerEnsure(false, Once) successfully after resume")));
		ASSERT_THAT(IsFalse(bEnsureReturnValue, TEXT("Debugger.Binding.DebugBreakAndEnsure should return false after TriggerEnsure(false, Once)")));

		// --- Ensure phase (repeat invocation - should NOT stop again) ---
		FBindingNoStopMonitorResult EnsureRepeatMonitorResult;
		bool bEnsureRepeatInvocationSucceeded = false;
		bool bEnsureRepeatReturnValue = true;
		{
			Ctx.Client.DrainPendingMessages();

			TAtomic<bool> bMonitorReady(false);
			TAtomic<bool> bMonitorShouldStop(false);
			TAtomic<bool> bInvocationCompleted(false);
			TFuture<FBindingNoStopMonitorResult> MonitorFuture = StartBindingNoStopMonitor(
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

			ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
				TEXT("Debugger.Binding.DebugBreakAndEnsure should bring the repeat-ensure monitor up before invoking TriggerEnsure(false, Repeat)"))));

			const TSharedRef<FAsyncGeneratedBoolInvocationState> InvocationState = DispatchGeneratedBoolInvocation(
				Engine,
				Runtime.Object,
				Runtime.TriggerEnsureFunction,
				false,
				TEXT("Repeat"));
			ASSERT_THAT(IsTrue(WaitForBindingInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
				TEXT("Debugger.Binding.DebugBreakAndEnsure should finish TriggerEnsure(false, Repeat) without needing another stop"))));

			bEnsureRepeatInvocationSucceeded = InvocationState->bSucceeded;
			bEnsureRepeatReturnValue = InvocationState->bReturnValue;
			bInvocationCompleted = true;
			bMonitorShouldStop = true;
			EnsureRepeatMonitorResult = MonitorFuture.Get();
		}

		ASSERT_THAT(IsTrue(EnsureRepeatMonitorResult.Error.IsEmpty()));
		if (!EnsureRepeatMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(EnsureRepeatMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(EnsureRepeatMonitorResult.bTimedOut));

		ASSERT_THAT(AreEqual(0, EnsureRepeatMonitorResult.UnexpectedStopCount, TEXT("Debugger.Binding.DebugBreakAndEnsure should not emit another stop for the same ensure location in the same session")));
		ASSERT_THAT(AreEqual(0, EnsureRepeatMonitorResult.ContinuedCount, TEXT("Debugger.Binding.DebugBreakAndEnsure should not need any extra HasContinued messages during the repeat ensure phase")));
		ASSERT_THAT(IsTrue(bEnsureRepeatInvocationSucceeded, TEXT("Debugger.Binding.DebugBreakAndEnsure should execute TriggerEnsure(false, Repeat) successfully without another stop")));
		ASSERT_THAT(IsFalse(bEnsureRepeatReturnValue, TEXT("Debugger.Binding.DebugBreakAndEnsure should still return false after TriggerEnsure(false, Repeat)")));
	}

	TEST_METHOD(CheckBreaksEveryInvocation)
	{
		using namespace AngelscriptDebuggerBindingTests_Private;

		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBindingFixture();
		FAngelscriptEngine& Engine = Ctx.GetEngine();

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine)));

		FBindingCheckFixtureRuntime Runtime;
		ASSERT_THAT(IsTrue(ResolveBindingCheckFixtureRuntime(*TestRunner, Engine, Fixture, Runtime)));

		// --- First check invocation (CheckA) ---
		TestRunner->AddExpectedError(TEXT("Check condition failed: CheckA"), EAutomationExpectedErrorFlags::Contains, 1);

		FBindingStopMonitorResult CheckAMonitorResult;
		bool bCheckAInvocationSucceeded = false;
		{
			Ctx.Client.DrainPendingMessages();

			TAtomic<bool> bMonitorReady(false);
			TAtomic<bool> bMonitorShouldStop(false);
			TFuture<FBindingStopMonitorResult> MonitorFuture = StartBindingStopMonitor(
				Ctx.GetPort(),
				bMonitorReady,
				bMonitorShouldStop,
				Ctx.GetDefaultTimeoutSeconds());
			ON_SCOPE_EXIT
			{
				bMonitorShouldStop = true;
				if (MonitorFuture.IsValid())
				{
					MonitorFuture.Wait();
				}
			};

			ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
				TEXT("Debugger.Binding.CheckBreaksEveryInvocation should bring the first check monitor up before invoking TriggerCheck(false, CheckA)"))));

			const TSharedRef<FAsyncGeneratedVoidInvocationState> InvocationState = DispatchGeneratedVoidInvocation(
				Engine,
				Runtime.Object,
				Runtime.TriggerCheckFunction,
				false,
				TEXT("CheckA"));
			ASSERT_THAT(IsTrue(WaitForBindingInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
				TEXT("Debugger.Binding.CheckBreaksEveryInvocation should finish TriggerCheck(false, CheckA) after the monitor resumes execution"))));

			bCheckAInvocationSucceeded = InvocationState->bSucceeded;
			bMonitorShouldStop = true;
			CheckAMonitorResult = MonitorFuture.Get();
		}

		ASSERT_THAT(IsTrue(CheckAMonitorResult.Error.IsEmpty()));
		if (!CheckAMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(CheckAMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(CheckAMonitorResult.bTimedOut));

		ASSERT_THAT(IsTrue(CheckAMonitorResult.StopEnvelopes.Num() == 1));

		ASSERT_THAT(IsTrue(CheckAMonitorResult.StopMessage.IsSet()));

		ASSERT_THAT(IsTrue(CheckAMonitorResult.Callstack.IsSet()));

		const FAngelscriptCallStack& CheckACallstack = CheckAMonitorResult.Callstack.GetValue();
		ASSERT_THAT(IsTrue(CheckACallstack.Frames.Num() > 0));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), CheckAMonitorResult.StopMessage->Reason, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should report a breakpoint stop for TriggerCheck(false, CheckA)")));
		ASSERT_THAT(IsTrue(CheckACallstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Binding.CheckBreaksEveryInvocation should report the binding fixture filename for TriggerCheck(false, CheckA)")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BindingCheckLine")), CheckACallstack.Frames[0].LineNumber, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should stop TriggerCheck(false, CheckA) at BindingCheckLine")));
		ASSERT_THAT(AreEqual(1, CheckAMonitorResult.ContinuedCount, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should observe a single HasContinued after TriggerCheck(false, CheckA)")));
		ASSERT_THAT(IsTrue(bCheckAInvocationSucceeded, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should execute TriggerCheck(false, CheckA) successfully after resume")));

		// --- Second check invocation (CheckB) - should STILL stop ---
		TestRunner->AddExpectedError(TEXT("Check condition failed: CheckB"), EAutomationExpectedErrorFlags::Contains, 1);

		FBindingStopMonitorResult CheckBMonitorResult;
		bool bCheckBInvocationSucceeded = false;
		{
			Ctx.Client.DrainPendingMessages();

			TAtomic<bool> bMonitorReady(false);
			TAtomic<bool> bMonitorShouldStop(false);
			TFuture<FBindingStopMonitorResult> MonitorFuture = StartBindingStopMonitor(
				Ctx.GetPort(),
				bMonitorReady,
				bMonitorShouldStop,
				Ctx.GetDefaultTimeoutSeconds());
			ON_SCOPE_EXIT
			{
				bMonitorShouldStop = true;
				if (MonitorFuture.IsValid())
				{
					MonitorFuture.Wait();
				}
			};

			ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
				TEXT("Debugger.Binding.CheckBreaksEveryInvocation should bring the second check monitor up before invoking TriggerCheck(false, CheckB)"))));

			const TSharedRef<FAsyncGeneratedVoidInvocationState> InvocationState = DispatchGeneratedVoidInvocation(
				Engine,
				Runtime.Object,
				Runtime.TriggerCheckFunction,
				false,
				TEXT("CheckB"));
			ASSERT_THAT(IsTrue(WaitForBindingInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
				TEXT("Debugger.Binding.CheckBreaksEveryInvocation should finish TriggerCheck(false, CheckB) after the monitor resumes execution"))));

			bCheckBInvocationSucceeded = InvocationState->bSucceeded;
			bMonitorShouldStop = true;
			CheckBMonitorResult = MonitorFuture.Get();
		}

		ASSERT_THAT(IsTrue(CheckBMonitorResult.Error.IsEmpty()));
		if (!CheckBMonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(CheckBMonitorResult.Error);
		}

		ASSERT_THAT(IsFalse(CheckBMonitorResult.bTimedOut));

		ASSERT_THAT(IsTrue(CheckBMonitorResult.StopEnvelopes.Num() == 1));

		ASSERT_THAT(IsTrue(CheckBMonitorResult.StopMessage.IsSet()));

		ASSERT_THAT(IsTrue(CheckBMonitorResult.Callstack.IsSet()));

		const FAngelscriptCallStack& CheckBCallstack = CheckBMonitorResult.Callstack.GetValue();
		ASSERT_THAT(IsTrue(CheckBCallstack.Frames.Num() > 0));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), CheckBMonitorResult.StopMessage->Reason, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should report a breakpoint stop for TriggerCheck(false, CheckB)")));
		ASSERT_THAT(IsTrue(CheckBCallstack.Frames[0].Source.EndsWith(Fixture.Filename), TEXT("Debugger.Binding.CheckBreaksEveryInvocation should report the binding fixture filename for TriggerCheck(false, CheckB)")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("BindingCheckLine")), CheckBCallstack.Frames[0].LineNumber, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should stop TriggerCheck(false, CheckB) at BindingCheckLine")));
		ASSERT_THAT(AreEqual(1, CheckBMonitorResult.ContinuedCount, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should observe a single HasContinued after TriggerCheck(false, CheckB)")));
		ASSERT_THAT(IsTrue(bCheckBInvocationSucceeded, TEXT("Debugger.Binding.CheckBreaksEveryInvocation should execute TriggerCheck(false, CheckB) successfully after resume")));
	}
};

#endif
