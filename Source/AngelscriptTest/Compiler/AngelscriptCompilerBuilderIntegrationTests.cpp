#include "Misc/CoreDelegates.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Compilation/AngelscriptCompilationEvents.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerBuilderIntegrationTests,
	"Angelscript.TestModule.Compiler.BuilderIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

static bool ContainsMessageFragment(const TArray<FString>& Messages, const TCHAR* Fragment)
{
	for (const FString& Message : Messages)
	{
		if (Message.Contains(Fragment))
		{
			return true;
		}
	}

	return false;
}

static bool ContainsDiagnosticFragment(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics, const TCHAR* Fragment)
{
	for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
	{
		if (Diagnostic.bIsError && Diagnostic.Message.Contains(Fragment))
		{
			return true;
		}
	}

	return false;
}

public:
	TEST_METHOD(RuntimeCompileRunsObservableBuilderStages)
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
			TEXT("BuilderIntegrationRuntimeStages"),
			TEXT("BuilderIntegrationRuntimeStages.as"),
			TEXT(R"AS(
int BuilderIntegrationAdd(int Delta)
{
	return 40 + Delta;
}

int Entry()
{
	return BuilderIntegrationAdd(2);
}
)AS"),
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Builder integration compile should succeed")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("Builder integration compile should be fully handled")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Builder integration compile should produce one compiled module")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Builder integration compile should not emit diagnostics")));

		const FName ExpectedPhases[] = {
			TEXT("Compile.ModuleAssembly"),
			TEXT("Compile.ModuleParse"),
			TEXT("Compile.ModuleGenerateTypes"),
			TEXT("Compile.ModuleGenerateFunctions"),
			TEXT("Compile.ModuleLayout"),
			TEXT("Compile.ModuleCompileCode"),
			TEXT("Compile.ModuleGlobals"),
		};

		int32 PreviousIndex = INDEX_NONE;
		for (FName Phase : ExpectedPhases)
		{
			const int32 PhaseIndex = FindFirstPhaseIndex(Events, Phase);
			if (this->Assert.IsTrue(PhaseIndex != INDEX_NONE, FString::Printf(TEXT("Builder runtime phase should be emitted: %s"), *Phase.ToString())))
			{
				ASSERT_THAT(IsTrue(PhaseIndex > PreviousIndex, FString::Printf(TEXT("Builder runtime phase should be ordered: %s"), *Phase.ToString())));
				PreviousIndex = PhaseIndex;
			}
		}

		const FAngelscriptCompilationEvent* CodeEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleCompileCode);
		if (this->Assert.IsNotNull(CodeEvent, TEXT("Builder integration compile should emit codegen event")))
		{
			ASSERT_THAT(IsTrue(CodeEvent->ModuleNames.Contains(TEXT("BuilderIntegrationRuntimeStages")), TEXT("Codegen event should carry module name")));
			ASSERT_THAT(IsFalse(CodeEvent->bLoadedPrecompiledCode, TEXT("Codegen event should report the builder path rather than precompiled bypass")));
			ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, CodeEvent->CompileType, TEXT("Codegen event should carry compile type")));
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			TEXT("BuilderIntegrationRuntimeStages"),
			TEXT("int Entry()"),
			EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Builder integration compile should execute Entry through runtime engine")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(42, EntryResult, TEXT("Builder integration compile should execute builder-produced bytecode")));
		}

		}
	}

	TEST_METHOD(RuntimeCompileFailureReportsBuilderDiagnostics)
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
			TEXT("BuilderIntegrationRuntimeFailure"),
			TEXT("BuilderIntegrationRuntimeFailure.as"),
			TEXT(R"AS(
int Entry()
{
	GhostBuilderType Value;
	return 42;
}
)AS"),
			false,
			Summary,
			true);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Builder integration invalid script should fail")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Builder integration invalid script should report error result")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Builder integration invalid script should report the attempted module count")));
		ASSERT_THAT(IsTrue(
			ContainsDiagnosticFragment(Summary.Diagnostics, TEXT("GhostBuilderType")),
			TEXT("Builder integration invalid script should capture unknown-type diagnostic")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		if (this->Assert.IsNotNull(EndEvent, TEXT("Builder integration invalid script should emit Compile.End")))
		{
			ASSERT_THAT(IsTrue(EndEvent->bFailed, TEXT("Builder integration invalid script should mark end event failed")));
			ASSERT_THAT(IsFalse(EndEvent->bSucceeded, TEXT("Builder integration invalid script should not mark end event succeeded")));
			ASSERT_THAT(AreEqual(ECompileResult::Error, EndEvent->CompileResult, TEXT("Builder integration invalid script should carry error result")));
			ASSERT_THAT(IsTrue(
				EndEvent->DiagnosticCount > 0 || ContainsMessageFragment(EndEvent->Messages, TEXT("GhostBuilderType")),
				TEXT("Builder integration invalid script should expose failure diagnostics on end event")));
		}

		if (TSharedPtr<FAngelscriptModuleDesc> FailedModule = Engine.GetModule(TEXT("BuilderIntegrationRuntimeFailure")))
		{
			ASSERT_THAT(IsNull(
				FailedModule->ScriptModule,
				TEXT("Builder integration invalid script should not leave executable script module state behind")));
		}

		}
	}
};

#endif
