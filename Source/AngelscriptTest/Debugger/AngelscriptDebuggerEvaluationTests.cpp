#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerTestSession.h"
#include "AngelscriptTestEngineHelper.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptDebuggerEvaluationTests_Private
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

	struct FAsyncGeneratedInvocationState : public TSharedFromThis<FAsyncGeneratedInvocationState>
	{
		TAtomic<bool> bCompleted = false;
		bool bResolvedClass = false;
		bool bResolvedFunction = false;
		bool bCreatedObject = false;
		bool bSucceeded = false;
		int32 Result = 0;
	};

	TSharedRef<FAsyncGeneratedInvocationState> DispatchGeneratedIntInvocation(
		FAngelscriptEngine& Engine,
		const FAngelscriptDebuggerScriptFixture& Fixture)
	{
		TSharedRef<FAsyncGeneratedInvocationState> State = MakeShared<FAsyncGeneratedInvocationState>();

		AsyncTask(ENamedThreads::GameThread, [&Engine, Fixture, State]()
		{
			UClass* GeneratedClass = Fixture.FindGeneratedClass(Engine);
			State->bResolvedClass = GeneratedClass != nullptr;

			UFunction* EntryFunction = Fixture.FindGeneratedFunction(Engine, Fixture.EntryFunctionName);
			State->bResolvedFunction = EntryFunction != nullptr;

			if (GeneratedClass != nullptr && EntryFunction != nullptr)
			{
				UObject* Object = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
				State->bCreatedObject = Object != nullptr;

				if (Object != nullptr)
				{
					int32 InvocationResult = 0;
					State->bSucceeded = ExecuteGeneratedIntEventOnGameThread(&Engine, Object, EntryFunction, InvocationResult);
					State->Result = InvocationResult;
				}
			}

			State->bCompleted = true;
		});

		return State;
	}

	bool WaitForGeneratedInvocationCompletion(
		FAutomationTestBase& Test,
		FAngelscriptDebuggerTestSession& Session,
		const TSharedRef<FAsyncGeneratedInvocationState>& InvocationState,
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
	bool WaitForTypedMessage(
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

	bool AssertLegacyVariablePayload(FAutomationTestBase& Test, const FAngelscriptVariable& Variable, const TCHAR* Context)
	{
		const FString AddressContext = FString::Printf(TEXT("%s should report ValueAddress = 0 for adapter v1"), Context);
		const FString SizeContext = FString::Printf(TEXT("%s should report ValueSize = 0 for adapter v1"), Context);
		const bool bAddressMatches = CheckEqual(Test, *AddressContext, Variable.ValueAddress, static_cast<uint64>(0));
		const bool bSizeMatches = CheckEqual(Test, *SizeContext, Variable.ValueSize, static_cast<uint8>(0));
		return bAddressMatches && bSizeMatches;
	}

	bool AssertLegacyVariablePayloads(
		FAutomationTestBase& Test,
		const TArray<FAngelscriptVariable>& Variables,
		const TCHAR* Context)
	{
		bool bAllMatch = true;
		for (const FAngelscriptVariable& Variable : Variables)
		{
			const FString VariableContext = FString::Printf(TEXT("%s variable '%s'"), Context, *Variable.Name);
			bAllMatch &= AssertLegacyVariablePayload(Test, Variable, *VariableContext);
		}

		return bAllMatch;
	}

	struct FEvaluationMonitorResult
	{
		TArray<FAngelscriptDebugMessageEnvelope> StopEnvelopes;
		TOptional<FStoppedMessage> StopMessage;
		TOptional<FAngelscriptCallStack> Callstack;
		TOptional<FAngelscriptVariable> LeafLocalValue;
		TOptional<FAngelscriptVariable> LeafCombinedValue;
		TOptional<FAngelscriptVariable> ThisMemberValue;
		TOptional<FAngelscriptVariable> ModuleGlobalCounterValue;
		TOptional<FAngelscriptVariables> LocalScopeVariables;
		TOptional<FAngelscriptVariables> ThisScopeVariables;
		TOptional<FAngelscriptVariables> ModuleScopeVariables;
		int32 ContinuedCount = 0;
		bool bTimedOut = false;
		FString Error;
	};

	TFuture<FEvaluationMonitorResult> StartEvaluationMonitor(
		int32 Port,
		TAtomic<bool>& bMonitorReady,
		TAtomic<bool>& bShouldStop,
		const FAngelscriptDebuggerScriptFixture& Fixture,
		float TimeoutSeconds,
		int32 AdapterVersion = 2)
	{
		return Async(EAsyncExecution::ThreadPool,
			[Port, &bMonitorReady, &bShouldStop, Fixture, TimeoutSeconds, AdapterVersion]() -> FEvaluationMonitorResult
			{
				FEvaluationMonitorResult Result;
				FAngelscriptDebuggerTestClient MonitorClient;
				ON_SCOPE_EXIT
				{
					MonitorClient.SendStopDebugging();
					MonitorClient.SendDisconnect();
					MonitorClient.Disconnect();
				};

				if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
				{
					Result.Error = FString::Printf(TEXT("Evaluation monitor failed to connect: %s"), *MonitorClient.GetLastError());
					bMonitorReady = true;
					return Result;
				}

				FString HandshakeError;
				if (!HandshakeMonitorClient(MonitorClient, bShouldStop, AdapterVersion, TimeoutSeconds, HandshakeError))
				{
					Result.Error = HandshakeError;
					Result.bTimedOut = true;
					bMonitorReady = true;
					return Result;
				}

				bMonitorReady = true;

				TOptional<FAngelscriptDebugMessageEnvelope> StopEnvelope = MonitorClient.WaitForMessageType(EDebugMessageType::HasStopped, TimeoutSeconds);
				if (!StopEnvelope.IsSet())
				{
					Result.Error = FString::Printf(TEXT("Evaluation monitor timed out waiting for HasStopped: %s"), *MonitorClient.GetLastError());
					Result.bTimedOut = true;
					return Result;
				}

				Result.StopEnvelopes.Add(StopEnvelope.GetValue());
				Result.StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(StopEnvelope.GetValue());
				if (!Result.StopMessage.IsSet())
				{
					Result.Error = TEXT("Evaluation monitor failed to deserialize the HasStopped payload.");
					return Result;
				}

				if (!MonitorClient.SendRequestCallStack() ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::CallStack, TimeoutSeconds, Result.Callstack, Result.Error, TEXT("Evaluation monitor failed to receive CallStack")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestEvaluate(Fixture.GetEvalPath(TEXT("LeafLocalValuePath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Evaluate, TimeoutSeconds, Result.LeafLocalValue, Result.Error, TEXT("Evaluation monitor failed to receive LocalValue evaluate reply")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestEvaluate(Fixture.GetEvalPath(TEXT("LeafCombinedPath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Evaluate, TimeoutSeconds, Result.LeafCombinedValue, Result.Error, TEXT("Evaluation monitor failed to receive Combined evaluate reply")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestEvaluate(Fixture.GetEvalPath(TEXT("ThisMemberValuePath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Evaluate, TimeoutSeconds, Result.ThisMemberValue, Result.Error, TEXT("Evaluation monitor failed to receive MemberValue evaluate reply")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestEvaluate(Fixture.GetEvalPath(TEXT("ModuleGlobalCounterPath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Evaluate, TimeoutSeconds, Result.ModuleGlobalCounterValue, Result.Error, TEXT("Evaluation monitor failed to receive GlobalCounter evaluate reply")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestVariables(Fixture.GetEvalPath(TEXT("LocalScopePath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Variables, TimeoutSeconds, Result.LocalScopeVariables, Result.Error, TEXT("Evaluation monitor failed to receive local scope variables")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestVariables(Fixture.GetEvalPath(TEXT("ThisScopePath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Variables, TimeoutSeconds, Result.ThisScopeVariables, Result.Error, TEXT("Evaluation monitor failed to receive this scope variables")))
				{
					return Result;
				}

				if (!MonitorClient.SendRequestVariables(Fixture.GetEvalPath(TEXT("ModuleScopePath"))) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Variables, TimeoutSeconds, Result.ModuleScopeVariables, Result.Error, TEXT("Evaluation monitor failed to receive module scope variables")))
				{
					return Result;
				}

				if (!MonitorClient.SendContinue())
				{
					Result.Error = FString::Printf(TEXT("Evaluation monitor failed to send Continue: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				if (!MonitorClient.WaitForMessageType(EDebugMessageType::HasContinued, TimeoutSeconds).IsSet())
				{
					Result.Error = FString::Printf(TEXT("Evaluation monitor timed out waiting for HasContinued: %s"), *MonitorClient.GetLastError());
					Result.bTimedOut = true;
					return Result;
				}

				Result.ContinuedCount = 1;
				return Result;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerEvaluationTests,
	"Angelscript.TestModule.Debugger.Evaluation",
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

	TEST_METHOD(ScopeValues)
	{
		using namespace AngelscriptDebuggerEvaluationTests_Private;

		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateCallstackFixture();
		FAngelscriptEngine& Engine = Ctx.GetEngine();

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Evaluation.ScopeValues should compile the callstack fixture")));

		TAtomic<bool> bMonitorReady(false);
		TFuture<FEvaluationMonitorResult> MonitorFuture = StartEvaluationMonitor(
			Ctx.GetPort(),
			bMonitorReady,
			Ctx.bMonitorShouldStop,
			Fixture,
			Ctx.GetDefaultTimeoutSeconds());
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (MonitorFuture.IsValid())
			{
				MonitorFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
			TEXT("Debugger.Evaluation.ScopeValues should bring the evaluation monitor up before execution"))));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Id = 7;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("CallstackLeafLine"));

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Evaluation.ScopeValues should send the leaf breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Debugger.Evaluation.ScopeValues should observe the breakpoint registration after both debugger clients finish handshaking"))));

		const TSharedRef<FAsyncGeneratedInvocationState> InvocationState = DispatchGeneratedIntInvocation(Engine, Fixture);
		ASSERT_THAT(IsTrue(WaitForGeneratedInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Debugger.Evaluation.ScopeValues should complete the generated invocation after the monitor resumes execution"))));

		Ctx.bMonitorShouldStop = true;
		const FEvaluationMonitorResult MonitorResult = MonitorFuture.Get();

		ASSERT_THAT(IsTrue(InvocationState->bResolvedClass, TEXT("Debugger.Evaluation.ScopeValues should resolve the generated class before invocation")));
		ASSERT_THAT(IsTrue(InvocationState->bResolvedFunction, TEXT("Debugger.Evaluation.ScopeValues should resolve the generated Entry function before invocation")));
		ASSERT_THAT(IsTrue(InvocationState->bCreatedObject, TEXT("Debugger.Evaluation.ScopeValues should create a generated UObject instance before invocation")));

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Evaluation.ScopeValues should finish without monitor errors"), MonitorResult.Error.IsEmpty()))
		{
			TestRunner->AddError(MonitorResult.Error);
			return;
		}

		ASSERT_THAT(IsFalse(MonitorResult.bTimedOut));
		ASSERT_THAT(AreEqual(1, MonitorResult.StopEnvelopes.Num(), TEXT("Debugger.Evaluation.ScopeValues should capture exactly one HasStopped event")));
		ASSERT_THAT(IsTrue(MonitorResult.StopMessage.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.Callstack.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.LeafLocalValue.IsSet() && MonitorResult.LeafCombinedValue.IsSet() && MonitorResult.ThisMemberValue.IsSet() && MonitorResult.ModuleGlobalCounterValue.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.LocalScopeVariables.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.ThisScopeVariables.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.ModuleScopeVariables.IsSet()));

		const FAngelscriptCallStack& Callstack = MonitorResult.Callstack.GetValue();
		ASSERT_THAT(IsTrue(Callstack.Frames.Num() >= 3));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), MonitorResult.StopMessage->Reason, TEXT("Debugger.Evaluation.ScopeValues should stop because of a breakpoint")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("CallstackLeafLine")), Callstack.Frames[0].LineNumber, TEXT("Debugger.Evaluation.ScopeValues should report the leaf line on the top frame")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("CallstackMiddleLine")), Callstack.Frames[1].LineNumber, TEXT("Debugger.Evaluation.ScopeValues should report the middle line on the second frame")));
		ASSERT_THAT(AreEqual(Fixture.GetLine(TEXT("CallstackEntryLine")), Callstack.Frames[2].LineNumber, TEXT("Debugger.Evaluation.ScopeValues should report the entry line on the third frame")));
		ASSERT_THAT(AreEqual(FString(TEXT("4")), MonitorResult.LeafLocalValue->Value, TEXT("Debugger.Evaluation.ScopeValues should evaluate LocalValue to 4")));
		ASSERT_THAT(AreEqual(FString(TEXT("16")), MonitorResult.LeafCombinedValue->Value, TEXT("Debugger.Evaluation.ScopeValues should evaluate Combined to 16")));
		ASSERT_THAT(AreEqual(FString(TEXT("5")), MonitorResult.ThisMemberValue->Value, TEXT("Debugger.Evaluation.ScopeValues should evaluate this.MemberValue to 5")));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), MonitorResult.ModuleGlobalCounterValue->Value, TEXT("Debugger.Evaluation.ScopeValues should evaluate %module%.GlobalCounter to 7")));
		ASSERT_THAT(AreEqual(1, MonitorResult.ContinuedCount, TEXT("Debugger.Evaluation.ScopeValues should observe a single HasContinued after resuming")));
		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Evaluation.ScopeValues should finish the generated invocation successfully")));
		ASSERT_THAT(AreEqual(16, InvocationState->Result, TEXT("Debugger.Evaluation.ScopeValues should preserve the generated invocation return value")));

		const TArray<FAngelscriptVariable>& LocalVariables = MonitorResult.LocalScopeVariables->Variables;
		const TArray<FAngelscriptVariable>& ThisVariables = MonitorResult.ThisScopeVariables->Variables;
		const TArray<FAngelscriptVariable>& ModuleVariables = MonitorResult.ModuleScopeVariables->Variables;
		const FAngelscriptVariable* LocalValueVariable = LocalVariables.FindByPredicate([](const FAngelscriptVariable& Variable)
		{
			return Variable.Name == TEXT("LocalValue");
		});
		const FAngelscriptVariable* CombinedVariable = LocalVariables.FindByPredicate([](const FAngelscriptVariable& Variable)
		{
			return Variable.Name == TEXT("Combined");
		});
		const FAngelscriptVariable* MemberValueVariable = ThisVariables.FindByPredicate([](const FAngelscriptVariable& Variable)
		{
			return Variable.Name == TEXT("MemberValue");
		});
		const FAngelscriptVariable* GlobalCounterVariable = ModuleVariables.FindByPredicate([](const FAngelscriptVariable& Variable)
		{
			return Variable.Name == TEXT("GlobalCounter");
		});

		ASSERT_THAT(IsNotNull(LocalValueVariable));
		ASSERT_THAT(IsNotNull(CombinedVariable));
		ASSERT_THAT(IsNotNull(MemberValueVariable));
		ASSERT_THAT(IsNotNull(GlobalCounterVariable));

		ASSERT_THAT(AreEqual(FString(TEXT("4")), LocalValueVariable->Value, TEXT("Debugger.Evaluation.ScopeValues should report LocalValue = 4 in the local scope")));
		ASSERT_THAT(AreEqual(FString(TEXT("16")), CombinedVariable->Value, TEXT("Debugger.Evaluation.ScopeValues should report Combined = 16 in the local scope")));
		ASSERT_THAT(AreEqual(FString(TEXT("5")), MemberValueVariable->Value, TEXT("Debugger.Evaluation.ScopeValues should report MemberValue = 5 in the this scope")));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), GlobalCounterVariable->Value, TEXT("Debugger.Evaluation.ScopeValues should report GlobalCounter = 7 in the module scope")));
		ASSERT_THAT(IsTrue(MemberValueVariable->ValueAddress != 0, TEXT("Debugger.Evaluation.ScopeValues should expose a non-zero ValueAddress for this.MemberValue through %this% scope")));
		ASSERT_THAT(IsTrue(GlobalCounterVariable->ValueAddress != 0, TEXT("Debugger.Evaluation.ScopeValues should expose a non-zero ValueAddress for %module%.GlobalCounter through module scope")));
	}

	TEST_METHOD(AdapterV1LegacyPayload)
	{
		using namespace AngelscriptDebuggerEvaluationTests_Private;

		// Re-initialize with adapter version 1
		Ctx.TearDown();
		ASSERT_THAT(IsTrue(Ctx.SetUp(*TestRunner, /*AdapterVersion=*/ 1)));

		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateCallstackFixture();
		FAngelscriptEngine& Engine = Ctx.GetEngine();

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine), TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should compile the callstack fixture")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Id = 20;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("CallstackLeafLine"));

		TAtomic<bool> bMonitorReady(false);
		TFuture<FEvaluationMonitorResult> MonitorFuture = StartEvaluationMonitor(
			Ctx.GetPort(),
			bMonitorReady,
			Ctx.bMonitorShouldStop,
			Fixture,
			Ctx.GetDefaultTimeoutSeconds(),
			1);
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (MonitorFuture.IsValid())
			{
				MonitorFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady,
			TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should bring the evaluation monitor up before execution"))));

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should send the leaf breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint)))
		{
			TestRunner->AddError(Ctx.Client.GetLastError());
			return;
		}

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should observe the breakpoint registration after both debugger clients finish handshaking"))));

		const TSharedRef<FAsyncGeneratedInvocationState> InvocationState = DispatchGeneratedIntInvocation(Engine, Fixture);
		ASSERT_THAT(IsTrue(WaitForGeneratedInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should complete the generated invocation after resume"))));

		Ctx.bMonitorShouldStop = true;
		const FEvaluationMonitorResult MonitorResult = MonitorFuture.Get();

		ASSERT_THAT(IsTrue(InvocationState->bResolvedClass, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should resolve the generated class before invocation")));
		ASSERT_THAT(IsTrue(InvocationState->bResolvedFunction, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should resolve the generated Entry function before invocation")));
		ASSERT_THAT(IsTrue(InvocationState->bCreatedObject, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should create a generated UObject instance before invocation")));

		if (!CheckTrue(*TestRunner, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should finish without monitor errors"), MonitorResult.Error.IsEmpty()))
		{
			TestRunner->AddError(MonitorResult.Error);
			return;
		}

		ASSERT_THAT(IsFalse(MonitorResult.bTimedOut));
		ASSERT_THAT(AreEqual(1, MonitorResult.StopEnvelopes.Num(), TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should capture exactly one HasStopped event")));
		ASSERT_THAT(IsTrue(MonitorResult.StopMessage.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.LeafCombinedValue.IsSet()));
		ASSERT_THAT(IsTrue(MonitorResult.LocalScopeVariables.IsSet()));

		ASSERT_THAT(AreEqual(FString(TEXT("breakpoint")), MonitorResult.StopMessage->Reason, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should stop because of a breakpoint")));
		ASSERT_THAT(AreEqual(FString(TEXT("Combined")), MonitorResult.LeafCombinedValue->Name, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should evaluate Name = Combined")));
		ASSERT_THAT(AreEqual(FString(TEXT("16")), MonitorResult.LeafCombinedValue->Value, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should evaluate Value = 16")));
		ASSERT_THAT(AreEqual(FString(TEXT("int")), MonitorResult.LeafCombinedValue->Type, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should evaluate Type = int")));

		if (!AssertLegacyVariablePayload(*TestRunner, MonitorResult.LeafCombinedValue.GetValue(), TEXT("Debugger.Evaluation.AdapterV1LegacyPayload evaluate reply")))
		{
			return;
		}

		const TArray<FAngelscriptVariable>& LocalVariables = MonitorResult.LocalScopeVariables->Variables;
		const FAngelscriptVariable* LocalValueVariable = LocalVariables.FindByPredicate([](const FAngelscriptVariable& Variable)
		{
			return Variable.Name == TEXT("LocalValue");
		});
		const FAngelscriptVariable* LocalCombinedVariable = LocalVariables.FindByPredicate([](const FAngelscriptVariable& Variable)
		{
			return Variable.Name == TEXT("Combined");
		});

		ASSERT_THAT(IsNotNull(LocalValueVariable));
		ASSERT_THAT(IsNotNull(LocalCombinedVariable));

		ASSERT_THAT(AreEqual(FString(TEXT("4")), LocalValueVariable->Value, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should report LocalValue = 4 in the local scope")));
		ASSERT_THAT(AreEqual(FString(TEXT("16")), LocalCombinedVariable->Value, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should report Combined = 16 in the local scope")));

		if (!AssertLegacyVariablePayloads(*TestRunner, LocalVariables, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload local scope")))
		{
			return;
		}

		if (MonitorResult.LeafLocalValue.IsSet() &&
			!AssertLegacyVariablePayload(*TestRunner, MonitorResult.LeafLocalValue.GetValue(), TEXT("Debugger.Evaluation.AdapterV1LegacyPayload local evaluate reply")))
		{
			return;
		}

		if (MonitorResult.ThisMemberValue.IsSet() &&
			!AssertLegacyVariablePayload(*TestRunner, MonitorResult.ThisMemberValue.GetValue(), TEXT("Debugger.Evaluation.AdapterV1LegacyPayload this-member evaluate reply")))
		{
			return;
		}

		if (MonitorResult.ModuleGlobalCounterValue.IsSet() &&
			!AssertLegacyVariablePayload(*TestRunner, MonitorResult.ModuleGlobalCounterValue.GetValue(), TEXT("Debugger.Evaluation.AdapterV1LegacyPayload module-global evaluate reply")))
		{
			return;
		}

		if (MonitorResult.ThisScopeVariables.IsSet() &&
			!AssertLegacyVariablePayloads(*TestRunner, MonitorResult.ThisScopeVariables->Variables, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload this scope")))
		{
			return;
		}

		if (MonitorResult.ModuleScopeVariables.IsSet() &&
			!AssertLegacyVariablePayloads(*TestRunner, MonitorResult.ModuleScopeVariables->Variables, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload module scope")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(InvocationState->bSucceeded, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should finish the generated invocation successfully")));
		ASSERT_THAT(AreEqual(16, InvocationState->Result, TEXT("Debugger.Evaluation.AdapterV1LegacyPayload should preserve the generated invocation return value")));
	}
};

#endif
