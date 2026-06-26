#include "CQTest.h"
#include "AngelscriptPerformanceTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadPerformanceTests,
	"Angelscript.TestModule.HotReload.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName SoftReloadModuleName = FName(TEXT("HotReloadPerformanceSoft"));
	inline static const FString SoftReloadFilename = FString(TEXT("HotReloadPerformanceSoft.as"));

	inline static const FName FullReloadModuleName = FName(TEXT("HotReloadPerformanceFull"));
	inline static const FString FullReloadFilename = FString(TEXT("HotReloadPerformanceFull.as"));

	inline static const FName RenameWindowModuleName = FName(TEXT("HotReloadPerformanceRename"));
	inline static const FString RenameWindowOldFilename = FString(TEXT("HotReloadPerformanceRenameOld.as"));
	inline static const FString RenameWindowNewFilename = FString(TEXT("HotReloadPerformanceRenameNew.as"));

	inline static const FName BurstChurnModuleName = FName(TEXT("HotReloadPerformanceBurst"));
	inline static const FString BurstChurnFilename = FString(TEXT("HotReloadPerformanceBurst.as"));

	struct FHotReloadPerformanceSample
	{
		double ReloadSeconds = 0.0;
		ECompileResult CompileResult = ECompileResult::Error;
	};

	template<typename MeasureFunc>
	static TArray<FHotReloadPerformanceSample> CollectHotReloadSamples(MeasureFunc&& Measure)
	{
		constexpr int32 WarmupRuns = 1;
		constexpr int32 MeasurementRuns = 3;

		for (int32 WarmupIndex = 0; WarmupIndex < WarmupRuns; ++WarmupIndex)
		{
			Measure();
		}

		TArray<FHotReloadPerformanceSample> Samples;
		Samples.Reserve(MeasurementRuns);
		for (int32 MeasurementIndex = 0; MeasurementIndex < MeasurementRuns; ++MeasurementIndex)
		{
			Samples.Add(Measure());
		}

		return Samples;
	}

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool IsRenameWindowResult(const ECompileResult ReloadResult)
	{
		return IsHandledReloadResult(ReloadResult)
			|| ReloadResult == ECompileResult::Error
			|| ReloadResult == ECompileResult::ErrorNeedFullReload;
	}

	static bool IsBurstStepResult(const ECompileResult ReloadResult)
	{
		return IsHandledReloadResult(ReloadResult) || ReloadResult == ECompileResult::ErrorNeedFullReload;
	}

	static ECompileResult AggregateBurstResult(
		const ECompileResult StepOne,
		const ECompileResult StepTwo,
		const ECompileResult StepThree)
	{
		if (!IsHandledReloadResult(StepOne) || !IsBurstStepResult(StepTwo) || !IsBurstStepResult(StepThree))
		{
			return ECompileResult::Error;
		}

		return StepTwo == ECompileResult::ErrorNeedFullReload || StepThree == ECompileResult::ErrorNeedFullReload
			? ECompileResult::ErrorNeedFullReload
			: ECompileResult::FullyHandled;
	}

	static bool WriteHotReloadMetrics(
		FAutomationTestBase& Test,
		const FString& RunId,
		const FString& TestGroup,
		const FString& MetricName,
		const TArray<FHotReloadPerformanceSample>& Samples,
		const TArray<FString>& Notes)
	{
		TArray<double> Durations;
		Durations.Reserve(Samples.Num());
		for (const FHotReloadPerformanceSample& Sample : Samples)
		{
			Durations.Add(Sample.ReloadSeconds);
		}

		LogPerformanceMetric(MetricName, Durations);

		TArray<FAngelscriptPerformanceMetric> Metrics;
		Metrics.Add({ MetricName, Durations, ComputeMedian(Durations) });

		const FString MetricsPath = WritePerformanceMetricsArtifact(RunId, TestGroup, Metrics, Notes);
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(
			FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetricsPath),
			TEXT("Hot reload performance test should write a metrics.json artifact"));
	}

	static void RegisterRenameWindowExpectedErrors(FAutomationTestBase& Test)
	{
		Test.AddExpectedError(
			TEXT("Cannot declare class UHotReloadPerformanceRename in module HotReloadPerformanceRenameNew. A class with this name already exists in module HotReloadPerformanceRenameOld."),
			EAutomationExpectedErrorFlags::Contains,
			4);
	}

	static void RegisterBurstChurnExpectedErrors(FAutomationTestBase& Test)
	{
		Test.AddExpectedErrorPlain(
			TEXT("Full Reload is required due to UPROPERTY() or UFUNCTION() changes, but cannot perform a full reload right now. Keeping old angelscript code active."),
			EAutomationExpectedErrorFlags::Contains,
			-1);
	}

	static FHotReloadPerformanceSample MeasureSoftReloadLatency()
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ResetSharedCloneEngine(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*SoftReloadModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceSoft : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					int Result = 1;
					Log(n"HotReloadPerformanceTests", "SoftReload V1 GetValue Result=" + Result);
					return Result;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceSoft : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					int Result = 2;
					Log(n"HotReloadPerformanceTests", "SoftReload V2 GetValue Result=" + Result);
					return Result;
				}
			}
			)AS");

		CompileAnnotatedModuleFromMemory(&Engine, SoftReloadModuleName, SoftReloadFilename, ScriptV1);

		ECompileResult ReloadResult = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadModuleName, SoftReloadFilename, ScriptV2, ReloadResult);

		return { FPlatformTime::Seconds() - StartTime, ReloadResult };
	}

	static FHotReloadPerformanceSample MeasureFullReloadLatency()
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ResetSharedCloneEngine(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FullReloadModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceFull : UObject
			{
				UPROPERTY()
				int Value;
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceFull : UObject
			{
				UPROPERTY()
				int Value;

				UPROPERTY()
				int ExtraValue;
			}
			)AS");

		CompileAnnotatedModuleFromMemory(&Engine, FullReloadModuleName, FullReloadFilename, ScriptV1);

		ECompileResult ReloadResult = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::FullReload, FullReloadModuleName, FullReloadFilename, ScriptV2, ReloadResult);

		return { FPlatformTime::Seconds() - StartTime, ReloadResult };
	}

	static FHotReloadPerformanceSample MeasureRenameWindowLatency()
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ResetSharedCloneEngine(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*RenameWindowModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceRename : UObject
			{
				UPROPERTY()
				int Value;
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceRename : UObject
			{
				UPROPERTY()
				int Value;

				UPROPERTY()
				int RenamedWindowExtraValue;
			}
			)AS");

		CompileAnnotatedModuleFromMemory(&Engine, RenameWindowModuleName, RenameWindowOldFilename, ScriptV1);

		ECompileResult ReloadResult = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameWindowModuleName, RenameWindowNewFilename, ScriptV2, ReloadResult);

		return { FPlatformTime::Seconds() - StartTime, ReloadResult };
	}

	static FHotReloadPerformanceSample MeasureBurstChurnLatency()
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ResetSharedCloneEngine(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*BurstChurnModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceBurst : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					int Result = 1;
					Log(n"HotReloadPerformanceTests", "BurstChurn V1 GetValue Result=" + Result);
					return Result;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceBurst : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					int Result = 2;
					Log(n"HotReloadPerformanceTests", "BurstChurn V2 GetValue Result=" + Result);
					return Result;
				}
			}
			)AS");

		const FString ScriptV3 = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadPerformanceBurst : UObject
			{
				UPROPERTY()
				int ExtraValue;

				UFUNCTION()
				int GetValue()
				{
					int Result = 3;
					Log(n"HotReloadPerformanceTests", "BurstChurn V3 GetValue Result=" + Result + " ExtraValue=" + ExtraValue);
					return Result;
				}
			}
			)AS");

		CompileAnnotatedModuleFromMemory(&Engine, BurstChurnModuleName, BurstChurnFilename, ScriptV1);

		ECompileResult StepOne = ECompileResult::Error;
		ECompileResult StepTwo = ECompileResult::Error;
		ECompileResult StepThree = ECompileResult::Error;
		const double StartTime = FPlatformTime::Seconds();
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, BurstChurnModuleName, BurstChurnFilename, ScriptV2, StepOne);
		CompileModuleWithResult(&Engine, ECompileType::FullReload, BurstChurnModuleName, BurstChurnFilename, ScriptV3, StepTwo);
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, BurstChurnModuleName, BurstChurnFilename, ScriptV2, StepThree);

		return { FPlatformTime::Seconds() - StartTime, AggregateBurstResult(StepOne, StepTwo, StepThree) };
	}

	static bool AssertSamples(
		FAutomationTestBase& Test,
		const TArray<FHotReloadPerformanceSample>& Samples,
		bool (*Predicate)(ECompileResult),
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
		{
			if (!LocalAssert.IsTrue(
					Predicate(Samples[SampleIndex].CompileResult),
					*FString::Printf(TEXT("%s sample %d should produce an expected reload result"), Context, SampleIndex)))
			{
				return false;
			}
		}

		return true;
	}

public:
	TEST_METHOD(SoftReloadLatency)
	{
		const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(&MeasureSoftReloadLatency);
		ASSERT_THAT(IsTrue(AssertSamples(*TestRunner, Samples, &IsHandledReloadResult, TEXT("Soft reload latency baseline"))));

		ASSERT_THAT(IsTrue(WriteHotReloadMetrics(
			*TestRunner,
			TEXT("P3_2_HotReloadPerformance_Soft"),
			TEXT("Angelscript.TestModule.HotReload.Performance.SoftReloadLatency"),
			TEXT("reload.modify.soft_seconds"),
			Samples,
			{ TEXT("Measured on a body-only module change via SoftReloadOnly compile path.") })));
	}

	TEST_METHOD(FullReloadLatency)
	{
		const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(&MeasureFullReloadLatency);
		ASSERT_THAT(IsTrue(AssertSamples(*TestRunner, Samples, &IsHandledReloadResult, TEXT("Full reload latency baseline"))));

		ASSERT_THAT(IsTrue(WriteHotReloadMetrics(
			*TestRunner,
			TEXT("P3_2_HotReloadPerformance_Full"),
			TEXT("Angelscript.TestModule.HotReload.Performance.FullReloadLatency"),
			TEXT("reload.full.seconds"),
			Samples,
			{ TEXT("Measured on a structural property change via FullReload compile path.") })));
	}

	TEST_METHOD(RenameWindowLatency)
	{
		RegisterRenameWindowExpectedErrors(*TestRunner);

		const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(&MeasureRenameWindowLatency);
		ASSERT_THAT(IsTrue(AssertSamples(*TestRunner, Samples, &IsRenameWindowResult, TEXT("Rename-window latency baseline"))));

		ASSERT_THAT(IsTrue(WriteHotReloadMetrics(
			*TestRunner,
			TEXT("P3_2_HotReloadPerformance_RenameWindow"),
			TEXT("Angelscript.TestModule.HotReload.Performance.RenameWindowLatency"),
			TEXT("reload.rename_window.full_seconds"),
			Samples,
			{ TEXT("Rename-window latency is modeled as old-file removal plus new-file addition on the full reload path.") })));
	}

	TEST_METHOD(BurstChurnLatency)
	{
		RegisterBurstChurnExpectedErrors(*TestRunner);

		const TArray<FHotReloadPerformanceSample> Samples = CollectHotReloadSamples(&MeasureBurstChurnLatency);
		ASSERT_THAT(IsTrue(AssertSamples(*TestRunner, Samples, &IsBurstStepResult, TEXT("Burst churn latency baseline"))));

		ASSERT_THAT(IsTrue(WriteHotReloadMetrics(
			*TestRunner,
			TEXT("P3_4_HotReloadPerformance_BurstChurn"),
			TEXT("Angelscript.TestModule.HotReload.Performance.BurstChurnLatency"),
			TEXT("reload.burst_churn.seconds"),
			Samples,
			{ TEXT("Burst churn baseline models repeated soft/full/soft reload operations on one module.") })));
	}
};
