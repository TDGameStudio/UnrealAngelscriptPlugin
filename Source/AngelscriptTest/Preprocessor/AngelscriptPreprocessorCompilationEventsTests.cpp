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

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorCompilationEventsTest,
	"Angelscript.TestModule.Preprocessor.CompilationEvents",
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

public:
 TEST_METHOD(HookMomentsEmitSummaryBackedCompilationEvents)
 {
	using namespace PreprocessorTestHelpers;

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

	const FAngelscriptCompilationEvent* ProcessChunksEvent = FindFirstEvent(
 		Events,
 		EAngelscriptCompilationEventType::PreprocessProcessChunks);
	const FAngelscriptCompilationEvent* PostProcessCodeEvent = FindFirstEvent(
 		Events,
 		EAngelscriptCompilationEventType::PreprocessPostProcessCode);

	ASSERT_THAT(IsNotNull(ProcessChunksEvent, TEXT("ProcessChunks compilation event should be emitted")));
	ASSERT_THAT(AreEqual(FName(TEXT("Preprocess.ProcessChunks")), ProcessChunksEvent->Phase, TEXT("ProcessChunks phase should be stable")));
	ASSERT_THAT(AreEqual(EAngelscriptPreprocessorSummaryStage::ProcessChunks, ProcessChunksEvent->PreprocessorSummary.Stage, TEXT("ProcessChunks summary should identify hook stage")));
	ASSERT_THAT(AreEqual(1, ProcessChunksEvent->PreprocessorSummary.FileCount, TEXT("ProcessChunks summary should report file count")));
	ASSERT_THAT(AreEqual(1, ProcessChunksEvent->PreprocessorSummary.ClassCount, TEXT("ProcessChunks summary should report class count")));
	ASSERT_THAT(AreEqual(1, ProcessChunksEvent->PreprocessorSummary.FunctionCount, TEXT("ProcessChunks summary should report function count")));
	ASSERT_THAT(AreEqual(0, ProcessChunksEvent->PreprocessorSummary.ProcessedCodeCharacterCount, TEXT("ProcessChunks summary should not report final processed code yet")));
	ASSERT_THAT(IsTrue(ProcessChunksEvent->ModuleNames.Contains(TEXT("Tests.Preprocessor.CompilationEvents.HookMoments")), TEXT("ProcessChunks event should carry module name")));
	ASSERT_THAT(AreEqual(1, ProcessChunksEvent->FileCount, TEXT("ProcessChunks event should carry file count")));

	ASSERT_THAT(IsNotNull(PostProcessCodeEvent, TEXT("PostProcessCode compilation event should be emitted")));
	ASSERT_THAT(AreEqual(FName(TEXT("Preprocess.PostProcessCode")), PostProcessCodeEvent->Phase, TEXT("PostProcessCode phase should be stable")));
	ASSERT_THAT(AreEqual(EAngelscriptPreprocessorSummaryStage::PostProcessCode, PostProcessCodeEvent->PreprocessorSummary.Stage, TEXT("PostProcessCode summary should identify hook stage")));
	ASSERT_THAT(AreEqual(1, PostProcessCodeEvent->PreprocessorSummary.FileCount, TEXT("PostProcessCode summary should report file count")));
	ASSERT_THAT(AreEqual(1, PostProcessCodeEvent->PreprocessorSummary.ClassCount, TEXT("PostProcessCode summary should report class count")));
	ASSERT_THAT(AreEqual(1, PostProcessCodeEvent->PreprocessorSummary.FunctionCount, TEXT("PostProcessCode summary should report function count")));
	ASSERT_THAT(IsTrue(PostProcessCodeEvent->PreprocessorSummary.ProcessedCodeCharacterCount > 0, TEXT("PostProcessCode summary should report final processed code")));
	ASSERT_THAT(IsTrue(PostProcessCodeEvent->ModuleNames.Contains(TEXT("Tests.Preprocessor.CompilationEvents.HookMoments")), TEXT("PostProcessCode event should carry module name")));
	ASSERT_THAT(AreEqual(1, PostProcessCodeEvent->FileCount, TEXT("PostProcessCode event should carry file count")));

	}
 }

 TEST_METHOD(ClassAnalyzeHookMutatesGeneratedStaticsThroughEngineHooks)
 {
	using namespace PreprocessorTestHelpers;

 	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
 	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

 	int32 ClassAnalyzeCount = 0;
 	Engine.GetClassAnalyze().BindLambda(
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
 		Engine.GetClassAnalyze().Unbind();
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
	ASSERT_THAT(AreEqual(1, ClassAnalyzeCount, TEXT("Class analyze hook should fire once for the class")));

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

#endif // WITH_ANGELSCRIPT_UNITTESTS
