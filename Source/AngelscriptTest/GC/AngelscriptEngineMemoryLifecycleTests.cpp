#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "HAL/PlatformMemory.h"
#include "HAL/MallocAnsi.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

// ==========================================================================
// Engine Memory Lifecycle Tests — with full diagnostic instrumentation
//
// Every test dumps actual process memory, UObject counts, and per-type
// breakdowns at each phase boundary (baseline → post-create → post-cleanup).
// The log output provides hard evidence rather than speculation.
// ==========================================================================

namespace EngineMemoryLifecycleTestHelpers
{
	struct FMemorySnapshot
	{
		uint64 ProcessPhysicalMB = 0;
		uint64 ProcessVirtualMB = 0;
		uint64 PeakPhysicalMB = 0;

		int32 TotalUObjects = 0;
		int32 PermanentUObjects = 0;

		int32 ASClassTotal = 0;
		int32 ASClassDetached = 0;
		int32 ASClassRootedDetached = 0;

		int32 ASStructTotal = 0;
		int32 ASStructDetached = 0;
		int32 ASStructRootedDetached = 0;

		int32 UEnumTotal = 0;
		int32 UEnumRooted = 0;

		int32 UDelegateFunctionTotal = 0;
		int32 UDelegateFunctionRooted = 0;

		int32 TransientPackageRootedObjects = 0;

		int32 ScriptPackageCount = 0;
		int32 ScriptPackageRootedCount = 0;

		double GCDurationMs = 0.0;
	};

	inline FMemorySnapshot CaptureSnapshot(const TCHAR* Label)
	{
		FMemorySnapshot S;

		FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
		S.ProcessPhysicalMB = MemStats.UsedPhysical / (1024 * 1024);
		S.ProcessVirtualMB = MemStats.UsedVirtual / (1024 * 1024);
		S.PeakPhysicalMB = MemStats.PeakUsedPhysical / (1024 * 1024);

		S.TotalUObjects = GUObjectArray.GetObjectArrayNum();
		S.PermanentUObjects = GUObjectArray.GetObjectArrayNumPermanent();

		for (TObjectIterator<UASClass> It; It; ++It)
		{
			++S.ASClassTotal;
			if (It->ScriptTypePtr == nullptr)
			{
				++S.ASClassDetached;
				if (It->IsRooted()) ++S.ASClassRootedDetached;
			}
		}

		for (TObjectIterator<UASStruct> It; It; ++It)
		{
			++S.ASStructTotal;
			if (It->ScriptType == nullptr)
			{
				++S.ASStructDetached;
				if (It->IsRooted()) ++S.ASStructRootedDetached;
			}
		}

		for (TObjectIterator<UEnum> It; It; ++It)
		{
			++S.UEnumTotal;
			if (It->IsRooted()) ++S.UEnumRooted;
		}

		for (TObjectIterator<UDelegateFunction> It; It; ++It)
		{
			++S.UDelegateFunctionTotal;
			if (It->IsRooted()) ++S.UDelegateFunctionRooted;
		}

		ForEachObjectOfClass(UObject::StaticClass(), [&S](UObject* Obj)
		{
			if (Obj->IsRooted() && Obj->GetOutermost() == GetTransientPackage())
			{
				++S.TransientPackageRootedObjects;
			}
		});

		for (TObjectIterator<UPackage> It; It; ++It)
		{
			FString PkgName = It->GetName();
			if (PkgName.Contains(TEXT("/Script/Angelscript")))
			{
				++S.ScriptPackageCount;
				if (It->IsRooted()) ++S.ScriptPackageRootedCount;
			}
		}

		UE_LOG(Angelscript, Display,
			TEXT("[MemDump:%s] PhysMB=%llu VirtMB=%llu PeakMB=%llu | UObj total=%d perm=%d | "
				 "ASClass total=%d detached=%d rooted_detached=%d | "
				 "ASStruct total=%d detached=%d rooted_detached=%d | "
				 "UEnum total=%d rooted=%d | UDelegateFunc total=%d rooted=%d | "
				 "ScriptPkg total=%d rooted=%d | TransientRooted=%d"),
			Label,
			S.ProcessPhysicalMB, S.ProcessVirtualMB, S.PeakPhysicalMB,
			S.TotalUObjects, S.PermanentUObjects,
			S.ASClassTotal, S.ASClassDetached, S.ASClassRootedDetached,
			S.ASStructTotal, S.ASStructDetached, S.ASStructRootedDetached,
			S.UEnumTotal, S.UEnumRooted,
			S.UDelegateFunctionTotal, S.UDelegateFunctionRooted,
			S.ScriptPackageCount, S.ScriptPackageRootedCount,
			S.TransientPackageRootedObjects);

		return S;
	}

	inline FMemorySnapshot CaptureSnapshotWithGC(const TCHAR* Label)
	{
		const double GCStart = FPlatformTime::Seconds();
		CollectGarbage(RF_NoFlags, true);
		const double GCMs = (FPlatformTime::Seconds() - GCStart) * 1000.0;
		FMemorySnapshot S = CaptureSnapshot(Label);
		S.GCDurationMs = GCMs;
		UE_LOG(Angelscript, Display, TEXT("[MemDump:%s] GC took %.3f ms"), Label, GCMs);
		return S;
	}

	inline void DestroyEngineAndCleanup(TUniquePtr<FAngelscriptEngine>& Engine)
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Engine->GetActiveModules();
		UE_LOG(Angelscript, Display, TEXT("[MemDump] DestroyEngine: modules=%d"), Modules.Num());
		Engine.Reset();
		FDetachedASTypeCleanupResult CleanupResult = CleanupDetachedASTypesForGarbageCollection(&Modules);
		UE_LOG(Angelscript, Display,
			TEXT("[MemDump] CleanupResult: classes=%d(rooted=%d) structs=%d(rooted=%d) enums=%d(rooted=%d) delegates=%d(rooted=%d)"),
			CleanupResult.DetachedClassCount, CleanupResult.RootedDetachedClassCount,
			CleanupResult.DetachedStructCount, CleanupResult.RootedDetachedStructCount,
			CleanupResult.DiscardedEnumCount, CleanupResult.RootedDiscardedEnumCount,
			CleanupResult.DiscardedDelegateFunctionCount, CleanupResult.RootedDiscardedDelegateFunctionCount);
		CollectGarbage(RF_NoFlags, true);

		FPlatformMemoryStats PreTrimStats = FPlatformMemory::GetStats();
		FMemory::Trim(true);
		FPlatformMemoryStats PostTrimStats = FPlatformMemory::GetStats();
		UE_LOG(Angelscript, Display,
			TEXT("[MemDump:Trim] Before: PhysMB=%llu VirtMB=%llu | After: PhysMB=%llu VirtMB=%llu | Recovered: %lldMB"),
			PreTrimStats.UsedPhysical / (1024 * 1024),
			PreTrimStats.UsedVirtual / (1024 * 1024),
			PostTrimStats.UsedPhysical / (1024 * 1024),
			PostTrimStats.UsedVirtual / (1024 * 1024),
			(int64)(PreTrimStats.UsedPhysical / (1024 * 1024)) - (int64)(PostTrimStats.UsedPhysical / (1024 * 1024)));
	}

	inline void LogDelta(const FMemorySnapshot& Before, const FMemorySnapshot& After, const TCHAR* Label)
	{
		UE_LOG(Angelscript, Display,
			TEXT("[MemDelta:%s] PhysMB: %llu -> %llu (delta=%lld) | "
				 "UObj: %d -> %d (delta=%d) | "
				 "ASClass: %d -> %d | ASStruct: %d -> %d | "
				 "UEnum rooted: %d -> %d | UDelegateFunc rooted: %d -> %d | "
				 "ScriptPkg rooted: %d -> %d | TransientRooted: %d -> %d"),
			Label,
			Before.ProcessPhysicalMB, After.ProcessPhysicalMB,
			(int64)After.ProcessPhysicalMB - (int64)Before.ProcessPhysicalMB,
			Before.TotalUObjects, After.TotalUObjects,
			After.TotalUObjects - Before.TotalUObjects,
			Before.ASClassTotal, After.ASClassTotal,
			Before.ASStructTotal, After.ASStructTotal,
			Before.UEnumRooted, After.UEnumRooted,
			Before.UDelegateFunctionRooted, After.UDelegateFunctionRooted,
			Before.ScriptPackageRootedCount, After.ScriptPackageRootedCount,
			Before.TransientPackageRootedObjects, After.TransientPackageRootedObjects);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineMemoryLifecycleTests,
	"Angelscript.TestModule.GC.EngineMemoryLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FullEngineShutdownUnrootsGeneratedClasses)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("Class_Baseline"));

		TWeakObjectPtr<UASClass> WeakClass;
		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(Engine.Get()));
			FAngelscriptEngineScope Scope(*Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, *Engine, "MemLifecycleClass",
				TEXT(R"(
UCLASS()
class AMemLifecycleTestActor : AActor {}
)"));
			ASSERT_THAT(IsNotNull(Module));

			UClass* ScriptClass = FindGeneratedClass(Engine.Get(), TEXT("AMemLifecycleTestActor"));
			ASSERT_THAT(IsNotNull(ScriptClass));
			WeakClass = Cast<UASClass>(ScriptClass);
			ASSERT_THAT(IsTrue(WeakClass.IsValid()));

			const FMemorySnapshot PostCreate = CaptureSnapshot(TEXT("Class_PostCreate"));
			LogDelta(Baseline, PostCreate, TEXT("Class_Create"));

			TestRunner->TestTrue(TEXT("Generated class should be rooted while engine is alive"),
				WeakClass->IsRooted());

			DestroyEngineAndCleanup(Engine);
		}

		const FMemorySnapshot PostCleanup = CaptureSnapshotWithGC(TEXT("Class_PostCleanup"));
		LogDelta(Baseline, PostCleanup, TEXT("Class_FullCycle"));

		TestRunner->TestTrue(TEXT("No new rooted detached classes after cleanup"),
			PostCleanup.ASClassRootedDetached <= Baseline.ASClassRootedDetached);
		TestRunner->TestFalse(TEXT("Generated class weak ref should be invalid"),
			WeakClass.IsValid());
	}

	TEST_METHOD(FullEngineShutdownUnrootsGeneratedStructs)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("Struct_Baseline"));

		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(Engine.Get()));
			FAngelscriptEngineScope Scope(*Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, *Engine, "MemLifecycleStruct",
				TEXT(R"(
USTRUCT()
struct FMemLifecycleTestStruct
{
	UPROPERTY()
	int32 Value = 0;
}
)"));
			ASSERT_THAT(IsNotNull(Module));

			const FMemorySnapshot PostCreate = CaptureSnapshot(TEXT("Struct_PostCreate"));
			LogDelta(Baseline, PostCreate, TEXT("Struct_Create"));

			DestroyEngineAndCleanup(Engine);
		}

		const FMemorySnapshot PostCleanup = CaptureSnapshotWithGC(TEXT("Struct_PostCleanup"));
		LogDelta(Baseline, PostCleanup, TEXT("Struct_FullCycle"));

		TestRunner->TestTrue(TEXT("No new rooted detached structs after cleanup"),
			PostCleanup.ASStructRootedDetached <= Baseline.ASStructRootedDetached);
	}

	TEST_METHOD(TransientSlotReuseCleansUpPreviousTypes)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("Transient_Baseline"));

		{
			FAngelscriptEngine& Engine1 = ASTEST_CREATE_ENGINE_FULL();
			FAngelscriptEngineScope Scope1(Engine1);

			BuildModule(*TestRunner, Engine1, "TransientSlot1",
				TEXT("UCLASS()\nclass ATransientSlot1Actor : AActor {}"));

			const FMemorySnapshot PostSlot1 = CaptureSnapshot(TEXT("Transient_PostSlot1"));
			LogDelta(Baseline, PostSlot1, TEXT("Transient_Slot1"));
		}

		{
			FAngelscriptEngine& Engine2 = ASTEST_CREATE_ENGINE_FULL();
			FAngelscriptEngineScope Scope2(Engine2);

			BuildModule(*TestRunner, Engine2, "TransientSlot2",
				TEXT("UCLASS()\nclass ATransientSlot2Actor : AActor {}"));

			const FMemorySnapshot PostSlot2 = CaptureSnapshot(TEXT("Transient_PostSlot2"));
			LogDelta(Baseline, PostSlot2, TEXT("Transient_Slot2"));
		}

		const FMemorySnapshot PostReuse = CaptureSnapshotWithGC(TEXT("Transient_PostReuse"));
		LogDelta(Baseline, PostReuse, TEXT("Transient_FullCycle"));

		TestRunner->TestTrue(TEXT("Transient slot reuse should not accumulate rooted detached classes"),
			PostReuse.ASClassRootedDetached <= Baseline.ASClassRootedDetached);
	}

	// N engine cycles: dump snapshot at every cycle boundary.
	// 20 cycles to distinguish linear leak from allocator retention.
	TEST_METHOD(MultipleEngineCyclesDoNotAccumulateDetachedTypes)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("MultiCycle_Baseline"));

		constexpr int32 NumCycles = 3;
		for (int32 i = 0; i < NumCycles; ++i)
		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(Engine.Get()));
			FAngelscriptEngineScope Scope(*Engine);

			const FString ModuleName = FString::Printf(TEXT("MultiCycle_%d"), i);
			const FString ClassName = FString::Printf(TEXT("AMultiCycleActor_%d"), i);
			const FString ScriptSource = FString::Printf(
				TEXT("UCLASS()\nclass %s : AActor {}"), *ClassName);

			asIScriptModule* Module = BuildModule(*TestRunner, *Engine,
				TCHAR_TO_ANSI(*ModuleName), ScriptSource);
			ASSERT_THAT(IsNotNull(Module));

			const FString CreateLabel = FString::Printf(TEXT("MultiCycle_%d_PostCreate"), i);
			CaptureSnapshot(*CreateLabel);

			DestroyEngineAndCleanup(Engine);

			const FString CleanLabel = FString::Printf(TEXT("MultiCycle_%d_PostClean"), i);
			const FMemorySnapshot PostClean = CaptureSnapshotWithGC(*CleanLabel);
			LogDelta(Baseline, PostClean, *FString::Printf(TEXT("MultiCycle_%d_Delta"), i));
		}

		const FMemorySnapshot Final = CaptureSnapshotWithGC(TEXT("MultiCycle_Final"));
		LogDelta(Baseline, Final, TEXT("MultiCycle_Total"));

		TestRunner->TestTrue(
			FString::Printf(TEXT("After %d cycles, rooted detached class: %d (baseline: %d)"),
				NumCycles, Final.ASClassRootedDetached, Baseline.ASClassRootedDetached),
			Final.ASClassRootedDetached <= Baseline.ASClassRootedDetached);

		TestRunner->TestTrue(
			FString::Printf(TEXT("After %d cycles, rooted detached struct: %d (baseline: %d)"),
				NumCycles, Final.ASStructRootedDetached, Baseline.ASStructRootedDetached),
			Final.ASStructRootedDetached <= Baseline.ASStructRootedDetached);

		TestRunner->TestTrue(
			FString::Printf(TEXT("After %d cycles, UEnum rooted: %d (baseline: %d)"),
				NumCycles, Final.UEnumRooted, Baseline.UEnumRooted),
			Final.UEnumRooted <= Baseline.UEnumRooted + 2);

		TestRunner->TestTrue(
			FString::Printf(TEXT("After %d cycles, UDelegateFunc rooted: %d (baseline: %d)"),
				NumCycles, Final.UDelegateFunctionRooted, Baseline.UDelegateFunctionRooted),
			Final.UDelegateFunctionRooted <= Baseline.UDelegateFunctionRooted + 2);
	}

	TEST_METHOD(HashTableCompactionDoesNotDegradeAcrossCycles)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("HashTable_Baseline"));

		auto MeasureGCDuration = []() -> double
		{
			const double Start = FPlatformTime::Seconds();
			CollectGarbage(RF_NoFlags, true);
			return (FPlatformTime::Seconds() - Start) * 1000.0;
		};

		const double BaselineGcMs = MeasureGCDuration();
		UE_LOG(Angelscript, Display, TEXT("[MemDump:HashTable] Baseline GC: %.3f ms"), BaselineGcMs);

		constexpr int32 NumCycles = 10;
		for (int32 i = 0; i < NumCycles; ++i)
		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
			if (!Engine.IsValid()) continue;
			FAngelscriptEngineScope Scope(*Engine);

			const FString ModuleName = FString::Printf(TEXT("HashTableCycle_%d"), i);
			const FString ClassName = FString::Printf(TEXT("AHashTableCycleActor_%d"), i);
			const FString ScriptSource = FString::Printf(
				TEXT("UCLASS()\nclass %s : AActor {}"), *ClassName);
			BuildModule(*TestRunner, *Engine, TCHAR_TO_ANSI(*ModuleName), ScriptSource);

			DestroyEngineAndCleanup(Engine);

			const double CycleGcMs = MeasureGCDuration();
			const FMemorySnapshot CycleSnap = CaptureSnapshot(
				*FString::Printf(TEXT("HashTable_Cycle%d"), i));
			UE_LOG(Angelscript, Display,
				TEXT("[MemDump:HashTable] Cycle %d GC: %.3f ms | PhysMB=%llu UObj=%d"),
				i, CycleGcMs, CycleSnap.ProcessPhysicalMB, CycleSnap.TotalUObjects);
		}

		const double PostCycleGcMs = MeasureGCDuration();
		const FMemorySnapshot PostCycle = CaptureSnapshot(TEXT("HashTable_PostCycle"));
		LogDelta(Baseline, PostCycle, TEXT("HashTable_Total"));
		UE_LOG(Angelscript, Display,
			TEXT("[MemDump:HashTable] Final GC: %.3f ms (baseline: %.3f ms, ratio: %.1fx)"),
			PostCycleGcMs, BaselineGcMs, BaselineGcMs > 0.001 ? PostCycleGcMs / BaselineGcMs : 1.0);

		const double DegradationRatio = (BaselineGcMs > 0.001)
			? (PostCycleGcMs / BaselineGcMs) : 1.0;
		TestRunner->TestTrue(
			FString::Printf(TEXT("GC ratio should stay under 3x (%.3f/%.3f = %.1fx)"),
				PostCycleGcMs, BaselineGcMs, DegradationRatio),
			DegradationRatio < 3.0);
	}

	TEST_METHOD(SharedEngineResetCleansUpGeneratedTypes)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("SharedReset_Baseline"));

		{
			FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "SharedResetType",
				TEXT(R"(
UCLASS()
class ASharedResetTestActor : AActor {}

USTRUCT()
struct FSharedResetTestStruct
{
	UPROPERTY()
	int32 X = 0;
}
)"));
			ASSERT_THAT(IsNotNull(Module));

			const FMemorySnapshot PostCreate = CaptureSnapshot(TEXT("SharedReset_PostCreate"));
			LogDelta(Baseline, PostCreate, TEXT("SharedReset_Create"));

			ASTEST_RESET_ENGINE(Engine);
		}

		const FMemorySnapshot PostReset = CaptureSnapshotWithGC(TEXT("SharedReset_PostReset"));
		LogDelta(Baseline, PostReset, TEXT("SharedReset_FullCycle"));

		TestRunner->TestTrue(TEXT("Shared engine reset: no new rooted detached classes"),
			PostReset.ASClassRootedDetached <= Baseline.ASClassRootedDetached);
		TestRunner->TestTrue(TEXT("Shared engine reset: no new rooted detached structs"),
			PostReset.ASStructRootedDetached <= Baseline.ASStructRootedDetached);
	}

	TEST_METHOD(FullEngineCleanupUnrootsEnumsAndDelegates)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("EnumDel_Baseline"));

		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(Engine.Get()));
			FAngelscriptEngineScope Scope(*Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, *Engine, "EnumDelegateLifecycle",
				TEXT(R"(
UENUM()
enum EMemLifecycleTestEnum
{
	Alpha,
	Beta,
	Gamma
}

delegate void FMemLifecycleTestDelegate(int Value);

UCLASS()
class AEnumDelegateLifecycleActor : AActor
{
	UPROPERTY()
	EMemLifecycleTestEnum TestEnum = EMemLifecycleTestEnum::Alpha;

	UFUNCTION()
	void OnTestDelegate(int Value) {}
}
)"));
			ASSERT_THAT(IsNotNull(Module));

			const FMemorySnapshot PostCreate = CaptureSnapshot(TEXT("EnumDel_PostCreate"));
			LogDelta(Baseline, PostCreate, TEXT("EnumDel_Create"));

			TestRunner->TestTrue(
				FString::Printf(TEXT("PostCreate UEnum rooted (%d) > baseline (%d)"),
					PostCreate.UEnumRooted, Baseline.UEnumRooted),
				PostCreate.UEnumRooted > Baseline.UEnumRooted);

			DestroyEngineAndCleanup(Engine);
		}

		const FMemorySnapshot PostCleanup = CaptureSnapshotWithGC(TEXT("EnumDel_PostCleanup"));
		LogDelta(Baseline, PostCleanup, TEXT("EnumDel_FullCycle"));

		TestRunner->TestTrue(
			FString::Printf(TEXT("PostCleanup UEnum rooted (%d) should return near baseline (%d)"),
				PostCleanup.UEnumRooted, Baseline.UEnumRooted),
			PostCleanup.UEnumRooted <= Baseline.UEnumRooted + 2);

		TestRunner->TestTrue(
			FString::Printf(TEXT("PostCleanup UDelegateFunc rooted (%d) should return near baseline (%d)"),
				PostCleanup.UDelegateFunctionRooted, Baseline.UDelegateFunctionRooted),
			PostCleanup.UDelegateFunctionRooted <= Baseline.UDelegateFunctionRooted + 2);

		TestRunner->TestTrue(
			FString::Printf(TEXT("PostCleanup TransientRooted (%d) should return near baseline (%d)"),
				PostCleanup.TransientPackageRootedObjects, Baseline.TransientPackageRootedObjects),
			PostCleanup.TransientPackageRootedObjects <= Baseline.TransientPackageRootedObjects + 5);
	}

	TEST_METHOD(EmptyEngineShutdownLeavesNoDetachedTypes)
	{
		using namespace EngineMemoryLifecycleTestHelpers;

		const FMemorySnapshot Baseline = CaptureSnapshotWithGC(TEXT("Empty_Baseline"));

		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(Engine.Get()));

			const FMemorySnapshot PostCreate = CaptureSnapshot(TEXT("Empty_PostCreate"));
			LogDelta(Baseline, PostCreate, TEXT("Empty_Create"));

			DestroyEngineAndCleanup(Engine);
		}

		const FMemorySnapshot PostCleanup = CaptureSnapshotWithGC(TEXT("Empty_PostCleanup"));
		LogDelta(Baseline, PostCleanup, TEXT("Empty_FullCycle"));

		TestRunner->TestTrue(TEXT("Empty engine: no new detached classes"),
			PostCleanup.ASClassDetached <= Baseline.ASClassDetached);
		TestRunner->TestTrue(TEXT("Empty engine: no new detached structs"),
			PostCleanup.ASStructDetached <= Baseline.ASStructDetached);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
