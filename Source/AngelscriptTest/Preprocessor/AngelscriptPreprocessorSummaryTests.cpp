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

using namespace PreprocessorTestHelpers;

namespace AngelscriptPreprocessorSummaryTests_Private
{
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorSummaryTest,
	"Angelscript.TestModule.Preprocessor.Summary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SummaryReportsProcessedScriptStructure)
	{
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
		TestRunner->TestTrue(TEXT("Summary should report success"), Summary.bSucceeded);
		TestRunner->TestEqual(TEXT("Summary should report two files"), Summary.FileCount, 2);
		TestRunner->TestEqual(TEXT("Summary should report two modules"), Summary.ModuleCount, 2);
		TestRunner->TestEqual(TEXT("Summary should report one explicit import"), Summary.ImportCount, 1);
		TestRunner->TestEqual(TEXT("Summary should report two classes"), Summary.ClassCount, 2);
		TestRunner->TestEqual(TEXT("Summary should report one function"), Summary.FunctionCount, 1);
		TestRunner->TestEqual(TEXT("Summary should report two properties"), Summary.PropertyCount, 2);
		TestRunner->TestEqual(TEXT("Summary should report one enum"), Summary.EnumCount, 1);
		TestRunner->TestEqual(TEXT("Summary should report one delegate"), Summary.DelegateCount, 1);
		TestRunner->TestTrue(TEXT("Summary should report generated code"), Summary.GeneratedCodeSectionCount > 0);
		TestRunner->TestTrue(TEXT("Summary should report processed code"), Summary.ProcessedCodeCharacterCount > 0);

		TestRunner->TestTrue(
			TEXT("Summary should contain shared module name"),
			Summary.ModuleNames.Contains(TEXT("Tests.Preprocessor.Summary.Shared")));
		TestRunner->TestTrue(
			TEXT("Summary should contain consumer module name"),
			Summary.ModuleNames.Contains(TEXT("Tests.Preprocessor.Summary.Consumer")));
		TestRunner->TestTrue(
			TEXT("Summary should contain class names"),
			Summary.ClassNames.Contains(TEXT("USummaryShared")) && Summary.ClassNames.Contains(TEXT("USummaryConsumer")));
		TestRunner->TestTrue(
			TEXT("Summary should contain function name"),
			Summary.FunctionNames.Contains(TEXT("GetAmount")));
		TestRunner->TestTrue(
			TEXT("Summary should contain property names"),
			Summary.PropertyNames.Contains(TEXT("SharedValue")) && Summary.PropertyNames.Contains(TEXT("ConsumerValue")));
		TestRunner->TestTrue(
			TEXT("Summary should contain enum name"),
			Summary.EnumNames.Contains(TEXT("ESummaryState")));
		TestRunner->TestTrue(
			TEXT("Summary should contain delegate name"),
			Summary.DelegateNames.Contains(TEXT("FSummaryDelegate")));

		const FAngelscriptPreprocessorFileSummary* ConsumerFile = Summary.Files.FindByPredicate(
			[](const FAngelscriptPreprocessorFileSummary& FileSummary)
			{
				return FileSummary.ModuleName == TEXT("Tests.Preprocessor.Summary.Consumer");
			});

		if (TestRunner->TestNotNull(TEXT("Summary should include a consumer file summary"), ConsumerFile))
		{
			TestRunner->TestEqual(TEXT("Consumer file should report one import"), ConsumerFile->ImportCount, 1);
			TestRunner->TestTrue(TEXT("Consumer file should report its imported module"), ConsumerFile->ImportedModuleNames.Contains(TEXT("Tests.Preprocessor.Summary.Shared")));
			TestRunner->TestEqual(TEXT("Consumer file should report one class"), ConsumerFile->ClassCount, 1);
		}

		}
	}

	TEST_METHOD(SummaryAvailableAtExistingHookPoints)
	{
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

		AngelscriptPreprocessorSummaryTests_Private::FHookSummaryCapture Capture;
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

		TestRunner->TestEqual(TEXT("ProcessChunks hook should capture one summary"), Capture.ProcessChunksSummaries.Num(), 1);
		TestRunner->TestEqual(TEXT("PostProcessCode hook should capture one summary"), Capture.PostProcessCodeSummaries.Num(), 1);

		if (Capture.ProcessChunksSummaries.Num() == 1)
		{
			const FAngelscriptPreprocessorSummary& ProcessSummary = Capture.ProcessChunksSummaries[0];
			TestRunner->TestEqual(TEXT("ProcessChunks summary should identify its phase"), ProcessSummary.Stage, EAngelscriptPreprocessorSummaryStage::ProcessChunks);
			TestRunner->TestEqual(TEXT("ProcessChunks summary should report class count"), ProcessSummary.ClassCount, 1);
			TestRunner->TestEqual(TEXT("ProcessChunks summary should report property count"), ProcessSummary.PropertyCount, 1);
			TestRunner->TestEqual(TEXT("ProcessChunks summary should not report final processed code yet"), ProcessSummary.ProcessedCodeCharacterCount, 0);
		}

		if (Capture.PostProcessCodeSummaries.Num() == 1)
		{
			const FAngelscriptPreprocessorSummary& PostSummary = Capture.PostProcessCodeSummaries[0];
			TestRunner->TestEqual(TEXT("PostProcessCode summary should identify its phase"), PostSummary.Stage, EAngelscriptPreprocessorSummaryStage::PostProcessCode);
			TestRunner->TestEqual(TEXT("PostProcessCode summary should report class count"), PostSummary.ClassCount, 1);
			TestRunner->TestEqual(TEXT("PostProcessCode summary should report property count"), PostSummary.PropertyCount, 1);
			TestRunner->TestTrue(TEXT("PostProcessCode summary should report final processed code"), PostSummary.ProcessedCodeCharacterCount > 0);
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
