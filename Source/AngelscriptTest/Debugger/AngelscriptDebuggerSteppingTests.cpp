#include "CQTest.h"
#include "Shared/AngelscriptDebuggerTestContext.h"
#include "Shared/AngelscriptDebuggerTestMonitor.h"
#include "Shared/AngelscriptDebuggerScriptFixture.h"
#include "Shared/AngelscriptDebuggerTestHelpers.h"

#include "Core/AngelscriptEngine.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS


// =============================================================================
// Local helpers for StepOutTopFrameCompletes (unique monitor pattern)
// =============================================================================

namespace AngelscriptDebuggerSteppingTests_Private
{
	struct FStepOutTopFrameMonitorResult
	{
		TOptional<FAngelscriptDebugMessageEnvelope> InitialStopEnvelope;
		TOptional<FAngelscriptCallStack> InitialCallstack;
		int32 UnexpectedStopCount = 0;
		bool bTimedOut = false;
		FString Error;
	};

	TFuture<FStepOutTopFrameMonitorResult> StartActionThenExpectCompletionMonitor(
		int32 Port,
		TAtomic<bool>& bMonitorReady,
		TAtomic<bool>& bShouldStop,
		TAtomic<bool>& bInvocationCompleted,
		float TimeoutSeconds)
	{
		return Async(EAsyncExecution::ThreadPool,
			[Port, &bMonitorReady, &bShouldStop, &bInvocationCompleted, TimeoutSeconds]() -> FStepOutTopFrameMonitorResult
			{
				FStepOutTopFrameMonitorResult Result;
				FAngelscriptDebuggerTestClient MonitorClient;
				if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
				{
					Result.Error = FString::Printf(TEXT("StepOut top-frame monitor failed to connect: %s"), *MonitorClient.GetLastError());
					bMonitorReady = true;
					return Result;
				}

				ON_SCOPE_EXIT
				{
					MonitorClient.SendStopDebugging();
					MonitorClient.SendDisconnect();
					MonitorClient.Disconnect();
				};

				const double HandshakeEnd = FPlatformTime::Seconds() + TimeoutSeconds;
				bool bSentStart = false;
				bool bReceivedVersion = false;
				while (FPlatformTime::Seconds() < HandshakeEnd && !bShouldStop.Load())
				{
					if (!bSentStart)
					{
						bSentStart = MonitorClient.SendStartDebugging(2);
					}

					if (bSentStart)
					{
						TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
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
					Result.Error = TEXT("StepOut top-frame monitor timed out waiting for DebugServerVersion.");
					Result.bTimedOut = true;
					bMonitorReady = true;
					return Result;
				}

				bMonitorReady = true;

				const double InitialStopEnd = FPlatformTime::Seconds() + TimeoutSeconds;
				while (FPlatformTime::Seconds() < InitialStopEnd && !bShouldStop.Load())
				{
					TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
					if (!Envelope.IsSet())
					{
						FPlatformProcess::Sleep(0.001f);
						continue;
					}

					if (Envelope->MessageType == EDebugMessageType::HasStopped)
					{
						Result.InitialStopEnvelope = MoveTemp(Envelope);
						break;
					}
				}

				if (!Result.InitialStopEnvelope.IsSet())
				{
					Result.Error = TEXT("StepOut top-frame monitor timed out waiting for the initial HasStopped.");
					Result.bTimedOut = true;
					return Result;
				}

				if (!MonitorClient.SendRequestCallStack())
				{
					Result.Error = FString::Printf(TEXT("StepOut top-frame monitor failed to request the initial callstack: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				const double CallstackEnd = FPlatformTime::Seconds() + 10.0;
				while (FPlatformTime::Seconds() < CallstackEnd && !bShouldStop.Load())
				{
					TOptional<FAngelscriptDebugMessageEnvelope> Envelope = MonitorClient.ReceiveEnvelope();
					if (Envelope.IsSet() && Envelope->MessageType == EDebugMessageType::CallStack)
					{
						Result.InitialCallstack = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptCallStack>(Envelope.GetValue());
						break;
					}

					FPlatformProcess::Sleep(0.001f);
				}

				if (!Result.InitialCallstack.IsSet())
				{
					Result.Error = TEXT("StepOut top-frame monitor failed to receive the initial callstack.");
					return Result;
				}

				if (!MonitorClient.SendStepOut())
				{
					Result.Error = FString::Printf(TEXT("StepOut top-frame monitor failed to send StepOut: %s"), *MonitorClient.GetLastError());
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
					if (!Envelope.IsSet())
					{
						FPlatformProcess::Sleep(0.001f);
						continue;
					}

					if (Envelope->MessageType == EDebugMessageType::HasStopped)
					{
						++Result.UnexpectedStopCount;
						if (Result.Error.IsEmpty())
						{
							Result.Error = FString::Printf(TEXT("StepOut top-frame monitor received an unexpected HasStopped after StepOut (count: %d)."), Result.UnexpectedStopCount);
						}

						MonitorClient.SendContinue();
					}
				}

				Result.bTimedOut = !bInvocationCompleted.Load();
				if (Result.bTimedOut && Result.Error.IsEmpty())
				{
					Result.Error = TEXT("StepOut top-frame monitor timed out waiting for invocation completion.");
				}

				return Result;
			});
	}

	// =========================================================================
	// Cross-file stepping fixture (two source files in one module)
	// =========================================================================

	struct FCrossFileScriptSection
	{
		FString Filename;
		FString AbsoluteFilename;
		FString ScriptSource;
	};

	struct FCrossFileSteppingFixture
	{
		FName ModuleName;
		FString EntryFunctionDeclaration;
		FString EntryFilename;
		FString CalleeFilename;
		TArray<FCrossFileScriptSection> Sections;
		TMap<FName, int32> LineMarkers;

		bool Compile(FAngelscriptEngine& Engine) const
		{
			TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
			ModuleDesc->ModuleName = ModuleName.ToString();

			for (const FCrossFileScriptSection& SectionData : Sections)
			{
				FAngelscriptModuleDesc::FCodeSection& Section = ModuleDesc->Code.AddDefaulted_GetRef();
				Section.RelativeFilename = FPaths::Combine(TEXT("Automation"), SectionData.Filename);
				Section.AbsoluteFilename = SectionData.AbsoluteFilename;
				Section.Code = SectionData.ScriptSource;
				Section.CodeHash = static_cast<int64>(FCrc::StrCrc32(*Section.Code));
				ModuleDesc->CodeHash ^= Section.CodeHash;
			}

			TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile;
			ModulesToCompile.Add(ModuleDesc);

			TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
			TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
			FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
			FAngelscriptEngineScope EngineScope(Engine);
			const ECompileResult CompileResult = Engine.CompileModules(ECompileType::SoftReloadOnly, ModulesToCompile, CompiledModules);
			return CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
		}

		int32 GetLine(FName Marker) const
		{
			const int32* Line = LineMarkers.Find(Marker);
			check(Line != nullptr);
			return *Line;
		}
	};

	FCrossFileScriptSection MakeCrossFileSection(const FString& Filename, const FString& RawScriptSource, TMap<FName, int32>& InOutLineMarkers)
	{
		TArray<FString> Lines;
		RawScriptSource.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString& Line = Lines[LineIndex];
			int32 MarkerStart = INDEX_NONE;
			while (Line.FindChar(TEXT('/'), MarkerStart))
			{
				if (!Line.Mid(MarkerStart).StartsWith(TEXT("/*MARK:")))
				{
					MarkerStart += 1;
					continue;
				}

				const int32 MarkerNameStart = MarkerStart + 7;
				const int32 MarkerEnd = Line.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, MarkerNameStart);
				if (MarkerEnd == INDEX_NONE)
				{
					break;
				}

				const FString MarkerName = Line.Mid(MarkerNameStart, MarkerEnd - MarkerNameStart);
				InOutLineMarkers.Add(FName(*MarkerName), LineIndex + 1);
				Line.RemoveAt(MarkerStart, (MarkerEnd - MarkerStart) + 2, EAllowShrinking::No);
			}
		}

		FCrossFileScriptSection Section;
		Section.Filename = Filename;
		Section.AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Filename);
		Section.ScriptSource = FString::Join(Lines, TEXT("\n"));
		return Section;
	}

	FCrossFileSteppingFixture CreateCrossFileSteppingFixture()
	{
		FCrossFileSteppingFixture Fixture;
		Fixture.ModuleName = TEXT("DebuggerCrossFileSteppingFixture");
		Fixture.EntryFunctionDeclaration = TEXT("int RunTestCase()");
		Fixture.EntryFilename = TEXT("DebuggerCrossFileSteppingA.as");
		Fixture.CalleeFilename = TEXT("DebuggerCrossFileSteppingB.as");
		Fixture.Sections.Add(MakeCrossFileSection(
			Fixture.CalleeFilename,
			TEXT(R"AS(int Inner(int Value)
{
	/*MARK:StepInnerEntryLine*/ int StoredValue = 9;
	int InnerValue = Value + StoredValue;
	return InnerValue;
}
)AS"),
			Fixture.LineMarkers));
		Fixture.Sections.Add(MakeCrossFileSection(
			Fixture.EntryFilename,
			TEXT(R"AS(int RunTestCase()
{
	int StartValue = 4;
	int Seed = StartValue + 1;
	int Offset = Seed * 2;
	int Warmup = Offset - 1;
	int Guard = Warmup + 3;
	/*MARK:StepCallLine*/ int Result = Inner(Guard);
	/*MARK:StepAfterCallLine*/ Result += 1;
	return Result;
}
)AS"),
			Fixture.LineMarkers));
		return Fixture;
	}

	bool AssertTopFrameMatches(
		FAutomationTestBase& Test,
		const TOptional<FAngelscriptCallStack>& Callstack,
		const FString& ExpectedSource,
		int32 ExpectedLine,
		const TCHAR* Context)
	{
		const FString CallstackContext = FString::Printf(TEXT("%s should include a callstack"), Context);
		if (!Test.TestTrue(*CallstackContext, Callstack.IsSet()))
		{
			return false;
		}

		const FString FrameContext = FString::Printf(TEXT("%s should include at least one frame"), Context);
		if (!Test.TestTrue(*FrameContext, Callstack->Frames.Num() > 0))
		{
			return false;
		}

		const FAngelscriptCallFrame& TopFrame = Callstack->Frames[0];
		bool bPassed = true;
		const FString SourceContext = FString::Printf(TEXT("%s should report the expected source file"), Context);
		bPassed &= Test.TestTrue(
			*SourceContext,
			FPaths::IsSamePath(TopFrame.Source, ExpectedSource));
		const FString LineContext = FString::Printf(TEXT("%s should report the expected line"), Context);
		bPassed &= Test.TestEqual(
			*LineContext,
			TopFrame.LineNumber,
			ExpectedLine);
		return bPassed;
	}

	bool AssertStopReason(
		FAutomationTestBase& Test,
		const FAngelscriptDebugMessageEnvelope& Envelope,
		const FString& ExpectedReason,
		const TCHAR* Context)
	{
		const TOptional<FStoppedMessage> StoppedMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(Envelope);
		const FString DeserializeContext = FString::Printf(TEXT("%s should deserialize into a stopped message"), Context);
		if (!Test.TestTrue(*DeserializeContext, StoppedMessage.IsSet()))
		{
			return false;
		}

		const FString ReasonContext = FString::Printf(TEXT("%s should report the expected stop reason"), Context);
		return Test.TestEqual(*ReasonContext, StoppedMessage->Reason, ExpectedReason);
	}

	bool AssertFrameMatches(
		FAutomationTestBase& Test,
		const TOptional<FAngelscriptCallStack>& Callstack,
		int32 FrameIndex,
		const FString& ExpectedFilename,
		int32 ExpectedLine,
		const TCHAR* Context)
	{
		const FString CallstackContext = FString::Printf(TEXT("%s should include a callstack"), Context);
		if (!Test.TestTrue(*CallstackContext, Callstack.IsSet()))
		{
			return false;
		}

		const FString FrameCountContext = FString::Printf(TEXT("%s should include frame %d"), Context, FrameIndex);
		if (!Test.TestTrue(*FrameCountContext, Callstack->Frames.IsValidIndex(FrameIndex)))
		{
			return false;
		}

		const FAngelscriptCallFrame& Frame = Callstack->Frames[FrameIndex];
		bool bPassed = true;
		const FString SourceContext = FString::Printf(TEXT("%s should stay on the expected source file"), Context);
		bPassed &= Test.TestEqual(*SourceContext, FPaths::GetCleanFilename(Frame.Source), ExpectedFilename);
		const FString LineContext = FString::Printf(TEXT("%s should report the expected line"), Context);
		bPassed &= Test.TestEqual(*LineContext, Frame.LineNumber, ExpectedLine);
		return bPassed;
	}
}

// =============================================================================
// FAngelscriptDebuggerSteppingTests — 8 tests merged from 5 source files
// =============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerSteppingTests,
	"Angelscript.TestModule.Debugger.Stepping",
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

	// =========================================================================
	// 1. StepIn — from AngelscriptDebuggerSteppingTests.cpp
	// =========================================================================

	TEST_METHOD(StepIn)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateSteppingFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Stepping.StepIn should compile the stepping fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepIn, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		TFuture<FStepMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Stepping.StepIn should bring the step monitor up before execution")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Stepping.StepIn should set the call-site breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Stepping.StepIn should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Stepping.StepIn should finish after monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		const FStepMonitorResult MonitorResult = MonitorFuture.Get();
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 2,
			TEXT("Stepping.StepIn should emit exactly 2 stops")));

		const TOptional<FStoppedMessage> FirstStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[0].StopEnvelope);
		const TOptional<FStoppedMessage> SecondStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[1].StopEnvelope);
		TestRunner->TestTrue(TEXT("Stepping.StepIn first stop should deserialize"), FirstStop.IsSet());
		TestRunner->TestTrue(TEXT("Stepping.StepIn second stop should deserialize"), SecondStop.IsSet());
		if (FirstStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Stepping.StepIn first stop should be a breakpoint"), FirstStop->Reason, FString(TEXT("breakpoint")));
		}
		if (SecondStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Stepping.StepIn second stop should be a step"), SecondStop->Reason, FString(TEXT("step")));
		}

		if (TestRunner->TestTrue(TEXT("Stepping.StepIn first stop should have a callstack"), MonitorResult.Stops[0].Callstack.IsSet()))
		{
			TestRunner->TestEqual(TEXT("Stepping.StepIn should first stop at the call line"),
				MonitorResult.Stops[0].Callstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepCallLine")));
		}

		if (TestRunner->TestTrue(TEXT("Stepping.StepIn second stop should have a callstack"), MonitorResult.Stops[1].Callstack.IsSet()))
		{
			TestRunner->TestTrue(TEXT("Stepping.StepIn should enter the callee frame"), MonitorResult.Stops[1].Callstack->Frames.Num() >= 2);
			TestRunner->TestEqual(TEXT("Stepping.StepIn should land inside Inner()"),
				MonitorResult.Stops[1].Callstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepInnerEntryLine")));
		}

		TestRunner->TestTrue(TEXT("Stepping.StepIn should execute successfully"), InvocationState->bSucceeded);
	}

	// =========================================================================
	// 2. StepOver — from AngelscriptDebuggerSteppingTests.cpp
	// =========================================================================

	TEST_METHOD(StepOver)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateSteppingFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Stepping.StepOver should compile the stepping fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepOver, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		TFuture<FStepMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Stepping.StepOver should bring the step monitor up before execution")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Stepping.StepOver should set the call-site breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Stepping.StepOver should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Stepping.StepOver should finish after monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		const FStepMonitorResult MonitorResult = MonitorFuture.Get();
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 2,
			TEXT("Stepping.StepOver should emit exactly 2 stops")));

		if (TestRunner->TestTrue(TEXT("Stepping.StepOver first stop should have a callstack"), MonitorResult.Stops[0].Callstack.IsSet()))
		{
			TestRunner->TestEqual(TEXT("Stepping.StepOver should first stop at the call line"),
				MonitorResult.Stops[0].Callstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepCallLine")));
		}

		if (TestRunner->TestTrue(TEXT("Stepping.StepOver second stop should have a callstack"), MonitorResult.Stops[1].Callstack.IsSet()))
		{
			TestRunner->TestEqual(TEXT("Stepping.StepOver should land at the line after the call"),
				MonitorResult.Stops[1].Callstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepAfterCallLine")));

			if (MonitorResult.Stops[0].Callstack.IsSet())
			{
				TestRunner->TestEqual(TEXT("Stepping.StepOver should stay in the same frame depth"),
					MonitorResult.Stops[1].Callstack->Frames.Num(),
					MonitorResult.Stops[0].Callstack->Frames.Num());
			}
		}

		TestRunner->TestTrue(TEXT("Stepping.StepOver should execute successfully"), InvocationState->bSucceeded);
	}

	// =========================================================================
	// 3. StepOut — from AngelscriptDebuggerSteppingTests.cpp
	// =========================================================================

	TEST_METHOD(StepOut)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateSteppingFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Stepping.StepOut should compile the stepping fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepIn, true });
		Phases.Add({ EStepMonitorAction::StepOut, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		TFuture<FStepMonitorResult> MonitorFuture;
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Stepping.StepOut should bring the step monitor up before execution")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Stepping.StepOut should set the call-site breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Stepping.StepOut should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Stepping.StepOut should finish after monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		const FStepMonitorResult MonitorResult = MonitorFuture.Get();
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 3,
			TEXT("Stepping.StepOut should emit exactly 3 stops")));

		if (TestRunner->TestTrue(TEXT("Stepping.StepOut second stop should have a callstack"), MonitorResult.Stops[1].Callstack.IsSet()))
		{
			TestRunner->TestTrue(TEXT("Stepping.StepOut should be inside the callee before stepping out"),
				MonitorResult.Stops[1].Callstack->Frames.Num() >= 2);
			TestRunner->TestEqual(TEXT("Stepping.StepOut should enter Inner() before stepping out"),
				MonitorResult.Stops[1].Callstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepInnerEntryLine")));
		}

		if (TestRunner->TestTrue(TEXT("Stepping.StepOut third stop should have a callstack"), MonitorResult.Stops[2].Callstack.IsSet()))
		{
			TestRunner->TestEqual(TEXT("Stepping.StepOut should return to the line after the call"),
				MonitorResult.Stops[2].Callstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepAfterCallLine")));
			if (MonitorResult.Stops[1].Callstack.IsSet())
			{
				TestRunner->TestTrue(TEXT("Stepping.StepOut should reduce stack depth after returning"),
					MonitorResult.Stops[2].Callstack->Frames.Num() < MonitorResult.Stops[1].Callstack->Frames.Num());
			}
		}

		TestRunner->TestTrue(TEXT("Stepping.StepOut should execute successfully"), InvocationState->bSucceeded);
	}

	// =========================================================================
	// 4. StepInOnStatementAdvancesWithinFrame — from AngelscriptDebuggerStepInStatementTests.cpp
	// =========================================================================

	TEST_METHOD(StepInOnStatementAdvancesWithinFrame)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateSteppingFixture();
		TFuture<FStepMonitorResult> MonitorFuture;
		bool bMonitorStarted = false;
		bool bMonitorResultConsumed = false;
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (bMonitorStarted && MonitorFuture.IsValid() && !bMonitorResultConsumed)
			{
				MonitorFuture.Wait();
			}
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Debugger StepIn-on-statement test should compile the stepping fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepIn, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Debugger StepIn-on-statement test should bring the step monitor up before execution")));
		bMonitorStarted = true;

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepInnerEntryLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Debugger StepIn-on-statement test should set the callee breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Debugger StepIn-on-statement test should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Debugger StepIn-on-statement test should finish after the monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		const FStepMonitorResult MonitorResult = MonitorFuture.Get();
		bMonitorResultConsumed = true;
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		TestRunner->TestFalse(TEXT("Debugger StepIn-on-statement monitor should not time out"), MonitorResult.bTimedOut);
		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 2,
			TEXT("Debugger StepIn-on-statement test should emit exactly 2 stops")));

		AssertStopReason(*TestRunner, MonitorResult.Stops[0].StopEnvelope, TEXT("breakpoint"), TEXT("Debugger StepIn-on-statement first stop"));
		AssertStopReason(*TestRunner, MonitorResult.Stops[1].StopEnvelope, TEXT("step"), TEXT("Debugger StepIn-on-statement second stop"));

		const int32 StepInnerEntryLine = Fixture.GetLine(TEXT("StepInnerEntryLine"));
		const int32 StepInnerLine = Fixture.GetLine(TEXT("StepInnerLine"));
		const int32 StepCallLine = Fixture.GetLine(TEXT("StepCallLine"));
		const int32 StepAfterCallLine = Fixture.GetLine(TEXT("StepAfterCallLine"));

		if (AssertFrameMatches(
			*TestRunner,
			MonitorResult.Stops[0].Callstack,
			0,
			Fixture.Filename,
			StepInnerEntryLine,
			TEXT("Debugger StepIn-on-statement first stop top frame")))
		{
			TestRunner->TestEqual(
				TEXT("Debugger StepIn-on-statement first stop should expose caller and callee frames"),
				MonitorResult.Stops[0].Callstack->Frames.Num(),
				2);
			AssertFrameMatches(
				*TestRunner,
				MonitorResult.Stops[0].Callstack,
				1,
				Fixture.Filename,
				StepCallLine,
				TEXT("Debugger StepIn-on-statement first stop caller frame"));
		}

		if (AssertFrameMatches(
			*TestRunner,
			MonitorResult.Stops[1].Callstack,
			0,
			Fixture.Filename,
			StepInnerLine,
			TEXT("Debugger StepIn-on-statement second stop top frame")))
		{
			TestRunner->TestEqual(
				TEXT("Debugger StepIn-on-statement second stop should stay inside the same frame depth"),
				MonitorResult.Stops[1].Callstack->Frames.Num(),
				2);
			AssertFrameMatches(
				*TestRunner,
				MonitorResult.Stops[1].Callstack,
				1,
				Fixture.Filename,
				StepCallLine,
				TEXT("Debugger StepIn-on-statement second stop caller frame"));
			TestRunner->TestTrue(
				TEXT("Debugger StepIn-on-statement second stop should advance beyond the entry line"),
				MonitorResult.Stops[1].Callstack->Frames[0].LineNumber != StepInnerEntryLine);
			TestRunner->TestTrue(
				TEXT("Debugger StepIn-on-statement second stop should not jump to the after-call line"),
				MonitorResult.Stops[1].Callstack->Frames[0].LineNumber != StepAfterCallLine);
		}

		TestRunner->TestTrue(TEXT("Debugger StepIn-on-statement test should execute successfully"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Debugger StepIn-on-statement test should preserve the stepping fixture result"), InvocationState->Result, 14);
	}

	// =========================================================================
	// 5. StepOutTopFrameCompletes — from AngelscriptDebuggerStepOutEdgeTests.cpp
	// =========================================================================

	TEST_METHOD(StepOutTopFrameCompletes)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateSteppingFixture();
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Stepping.StepOutTopFrameCompletes should compile the stepping fixture")));

		TAtomic<bool> bInvocationCompleted{ false };
		TAtomic<bool> bMonitorReady{ false };
		TFuture<FStepOutTopFrameMonitorResult> MonitorFuture;

		MonitorFuture = StartActionThenExpectCompletionMonitor(
			Ctx.GetPort(),
			bMonitorReady,
			Ctx.bMonitorShouldStop,
			bInvocationCompleted,
			Ctx.GetDefaultTimeoutSeconds());

		const bool bReady = Ctx.Session.PumpUntil(
			[&bMonitorReady]()
			{
				return bMonitorReady.Load();
			},
			Ctx.GetDefaultTimeoutSeconds());

		ASSERT_THAT(IsTrue(bReady,
			TEXT("Stepping.StepOutTopFrameCompletes should bring the monitor up before execution")));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepAfterCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Stepping.StepOutTopFrameCompletes should set the top-frame breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Stepping.StepOutTopFrameCompletes should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Stepping.StepOutTopFrameCompletes should finish after the top-frame StepOut"))));

		bInvocationCompleted = true;
		Ctx.bMonitorShouldStop = true;

		const FStepOutTopFrameMonitorResult MonitorResult = MonitorFuture.Get();
		if (!TestRunner->TestTrue(TEXT("Stepping.StepOutTopFrameCompletes should keep the completion monitor error-free"), MonitorResult.Error.IsEmpty()))
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		ASSERT_THAT(IsTrue(!MonitorResult.bTimedOut,
			TEXT("Stepping.StepOutTopFrameCompletes should not time out while waiting for completion")));

		ASSERT_THAT(IsTrue(MonitorResult.UnexpectedStopCount == 0,
			TEXT("Stepping.StepOutTopFrameCompletes should not receive any extra HasStopped messages after StepOut")));

		const TOptional<FStoppedMessage> InitialStop = MonitorResult.InitialStopEnvelope.IsSet()
			? FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.InitialStopEnvelope.GetValue())
			: TOptional<FStoppedMessage>();
		ASSERT_THAT(IsTrue(InitialStop.IsSet(),
			TEXT("Stepping.StepOutTopFrameCompletes should deserialize the initial HasStopped payload")));

		if (InitialStop.IsSet())
		{
			TestRunner->TestEqual(
				TEXT("Stepping.StepOutTopFrameCompletes should first stop because of the breakpoint"),
				InitialStop->Reason,
				FString(TEXT("breakpoint")));
		}

		ASSERT_THAT(IsTrue(MonitorResult.InitialCallstack.IsSet(),
			TEXT("Stepping.StepOutTopFrameCompletes should receive the initial callstack")));

		if (MonitorResult.InitialCallstack.IsSet())
		{
			TestRunner->TestEqual(
				TEXT("Stepping.StepOutTopFrameCompletes should stop on the line after the call before issuing StepOut"),
				MonitorResult.InitialCallstack->Frames[0].LineNumber,
				Fixture.GetLine(TEXT("StepAfterCallLine")));
			TestRunner->TestEqual(
				TEXT("Stepping.StepOutTopFrameCompletes should stay in the top frame when the breakpoint hits after the call"),
				MonitorResult.InitialCallstack->Frames.Num(),
				1);
		}

		TestRunner->TestTrue(TEXT("Stepping.StepOutTopFrameCompletes should execute successfully"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Stepping.StepOutTopFrameCompletes should preserve the expected test case result"), InvocationState->Result, 14);
	}

	// =========================================================================
	// 6. StepOverWithinCallee — from AngelscriptDebuggerStepOverInFunctionTests.cpp
	// =========================================================================

	TEST_METHOD(StepOverWithinCallee)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateSteppingFixture();
		TFuture<FStepMonitorResult> MonitorFuture;
		bool bMonitorStarted = false;
		bool bMonitorResultConsumed = false;
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (bMonitorStarted && MonitorFuture.IsValid() && !bMonitorResultConsumed)
			{
				MonitorFuture.Wait();
			}
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Debugger StepOver-within-callee test should compile the stepping fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepIn, true });
		Phases.Add({ EStepMonitorAction::StepOver, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Debugger StepOver-within-callee test should bring the step monitor up before execution")));
		bMonitorStarted = true;

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Debugger StepOver-within-callee test should set the caller breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Debugger StepOver-within-callee test should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Fixture.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Debugger StepOver-within-callee test should finish after the monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		const FStepMonitorResult MonitorResult = MonitorFuture.Get();
		bMonitorResultConsumed = true;
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		TestRunner->TestFalse(TEXT("Debugger StepOver-within-callee monitor should not time out"), MonitorResult.bTimedOut);
		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 3,
			TEXT("Debugger StepOver-within-callee test should emit exactly 3 stops")));

		AssertStopReason(*TestRunner, MonitorResult.Stops[0].StopEnvelope, TEXT("breakpoint"), TEXT("Debugger StepOver-within-callee first stop"));
		AssertStopReason(*TestRunner, MonitorResult.Stops[1].StopEnvelope, TEXT("step"), TEXT("Debugger StepOver-within-callee second stop"));
		AssertStopReason(*TestRunner, MonitorResult.Stops[2].StopEnvelope, TEXT("step"), TEXT("Debugger StepOver-within-callee third stop"));

		if (AssertFrameMatches(
			*TestRunner,
			MonitorResult.Stops[0].Callstack,
			0,
			Fixture.Filename,
			Fixture.GetLine(TEXT("StepCallLine")),
			TEXT("Debugger StepOver-within-callee first stop top frame")))
		{
			TestRunner->TestEqual(
				TEXT("Debugger StepOver-within-callee first stop should stay at caller depth"),
				MonitorResult.Stops[0].Callstack->Frames.Num(),
				1);
		}

		if (AssertFrameMatches(
			*TestRunner,
			MonitorResult.Stops[1].Callstack,
			0,
			Fixture.Filename,
			Fixture.GetLine(TEXT("StepInnerEntryLine")),
			TEXT("Debugger StepOver-within-callee second stop top frame")))
		{
			TestRunner->TestEqual(
				TEXT("Debugger StepOver-within-callee second stop should expose caller and callee frames"),
				MonitorResult.Stops[1].Callstack->Frames.Num(),
				2);
			AssertFrameMatches(
				*TestRunner,
				MonitorResult.Stops[1].Callstack,
				1,
				Fixture.Filename,
				Fixture.GetLine(TEXT("StepCallLine")),
				TEXT("Debugger StepOver-within-callee second stop caller frame"));
		}

		if (AssertFrameMatches(
			*TestRunner,
			MonitorResult.Stops[2].Callstack,
			0,
			Fixture.Filename,
			Fixture.GetLine(TEXT("StepInnerLine")),
			TEXT("Debugger StepOver-within-callee third stop top frame")))
		{
			TestRunner->TestEqual(
				TEXT("Debugger StepOver-within-callee third stop should stay inside the callee"),
				MonitorResult.Stops[2].Callstack->Frames.Num(),
				2);
			AssertFrameMatches(
				*TestRunner,
				MonitorResult.Stops[2].Callstack,
				1,
				Fixture.Filename,
				Fixture.GetLine(TEXT("StepCallLine")),
				TEXT("Debugger StepOver-within-callee third stop caller frame"));
			TestRunner->TestTrue(
				TEXT("Debugger StepOver-within-callee third stop should not jump to the after-call line"),
				MonitorResult.Stops[2].Callstack->Frames[0].LineNumber != Fixture.GetLine(TEXT("StepAfterCallLine")));
		}

		TestRunner->TestTrue(TEXT("Debugger StepOver-within-callee test should execute successfully"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Debugger StepOver-within-callee test should preserve the stepping fixture result"), InvocationState->Result, 14);
	}

	// =========================================================================
	// 7. CrossFileStepping — from AngelscriptDebuggerCrossFileSteppingTests.cpp
	// =========================================================================

	TEST_METHOD(CrossFileStepping)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FCrossFileSteppingFixture Fixture = CreateCrossFileSteppingFixture();
		TFuture<FStepMonitorResult> MonitorFuture;
		bool bMonitorStarted = false;
		bool bMonitorResultConsumed = false;
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (bMonitorStarted && MonitorFuture.IsValid() && !bMonitorResultConsumed)
			{
				MonitorFuture.Wait();
			}
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Cross-file stepping should compile the multi-file fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepIn, true });
		Phases.Add({ EStepMonitorAction::StepOut, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Cross-file stepping should bring the step monitor up before execution")));
		bMonitorStarted = true;

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Fixture.EntryFilename);
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Cross-file stepping should set the caller-side breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Cross-file stepping should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Breakpoint.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Cross-file stepping should finish after the monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		FStepMonitorResult MonitorResult = MonitorFuture.Get();
		bMonitorResultConsumed = true;

		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		TestRunner->TestFalse(TEXT("Cross-file stepping should not time out"), MonitorResult.bTimedOut);
		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 3,
			TEXT("Cross-file stepping should emit exactly 3 stops")));

		const TOptional<FStoppedMessage> FirstStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[0].StopEnvelope);
		const TOptional<FStoppedMessage> SecondStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[1].StopEnvelope);
		const TOptional<FStoppedMessage> ThirdStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[2].StopEnvelope);
		TestRunner->TestTrue(TEXT("Cross-file stepping should deserialize the breakpoint stop"), FirstStop.IsSet());
		TestRunner->TestTrue(TEXT("Cross-file stepping should deserialize the step-in stop"), SecondStop.IsSet());
		TestRunner->TestTrue(TEXT("Cross-file stepping should deserialize the step-out stop"), ThirdStop.IsSet());
		if (FirstStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Cross-file stepping should first stop because of the breakpoint"), FirstStop->Reason, FString(TEXT("breakpoint")));
		}
		if (SecondStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Cross-file stepping should report StepIn as a step stop"), SecondStop->Reason, FString(TEXT("step")));
		}
		if (ThirdStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Cross-file stepping should report StepOut as a step stop"), ThirdStop->Reason, FString(TEXT("step")));
		}

		const FString EntryAbsolutePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Fixture.EntryFilename);
		const FString CalleeAbsolutePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Fixture.CalleeFilename);
		(void)AssertTopFrameMatches(*TestRunner, MonitorResult.Stops[0].Callstack, EntryAbsolutePath, Fixture.GetLine(TEXT("StepCallLine")), TEXT("Cross-file stepping first stop"));
		if (AssertTopFrameMatches(*TestRunner, MonitorResult.Stops[1].Callstack, CalleeAbsolutePath, Fixture.GetLine(TEXT("StepInnerEntryLine")), TEXT("Cross-file stepping second stop")))
		{
			TestRunner->TestTrue(TEXT("Cross-file stepping should enter a deeper stack frame after StepIn"), MonitorResult.Stops[1].Callstack->Frames.Num() >= 2);
		}
		if (AssertTopFrameMatches(*TestRunner, MonitorResult.Stops[2].Callstack, EntryAbsolutePath, Fixture.GetLine(TEXT("StepAfterCallLine")), TEXT("Cross-file stepping third stop"))
			&& MonitorResult.Stops[1].Callstack.IsSet())
		{
			TestRunner->TestTrue(TEXT("Cross-file stepping should reduce stack depth after StepOut"),
				MonitorResult.Stops[2].Callstack->Frames.Num() < MonitorResult.Stops[1].Callstack->Frames.Num());
		}

		TestRunner->TestTrue(TEXT("Cross-file stepping should execute successfully"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Cross-file stepping should return the expected final result"), InvocationState->Result, 22);
	}

	// =========================================================================
	// 8. CrossFileStepOverStaysInCaller — from AngelscriptDebuggerCrossFileSteppingTests.cpp
	// =========================================================================

	TEST_METHOD(CrossFileStepOverStaysInCaller)
	{
		using namespace AngelscriptDebuggerSteppingTests_Private;

		FAngelscriptEngine& Engine = Ctx.GetEngine();
		const FCrossFileSteppingFixture Fixture = CreateCrossFileSteppingFixture();
		TFuture<FStepMonitorResult> MonitorFuture;
		bool bMonitorStarted = false;
		bool bMonitorResultConsumed = false;
		ON_SCOPE_EXIT
		{
			Ctx.bMonitorShouldStop = true;
			if (bMonitorStarted && MonitorFuture.IsValid() && !bMonitorResultConsumed)
			{
				MonitorFuture.Wait();
			}
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(Fixture.Compile(Engine),
			TEXT("Cross-file StepOver should compile the multi-file fixture")));

		TArray<FStepMonitorPhase> Phases;
		Phases.Add({ EStepMonitorAction::StepOver, true });
		Phases.Add({ EStepMonitorAction::Continue, true });

		TAtomic<bool> bMonitorReady{ false };
		ASSERT_THAT(IsTrue(StartAndWaitForStepMonitorReady(
			*TestRunner, Ctx.Session, Ctx.GetPort(), bMonitorReady, Ctx.bMonitorShouldStop, MoveTemp(Phases), MonitorFuture),
			TEXT("Cross-file StepOver should bring the step monitor up before execution")));
		bMonitorStarted = true;

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Fixture.EntryFilename);
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("StepCallLine"));
		ASSERT_THAT(IsTrue(Ctx.Client.SendSetBreakpoint(Breakpoint),
			TEXT("Cross-file StepOver should set the caller-side breakpoint")));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1,
			TEXT("Cross-file StepOver should observe the breakpoint registration"))));

		const TSharedRef<FAsyncModuleInvocationState> InvocationState = DispatchModuleInvocation(
			Engine,
			Breakpoint.Filename,
			Fixture.ModuleName,
			Fixture.EntryFunctionDeclaration);

		ASSERT_THAT(IsTrue(WaitForInvocationCompletion(*TestRunner, Ctx.Session, InvocationState,
			TEXT("Cross-file StepOver should finish after the monitor continues execution"))));

		Ctx.bMonitorShouldStop = true;
		FStepMonitorResult MonitorResult = MonitorFuture.Get();
		bMonitorResultConsumed = true;

		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		TestRunner->TestFalse(TEXT("Cross-file StepOver should not time out"), MonitorResult.bTimedOut);
		ASSERT_THAT(AreEqual(MonitorResult.Stops.Num(), 2,
			TEXT("Cross-file StepOver should emit exactly 2 stops")));

		const TOptional<FStoppedMessage> FirstStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[0].StopEnvelope);
		const TOptional<FStoppedMessage> SecondStop = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(MonitorResult.Stops[1].StopEnvelope);
		TestRunner->TestTrue(TEXT("Cross-file StepOver should deserialize the breakpoint stop"), FirstStop.IsSet());
		TestRunner->TestTrue(TEXT("Cross-file StepOver should deserialize the step stop"), SecondStop.IsSet());
		if (FirstStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Cross-file StepOver should first stop because of the breakpoint"), FirstStop->Reason, FString(TEXT("breakpoint")));
		}
		if (SecondStop.IsSet())
		{
			TestRunner->TestEqual(TEXT("Cross-file StepOver should report the second stop as a step"), SecondStop->Reason, FString(TEXT("step")));
		}

		const FString EntryAbsolutePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Fixture.EntryFilename);
		const FString CalleeAbsolutePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Fixture.CalleeFilename);
		const bool bFirstFrameMatched = AssertTopFrameMatches(
			*TestRunner,
			MonitorResult.Stops[0].Callstack,
			EntryAbsolutePath,
			Fixture.GetLine(TEXT("StepCallLine")),
			TEXT("Cross-file StepOver first stop"));
		const bool bSecondFrameMatched = AssertTopFrameMatches(
			*TestRunner,
			MonitorResult.Stops[1].Callstack,
			EntryAbsolutePath,
			Fixture.GetLine(TEXT("StepAfterCallLine")),
			TEXT("Cross-file StepOver second stop"));
		if (bSecondFrameMatched)
		{
			const FAngelscriptCallFrame& TopFrame = MonitorResult.Stops[1].Callstack->Frames[0];
			TestRunner->TestFalse(TEXT("Cross-file StepOver second stop should not switch the top frame source to the callee file"), FPaths::IsSamePath(TopFrame.Source, CalleeAbsolutePath));
		}
		if (bFirstFrameMatched && bSecondFrameMatched)
		{
			TestRunner->TestEqual(
				TEXT("Cross-file StepOver should keep the same stack depth before and after stepping over the call"),
				MonitorResult.Stops[1].Callstack->Frames.Num(),
				MonitorResult.Stops[0].Callstack->Frames.Num());
		}

		TestRunner->TestTrue(TEXT("Cross-file StepOver should execute successfully"), InvocationState->bSucceeded);
		TestRunner->TestEqual(TEXT("Cross-file StepOver should return the expected final result"), InvocationState->Result, 22);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
