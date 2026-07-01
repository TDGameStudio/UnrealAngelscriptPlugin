// ============================================================================
// AngelscriptCompilerEventsTests.cpp
//
// Runtime integration tests for structured Angelscript compilation events.
//
// Automation prefix: Angelscript.TestModule.Compiler.Events.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptStateDumpDiffTestHelper.h"

#include "Compilation/AngelscriptCompilationContext.h"
#include "Compilation/AngelscriptCompilationEvents.h"

#include "HAL/PlatformTLS.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerEventsTests,
	"Angelscript.TestModule.Compiler.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FCompileDelegateCounters
	{
		FDelegateHandle PreCompileHandle;
		FDelegateHandle PostCompileHandle;
		FDelegateHandle PreGenerateClassesHandle;
		int32 PreCompileCount = 0;
		int32 PostCompileCount = 0;
		int32 PreGenerateClassesCount = 0;
		int32 PreGenerateClassesModuleCount = 0;
		FAngelscriptEngine* BoundEngine = nullptr;

		explicit FCompileDelegateCounters(FAngelscriptEngine& Engine)
			: BoundEngine(&Engine)
		{
			PreCompileHandle = Engine.GetPreCompile().AddRaw(this, &FCompileDelegateCounters::HandlePreCompile);
			PostCompileHandle = Engine.GetPostCompile().AddRaw(this, &FCompileDelegateCounters::HandlePostCompile);
			PreGenerateClassesHandle = Engine.GetPreGenerateClasses().AddRaw(this, &FCompileDelegateCounters::HandlePreGenerateClasses);
		}

		~FCompileDelegateCounters()
		{
			if (BoundEngine == nullptr)
			{
				return;
			}

			if (PreCompileHandle.IsValid())
			{
				BoundEngine->GetPreCompile().Remove(PreCompileHandle);
			}
			if (PostCompileHandle.IsValid())
			{
				BoundEngine->GetPostCompile().Remove(PostCompileHandle);
			}
			if (PreGenerateClassesHandle.IsValid())
			{
				BoundEngine->GetPreGenerateClasses().Remove(PreGenerateClassesHandle);
			}
		}

		void HandlePreCompile()
		{
			++PreCompileCount;
		}

		void HandlePostCompile()
		{
			++PostCompileCount;
		}

		void HandlePreGenerateClasses(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
		{
			++PreGenerateClassesCount;
			PreGenerateClassesModuleCount += Modules.Num();
		}
	};

	static const FAngelscriptCompilationEvent* FindFirstEvent(
		const TArray<FAngelscriptCompilationEvent>& Events,
		EAngelscriptCompilationEventType EventType)
	{
		return Events.FindByPredicate(
			[EventType](const FAngelscriptCompilationEvent& Event)
			{
				return Event.Type == EventType;
			});
	}

	static const FAngelscriptCompilationEvent* FindFirstEventForModule(
		const TArray<FAngelscriptCompilationEvent>& Events,
		EAngelscriptCompilationEventType EventType,
		const FString& ModuleName)
	{
		return Events.FindByPredicate(
			[EventType, &ModuleName](const FAngelscriptCompilationEvent& Event)
			{
				return Event.Type == EventType && Event.ModuleNames.Contains(ModuleName);
			});
	}

	static int32 FindFirstPhaseIndex(
		const TArray<FAngelscriptCompilationEvent>& Events,
		FName Phase)
	{
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			if (Events[Index].Phase == Phase)
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	static bool SummaryHasDiagnosticMessage(
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& Message)
	{
		return Summary.Diagnostics.ContainsByPredicate(
			[&Message](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
			{
				return Diagnostic.Message == Message;
			});
	}

	void ExpectEndEventMatchesSummary(
		const FAngelscriptCompilationEvent& EndEvent,
		const FAngelscriptCompileTraceSummary& Summary,
		const FName ModuleName,
		const TCHAR* Context)
	{
		ASSERT_THAT(AreEqual(
			Summary.CompileType,
			EndEvent.CompileType,
			FString::Printf(TEXT("%s should carry the summary compile type"), Context)));
		ASSERT_THAT(AreEqual(
			Summary.CompileResult,
			EndEvent.CompileResult,
			FString::Printf(TEXT("%s should carry the summary compile result"), Context)));
		ASSERT_THAT(AreEqual(
			Summary.CompiledModuleCount,
			EndEvent.CompiledModuleCount,
			FString::Printf(TEXT("%s should carry the summary compiled module count"), Context)));
		ASSERT_THAT(AreEqual(
			Summary.Diagnostics.Num(),
			EndEvent.DiagnosticCount,
			FString::Printf(TEXT("%s should carry the summary diagnostic count"), Context)));
		ASSERT_THAT(AreEqual(
			Summary.bCompileSucceeded,
			EndEvent.bSucceeded,
			FString::Printf(TEXT("%s should mirror summary compile success"), Context)));
		ASSERT_THAT(AreEqual(
			!Summary.bCompileSucceeded,
			EndEvent.bFailed,
			FString::Printf(TEXT("%s should mirror summary compile failure"), Context)));
		ASSERT_THAT(IsTrue(
			EndEvent.ModuleNames.Contains(ModuleName.ToString()),
			FString::Printf(TEXT("%s should carry the compiled module name"), Context)));
		ASSERT_THAT(AreEqual(
			Summary.Diagnostics.Num(),
			EndEvent.Messages.Num(),
			FString::Printf(TEXT("%s should carry one message per summary diagnostic"), Context)));

		for (const FString& Message : EndEvent.Messages)
		{
			ASSERT_THAT(IsTrue(
				SummaryHasDiagnosticMessage(Summary, Message),
				FString::Printf(TEXT("%s diagnostic message should match summary: %s"), Context, *Message)));
		}
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(NoListenerCompileIsSilentAndPreservesResult)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName ModuleName(TEXT("CompilationEventsNoListener"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		ASSERT_THAT(IsFalse(
			FAngelscriptCompilationEvents::HasListeners(),
			TEXT("Compilation events should start with no listeners")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 7;
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("CompilationEventsNoListener.as"),
			ScriptSource,
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("No-listener compile should still compile")));
		ASSERT_THAT(IsTrue(Summary.bCompileSucceeded, TEXT("No-listener compile should mark the summary as successful")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("No-listener compile should keep the compile result")));
		ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, Summary.CompileType, TEXT("No-listener compile should preserve requested compile type")));
		ASSERT_THAT(AreEqual(1, Summary.ModuleDescCount, TEXT("No-listener compile should describe one input module")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("No-listener compile should produce one compiled module")));
		ASSERT_THAT(IsTrue(Summary.ModuleNames.Contains(ModuleName.ToString()), TEXT("No-listener compile summary should carry module name")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("No-listener compile should keep diagnostics empty")));
		ASSERT_THAT(IsFalse(
			FAngelscriptCompilationEvents::HasListeners(),
			TEXT("No-listener compile should not leave structured listeners registered")));
	}

	TEST_METHOD(RegisteredListenerReceivesValueStyleCompileEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName ModuleName(TEXT("CompilationEventsListener"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		ASSERT_THAT(IsTrue(
			FAngelscriptCompilationEvents::HasListeners(),
			TEXT("Compilation events should report a registered listener")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 11;
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("CompilationEventsListener.as"),
			ScriptSource,
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Listener compile should compile")));
		ASSERT_THAT(IsTrue(Events.Num() >= 2, TEXT("Listener should receive compilation events")));

		const FAngelscriptCompilationEvent* BeginEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileBegin);
		ASSERT_THAT(IsNotNull(BeginEvent, TEXT("Listener should receive Compile.Begin")));
		if (BeginEvent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("Compile.Begin")), BeginEvent->Phase, TEXT("Compile.Begin phase name should be stable")));
		ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, BeginEvent->CompileType, TEXT("Compile.Begin should carry compile type")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, BeginEvent->CompileResult, TEXT("Compile.Begin should not report a final compile result")));
		ASSERT_THAT(IsFalse(BeginEvent->bSucceeded, TEXT("Compile.Begin should not report compile success before compilation")));
		ASSERT_THAT(IsFalse(BeginEvent->bFailed, TEXT("Compile.Begin should not report compile failure before compilation")));
		ASSERT_THAT(AreEqual(1, BeginEvent->ModuleCount, TEXT("Compile.Begin should carry module count")));
		ASSERT_THAT(IsTrue(BeginEvent->ModuleNames.Contains(ModuleName.ToString()), TEXT("Compile.Begin should carry module name")));
		ASSERT_THAT(IsTrue(BeginEvent->CompilationRunId != 0, TEXT("Compile.Begin should carry a non-zero run id")));
		ASSERT_THAT(AreEqual(0, BeginEvent->CompiledModuleCount, TEXT("Compile.Begin should not report compiled modules before compilation")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		ASSERT_THAT(IsNotNull(EndEvent, TEXT("Listener should receive Compile.End")));
		if (EndEvent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("Compile.End")), EndEvent->Phase, TEXT("Compile.End phase name should be stable")));
		ASSERT_THAT(AreEqual(BeginEvent->CompilationRunId, EndEvent->CompilationRunId, TEXT("Compile begin/end should share one run id")));
		ASSERT_THAT(AreEqual(1, EndEvent->ModuleCount, TEXT("Compile.End should carry one compiled module")));
		ExpectEndEventMatchesSummary(*EndEvent, Summary, ModuleName, TEXT("Compile.End"));
	}

	TEST_METHOD(ExistingCompileDelegatesRemainCompatible)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName ModuleName(TEXT("CompilationEventsDelegates"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		FCompileDelegateCounters DelegateCounters(Engine);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCompilationEventsDelegates : UObject
			{
				UFUNCTION()
				int Entry()
				{
					return 13;
				}
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("CompilationEventsDelegates.as"),
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Delegate compatibility compile should compile")));
		ASSERT_THAT(IsTrue(Summary.bCompileSucceeded, TEXT("Delegate compatibility summary should report success")));
		ASSERT_THAT(AreEqual(ECompileResult::PartiallyHandled, Summary.CompileResult, TEXT("Delegate compatibility compile should report the deferred full-reload result for annotated soft reload")));
		ASSERT_THAT(AreEqual(1, Summary.ModuleDescCount, TEXT("Delegate compatibility summary should describe one input module")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Delegate compatibility summary should report one compiled module")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PreCompileCount, TEXT("Existing pre-compile delegate should still fire once")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PostCompileCount, TEXT("Existing post-compile delegate should still fire once")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PreGenerateClassesCount, TEXT("Existing pre-generate-classes delegate should still fire once")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PreGenerateClassesModuleCount, TEXT("Existing pre-generate-classes delegate should still carry the compiled module")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		ASSERT_THAT(IsNotNull(EndEvent, TEXT("Structured compile events should also be emitted")));
		if (EndEvent == nullptr)
		{
			return;
		}

		ExpectEndEventMatchesSummary(*EndEvent, Summary, ModuleName, TEXT("Legacy delegate compatibility Compile.End"));
	}

	TEST_METHOD(SuccessfulCompileEmitsOrderedStageEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName ModuleName(TEXT("CompilationEventsStages"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 17;
			}
			)AS");

		const FAngelscriptStateSnapshot BeforeState = FAngelscriptStateDumpDiffTestHelper::Capture(Engine);

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("CompilationEventsStages.as"),
			ScriptSource,
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Stage event compile should compile")));

		const FAngelscriptStateSnapshot AfterState = FAngelscriptStateDumpDiffTestHelper::Capture(Engine);
		FAngelscriptStateDumpDiffTestArtifacts StateArtifacts;
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::DumpDiffArtifacts(
				*TestRunner,
				TEXT("CompilerEventsSuccessfulCompile"),
				BeforeState,
				AfterState,
				StateArtifacts),
			TEXT("Successful compiler event test should dump before/after engine state artifacts")));
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::ContainsDiff(
				StateArtifacts.Diff,
				TEXT("EngineCollection"),
				TEXT("ActiveModules"),
				TEXT("Count"),
				EAngelscriptStateDiffChangeType::Changed),
			TEXT("Compiler event state diff should include FAS active module impact")));
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::ContainsDiff(
				StateArtifacts.Diff,
				TEXT("AsEngineInternal"),
				TEXT("ScriptEngine"),
				TEXT("ScriptModulesByNameCount"),
				EAngelscriptStateDiffChangeType::Changed),
			TEXT("Compiler event state diff should include AS engine module-name map impact")));
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::ContainsDiff(
				StateArtifacts.Diff,
				TEXT("AsEngineInternal"),
				TEXT("ScriptEngine"),
				TEXT("AllScriptGlobalFunctionCount"),
				EAngelscriptStateDiffChangeType::Changed),
			TEXT("Compiler event state diff should include AS engine global function impact")));
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::ContainsDiff(
				StateArtifacts.Diff,
				TEXT("AsEngineInternal"),
				TEXT("ScriptEngine"),
				TEXT("TypeIdMapCount"),
				EAngelscriptStateDiffChangeType::Changed),
			TEXT("Compiler event state diff should include AS engine type-id map impact")));
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::ContainsDiff(
				StateArtifacts.Diff,
				TEXT("AsModuleInternal"),
				ModuleName.ToString(),
				TEXT("ScriptFunctionCount"),
				EAngelscriptStateDiffChangeType::Added),
			TEXT("Compiler event state diff should include compiled AS module internals")));
		ASSERT_THAT(IsTrue(
			FAngelscriptStateDumpDiffTestHelper::ContainsDiff(
				StateArtifacts.Diff,
				TEXT("AsFunctionInternal"),
				FString::Printf(TEXT("%s::Entry"), *ModuleName.ToString()),
				TEXT("Declaration"),
				EAngelscriptStateDiffChangeType::Added),
			TEXT("Compiler event state diff should include compiled AS function internals")));

		ASSERT_THAT(IsTrue(Summary.bCompileSucceeded, TEXT("Stage event compile should mark summary success")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("Stage event compile should finish fully handled")));
		ASSERT_THAT(AreEqual(1, Summary.ModuleDescCount, TEXT("Stage event compile should describe one input module")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Stage event compile should report one compiled module")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Stage event compile should not report diagnostics")));

		const FName ExpectedPhases[] = {
			TEXT("Compile.Begin"),
			TEXT("Compile.ModuleAssembly"),
			TEXT("Compile.ModuleParse"),
			TEXT("Compile.ModuleGenerateTypes"),
			TEXT("Compile.ModuleGenerateFunctions"),
			TEXT("Compile.ModuleLayout"),
			TEXT("Compile.ModuleCompileCode"),
			TEXT("Compile.ModuleGlobals"),
			TEXT("Compile.ClassGenerationHandoff"),
			TEXT("Compile.End"),
		};

		int32 PreviousIndex = INDEX_NONE;
		for (FName Phase : ExpectedPhases)
		{
			const int32 PhaseIndex = FindFirstPhaseIndex(Events, Phase);
			ASSERT_THAT(IsTrue(
				PhaseIndex != INDEX_NONE,
				FString::Printf(TEXT("Expected phase should be emitted: %s"), *Phase.ToString())));
			ASSERT_THAT(IsTrue(
				PhaseIndex > PreviousIndex,
				FString::Printf(TEXT("Expected phase should be ordered: %s"), *Phase.ToString())));
			PreviousIndex = PhaseIndex;
		}

		const FAngelscriptCompilationEvent* AssemblyEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleAssembly);
		ASSERT_THAT(IsNotNull(AssemblyEvent, TEXT("Module assembly event should be emitted")));
		if (AssemblyEvent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, AssemblyEvent->ModuleCount, TEXT("Assembly event should carry one module")));
		ASSERT_THAT(AreEqual(0, AssemblyEvent->CompiledModuleCount, TEXT("Assembly event should not report compiled modules before compilation")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, AssemblyEvent->CompileResult, TEXT("Assembly event should leave the final compile result for Compile.End")));
		ASSERT_THAT(IsTrue(AssemblyEvent->bSucceeded, TEXT("Assembly event should report stage success")));
		ASSERT_THAT(IsFalse(AssemblyEvent->bFailed, TEXT("Assembly event should not report stage failure")));
		ASSERT_THAT(IsTrue(AssemblyEvent->ModuleNames.Contains(ModuleName.ToString()), TEXT("Assembly event should carry module name")));
		ASSERT_THAT(AreEqual(1, AssemblyEvent->FileCount, TEXT("Assembly event should carry one file")));

		const FAngelscriptCompilationEvent* CodeEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleCompileCode);
		ASSERT_THAT(IsNotNull(CodeEvent, TEXT("Code compilation event should be emitted")));
		if (CodeEvent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("Compile.ModuleCompileCode")), CodeEvent->Phase, TEXT("Code event should carry its stable phase")));
		ASSERT_THAT(AreEqual(1, CodeEvent->ModuleCount, TEXT("Code event should carry one module")));
		ASSERT_THAT(AreEqual(0, CodeEvent->CompiledModuleCount, TEXT("Code event should not report final compiled module count before Compile.End")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, CodeEvent->CompileResult, TEXT("Code event should not report a final compile result")));
		ASSERT_THAT(IsTrue(CodeEvent->bSucceeded, TEXT("Code event should report stage success")));
		ASSERT_THAT(IsFalse(CodeEvent->bFailed, TEXT("Code event should not report stage failure")));
		ASSERT_THAT(IsTrue(CodeEvent->ModuleNames.Contains(ModuleName.ToString()), TEXT("Code event should carry module name")));
		ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, CodeEvent->CompileType, TEXT("Code event should expose compile type")));
		ASSERT_THAT(IsFalse(!CodeEvent->bJitAvailable && CodeEvent->bJitHandoff, TEXT("Code event should not report a JIT handoff when JIT is unavailable")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		ASSERT_THAT(IsNotNull(EndEvent, TEXT("Successful stage compile should emit Compile.End")));
		if (EndEvent == nullptr)
		{
			return;
		}

		ExpectEndEventMatchesSummary(*EndEvent, Summary, ModuleName, TEXT("Successful stage Compile.End"));
	}

	TEST_METHOD(FailedCompileEmitsPairedEndEvent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName ModuleName(TEXT("CompilationEventsFailure"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		const FString BrokenScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return ;
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("CompilationEventsFailure.as"),
			BrokenScriptSource,
			false,
			Summary,
			true);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Invalid script should not compile")));
		ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, TEXT("Invalid script summary should report compile failure")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Invalid script should report error result")));
		ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, Summary.CompileType, TEXT("Invalid script summary should preserve requested compile type")));
		ASSERT_THAT(AreEqual(1, Summary.ModuleDescCount, TEXT("Invalid script summary should describe one attempted module")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Invalid script summary should report the attempted module in its compiled module summary")));
		ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, TEXT("Invalid script summary should collect diagnostics")));

		const int32 BeginIndex = FindFirstPhaseIndex(Events, TEXT("Compile.Begin"));
		const int32 EndIndex = FindFirstPhaseIndex(Events, TEXT("Compile.End"));
		ASSERT_THAT(IsTrue(BeginIndex != INDEX_NONE, TEXT("Failed compile should emit begin")));
		ASSERT_THAT(IsTrue(EndIndex != INDEX_NONE, TEXT("Failed compile should emit end")));
		ASSERT_THAT(IsTrue(EndIndex > BeginIndex, TEXT("Failed compile should emit end after begin")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		ASSERT_THAT(IsNotNull(EndEvent, TEXT("Failed compile should include Compile.End payload")));
		if (EndEvent == nullptr)
		{
			return;
		}

		ExpectEndEventMatchesSummary(*EndEvent, Summary, ModuleName, TEXT("Failed Compile.End"));
		ASSERT_THAT(IsTrue(EndEvent->Messages.Num() > 0, TEXT("Failed end event should carry diagnostic messages")));
	}

	TEST_METHOD(ParseEventsAreBroadcastFromMainThreadInDeterministicOrder)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName ModuleName(TEXT("CompilationEventsParseMainThread"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const uint32 MainThreadId = FPlatformTLS::GetCurrentThreadId();
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				if (Event.Type == EAngelscriptCompilationEventType::CompileModuleParse
					|| Event.Type == EAngelscriptCompilationEventType::CompileEnd)
				{
					Events.Add(Event);
				}
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 19;
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("CompilationEventsParseMainThread.as"),
			ScriptSource,
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Parse event compile should compile")));
		ASSERT_THAT(IsTrue(Summary.bCompileSucceeded, TEXT("Parse event compile should mark summary success")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("Parse event compile should finish fully handled")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Parse event compile should produce one compiled module")));

		const FAngelscriptCompilationEvent* ParseEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleParse);
		ASSERT_THAT(IsNotNull(ParseEvent, TEXT("One module should emit one parse event")));
		if (ParseEvent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(MainThreadId, ParseEvent->ThreadId, TEXT("Parse event should be emitted on the compile caller thread")));
		ASSERT_THAT(IsTrue(ParseEvent->bOnGameThread, TEXT("Parse event should be emitted on the game thread")));
		ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, ParseEvent->CompileType, TEXT("Parse event should carry the requested compile type")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, ParseEvent->CompileResult, TEXT("Parse event should not report a final compile result")));
		ASSERT_THAT(IsTrue(ParseEvent->bSucceeded, TEXT("Parse event should report stage success")));
		ASSERT_THAT(IsFalse(ParseEvent->bFailed, TEXT("Parse event should not report stage failure")));
		ASSERT_THAT(AreEqual(1, ParseEvent->ModuleCount, TEXT("Parse event should carry one parsed module")));
		ASSERT_THAT(AreEqual(0, ParseEvent->CompiledModuleCount, TEXT("Parse event should not report final compiled module count")));
		ASSERT_THAT(IsTrue(ParseEvent->ModuleNames.Contains(ModuleName.ToString()), TEXT("Parse event should carry module name")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		ASSERT_THAT(IsNotNull(EndEvent, TEXT("Parse event compile should still emit a final result event")));
		if (EndEvent == nullptr)
		{
			return;
		}

		ExpectEndEventMatchesSummary(*EndEvent, Summary, ModuleName, TEXT("Parse event Compile.End"));
	}

	TEST_METHOD(CompilationContextIsScopedPerCompileRun)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FName FirstModuleName(TEXT("CompilationContextFirstRun"));
		const FName SecondModuleName(TEXT("CompilationContextSecondRun"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FirstModuleName.ToString());
			Engine.DiscardModule(*SecondModuleName.ToString());
		};

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				if (Event.Type == EAngelscriptCompilationEventType::CompileBegin || Event.Type == EAngelscriptCompilationEventType::CompileEnd)
				{
					Events.Add(Event);
				}
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		const FString FirstScriptSource = ASTEST_AS(R"AS(
			int FirstEntry()
			{
				return 23;
			}
			)AS");

		FAngelscriptCompileTraceSummary FirstSummary;
		const bool bFirstCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			FirstModuleName,
			TEXT("CompilationContextFirstRun.as"),
			FirstScriptSource,
			false,
			FirstSummary);

		const FString SecondScriptSource = ASTEST_AS(R"AS(
			int SecondEntry()
			{
				return 29;
			}
			)AS");

		FAngelscriptCompileTraceSummary SecondSummary;
		const bool bSecondCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			SecondModuleName,
			TEXT("CompilationContextSecondRun.as"),
			SecondScriptSource,
			false,
			SecondSummary);

		ASSERT_THAT(IsTrue(bFirstCompiled, TEXT("First context-scoping compile should compile")));
		ASSERT_THAT(IsTrue(bSecondCompiled, TEXT("Second context-scoping compile should compile")));
		ASSERT_THAT(IsTrue(FirstSummary.bCompileSucceeded, TEXT("First context-scoping summary should report success")));
		ASSERT_THAT(IsTrue(SecondSummary.bCompileSucceeded, TEXT("Second context-scoping summary should report success")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, FirstSummary.CompileResult, TEXT("First context-scoping compile should be fully handled")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, SecondSummary.CompileResult, TEXT("Second context-scoping compile should be fully handled")));
		ASSERT_THAT(AreEqual(1, FirstSummary.CompiledModuleCount, TEXT("First context-scoping summary should report one compiled module")));
		ASSERT_THAT(AreEqual(1, SecondSummary.CompiledModuleCount, TEXT("Second context-scoping summary should report one compiled module")));

		const FAngelscriptCompilationEvent* FirstBegin = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileBegin, FirstModuleName.ToString());
		const FAngelscriptCompilationEvent* FirstEnd = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileEnd, FirstModuleName.ToString());
		const FAngelscriptCompilationEvent* SecondBegin = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileBegin, SecondModuleName.ToString());
		const FAngelscriptCompilationEvent* SecondEnd = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileEnd, SecondModuleName.ToString());

		ASSERT_THAT(IsNotNull(FirstBegin, TEXT("First compile should emit a begin event")));
		ASSERT_THAT(IsNotNull(FirstEnd, TEXT("First compile should emit an end event")));
		if (FirstBegin == nullptr || FirstEnd == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FirstBegin->CompilationRunId != 0, TEXT("First compile should carry a run id")));
		ASSERT_THAT(AreEqual(FirstBegin->CompilationRunId, FirstEnd->CompilationRunId, TEXT("First compile begin/end should share one run id")));
		ASSERT_THAT(IsFalse(FirstBegin->ModuleNames.Contains(SecondModuleName.ToString()), TEXT("First compile should not leak second module into begin summary")));
		ASSERT_THAT(IsFalse(FirstEnd->ModuleNames.Contains(SecondModuleName.ToString()), TEXT("First compile should not leak second module into end summary")));
		ExpectEndEventMatchesSummary(*FirstEnd, FirstSummary, FirstModuleName, TEXT("First context Compile.End"));

		ASSERT_THAT(IsNotNull(SecondBegin, TEXT("Second compile should emit a begin event")));
		ASSERT_THAT(IsNotNull(SecondEnd, TEXT("Second compile should emit an end event")));
		if (SecondBegin == nullptr || SecondEnd == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SecondBegin->CompilationRunId != 0, TEXT("Second compile should carry a run id")));
		ASSERT_THAT(AreEqual(SecondBegin->CompilationRunId, SecondEnd->CompilationRunId, TEXT("Second compile begin/end should share one run id")));
		ASSERT_THAT(IsFalse(SecondBegin->ModuleNames.Contains(FirstModuleName.ToString()), TEXT("Second compile should not leak first module into begin summary")));
		ASSERT_THAT(IsFalse(SecondEnd->ModuleNames.Contains(FirstModuleName.ToString()), TEXT("Second compile should not leak first module into end summary")));
		ExpectEndEventMatchesSummary(*SecondEnd, SecondSummary, SecondModuleName, TEXT("Second context Compile.End"));
		ASSERT_THAT(AreNotEqual(SecondBegin->CompilationRunId, FirstBegin->CompilationRunId, TEXT("Each compile should receive a distinct run id")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
