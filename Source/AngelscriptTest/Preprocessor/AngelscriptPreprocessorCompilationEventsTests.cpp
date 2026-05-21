// ============================================================================
// AngelscriptPreprocessorCompilationEventsTests.cpp
//
// Preprocessor tests for structured compilation events emitted at hook points.
//
// Automation prefix: Angelscript.TestModule.Preprocessor.CompilationEvents.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "Compilation/AngelscriptCompilationEvents.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;
using namespace AngelscriptTestSupport;

namespace AngelscriptPreprocessorCompilationEventsTests_Private
{
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorCompilationEventsTest,
 	"Angelscript.TestModule.Preprocessor.CompilationEvents",
 	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
 TEST_METHOD(HookMomentsEmitSummaryBackedCompilationEvents)
 {
 	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
 	{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

 	FFixtureFile File(TEXT("Tests/Preprocessor/CompilationEvents/HookMoments.as"), TEXT(R"(
UCLASS()
class UCompilationEventsHookMoments : UObject
{
    UPROPERTY()
    int Value;

    UFUNCTION()
    int Entry()
    {
        return Value;
    }
}
)"));

 	FPreprocessResult Result = RunPreprocess(Engine, File);

 	AssertPreprocessSucceeded(*TestRunner, Result);
 	AssertNoDiagnostics(*TestRunner, Result);

 	const FAngelscriptCompilationEvent* ProcessChunksEvent = AngelscriptPreprocessorCompilationEventsTests_Private::FindFirstEvent(
 		Events,
 		EAngelscriptCompilationEventType::PreprocessProcessChunks);
 	const FAngelscriptCompilationEvent* PostProcessCodeEvent = AngelscriptPreprocessorCompilationEventsTests_Private::FindFirstEvent(
 		Events,
 		EAngelscriptCompilationEventType::PreprocessPostProcessCode);

 	if (TestRunner->TestNotNull(TEXT("ProcessChunks compilation event should be emitted"), ProcessChunksEvent))
 	{
 		TestRunner->TestEqual(TEXT("ProcessChunks phase should be stable"), ProcessChunksEvent->Phase, FName(TEXT("Preprocess.ProcessChunks")));
 		TestRunner->TestEqual(TEXT("ProcessChunks summary should identify hook stage"), ProcessChunksEvent->PreprocessorSummary.Stage, EAngelscriptPreprocessorSummaryStage::ProcessChunks);
 		TestRunner->TestEqual(TEXT("ProcessChunks summary should report file count"), ProcessChunksEvent->PreprocessorSummary.FileCount, 1);
 		TestRunner->TestEqual(TEXT("ProcessChunks summary should report class count"), ProcessChunksEvent->PreprocessorSummary.ClassCount, 1);
 		TestRunner->TestEqual(TEXT("ProcessChunks summary should report function count"), ProcessChunksEvent->PreprocessorSummary.FunctionCount, 1);
 		TestRunner->TestEqual(TEXT("ProcessChunks summary should not report final processed code yet"), ProcessChunksEvent->PreprocessorSummary.ProcessedCodeCharacterCount, 0);
 		TestRunner->TestTrue(TEXT("ProcessChunks event should carry module name"), ProcessChunksEvent->ModuleNames.Contains(TEXT("Tests.Preprocessor.CompilationEvents.HookMoments")));
 		TestRunner->TestEqual(TEXT("ProcessChunks event should carry file count"), ProcessChunksEvent->FileCount, 1);
 	}

 	if (TestRunner->TestNotNull(TEXT("PostProcessCode compilation event should be emitted"), PostProcessCodeEvent))
 	{
 		TestRunner->TestEqual(TEXT("PostProcessCode phase should be stable"), PostProcessCodeEvent->Phase, FName(TEXT("Preprocess.PostProcessCode")));
 		TestRunner->TestEqual(TEXT("PostProcessCode summary should identify hook stage"), PostProcessCodeEvent->PreprocessorSummary.Stage, EAngelscriptPreprocessorSummaryStage::PostProcessCode);
 		TestRunner->TestEqual(TEXT("PostProcessCode summary should report file count"), PostProcessCodeEvent->PreprocessorSummary.FileCount, 1);
 		TestRunner->TestEqual(TEXT("PostProcessCode summary should report class count"), PostProcessCodeEvent->PreprocessorSummary.ClassCount, 1);
 		TestRunner->TestEqual(TEXT("PostProcessCode summary should report function count"), PostProcessCodeEvent->PreprocessorSummary.FunctionCount, 1);
 		TestRunner->TestTrue(TEXT("PostProcessCode summary should report final processed code"), PostProcessCodeEvent->PreprocessorSummary.ProcessedCodeCharacterCount > 0);
 		TestRunner->TestTrue(TEXT("PostProcessCode event should carry module name"), PostProcessCodeEvent->ModuleNames.Contains(TEXT("Tests.Preprocessor.CompilationEvents.HookMoments")));
 		TestRunner->TestEqual(TEXT("PostProcessCode event should carry file count"), PostProcessCodeEvent->FileCount, 1);
 	}

	}
 }

 TEST_METHOD(ClassAnalyzeHookMutatesGeneratedStaticsThroughEngineHooks)
 {
 	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
 	{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

 	int32 ClassAnalyzeCount = 0;
 	Engine.GetHooks().GetClassAnalyze().BindLambda(
 		[&ClassAnalyzeCount](FString& GeneratedStatics, TSharedPtr<FAngelscriptClassDesc> ClassDesc, bool& bHasStatics)
 		{
 			++ClassAnalyzeCount;
 			if (ClassDesc.IsValid() && ClassDesc->ClassName == TEXT("UClassAnalyzeHookCarrier"))
 			{
 				GeneratedStatics += TEXT("\n int EngineHookValue() __generated { return 31; }");
 				bHasStatics = true;
 			}
 		});
 	ON_SCOPE_EXIT
 	{
 		Engine.GetHooks().GetClassAnalyze().Unbind();
 	};

 	FFixtureFile File(TEXT("Tests/Preprocessor/CompilationEvents/ClassAnalyzeHook.as"), TEXT(R"(
UCLASS()
class UClassAnalyzeHookCarrier : UObject
{
    UFUNCTION()
    int Entry()
    {
        return 5;
    }
}
)"));

 	FPreprocessResult Result = RunPreprocess(Engine, File);

 	AssertPreprocessSucceeded(*TestRunner, Result);
 	AssertNoDiagnostics(*TestRunner, Result);
 	TestRunner->TestEqual(TEXT("Class analyze hook should fire once for the class"), ClassAnalyzeCount, 1);

 	FAngelscriptModuleDesc* Module = AssertModuleExists(
 		*TestRunner,
 		Result,
 		TEXT("Tests.Preprocessor.CompilationEvents.ClassAnalyzeHook"));
 	if (Module != nullptr)
 	{
 		AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("EngineHookValue"));
 		AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 31;"));
 	}

 	}
 }
};

#endif // WITH_DEV_AUTOMATION_TESTS
