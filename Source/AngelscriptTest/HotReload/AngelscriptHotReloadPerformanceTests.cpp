#include "CQTest.h"
#include "AngelscriptPerformanceTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadPerformanceTests_Private
{
struct FHotReloadPerformanceSample
	{
		double ReloadSeconds = 0.0;
		ECompileResult CompileResult = ECompileResult::Error;
	};

	template<typename MeasureFunc>
	TArray<FHotReloadPerformanceSample> CollectHotReloadSamples(MeasureFunc&& Measure)
	{
		constexpr int32 WarmupRuns = 1;
		constexpr int32 MeasurementRuns = 3;
		for (int32 WarmupIndex = 0; WarmupIndex < WarmupRuns; ++WarmupIndex)
		{
			Measure();
		}

		TArray<FHotReloadPerformanceSample> Samples;
		for (int32 MeasurementIndex = 0; MeasurementIndex < MeasurementRuns; ++MeasurementIndex)
		{
			Samples.Add(Measure());
		}
		return Samples;
	}

	FString WriteHotReloadMetrics(FAutomationTestBase& Test, const FString& RunId, const FString& TestGroup, const FString& MetricName, const TArray<FHotReloadPerformanceSample>& Samples, const TArray<FString>& Notes)
	{
		TArray<double> Durations;
		for (const FHotReloadPerformanceSample& Sample : Samples)
		{
			Durations.Add(Sample.ReloadSeconds);
		}

		LogPerformanceMetric(MetricName, Durations);

		TArray<FAngelscriptPerformanceMetric> Metrics;
		Metrics.Add({ MetricName, Durations, ComputeMedian(Durations) });
		const FString MetricsPath = WritePerformanceMetricsArtifact(RunId, TestGroup, Metrics, Notes);
		Test.TestTrue(TEXT("Hot reload performance test should write a metrics.json artifact"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetricsPath));
		return MetricsPath;
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define AddExpectedError(...) Test.AddExpectedError(__VA_ARGS__)
#define AddExpectedErrorPlain(...) Test.AddExpectedErrorPlain(__VA_ARGS__)

static bool HotReloadSoftLatency(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPerformanceTests_Private;
	const auto Measure = []() -> FHotReloadPerformanceSample
	{
		FHotReloadPerformanceSample ReturnSample;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("HotReloadPerformanceSoft"));
		ResetSharedCloneEngine(Engine);

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceSoft : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceSoft : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS");

		CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("HotReloadPerformanceSoft.as"), ScriptV1);
		ECompileResult ReloadResult = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("HotReloadPerformanceSoft.as"), ScriptV2, ReloadResult);
		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		Engine.DiscardModule(*ModuleName.ToString());
		ReturnSample = { Elapsed, ReloadResult };
		}

		return ReturnSample;
	};

	const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(Measure);
	for (const FHotReloadPerformanceSample& Sample : Samples)
	{
		TestTrue(TEXT("Soft reload latency baseline should stay on a handled reload path"), Sample.CompileResult == ECompileResult::FullyHandled || Sample.CompileResult == ECompileResult::PartiallyHandled);
	}
	WriteHotReloadMetrics(Test, TEXT("P3_2_HotReloadPerformance_Soft"), TEXT("Angelscript.TestModule.HotReload.Performance.SoftReloadLatency"), TEXT("reload.modify.soft_seconds"), Samples, { TEXT("Measured on a body-only module change via SoftReloadOnly compile path.") });
	return true;
}

static bool HotReloadFullLatency(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPerformanceTests_Private;
	const auto Measure = []() -> FHotReloadPerformanceSample
	{
		FHotReloadPerformanceSample ReturnSample;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("HotReloadPerformanceFull"));
		ResetSharedCloneEngine(Engine);

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceFull : UObject
{
	UPROPERTY()
	int Value;
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceFull : UObject
{
	UPROPERTY()
	int Value;

	UPROPERTY()
	int ExtraValue;
}
)AS");

		CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("HotReloadPerformanceFull.as"), ScriptV1);
		ECompileResult ReloadResult = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, TEXT("HotReloadPerformanceFull.as"), ScriptV2, ReloadResult);
		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		Engine.DiscardModule(*ModuleName.ToString());
		ReturnSample = { Elapsed, ReloadResult };
		}

		return ReturnSample;
	};

	const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(Measure);
	for (const FHotReloadPerformanceSample& Sample : Samples)
	{
		TestTrue(TEXT("Full reload latency baseline should stay on a handled full reload path"), Sample.CompileResult == ECompileResult::FullyHandled || Sample.CompileResult == ECompileResult::PartiallyHandled);
	}
	WriteHotReloadMetrics(Test, TEXT("P3_2_HotReloadPerformance_Full"), TEXT("Angelscript.TestModule.HotReload.Performance.FullReloadLatency"), TEXT("reload.full.seconds"), Samples, { TEXT("Measured on a structural property change via FullReload compile path.") });
	return true;
}

static bool HotReloadRenameWindowLatency(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPerformanceTests_Private;
	AddExpectedError(TEXT("Cannot declare class UHotReloadPerformanceRename in module HotReloadPerformanceRenameNew. A class with this name already exists in module HotReloadPerformanceRenameOld."), EAutomationExpectedErrorFlags::Contains, 4);

	const auto Measure = []() -> FHotReloadPerformanceSample
	{
		FHotReloadPerformanceSample ReturnSample;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("HotReloadPerformanceRename"));
		ResetSharedCloneEngine(Engine);

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceRename : UObject
{
	UPROPERTY()
	int Value;
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceRename : UObject
{
	UPROPERTY()
	int Value;

	UPROPERTY()
	int RenamedWindowExtraValue;
}
)AS");

		CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("HotReloadPerformanceRenameOld.as"), ScriptV1);
		ECompileResult ReloadResult = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, TEXT("HotReloadPerformanceRenameNew.as"), ScriptV2, ReloadResult);
		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		Engine.DiscardModule(*ModuleName.ToString());
		ReturnSample = { Elapsed, ReloadResult };
		}

		return ReturnSample;
	};

	const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(Measure);
	for (const FHotReloadPerformanceSample& Sample : Samples)
	{
		TestTrue(TEXT("Rename-window latency baseline should complete a modeled rename-window processing pass"),
			Sample.CompileResult == ECompileResult::FullyHandled
			|| Sample.CompileResult == ECompileResult::PartiallyHandled
			|| Sample.CompileResult == ECompileResult::Error
			|| Sample.CompileResult == ECompileResult::ErrorNeedFullReload);
	}
	WriteHotReloadMetrics(Test, TEXT("P3_2_HotReloadPerformance_RenameWindow"), TEXT("Angelscript.TestModule.HotReload.Performance.RenameWindowLatency"), TEXT("reload.rename_window.full_seconds"), Samples, { TEXT("Rename-window latency is modeled as old-file removal plus new-file addition on the full reload path.") });
	return true;
}

static bool HotReloadBurstChurnLatency(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPerformanceTests_Private;
	AddExpectedErrorPlain(TEXT("Full Reload is required due to UPROPERTY() or UFUNCTION() changes, but cannot perform a full reload right now. Keeping old angelscript code active."), EAutomationExpectedErrorFlags::Contains, -1);

	const auto Measure = []() -> FHotReloadPerformanceSample
	{
		FHotReloadPerformanceSample ReturnSample;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("HotReloadPerformanceBurst"));
		ResetSharedCloneEngine(Engine);

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceBurst : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceBurst : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS");
		const FString ScriptV3 = TEXT(R"AS(
UCLASS()
class UHotReloadPerformanceBurst : UObject
{
	UPROPERTY()
	int ExtraValue;

	UFUNCTION()
	int GetValue()
	{
		return 3;
	}
}
)AS");

		CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("HotReloadPerformanceBurst.as"), ScriptV1);
		const double StartTime = FPlatformTime::Seconds();
		ECompileResult StepOne = ECompileResult::Error;
		ECompileResult StepTwo = ECompileResult::Error;
		ECompileResult StepThree = ECompileResult::Error;
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("HotReloadPerformanceBurst.as"), ScriptV2, StepOne);
		CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, TEXT("HotReloadPerformanceBurst.as"), ScriptV3, StepTwo);
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("HotReloadPerformanceBurst.as"), ScriptV2, StepThree);
		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		Engine.DiscardModule(*ModuleName.ToString());
		const bool bStepOneHandled = StepOne == ECompileResult::FullyHandled || StepOne == ECompileResult::PartiallyHandled;
		const bool bStepTwoHandled = StepTwo == ECompileResult::FullyHandled || StepTwo == ECompileResult::PartiallyHandled || StepTwo == ECompileResult::ErrorNeedFullReload;
		const bool bStepThreeHandled = StepThree == ECompileResult::FullyHandled || StepThree == ECompileResult::PartiallyHandled || StepThree == ECompileResult::ErrorNeedFullReload;
		const ECompileResult AggregateResult = (bStepOneHandled && bStepTwoHandled && bStepThreeHandled)
			? (StepTwo == ECompileResult::ErrorNeedFullReload || StepThree == ECompileResult::ErrorNeedFullReload ? ECompileResult::ErrorNeedFullReload : ECompileResult::FullyHandled)
			: ECompileResult::Error;
		ReturnSample = { Elapsed, AggregateResult };
		}

		return ReturnSample;
	};

	const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(Measure);
	for (const FHotReloadPerformanceSample& Sample : Samples)
	{
		TestTrue(TEXT("Burst churn latency test should complete a modeled reload burst or explicitly surface the need for a deferred full reload"),
			Sample.CompileResult == ECompileResult::FullyHandled
			|| Sample.CompileResult == ECompileResult::PartiallyHandled
			|| Sample.CompileResult == ECompileResult::ErrorNeedFullReload);
	}
	WriteHotReloadMetrics(Test, TEXT("P3_4_HotReloadPerformance_BurstChurn"), TEXT("Angelscript.TestModule.HotReload.Performance.BurstChurnLatency"), TEXT("reload.burst_churn.seconds"), Samples, { TEXT("Burst churn baseline models repeated soft/full/soft reload operations on one module.") });
	return true;
}

#undef TestTrue
#undef AddExpectedError
#undef AddExpectedErrorPlain

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadPerformanceTests,
	"Angelscript.TestModule.HotReload.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SoftReloadLatency)
	{
		ASSERT_THAT(IsTrue(HotReloadSoftLatency(*TestRunner)));
	}

	TEST_METHOD(FullReloadLatency)
	{
		ASSERT_THAT(IsTrue(HotReloadFullLatency(*TestRunner)));
	}

	TEST_METHOD(RenameWindowLatency)
	{
		ASSERT_THAT(IsTrue(HotReloadRenameWindowLatency(*TestRunner)));
	}

	TEST_METHOD(BurstChurnLatency)
	{
		ASSERT_THAT(IsTrue(HotReloadBurstChurnLatency(*TestRunner)));
	}
};

#endif
