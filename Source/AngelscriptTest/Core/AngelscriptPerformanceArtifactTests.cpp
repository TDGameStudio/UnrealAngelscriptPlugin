#include "AngelscriptPerformanceTestUtils.h"

#include "CQTest.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS


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
		ASSERT_THAT(IsTrue(PlatformFile.FileExists(*MetricsPath), TEXT("Performance artifact generation test should write metrics.json")));
		ASSERT_THAT(IsTrue(PlatformFile.DirectoryExists(*FPaths::GetPath(MetricsPath)), TEXT("Performance artifact generation test should create the Metrics directory")));
		FString Contents;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Contents, *MetricsPath), TEXT("Performance artifact generation test should read the metrics artifact")));
		ASSERT_THAT(IsTrue(Contents.Contains(TEXT("artifact.generation.seconds")), TEXT("Performance artifact generation test should persist the metric name")));
		ASSERT_THAT(IsTrue(Contents.Contains(TEXT("\"unit\":\"seconds\"")), TEXT("Performance artifact generation test should persist the metric unit")));
		ASSERT_THAT(IsTrue(Contents.Contains(TEXT("\"source\":\"RuntimeInstrumentation\"")), TEXT("Performance artifact generation test should persist the metric source")));
		ASSERT_THAT(IsTrue(Contents.Contains(RunId), TEXT("Performance artifact generation test should persist the run id")));
	}
};

#endif
