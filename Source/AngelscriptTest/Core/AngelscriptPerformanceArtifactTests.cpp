#include "AngelscriptPerformanceTestUtils.h"

#include "CQTest.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptPerformanceArtifactTests,
	"Angelscript.TestModule.Core.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ArtifactGeneration)
	{
		const FString RunId(TEXT("P3_4_PerformanceArtifactGeneration"));
		const TArray<FAngelscriptPerformanceMetric> Metrics = {
			{ TEXT("artifact.generation.seconds"), { 0.1, 0.2, 0.3 }, ComputeMedian({ 0.1, 0.2, 0.3 }), TEXT("seconds"), TEXT("RuntimeInstrumentation") }
		};
		const FString MetricsPath = WritePerformanceMetricsArtifact(RunId, TEXT("Angelscript.TestModule.Core.Performance.ArtifactGeneration"), Metrics, { TEXT("Artifact generation regression writes a minimal metrics payload.") });
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		TestRunner->TestTrue(TEXT("Performance artifact generation test should write metrics.json"), PlatformFile.FileExists(*MetricsPath));
		TestRunner->TestTrue(TEXT("Performance artifact generation test should create the Metrics directory"), PlatformFile.DirectoryExists(*FPaths::GetPath(MetricsPath)));
		FString Contents;
		TestRunner->TestTrue(TEXT("Performance artifact generation test should read the metrics artifact"), FFileHelper::LoadFileToString(Contents, *MetricsPath));
		TestRunner->TestTrue(TEXT("Performance artifact generation test should persist the metric name"), Contents.Contains(TEXT("artifact.generation.seconds")));
		TestRunner->TestTrue(TEXT("Performance artifact generation test should persist the metric unit"), Contents.Contains(TEXT("\"unit\":\"seconds\"")));
		TestRunner->TestTrue(TEXT("Performance artifact generation test should persist the metric source"), Contents.Contains(TEXT("\"source\":\"RuntimeInstrumentation\"")));
		TestRunner->TestTrue(TEXT("Performance artifact generation test should persist the run id"), Contents.Contains(RunId));
	}
};

#endif
