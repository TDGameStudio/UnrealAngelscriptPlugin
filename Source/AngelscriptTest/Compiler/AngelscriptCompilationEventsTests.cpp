// ============================================================================
// AngelscriptCompilationEventsTests.cpp
//
// Runtime integration tests for structured Angelscript compilation events.
//
// Automation prefix: Angelscript.TestModule.Compiler.Events.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Compilation/AngelscriptCompilationContext.h"
#include "Compilation/AngelscriptCompilationEvents.h"

#include "HAL/PlatformTLS.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptCompilationEventsTests_Private
{
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
			PreCompileHandle = Engine.GetHooks().GetPreCompile().AddRaw(this, &FCompileDelegateCounters::HandlePreCompile);
			PostCompileHandle = Engine.GetHooks().GetPostCompile().AddRaw(this, &FCompileDelegateCounters::HandlePostCompile);
			PreGenerateClassesHandle = Engine.GetHooks().GetPreGenerateClasses().AddRaw(this, &FCompileDelegateCounters::HandlePreGenerateClasses);
		}

		~FCompileDelegateCounters()
		{
			if (BoundEngine == nullptr)
			{
				return;
			}

			if (PreCompileHandle.IsValid())
			{
				BoundEngine->GetHooks().GetPreCompile().Remove(PreCompileHandle);
			}
			if (PostCompileHandle.IsValid())
			{
				BoundEngine->GetHooks().GetPostCompile().Remove(PostCompileHandle);
			}
			if (PreGenerateClassesHandle.IsValid())
			{
				BoundEngine->GetHooks().GetPreGenerateClasses().Remove(PreGenerateClassesHandle);
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

	const FAngelscriptCompilationEvent* FindFirstEvent(
		const TArray<FAngelscriptCompilationEvent>& Events,
		EAngelscriptCompilationEventType EventType)
	{
		return Events.FindByPredicate(
			[EventType](const FAngelscriptCompilationEvent& Event)
			{
				return Event.Type == EventType;
		});
	}

	const FAngelscriptCompilationEvent* FindFirstEventForModule(
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

	int32 FindFirstPhaseIndex(
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

	bool ContainsPhase(const TArray<FAngelscriptCompilationEvent>& Events, FName Phase)
	{
		return FindFirstPhaseIndex(Events, Phase) != INDEX_NONE;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilationEventsTest,
	"Angelscript.TestModule.Compiler.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoListenerCompileIsSilentAndPreservesResult)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TestRunner->TestFalse(
			TEXT("Compilation events should start with no listeners"),
			FAngelscriptCompilationEvents::HasListeners());

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsNoListener"),
			TEXT("CompilationEventsNoListener.as"),
			TEXT("int Entry() { return 7; }"),
			false,
			Summary);

		TestRunner->TestTrue(TEXT("No-listener compile should still compile"), bCompiled);
		TestRunner->TestEqual(TEXT("No-listener compile should keep the compile result"), Summary.CompileResult, ECompileResult::FullyHandled);
		TestRunner->TestEqual(TEXT("No-listener compile should produce one compiled module"), Summary.CompiledModuleCount, 1);
		TestRunner->TestEqual(TEXT("No-listener compile should keep diagnostics empty"), Summary.Diagnostics.Num(), 0);

		}
	}

	TEST_METHOD(RegisteredListenerReceivesValueStyleCompileEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		TestRunner->TestTrue(
			TEXT("Compilation events should report a registered listener"),
			FAngelscriptCompilationEvents::HasListeners());

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsListener"),
			TEXT("CompilationEventsListener.as"),
			TEXT("int Entry() { return 11; }"),
			false,
			Summary);

		TestRunner->TestTrue(TEXT("Listener compile should compile"), bCompiled);
		TestRunner->TestTrue(TEXT("Listener should receive compilation events"), Events.Num() >= 2);

		const FAngelscriptCompilationEvent* BeginEvent = AngelscriptCompilationEventsTests_Private::FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileBegin);
		const FAngelscriptCompilationEvent* EndEvent = AngelscriptCompilationEventsTests_Private::FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		if (TestRunner->TestNotNull(TEXT("Listener should receive Compile.Begin"), BeginEvent))
		{
			TestRunner->TestEqual(TEXT("Compile.Begin phase name should be stable"), BeginEvent->Phase, FName(TEXT("Compile.Begin")));
			TestRunner->TestEqual(TEXT("Compile.Begin should carry compile type"), BeginEvent->CompileType, ECompileType::SoftReloadOnly);
			TestRunner->TestEqual(TEXT("Compile.Begin should carry module count"), BeginEvent->ModuleCount, 1);
			TestRunner->TestTrue(TEXT("Compile.Begin should carry module name"), BeginEvent->ModuleNames.Contains(TEXT("CompilationEventsListener")));
		}

		if (TestRunner->TestNotNull(TEXT("Listener should receive Compile.End"), EndEvent))
		{
			TestRunner->TestEqual(TEXT("Compile.End phase name should be stable"), EndEvent->Phase, FName(TEXT("Compile.End")));
			TestRunner->TestEqual(TEXT("Compile.End should carry compile type"), EndEvent->CompileType, ECompileType::SoftReloadOnly);
			TestRunner->TestEqual(TEXT("Compile.End should carry result"), EndEvent->CompileResult, Summary.CompileResult);
			TestRunner->TestTrue(TEXT("Compile.End should report success"), EndEvent->bSucceeded);
			TestRunner->TestFalse(TEXT("Compile.End should not report failure"), EndEvent->bFailed);
			TestRunner->TestEqual(TEXT("Compile.End should carry compiled module count"), EndEvent->CompiledModuleCount, Summary.CompiledModuleCount);
		}

		}
	}

	TEST_METHOD(ExistingCompileDelegatesRemainCompatible)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		AngelscriptCompilationEventsTests_Private::FCompileDelegateCounters DelegateCounters(Engine);
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

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsDelegates"),
			TEXT("CompilationEventsDelegates.as"),
			TEXT(R"(
UCLASS()
class UCompilationEventsDelegates : UObject
{
    UFUNCTION()
    int Entry()
    {
        return 13;
    }
}
)"),
			true,
			Summary);

		TestRunner->TestTrue(TEXT("Delegate compatibility compile should compile"), bCompiled);
		TestRunner->TestEqual(TEXT("Existing pre-compile delegate should still fire once"), DelegateCounters.PreCompileCount, 1);
		TestRunner->TestEqual(TEXT("Existing post-compile delegate should still fire once"), DelegateCounters.PostCompileCount, 1);
		TestRunner->TestEqual(TEXT("Existing pre-generate-classes delegate should still fire once"), DelegateCounters.PreGenerateClassesCount, 1);
		TestRunner->TestEqual(TEXT("Existing pre-generate-classes delegate should still carry the compiled module"), DelegateCounters.PreGenerateClassesModuleCount, 1);
		TestRunner->TestNotNull(
			TEXT("Structured compile events should also be emitted"),
			AngelscriptCompilationEventsTests_Private::FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd));

		}
	}

	TEST_METHOD(SuccessfulCompileEmitsOrderedStageEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsStages"),
			TEXT("CompilationEventsStages.as"),
			TEXT("int Entry() { return 17; }"),
			false,
			Summary);

		TestRunner->TestTrue(TEXT("Stage event compile should compile"), bCompiled);

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
			const int32 PhaseIndex = AngelscriptCompilationEventsTests_Private::FindFirstPhaseIndex(Events, Phase);
			if (TestRunner->TestTrue(FString::Printf(TEXT("Expected phase should be emitted: %s"), *Phase.ToString()), PhaseIndex != INDEX_NONE))
			{
				TestRunner->TestTrue(FString::Printf(TEXT("Expected phase should be ordered: %s"), *Phase.ToString()), PhaseIndex > PreviousIndex);
				PreviousIndex = PhaseIndex;
			}
		}

		const FAngelscriptCompilationEvent* AssemblyEvent = AngelscriptCompilationEventsTests_Private::FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleAssembly);
		if (TestRunner->TestNotNull(TEXT("Module assembly event should be emitted"), AssemblyEvent))
		{
			TestRunner->TestEqual(TEXT("Assembly event should carry one module"), AssemblyEvent->ModuleCount, 1);
			TestRunner->TestTrue(TEXT("Assembly event should carry module name"), AssemblyEvent->ModuleNames.Contains(TEXT("CompilationEventsStages")));
			TestRunner->TestEqual(TEXT("Assembly event should carry one file"), AssemblyEvent->FileCount, 1);
		}

		const FAngelscriptCompilationEvent* CodeEvent = AngelscriptCompilationEventsTests_Private::FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleCompileCode);
		if (TestRunner->TestNotNull(TEXT("Code compilation event should be emitted"), CodeEvent))
		{
			TestRunner->TestEqual(TEXT("Code event should carry its stable phase"), CodeEvent->Phase, FName(TEXT("Compile.ModuleCompileCode")));
			TestRunner->TestEqual(TEXT("Code event should carry one module"), CodeEvent->ModuleCount, 1);
			TestRunner->TestTrue(TEXT("Code event should carry module name"), CodeEvent->ModuleNames.Contains(TEXT("CompilationEventsStages")));
			TestRunner->TestEqual(TEXT("Code event should expose compile type"), CodeEvent->CompileType, ECompileType::SoftReloadOnly);
			TestRunner->TestTrue(TEXT("Code event should report an explicit JIT availability value"), CodeEvent->bJitAvailable || !CodeEvent->bJitAvailable);
			TestRunner->TestFalse(TEXT("Code event should not report a JIT handoff when JIT is unavailable"), !CodeEvent->bJitAvailable && CodeEvent->bJitHandoff);
		}

		}
	}

	TEST_METHOD(FailedCompileEmitsPairedEndEvent)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsFailure"),
			TEXT("CompilationEventsFailure.as"),
			TEXT("int Entry() { return ; }"),
			false,
			Summary,
			true);

		TestRunner->TestFalse(TEXT("Invalid script should not compile"), bCompiled);
		TestRunner->TestEqual(TEXT("Invalid script should report error result"), Summary.CompileResult, ECompileResult::Error);

		const int32 BeginIndex = AngelscriptCompilationEventsTests_Private::FindFirstPhaseIndex(Events, TEXT("Compile.Begin"));
		const int32 EndIndex = AngelscriptCompilationEventsTests_Private::FindFirstPhaseIndex(Events, TEXT("Compile.End"));
		TestRunner->TestTrue(TEXT("Failed compile should emit begin"), BeginIndex != INDEX_NONE);
		TestRunner->TestTrue(TEXT("Failed compile should emit end"), EndIndex != INDEX_NONE);
		TestRunner->TestTrue(TEXT("Failed compile should emit end after begin"), BeginIndex != INDEX_NONE && EndIndex > BeginIndex);

		const FAngelscriptCompilationEvent* EndEvent = AngelscriptCompilationEventsTests_Private::FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		if (TestRunner->TestNotNull(TEXT("Failed compile should include Compile.End payload"), EndEvent))
		{
			TestRunner->TestEqual(TEXT("Failed end event should carry result"), EndEvent->CompileResult, ECompileResult::Error);
			TestRunner->TestFalse(TEXT("Failed end event should not report success"), EndEvent->bSucceeded);
			TestRunner->TestTrue(TEXT("Failed end event should report failure"), EndEvent->bFailed);
			TestRunner->TestTrue(TEXT("Failed end event should carry diagnostics"), EndEvent->DiagnosticCount > 0 || EndEvent->Messages.Num() > 0);
		}

		}
	}

	TEST_METHOD(ParseEventsAreBroadcastFromMainThreadInDeterministicOrder)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		const uint32 MainThreadId = FPlatformTLS::GetCurrentThreadId();
		TArray<FAngelscriptCompilationEvent> ParseEvents;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&ParseEvents](const FAngelscriptCompilationEvent& Event)
			{
				if (Event.Type == EAngelscriptCompilationEventType::CompileModuleParse)
				{
					ParseEvents.Add(Event);
				}
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsParseMainThread"),
			TEXT("CompilationEventsParseMainThread.as"),
			TEXT("int Entry() { return 19; }"),
			false,
			Summary);

		TestRunner->TestTrue(TEXT("Parse event compile should compile"), bCompiled);
		TestRunner->TestEqual(TEXT("One module should emit one parse event"), ParseEvents.Num(), 1);
		if (ParseEvents.Num() == 1)
		{
			TestRunner->TestEqual(TEXT("Parse event should be emitted on the compile caller thread"), ParseEvents[0].ThreadId, MainThreadId);
			TestRunner->TestTrue(TEXT("Parse event should be emitted on the game thread"), ParseEvents[0].bOnGameThread);
			TestRunner->TestTrue(TEXT("Parse event should carry module name"), ParseEvents[0].ModuleNames.Contains(TEXT("CompilationEventsParseMainThread")));
		}

		}
	}

	TEST_METHOD(CompilationContextIsScopedPerCompileRun)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		FAngelscriptCompileTraceSummary FirstSummary;
		const bool bFirstCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationContextFirstRun"),
			TEXT("CompilationContextFirstRun.as"),
			TEXT("int FirstEntry() { return 23; }"),
			false,
			FirstSummary);

		FAngelscriptCompileTraceSummary SecondSummary;
		const bool bSecondCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationContextSecondRun"),
			TEXT("CompilationContextSecondRun.as"),
			TEXT("int SecondEntry() { return 29; }"),
			false,
			SecondSummary);

		TestRunner->TestTrue(TEXT("First context-scoping compile should compile"), bFirstCompiled);
		TestRunner->TestTrue(TEXT("Second context-scoping compile should compile"), bSecondCompiled);

		const FAngelscriptCompilationEvent* FirstBegin = AngelscriptCompilationEventsTests_Private::FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileBegin, TEXT("CompilationContextFirstRun"));
		const FAngelscriptCompilationEvent* FirstEnd = AngelscriptCompilationEventsTests_Private::FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileEnd, TEXT("CompilationContextFirstRun"));
		const FAngelscriptCompilationEvent* SecondBegin = AngelscriptCompilationEventsTests_Private::FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileBegin, TEXT("CompilationContextSecondRun"));
		const FAngelscriptCompilationEvent* SecondEnd = AngelscriptCompilationEventsTests_Private::FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileEnd, TEXT("CompilationContextSecondRun"));

		if (TestRunner->TestNotNull(TEXT("First compile should emit a begin event"), FirstBegin)
			&& TestRunner->TestNotNull(TEXT("First compile should emit an end event"), FirstEnd))
		{
			TestRunner->TestTrue(TEXT("First compile should carry a run id"), FirstBegin->CompilationRunId != 0);
			TestRunner->TestEqual(TEXT("First compile begin/end should share one run id"), FirstEnd->CompilationRunId, FirstBegin->CompilationRunId);
			TestRunner->TestFalse(TEXT("First compile should not leak second module into begin summary"), FirstBegin->ModuleNames.Contains(TEXT("CompilationContextSecondRun")));
			TestRunner->TestFalse(TEXT("First compile should not leak second module into end summary"), FirstEnd->ModuleNames.Contains(TEXT("CompilationContextSecondRun")));
		}

		if (TestRunner->TestNotNull(TEXT("Second compile should emit a begin event"), SecondBegin)
			&& TestRunner->TestNotNull(TEXT("Second compile should emit an end event"), SecondEnd))
		{
			TestRunner->TestTrue(TEXT("Second compile should carry a run id"), SecondBegin->CompilationRunId != 0);
			TestRunner->TestEqual(TEXT("Second compile begin/end should share one run id"), SecondEnd->CompilationRunId, SecondBegin->CompilationRunId);
			TestRunner->TestFalse(TEXT("Second compile should not leak first module into begin summary"), SecondBegin->ModuleNames.Contains(TEXT("CompilationContextFirstRun")));
			TestRunner->TestFalse(TEXT("Second compile should not leak first module into end summary"), SecondEnd->ModuleNames.Contains(TEXT("CompilationContextFirstRun")));
		}

		if (FirstBegin != nullptr && SecondBegin != nullptr)
		{
			TestRunner->TestNotEqual(TEXT("Each compile should receive a distinct run id"), FirstBegin->CompilationRunId, SecondBegin->CompilationRunId);
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
