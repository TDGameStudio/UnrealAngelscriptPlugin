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


TEST_CLASS_WITH_FLAGS(FAngelscriptCompilationEventsTest,
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

static bool ContainsPhase(const TArray<FAngelscriptCompilationEvent>& Events, FName Phase)
{
	return FindFirstPhaseIndex(Events, Phase) != INDEX_NONE;
}

public:
	TEST_METHOD(NoListenerCompileIsSilentAndPreservesResult)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		ASSERT_THAT(IsFalse(
			FAngelscriptCompilationEvents::HasListeners(),
			TEXT("Compilation events should start with no listeners")));

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsNoListener"),
			TEXT("CompilationEventsNoListener.as"),
			TEXT("int Entry() { return 7; }"),
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("No-listener compile should still compile")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("No-listener compile should keep the compile result")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("No-listener compile should produce one compiled module")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("No-listener compile should keep diagnostics empty")));

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

		ASSERT_THAT(IsTrue(
			FAngelscriptCompilationEvents::HasListeners(),
			TEXT("Compilation events should report a registered listener")));

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("CompilationEventsListener"),
			TEXT("CompilationEventsListener.as"),
			TEXT("int Entry() { return 11; }"),
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Listener compile should compile")));
		ASSERT_THAT(IsTrue(Events.Num() >= 2, TEXT("Listener should receive compilation events")));

		const FAngelscriptCompilationEvent* BeginEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileBegin);
		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		if (this->Assert.IsNotNull(BeginEvent, TEXT("Listener should receive Compile.Begin")))
		{
			ASSERT_THAT(AreEqual(FName(TEXT("Compile.Begin")), BeginEvent->Phase, TEXT("Compile.Begin phase name should be stable")));
			ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, BeginEvent->CompileType, TEXT("Compile.Begin should carry compile type")));
			ASSERT_THAT(AreEqual(1, BeginEvent->ModuleCount, TEXT("Compile.Begin should carry module count")));
			ASSERT_THAT(IsTrue(BeginEvent->ModuleNames.Contains(TEXT("CompilationEventsListener")), TEXT("Compile.Begin should carry module name")));
		}

		if (this->Assert.IsNotNull(EndEvent, TEXT("Listener should receive Compile.End")))
		{
			ASSERT_THAT(AreEqual(FName(TEXT("Compile.End")), EndEvent->Phase, TEXT("Compile.End phase name should be stable")));
			ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, EndEvent->CompileType, TEXT("Compile.End should carry compile type")));
			ASSERT_THAT(AreEqual(Summary.CompileResult, EndEvent->CompileResult, TEXT("Compile.End should carry result")));
			ASSERT_THAT(IsTrue(EndEvent->bSucceeded, TEXT("Compile.End should report success")));
			ASSERT_THAT(IsFalse(EndEvent->bFailed, TEXT("Compile.End should not report failure")));
			ASSERT_THAT(AreEqual(Summary.CompiledModuleCount, EndEvent->CompiledModuleCount, TEXT("Compile.End should carry compiled module count")));
		}

		}
	}

	TEST_METHOD(ExistingCompileDelegatesRemainCompatible)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Delegate compatibility compile should compile")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PreCompileCount, TEXT("Existing pre-compile delegate should still fire once")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PostCompileCount, TEXT("Existing post-compile delegate should still fire once")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PreGenerateClassesCount, TEXT("Existing pre-generate-classes delegate should still fire once")));
		ASSERT_THAT(AreEqual(1, DelegateCounters.PreGenerateClassesModuleCount, TEXT("Existing pre-generate-classes delegate should still carry the compiled module")));
		ASSERT_THAT(IsNotNull(
			FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd),
			TEXT("Structured compile events should also be emitted")));

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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Stage event compile should compile")));

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
			if (this->Assert.IsTrue(PhaseIndex != INDEX_NONE, FString::Printf(TEXT("Expected phase should be emitted: %s"), *Phase.ToString())))
			{
				ASSERT_THAT(IsTrue(PhaseIndex > PreviousIndex, FString::Printf(TEXT("Expected phase should be ordered: %s"), *Phase.ToString())));
				PreviousIndex = PhaseIndex;
			}
		}

		const FAngelscriptCompilationEvent* AssemblyEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleAssembly);
		if (this->Assert.IsNotNull(AssemblyEvent, TEXT("Module assembly event should be emitted")))
		{
			ASSERT_THAT(AreEqual(1, AssemblyEvent->ModuleCount, TEXT("Assembly event should carry one module")));
			ASSERT_THAT(IsTrue(AssemblyEvent->ModuleNames.Contains(TEXT("CompilationEventsStages")), TEXT("Assembly event should carry module name")));
			ASSERT_THAT(AreEqual(1, AssemblyEvent->FileCount, TEXT("Assembly event should carry one file")));
		}

		const FAngelscriptCompilationEvent* CodeEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleCompileCode);
		if (this->Assert.IsNotNull(CodeEvent, TEXT("Code compilation event should be emitted")))
		{
			ASSERT_THAT(AreEqual(FName(TEXT("Compile.ModuleCompileCode")), CodeEvent->Phase, TEXT("Code event should carry its stable phase")));
			ASSERT_THAT(AreEqual(1, CodeEvent->ModuleCount, TEXT("Code event should carry one module")));
			ASSERT_THAT(IsTrue(CodeEvent->ModuleNames.Contains(TEXT("CompilationEventsStages")), TEXT("Code event should carry module name")));
			ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, CodeEvent->CompileType, TEXT("Code event should expose compile type")));
			ASSERT_THAT(IsTrue(CodeEvent->bJitAvailable || !CodeEvent->bJitAvailable, TEXT("Code event should report an explicit JIT availability value")));
			ASSERT_THAT(IsFalse(!CodeEvent->bJitAvailable && CodeEvent->bJitHandoff, TEXT("Code event should not report a JIT handoff when JIT is unavailable")));
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

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Invalid script should not compile")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Invalid script should report error result")));

		const int32 BeginIndex = FindFirstPhaseIndex(Events, TEXT("Compile.Begin"));
		const int32 EndIndex = FindFirstPhaseIndex(Events, TEXT("Compile.End"));
		ASSERT_THAT(IsTrue(BeginIndex != INDEX_NONE, TEXT("Failed compile should emit begin")));
		ASSERT_THAT(IsTrue(EndIndex != INDEX_NONE, TEXT("Failed compile should emit end")));
		ASSERT_THAT(IsTrue(BeginIndex != INDEX_NONE && EndIndex > BeginIndex, TEXT("Failed compile should emit end after begin")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		if (this->Assert.IsNotNull(EndEvent, TEXT("Failed compile should include Compile.End payload")))
		{
			ASSERT_THAT(AreEqual(ECompileResult::Error, EndEvent->CompileResult, TEXT("Failed end event should carry result")));
			ASSERT_THAT(IsFalse(EndEvent->bSucceeded, TEXT("Failed end event should not report success")));
			ASSERT_THAT(IsTrue(EndEvent->bFailed, TEXT("Failed end event should report failure")));
			ASSERT_THAT(IsTrue(EndEvent->DiagnosticCount > 0 || EndEvent->Messages.Num() > 0, TEXT("Failed end event should carry diagnostics")));
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Parse event compile should compile")));
		ASSERT_THAT(AreEqual(1, ParseEvents.Num(), TEXT("One module should emit one parse event")));
		if (ParseEvents.Num() == 1)
		{
			ASSERT_THAT(AreEqual(MainThreadId, ParseEvents[0].ThreadId, TEXT("Parse event should be emitted on the compile caller thread")));
			ASSERT_THAT(IsTrue(ParseEvents[0].bOnGameThread, TEXT("Parse event should be emitted on the game thread")));
			ASSERT_THAT(IsTrue(ParseEvents[0].ModuleNames.Contains(TEXT("CompilationEventsParseMainThread")), TEXT("Parse event should carry module name")));
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

		ASSERT_THAT(IsTrue(bFirstCompiled, TEXT("First context-scoping compile should compile")));
		ASSERT_THAT(IsTrue(bSecondCompiled, TEXT("Second context-scoping compile should compile")));

		const FAngelscriptCompilationEvent* FirstBegin = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileBegin, TEXT("CompilationContextFirstRun"));
		const FAngelscriptCompilationEvent* FirstEnd = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileEnd, TEXT("CompilationContextFirstRun"));
		const FAngelscriptCompilationEvent* SecondBegin = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileBegin, TEXT("CompilationContextSecondRun"));
		const FAngelscriptCompilationEvent* SecondEnd = FindFirstEventForModule(Events, EAngelscriptCompilationEventType::CompileEnd, TEXT("CompilationContextSecondRun"));

		if (this->Assert.IsNotNull(FirstBegin, TEXT("First compile should emit a begin event"))
			&& this->Assert.IsNotNull(FirstEnd, TEXT("First compile should emit an end event")))
		{
			ASSERT_THAT(IsTrue(FirstBegin->CompilationRunId != 0, TEXT("First compile should carry a run id")));
			ASSERT_THAT(AreEqual(FirstBegin->CompilationRunId, FirstEnd->CompilationRunId, TEXT("First compile begin/end should share one run id")));
			ASSERT_THAT(IsFalse(FirstBegin->ModuleNames.Contains(TEXT("CompilationContextSecondRun")), TEXT("First compile should not leak second module into begin summary")));
			ASSERT_THAT(IsFalse(FirstEnd->ModuleNames.Contains(TEXT("CompilationContextSecondRun")), TEXT("First compile should not leak second module into end summary")));
		}

		if (this->Assert.IsNotNull(SecondBegin, TEXT("Second compile should emit a begin event"))
			&& this->Assert.IsNotNull(SecondEnd, TEXT("Second compile should emit an end event")))
		{
			ASSERT_THAT(IsTrue(SecondBegin->CompilationRunId != 0, TEXT("Second compile should carry a run id")));
			ASSERT_THAT(AreEqual(SecondBegin->CompilationRunId, SecondEnd->CompilationRunId, TEXT("Second compile begin/end should share one run id")));
			ASSERT_THAT(IsFalse(SecondBegin->ModuleNames.Contains(TEXT("CompilationContextFirstRun")), TEXT("Second compile should not leak first module into begin summary")));
			ASSERT_THAT(IsFalse(SecondEnd->ModuleNames.Contains(TEXT("CompilationContextFirstRun")), TEXT("Second compile should not leak first module into end summary")));
		}

		if (FirstBegin != nullptr && SecondBegin != nullptr)
		{
			ASSERT_THAT(AreNotEqual(SecondBegin->CompilationRunId, FirstBegin->CompilationRunId, TEXT("Each compile should receive a distinct run id")));
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
