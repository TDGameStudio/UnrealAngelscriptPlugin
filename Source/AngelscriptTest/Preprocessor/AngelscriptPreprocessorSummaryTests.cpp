// ============================================================================
// AngelscriptPreprocessorSummaryTests.cpp
//
// Preprocessor tests for read-only value-style preprocessing summaries.
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Summary.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorSummaryTest,
	"Angelscript.TestModule.Preprocessor.Summary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FHookSummaryCapture
{
	FDelegateHandle ProcessChunksHandle;
	FDelegateHandle PostProcessCodeHandle;
	TArray<FAngelscriptPreprocessorSummary> ProcessChunksSummaries;
	TArray<FAngelscriptPreprocessorSummary> PostProcessCodeSummaries;

	FHookSummaryCapture()
	{
		ProcessChunksHandle = FAngelscriptPreprocessor::OnProcessChunks.AddRaw(this, &FHookSummaryCapture::HandleProcessChunks);
		PostProcessCodeHandle = FAngelscriptPreprocessor::OnPostProcessCode.AddRaw(this, &FHookSummaryCapture::HandlePostProcessCode);
	}

	~FHookSummaryCapture()
	{
		if (ProcessChunksHandle.IsValid())
		{
			FAngelscriptPreprocessor::OnProcessChunks.Remove(ProcessChunksHandle);
		}

		if (PostProcessCodeHandle.IsValid())
		{
			FAngelscriptPreprocessor::OnPostProcessCode.Remove(PostProcessCodeHandle);
		}
	}

	void HandleProcessChunks(FAngelscriptPreprocessor& Preprocessor)
	{
		ProcessChunksSummaries.Add(Preprocessor.GetSummary());
	}

	void HandlePostProcessCode(FAngelscriptPreprocessor& Preprocessor)
	{
		PostProcessCodeSummaries.Add(Preprocessor.GetSummary());
	}
};

public:
	TEST_METHOD(SummaryReportsProcessedScriptStructure)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TPair<FString, FString> FixtureData[] = {
			{
				TEXT("Tests/Preprocessor/Summary/Shared.as"),
				TEXT(R"(
UENUM()
enum ESummaryState
{
    Idle,
    Active
}

delegate float FSummaryDelegate();

UCLASS()
class USummaryShared : UObject
{
    UFUNCTION()
    float GetAmount()
    {
        return 3.0;
    }

    UPROPERTY()
    int SharedValue;
}
)"),
			},
			{
				TEXT("Tests/Preprocessor/Summary/Consumer.as"),
				TEXT(R"(
import Tests.Preprocessor.Summary.Shared;

UCLASS()
class USummaryConsumer : UObject
{
    UPROPERTY()
    int ConsumerValue;
}
)"),
			},
		};

		TArray<FFixtureFile> Files = WriteFixtures(MakeArrayView(FixtureData));

		FAngelscriptPreprocessorContext Context = FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		Context.bUseAutomaticImportMethod = false;
		FPreprocessSession Session = RunPreprocessSession(Engine, Files, Context);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertNoDiagnostics(*TestRunner, Session.Result);

		const FAngelscriptPreprocessorSummary Summary = Session.Preprocessor.GetSummary();
		ASSERT_THAT(IsTrue(Summary.bSucceeded, TEXT("Summary should report success")));
		ASSERT_THAT(AreEqual(2, Summary.FileCount, TEXT("Summary should report two files")));
		ASSERT_THAT(AreEqual(2, Summary.ModuleCount, TEXT("Summary should report two modules")));
		ASSERT_THAT(AreEqual(1, Summary.ImportCount, TEXT("Summary should report one explicit import")));
		ASSERT_THAT(AreEqual(2, Summary.ClassCount, TEXT("Summary should report two classes")));
		ASSERT_THAT(AreEqual(1, Summary.FunctionCount, TEXT("Summary should report one function")));
		ASSERT_THAT(AreEqual(2, Summary.PropertyCount, TEXT("Summary should report two properties")));
		ASSERT_THAT(AreEqual(1, Summary.EnumCount, TEXT("Summary should report one enum")));
		ASSERT_THAT(AreEqual(1, Summary.DelegateCount, TEXT("Summary should report one delegate")));
		ASSERT_THAT(IsTrue(Summary.GeneratedCodeSectionCount > 0, TEXT("Summary should report generated code")));
		ASSERT_THAT(IsTrue(Summary.ProcessedCodeCharacterCount > 0, TEXT("Summary should report processed code")));

		ASSERT_THAT(IsTrue(
			Summary.ModuleNames.Contains(TEXT("Tests.Preprocessor.Summary.Shared")),
			TEXT("Summary should contain shared module name")));
		ASSERT_THAT(IsTrue(
			Summary.ModuleNames.Contains(TEXT("Tests.Preprocessor.Summary.Consumer")),
			TEXT("Summary should contain consumer module name")));
		ASSERT_THAT(IsTrue(
			Summary.ClassNames.Contains(TEXT("USummaryShared")) && Summary.ClassNames.Contains(TEXT("USummaryConsumer")),
			TEXT("Summary should contain class names")));
		ASSERT_THAT(IsTrue(
			Summary.FunctionNames.Contains(TEXT("GetAmount")),
			TEXT("Summary should contain function name")));
		ASSERT_THAT(IsTrue(
			Summary.PropertyNames.Contains(TEXT("SharedValue")) && Summary.PropertyNames.Contains(TEXT("ConsumerValue")),
			TEXT("Summary should contain property names")));
		ASSERT_THAT(IsTrue(
			Summary.EnumNames.Contains(TEXT("ESummaryState")),
			TEXT("Summary should contain enum name")));
		ASSERT_THAT(IsTrue(
			Summary.DelegateNames.Contains(TEXT("FSummaryDelegate")),
			TEXT("Summary should contain delegate name")));

		const FAngelscriptPreprocessorFileSummary* ConsumerFile = Summary.Files.FindByPredicate(
			[](const FAngelscriptPreprocessorFileSummary& FileSummary)
			{
				return FileSummary.ModuleName == TEXT("Tests.Preprocessor.Summary.Consumer");
			});

		if (this->Assert.IsNotNull(ConsumerFile, TEXT("Summary should include a consumer file summary")))
		{
			ASSERT_THAT(AreEqual(1, ConsumerFile->ImportCount, TEXT("Consumer file should report one import")));
			ASSERT_THAT(IsTrue(ConsumerFile->ImportedModuleNames.Contains(TEXT("Tests.Preprocessor.Summary.Shared")), TEXT("Consumer file should report its imported module")));
			ASSERT_THAT(AreEqual(1, ConsumerFile->ClassCount, TEXT("Consumer file should report one class")));
		}

		}
	}

	TEST_METHOD(SummaryAvailableAtExistingHookPoints)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/Summary/HookAvailability.as"), TEXT(R"(
UCLASS()
class USummaryHookCarrier : UObject
{
    UPROPERTY()
    int Value;
}
)"));

		FHookSummaryCapture Capture;
		ON_SCOPE_EXIT
		{
			FAngelscriptPreprocessor::OnProcessChunks.Remove(Capture.ProcessChunksHandle);
			Capture.ProcessChunksHandle.Reset();
			FAngelscriptPreprocessor::OnPostProcessCode.Remove(Capture.PostProcessCodeHandle);
			Capture.PostProcessCodeHandle.Reset();
		};

		FPreprocessResult Result = RunPreprocess(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertNoDiagnostics(*TestRunner, Result);

		ASSERT_THAT(AreEqual(1, Capture.ProcessChunksSummaries.Num(), TEXT("ProcessChunks hook should capture one summary")));
		ASSERT_THAT(AreEqual(1, Capture.PostProcessCodeSummaries.Num(), TEXT("PostProcessCode hook should capture one summary")));

		if (Capture.ProcessChunksSummaries.Num() == 1)
		{
			const FAngelscriptPreprocessorSummary& ProcessSummary = Capture.ProcessChunksSummaries[0];
			ASSERT_THAT(AreEqual(EAngelscriptPreprocessorSummaryStage::ProcessChunks, ProcessSummary.Stage, TEXT("ProcessChunks summary should identify its phase")));
			ASSERT_THAT(AreEqual(1, ProcessSummary.ClassCount, TEXT("ProcessChunks summary should report class count")));
			ASSERT_THAT(AreEqual(1, ProcessSummary.PropertyCount, TEXT("ProcessChunks summary should report property count")));
			ASSERT_THAT(AreEqual(0, ProcessSummary.ProcessedCodeCharacterCount, TEXT("ProcessChunks summary should not report final processed code yet")));
		}

		if (Capture.PostProcessCodeSummaries.Num() == 1)
		{
			const FAngelscriptPreprocessorSummary& PostSummary = Capture.PostProcessCodeSummaries[0];
			ASSERT_THAT(AreEqual(EAngelscriptPreprocessorSummaryStage::PostProcessCode, PostSummary.Stage, TEXT("PostProcessCode summary should identify its phase")));
			ASSERT_THAT(AreEqual(1, PostSummary.ClassCount, TEXT("PostProcessCode summary should report class count")));
			ASSERT_THAT(AreEqual(1, PostSummary.PropertyCount, TEXT("PostProcessCode summary should report property count")));
			ASSERT_THAT(IsTrue(PostSummary.ProcessedCodeCharacterCount > 0, TEXT("PostProcessCode summary should report final processed code")));
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
