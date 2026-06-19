#include "AngelscriptEngine.h"

#include "AngelscriptPerformanceTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "Testing/AngelscriptBindExecutionObservation.h"

#include "ClassGenerator/ASClass.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "CQTest.h"
#include "Misc/Guid.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private
{
	struct FStartupPerformanceSample
	{
		double StartupTotalSeconds = 0.0;
		double BindScriptTypesSeconds = 0.0;
		double CallBindsSeconds = 0.0;
	};

	struct FShareCleanCycleSample
	{
		FString WorkloadName;
		int32 CycleIndex = 0;
		double AcquireSeconds = 0.0;
		double CompileSeconds = 0.0;
		double ResetSeconds = 0.0;
		double TotalSeconds = 0.0;
		bool bCompileSucceeded = false;
		int32 ResetActiveModuleCount = 0;
		bool bGeneratedClassExpected = false;
		bool bGeneratedClassFoundBeforeReset = false;
		bool bGeneratedClassWeakValidAfterReset = false;
		bool bGeneratedClassPathFindableAfterReset = false;
		int32 GeneratedClassCountAfterReset = 0;
		int32 DetachedGeneratedClassCountAfterReset = 0;
		int32 RootedGeneratedClassCountAfterReset = 0;
		int32 StandaloneGeneratedClassCountAfterReset = 0;
		int32 StrongReferencerCountAfterReset = 0;
		int32 ExternalStrongReferencerCountAfterReset = 0;
		bool bReferenceRootPathCapturedAfterReset = false;
		FString GeneratedClassName;
		FString GeneratedClassPathName;
		FString ReferenceRootPathAfterReset;
		TArray<FString> ReferencerSummariesAfterReset;

		bool WasGeneratedClassGarbageCollectedAfterReset() const
		{
			return bGeneratedClassExpected
				&& bGeneratedClassFoundBeforeReset
				&& !bGeneratedClassWeakValidAfterReset
				&& !bGeneratedClassPathFindableAfterReset
				&& GeneratedClassCountAfterReset == 0;
		}
	};

	enum class EShareCleanWorkloadKind : uint8
	{
		Minimal,
		UClass,
	};

	struct FShareCleanWorkload
	{
		const TCHAR* Name = TEXT("");
		EShareCleanWorkloadKind Kind = EShareCleanWorkloadKind::Minimal;
	};

	void ResetPerformanceEngineState()
	{
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
	}

	template<typename MeasureFunc>
	TArray<FStartupPerformanceSample> CollectStartupSamples(MeasureFunc&& Measure)
	{
		constexpr int32 WarmupRuns = 1;
		constexpr int32 MeasurementRuns = 3;
		for (int32 WarmupIndex = 0; WarmupIndex < WarmupRuns; ++WarmupIndex)
		{
			ResetPerformanceEngineState();
			Measure();
			ResetPerformanceEngineState();
		}

		TArray<FStartupPerformanceSample> Samples;
		for (int32 MeasurementIndex = 0; MeasurementIndex < MeasurementRuns; ++MeasurementIndex)
		{
			ResetPerformanceEngineState();
			Samples.Add(Measure());
			ResetPerformanceEngineState();
		}
		return Samples;
	}

	FString ValidateAndWriteStartupMetrics(FAutomationTestBase& Test, const FString& RunId, const FString& TestGroup, const TArray<FStartupPerformanceSample>& Samples, const TArray<FString>& Notes)
	{

		TArray<double> StartupTotals;
		TArray<double> BindTotals;
		TArray<double> CallBindTotals;
		for (const FStartupPerformanceSample& Sample : Samples)
		{
			StartupTotals.Add(Sample.StartupTotalSeconds);
			BindTotals.Add(Sample.BindScriptTypesSeconds);
			CallBindTotals.Add(Sample.CallBindsSeconds);
		}

		LogPerformanceMetric(TEXT("startup.total_seconds"), StartupTotals);
		LogPerformanceMetric(TEXT("startup.bind_script_types_seconds"), BindTotals);
		LogPerformanceMetric(TEXT("startup.call_binds_seconds"), CallBindTotals);

		TArray<FAngelscriptPerformanceMetric> Metrics;
		Metrics.Add({ TEXT("startup.total_seconds"), StartupTotals, ComputeMedian(StartupTotals) });
		Metrics.Add({ TEXT("startup.bind_script_types_seconds"), BindTotals, ComputeMedian(BindTotals) });
		Metrics.Add({ TEXT("startup.call_binds_seconds"), CallBindTotals, ComputeMedian(CallBindTotals) });

		const FString MetricsPath = WritePerformanceMetricsArtifact(RunId, TestGroup, Metrics, Notes);
		FNoDiscardAsserter Assert(Test);
		const bool bMetricsArtifactExists = Assert.IsTrue(
			FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetricsPath),
			TEXT("Startup performance test should write a metrics.json artifact"));
		(void)bMetricsArtifactExists;
		return MetricsPath;
	}

	FStartupPerformanceSample MeasureFullStartup()
	{
		FAngelscriptBindExecutionObservation::Reset();
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		const double StartTime = FPlatformTime::Seconds();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		const double TotalSeconds = FPlatformTime::Seconds() - StartTime;
		check(Engine.IsValid());
		const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
		return { TotalSeconds, Snapshot.BindScriptTypesDurationSeconds, Snapshot.CallBindsDurationSeconds };
	}

	FStartupPerformanceSample MeasureCloneStartup()
	{
		FAngelscriptBindExecutionObservation::Reset();
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> SourceEngine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		check(SourceEngine.IsValid());
		FAngelscriptBindExecutionObservation::Reset();
		const double StartTime = FPlatformTime::Seconds();
		TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
		const double TotalSeconds = FPlatformTime::Seconds() - StartTime;
		check(CloneEngine.IsValid());
		const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
		return { TotalSeconds, Snapshot.BindScriptTypesDurationSeconds, Snapshot.CallBindsDurationSeconds };
	}

	FStartupPerformanceSample MeasureCreateForTestingFallbackStartup()
	{
		FAngelscriptBindExecutionObservation::Reset();
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		const double StartTime = FPlatformTime::Seconds();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
		const double TotalSeconds = FPlatformTime::Seconds() - StartTime;
		check(Engine.IsValid());
		const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
		return { TotalSeconds, Snapshot.BindScriptTypesDurationSeconds, Snapshot.CallBindsDurationSeconds };
	}

	FStartupPerformanceSample MeasureCreateForTestingCloneStartup()
	{
		FAngelscriptBindExecutionObservation::Reset();
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> SourceEngine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		check(SourceEngine.IsValid());
		FAngelscriptEngineScope GlobalScope(*SourceEngine);
		FAngelscriptBindExecutionObservation::Reset();
		const double StartTime = FPlatformTime::Seconds();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
		const double TotalSeconds = FPlatformTime::Seconds() - StartTime;
		check(Engine.IsValid());
		const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
		return { TotalSeconds, Snapshot.BindScriptTypesDurationSeconds, Snapshot.CallBindsDurationSeconds };
	}

	FString MakeShareCleanModuleName(const FShareCleanWorkload& Workload, const FString& RunSuffix, int32 CycleIndex)
	{
		return FString::Printf(TEXT("ASShareCleanCycle_%s_%d_%s"), Workload.Name, CycleIndex, *RunSuffix);
	}

	FString MakeShareCleanGeneratedClassName(const FShareCleanWorkload& Workload, const FString& RunSuffix, int32 CycleIndex)
	{
		return FString::Printf(TEXT("UASShareCleanCycle_%s_%d_%s"), Workload.Name, CycleIndex, *RunSuffix);
	}

	FString FormatReferenceDiagnosticFlags(const UObject* Object)
	{
		if (Object == nullptr)
		{
			return TEXT("null");
		}

		TArray<FString> Flags;
		if (Object->IsRooted())
		{
			Flags.Add(TEXT("Rooted"));
		}
		if (Object->HasAnyFlags(RF_Standalone))
		{
			Flags.Add(TEXT("Standalone"));
		}
		if (Object->HasAnyFlags(RF_Public))
		{
			Flags.Add(TEXT("Public"));
		}
		if (Object->HasAnyFlags(RF_Transient))
		{
			Flags.Add(TEXT("Transient"));
		}
		if (Object->HasAnyFlags(RF_BeginDestroyed))
		{
			Flags.Add(TEXT("BeginDestroyed"));
		}
		if (Object->HasAnyFlags(RF_FinishDestroyed))
		{
			Flags.Add(TEXT("FinishDestroyed"));
		}
		if (Object->IsUnreachable())
		{
			Flags.Add(TEXT("Unreachable"));
		}
		if (Object->HasAnyInternalFlags(EInternalObjectFlags::Garbage))
		{
			Flags.Add(TEXT("Garbage"));
		}

		return Flags.Num() > 0 ? FString::Join(Flags, TEXT("|")) : FString(TEXT("None"));
	}

	FString FormatReferenceDiagnosticObject(const UObject* Object, const UObject* TargetObject)
	{
		if (Object == nullptr)
		{
			return TEXT("<null>");
		}

		const UObject* Outer = Object->GetOuter();
		const bool bInnerOfTarget = TargetObject != nullptr && Object->IsIn(TargetObject);
		return FString::Printf(
			TEXT("class=%s path=%s outer=%s relation=%s flags=%s"),
			*Object->GetClass()->GetName(),
			*Object->GetPathName(),
			Outer != nullptr ? *Outer->GetPathName() : TEXT("<null>"),
			bInnerOfTarget ? TEXT("inner") : TEXT("external"),
			*FormatReferenceDiagnosticFlags(Object));
	}

	FString MakeSingleLineReferenceDiagnostic(FString Value)
	{
		Value.ReplaceInline(TEXT("\r\n"), TEXT(" | "));
		Value.ReplaceInline(TEXT("\n"), TEXT(" | "));
		Value.ReplaceInline(TEXT("\t"), TEXT(" "));
		if (Value.Len() > 2000)
		{
			Value.LeftInline(2000);
			Value += TEXT("...");
		}
		return Value;
	}

	FString MakeShareCleanScript(const FShareCleanWorkload& Workload, const FString& RunSuffix, int32 CycleIndex)
	{
		if (Workload.Kind == EShareCleanWorkloadKind::Minimal)
		{
			return TEXT("int DoubleValue(int Value) { return Value * 2; } int Run() { return DoubleValue(21); }");
		}

		const FString ClassName = MakeShareCleanGeneratedClassName(Workload, RunSuffix, CycleIndex);
		return FString::Printf(
			TEXT(R"AS(
UCLASS()
class %s : UObject
{
	UPROPERTY()
	int Value;

	UFUNCTION()
	int GetValue()
	{
		return Value + 42;
	}
}
)AS"),
			*ClassName);
	}

	bool CompileShareCleanWorkload(FAngelscriptEngine& Engine, const FShareCleanWorkload& Workload, const FString& RunSuffix, int32 CycleIndex)
	{
		const FString ModuleNameString = MakeShareCleanModuleName(Workload, RunSuffix, CycleIndex);
		const FName ModuleName(*ModuleNameString);
		const FString Filename = ModuleNameString + TEXT(".as");
		const FString Script = MakeShareCleanScript(Workload, RunSuffix, CycleIndex);

		if (Workload.Kind == EShareCleanWorkloadKind::Minimal)
		{
			return CompileModuleFromMemory(&Engine, ModuleName, Filename, Script);
		}

		return CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, Script);
	}

	void CaptureGeneratedClassBeforeReset(FAngelscriptEngine& Engine, const FShareCleanWorkload& Workload, const FString& RunSuffix, int32 CycleIndex, FShareCleanCycleSample& Sample, TWeakObjectPtr<UASClass>& OutWeakGeneratedClass)
	{
		if (Workload.Kind != EShareCleanWorkloadKind::UClass)
		{
			return;
		}

		Sample.bGeneratedClassExpected = true;
		Sample.GeneratedClassName = MakeShareCleanGeneratedClassName(Workload, RunSuffix, CycleIndex);

		UASClass* GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, FName(*Sample.GeneratedClassName)));
		Sample.bGeneratedClassFoundBeforeReset = GeneratedClass != nullptr;
		if (GeneratedClass == nullptr)
		{
			return;
		}

		Sample.GeneratedClassPathName = GeneratedClass->GetPathName();
		OutWeakGeneratedClass = GeneratedClass;
	}

	void CaptureGeneratedClassAfterReset(FShareCleanCycleSample& Sample, const TWeakObjectPtr<UASClass>& WeakGeneratedClass)
	{
		if (!Sample.bGeneratedClassExpected)
		{
			return;
		}

		Sample.bGeneratedClassWeakValidAfterReset = WeakGeneratedClass.IsValid();
		if (!Sample.GeneratedClassPathName.IsEmpty())
		{
			Sample.bGeneratedClassPathFindableAfterReset = FindObject<UASClass>(nullptr, *Sample.GeneratedClassPathName) != nullptr;
		}

		const FName GeneratedClassFName(*Sample.GeneratedClassName);
		for (TObjectIterator<UASClass> It; It; ++It)
		{
			if (It->GetFName() != GeneratedClassFName)
			{
				continue;
			}

			++Sample.GeneratedClassCountAfterReset;
			if (It->ScriptTypePtr == nullptr)
			{
				++Sample.DetachedGeneratedClassCountAfterReset;
			}
			if (It->IsRooted())
			{
				++Sample.RootedGeneratedClassCountAfterReset;
			}
			if (It->HasAnyFlags(RF_Standalone))
			{
				++Sample.StandaloneGeneratedClassCountAfterReset;
			}
		}
	}

	void CaptureGeneratedClassReferenceDiagnosticsAfterReset(FShareCleanCycleSample& Sample, const TWeakObjectPtr<UASClass>& WeakGeneratedClass)
	{
		if (!Sample.bGeneratedClassExpected)
		{
			return;
		}

		UASClass* GeneratedClass = WeakGeneratedClass.Get();
		if (GeneratedClass == nullptr && !Sample.GeneratedClassPathName.IsEmpty())
		{
			GeneratedClass = FindObject<UASClass>(nullptr, *Sample.GeneratedClassPathName);
		}
		if (GeneratedClass == nullptr || !IsValid(GeneratedClass))
		{
			return;
		}

		constexpr int32 MaxReferencerSummaries = 8;
		TArray<UObject*> Referencees;
		Referencees.Add(GeneratedClass);
		const TArray<UObject*> Referencers = FReferencerFinder::GetAllReferencers(
			Referencees,
			nullptr,
			EReferencerFinderFlags::SkipWeakReferences);

		for (UObject* Referencer : Referencers)
		{
			if (Referencer == nullptr)
			{
				continue;
			}

			++Sample.StrongReferencerCountAfterReset;
			if (!Referencer->IsIn(GeneratedClass))
			{
				++Sample.ExternalStrongReferencerCountAfterReset;
			}
			if (Sample.ReferencerSummariesAfterReset.Num() < MaxReferencerSummaries)
			{
				Sample.ReferencerSummariesAfterReset.Add(FormatReferenceDiagnosticObject(Referencer, GeneratedClass));
			}
		}

		if (Sample.CycleIndex == 0)
		{
			FReferenceChainSearch ReferenceChainSearch(
				GeneratedClass,
				EReferenceChainSearchMode::Shortest,
				ELogVerbosity::Verbose);
			Sample.ReferenceRootPathAfterReset = MakeSingleLineReferenceDiagnostic(ReferenceChainSearch.GetRootPath(GeneratedClass));
			Sample.bReferenceRootPathCapturedAfterReset = !Sample.ReferenceRootPathAfterReset.IsEmpty();
		}
	}

	FShareCleanCycleSample MeasureShareCleanCycle(const FShareCleanWorkload& Workload, const FString& RunSuffix, int32 CycleIndex)
	{
		FShareCleanCycleSample Sample;
		Sample.WorkloadName = Workload.Name;
		Sample.CycleIndex = CycleIndex;
		TWeakObjectPtr<UASClass> WeakGeneratedClass;

		const double TotalStartTime = FPlatformTime::Seconds();
		const double AcquireStartTime = FPlatformTime::Seconds();
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		Sample.AcquireSeconds = FPlatformTime::Seconds() - AcquireStartTime;

		FAngelscriptEngineScope EngineScope(Engine);

		const double CompileStartTime = FPlatformTime::Seconds();
		Sample.bCompileSucceeded = CompileShareCleanWorkload(Engine, Workload, RunSuffix, CycleIndex);
		Sample.CompileSeconds = FPlatformTime::Seconds() - CompileStartTime;
		CaptureGeneratedClassBeforeReset(Engine, Workload, RunSuffix, CycleIndex, Sample, WeakGeneratedClass);

		const double ResetStartTime = FPlatformTime::Seconds();
		ASTEST_RESET_ENGINE(Engine);
		Sample.ResetSeconds = FPlatformTime::Seconds() - ResetStartTime;
		Sample.ResetActiveModuleCount = Engine.GetActiveModules().Num();
		CaptureGeneratedClassAfterReset(Sample, WeakGeneratedClass);
		Sample.TotalSeconds = FPlatformTime::Seconds() - TotalStartTime;
		CaptureGeneratedClassReferenceDiagnosticsAfterReset(Sample, WeakGeneratedClass);
		return Sample;
	}

	void LogShareCleanCycleSample(FAutomationTestBase& Test, const FShareCleanCycleSample& Sample)
	{
		const FString Line = FString::Printf(
			TEXT("[PERF] share_clean.%s cycle=%d acquire_seconds=%.6f compile_seconds=%.6f reset_seconds=%.6f total_seconds=%.6f compile_success=%s reset_active_modules=%d"),
			*Sample.WorkloadName,
			Sample.CycleIndex,
			Sample.AcquireSeconds,
			Sample.CompileSeconds,
			Sample.ResetSeconds,
			Sample.TotalSeconds,
			Sample.bCompileSucceeded ? TEXT("true") : TEXT("false"),
			Sample.ResetActiveModuleCount);
		UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
		Test.AddInfo(Line);

		if (Sample.bGeneratedClassExpected)
		{
			const FString GeneratedClassLine = FString::Printf(
				TEXT("[PERF] share_clean.%s cycle=%d generated_class=%s found_before_reset=%s gc_after_reset=%s weak_valid_after_reset=%s path_findable_after_reset=%s matching_uasclass_after_reset=%d detached_after_reset=%d rooted_after_reset=%d standalone_after_reset=%d strong_referencers_after_reset=%d external_strong_referencers_after_reset=%d"),
				*Sample.WorkloadName,
				Sample.CycleIndex,
				*Sample.GeneratedClassName,
				Sample.bGeneratedClassFoundBeforeReset ? TEXT("true") : TEXT("false"),
				Sample.WasGeneratedClassGarbageCollectedAfterReset() ? TEXT("true") : TEXT("false"),
				Sample.bGeneratedClassWeakValidAfterReset ? TEXT("true") : TEXT("false"),
				Sample.bGeneratedClassPathFindableAfterReset ? TEXT("true") : TEXT("false"),
				Sample.GeneratedClassCountAfterReset,
				Sample.DetachedGeneratedClassCountAfterReset,
				Sample.RootedGeneratedClassCountAfterReset,
				Sample.StandaloneGeneratedClassCountAfterReset,
				Sample.StrongReferencerCountAfterReset,
				Sample.ExternalStrongReferencerCountAfterReset);
			UE_LOG(LogTemp, Log, TEXT("%s"), *GeneratedClassLine);
			Test.AddInfo(GeneratedClassLine);

			if (Sample.bReferenceRootPathCapturedAfterReset)
			{
				const FString ReferenceRootLine = FString::Printf(
					TEXT("[PERF] share_clean.%s cycle=%d generated_class_reference_root_path=%s"),
					*Sample.WorkloadName,
					Sample.CycleIndex,
					*Sample.ReferenceRootPathAfterReset);
				UE_LOG(LogTemp, Log, TEXT("%s"), *ReferenceRootLine);
				Test.AddInfo(ReferenceRootLine);
			}

			for (int32 ReferencerIndex = 0; ReferencerIndex < Sample.ReferencerSummariesAfterReset.Num(); ++ReferencerIndex)
			{
				const FString ReferencerLine = FString::Printf(
					TEXT("[PERF] share_clean.%s cycle=%d generated_class_referencer[%d]=%s"),
					*Sample.WorkloadName,
					Sample.CycleIndex,
					ReferencerIndex,
					*Sample.ReferencerSummariesAfterReset[ReferencerIndex]);
				UE_LOG(LogTemp, Log, TEXT("%s"), *ReferencerLine);
				Test.AddInfo(ReferencerLine);
			}
		}
	}

	void AddShareCleanMetric(TArray<FAngelscriptPerformanceMetric>& Metrics, const FString& MetricName, const TArray<double>& Samples, const FString& Unit = TEXT("seconds"))
	{
		LogPerformanceMetric(MetricName, Samples);
		Metrics.Add({ MetricName, Samples, ComputeMedian(Samples), Unit, TEXT("AutomationTest") });
	}

	void AddShareCleanWorkloadMetrics(TArray<FAngelscriptPerformanceMetric>& Metrics, const FString& WorkloadName, const TArray<FShareCleanCycleSample>& Samples)
	{
		TArray<double> AcquireSeconds;
		TArray<double> CompileSeconds;
		TArray<double> ResetSeconds;
		TArray<double> TotalSeconds;

		for (const FShareCleanCycleSample& Sample : Samples)
		{
			if (!Sample.WorkloadName.Equals(WorkloadName, ESearchCase::CaseSensitive))
			{
				continue;
			}

			AcquireSeconds.Add(Sample.AcquireSeconds);
			CompileSeconds.Add(Sample.CompileSeconds);
			ResetSeconds.Add(Sample.ResetSeconds);
			TotalSeconds.Add(Sample.TotalSeconds);
		}

		AddShareCleanMetric(Metrics, FString::Printf(TEXT("share_clean.%s.acquire_seconds"), *WorkloadName), AcquireSeconds);
		AddShareCleanMetric(Metrics, FString::Printf(TEXT("share_clean.%s.compile_seconds"), *WorkloadName), CompileSeconds);
		AddShareCleanMetric(Metrics, FString::Printf(TEXT("share_clean.%s.reset_seconds"), *WorkloadName), ResetSeconds);
		AddShareCleanMetric(Metrics, FString::Printf(TEXT("share_clean.%s.total_seconds"), *WorkloadName), TotalSeconds);
	}

	void AddShareCleanGeneratedClassMetrics(TArray<FAngelscriptPerformanceMetric>& Metrics, const TArray<FShareCleanCycleSample>& Samples)
	{
		TArray<double> GcAfterReset;
		TArray<double> WeakValidAfterReset;
		TArray<double> PathFindableAfterReset;
		TArray<double> MatchingClassCountAfterReset;
		TArray<double> DetachedClassCountAfterReset;
		TArray<double> RootedClassCountAfterReset;
		TArray<double> StandaloneClassCountAfterReset;
		TArray<double> StrongReferencerCountAfterReset;
		TArray<double> ExternalStrongReferencerCountAfterReset;

		for (const FShareCleanCycleSample& Sample : Samples)
		{
			if (!Sample.bGeneratedClassExpected)
			{
				continue;
			}

			GcAfterReset.Add(Sample.WasGeneratedClassGarbageCollectedAfterReset() ? 1.0 : 0.0);
			WeakValidAfterReset.Add(Sample.bGeneratedClassWeakValidAfterReset ? 1.0 : 0.0);
			PathFindableAfterReset.Add(Sample.bGeneratedClassPathFindableAfterReset ? 1.0 : 0.0);
			MatchingClassCountAfterReset.Add(static_cast<double>(Sample.GeneratedClassCountAfterReset));
			DetachedClassCountAfterReset.Add(static_cast<double>(Sample.DetachedGeneratedClassCountAfterReset));
			RootedClassCountAfterReset.Add(static_cast<double>(Sample.RootedGeneratedClassCountAfterReset));
			StandaloneClassCountAfterReset.Add(static_cast<double>(Sample.StandaloneGeneratedClassCountAfterReset));
			StrongReferencerCountAfterReset.Add(static_cast<double>(Sample.StrongReferencerCountAfterReset));
			ExternalStrongReferencerCountAfterReset.Add(static_cast<double>(Sample.ExternalStrongReferencerCountAfterReset));
		}

		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_gc_after_reset"), GcAfterReset, TEXT("boolean"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_weak_valid_after_reset"), WeakValidAfterReset, TEXT("boolean"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_path_findable_after_reset"), PathFindableAfterReset, TEXT("boolean"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_count_after_reset"), MatchingClassCountAfterReset, TEXT("count"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_detached_count_after_reset"), DetachedClassCountAfterReset, TEXT("count"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_rooted_count_after_reset"), RootedClassCountAfterReset, TEXT("count"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_standalone_count_after_reset"), StandaloneClassCountAfterReset, TEXT("count"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_strong_referencer_count_after_reset"), StrongReferencerCountAfterReset, TEXT("count"));
		AddShareCleanMetric(Metrics, TEXT("share_clean.uclass.generated_class_external_strong_referencer_count_after_reset"), ExternalStrongReferencerCountAfterReset, TEXT("count"));
	}

	FString ValidateAndWriteShareCleanMetrics(FAutomationTestBase& Test, const TArray<FShareCleanCycleSample>& Samples)
	{

		TArray<FAngelscriptPerformanceMetric> Metrics;
		AddShareCleanWorkloadMetrics(Metrics, TEXT("minimal"), Samples);
		AddShareCleanWorkloadMetrics(Metrics, TEXT("uclass"), Samples);
		AddShareCleanGeneratedClassMetrics(Metrics, Samples);

		const FString MetricsPath = WritePerformanceMetricsArtifact(
			TEXT("P3_4_ShareCleanCyclePerformance"),
			TEXT("Angelscript.TestModule.Core.Performance.ShareCleanCycle"),
			Metrics,
			{
				TEXT("Measures ASTEST_CREATE_ENGINE acquire/create, AS compile, and explicit ASTEST_RESET_ENGINE latency across serial cycles."),
				TEXT("Cycle 0 starts after destroying the shared test engine; later cycles observe the hot shared-engine path."),
				TEXT("Generated UASClass diagnostics run after timing capture and report GC state plus strong referencers."),
				TEXT("No timing thresholds are enforced; this is an optimization baseline artifact.")
			});
		FNoDiscardAsserter Assert(Test);
		const bool bMetricsArtifactExists = Assert.IsTrue(
			FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetricsPath),
			TEXT("Share-clean cycle performance test should write a metrics.json artifact"));
		(void)bMetricsArtifactExists;
		return MetricsPath;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptEnginePerformanceTests,
	"Angelscript.TestModule.Core.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Startup_Full)
	{
		using namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private;
	using namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private;
	const TArray<FStartupPerformanceSample> Samples = CollectStartupSamples([]() { return MeasureFullStartup(); });
	ValidateAndWriteStartupMetrics(*TestRunner, TEXT("P3_1_StartupPerformance_Full"), TEXT("Angelscript.TestModule.Core.Performance.Startup.Full"), Samples, { TEXT("Measured with fresh full-engine startup samples." )});
	}

	TEST_METHOD(Startup_CreateForTestingFallbackFull)
	{
		using namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private;
	using namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private;
	const TArray<FStartupPerformanceSample> Samples = CollectStartupSamples([]() { return MeasureCreateForTestingFallbackStartup(); });
	ValidateAndWriteStartupMetrics(*TestRunner, TEXT("P3_1_StartupPerformance_CreateForTestingFallback"), TEXT("Angelscript.TestModule.Core.Performance.Startup.CreateForTestingFallbackFull"), Samples, { TEXT("CreateForTesting falls back to a full engine when no global source engine exists.") });
	}

	TEST_METHOD(ShareCleanCycle)
	{
		using namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private;
	using namespace AngelscriptTest_Core_AngelscriptEnginePerformanceTests_Private;
	constexpr int32 CycleCount = 3;
	const FString RunSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	const FShareCleanWorkload Workloads[] = {
		{ TEXT("minimal"), EShareCleanWorkloadKind::Minimal },
		{ TEXT("uclass"), EShareCleanWorkloadKind::UClass },
	};

	DestroySharedTestEngine();

	TArray<FShareCleanCycleSample> Samples;
	for (const FShareCleanWorkload& Workload : Workloads)
	{
		for (int32 CycleIndex = 0; CycleIndex < CycleCount; ++CycleIndex)
		{
			FShareCleanCycleSample Sample = MeasureShareCleanCycle(Workload, RunSuffix, CycleIndex);
			LogShareCleanCycleSample(*TestRunner, Sample);
			ASSERT_THAT(IsTrue(
				Sample.bCompileSucceeded,
				FString::Printf(TEXT("Share-clean %s cycle %d should compile successfully"), Workload.Name, CycleIndex)));
			ASSERT_THAT(AreEqual(
				0,
				Sample.ResetActiveModuleCount,
				FString::Printf(TEXT("Share-clean %s cycle %d reset should leave no active modules"), Workload.Name, CycleIndex)));
			if (Sample.bGeneratedClassExpected)
			{
				ASSERT_THAT(IsTrue(
					Sample.bGeneratedClassFoundBeforeReset,
					FString::Printf(TEXT("Share-clean %s cycle %d should locate the generated UASClass before reset"), Workload.Name, CycleIndex)));
				ASSERT_THAT(AreEqual(
					Sample.GeneratedClassCountAfterReset,
					Sample.DetachedGeneratedClassCountAfterReset,
					FString::Printf(TEXT("Share-clean %s cycle %d should detach every matching generated UASClass left after reset"), Workload.Name, CycleIndex)));
				ASSERT_THAT(AreEqual(
					0,
					Sample.RootedGeneratedClassCountAfterReset,
					FString::Printf(TEXT("Share-clean %s cycle %d generated UASClass should not remain rooted after reset"), Workload.Name, CycleIndex)));
				ASSERT_THAT(AreEqual(
					0,
					Sample.StandaloneGeneratedClassCountAfterReset,
					FString::Printf(TEXT("Share-clean %s cycle %d generated UASClass should not remain standalone after reset"), Workload.Name, CycleIndex)));
				ASSERT_THAT(AreEqual(
					0,
					Sample.StrongReferencerCountAfterReset,
					FString::Printf(TEXT("Share-clean %s cycle %d generated UASClass should have no strong referencers after reset"), Workload.Name, CycleIndex)));
				ASSERT_THAT(AreEqual(
					0,
					Sample.ExternalStrongReferencerCountAfterReset,
					FString::Printf(TEXT("Share-clean %s cycle %d generated UASClass should have no external strong referencers after reset"), Workload.Name, CycleIndex)));
				ASSERT_THAT(IsTrue(
					Sample.WasGeneratedClassGarbageCollectedAfterReset(),
					FString::Printf(TEXT("Share-clean %s cycle %d generated UASClass should be garbage collected after reset"), Workload.Name, CycleIndex)));
			}
			Samples.Add(MoveTemp(Sample));
		}
	}

	ValidateAndWriteShareCleanMetrics(*TestRunner, Samples);
	DestroySharedTestEngine();
	}

};

#endif
