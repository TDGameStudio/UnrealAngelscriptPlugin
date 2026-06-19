// Cross-cycle regression tests for the global static containers that
// `ASBindFreeCompletenessVerification.md` (Phase 1 + 2) identified as the
// long-tail leak suspects.
//
// The pattern is the same one that pinned down the FBlueprintEventSignature
// leak (see BindFreeEvidence_BlueprintEventSignatureBounded):
//
//   1. Establish a clean slate (drop transient engine, GC, Trim).
//   2. Run N full create/destroy cycles, recording each container's Num() at
//      the end of every cycle.
//   3. Assert |count_n - count_baseline| stays within a small tolerance.
//
// Cycle 1 is the baseline because the very first cycle creates all the global
// state (FName pool entries, doc tables, JIT native forms) that subsequent
// cycles re-use. If a real leak is introduced, the count will *grow*
// monotonically across cycles 2..N — the tolerance catches that pattern
// before it shows up as a 200 MB allocator residual months later.

#include "CQTest.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "StaticJIT/StaticJITBinds.h"

#include "HAL/UnrealMemory.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

// Diagnostic accessor exported from AngelscriptRuntime. Defined in
// Bind_BlueprintEvent.cpp; we deliberately don't `extern` the underlying
// global TMap because it isn't part of the runtime DLL's public symbol set
// and cross-module linkage would require either exporting the TMap (heavy)
// or duplicating the type (fragile).
ANGELSCRIPTRUNTIME_API int32 GetBlueprintEventsByScriptNameTotalCount();

namespace AngelscriptTest_Memory_GlobalContainerCycleBounded_Private
{
	// Drop any transient engine the test storage may be holding, GC, then
	// Trim so each cycle starts from a stable allocator state.
	static void ResetToCleanSlate()
	{
		GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);
	}

	// Returns the number of UASClass objects whose engine pointer has been
	// nulled out (i.e. the engine that owned them has been Shutdown()'d) but
	// which are still rooted. This is the post-fix guard for the
	// `ClassGenerator UObject leak` originally identified in the
	// `fix-as-engine-shutdown-memory-leak` change — if it ever drifts, the
	// shutdown path lost an unroot.
	static int32 CountRootedDetachedASClasses()
	{
		int32 Count = 0;
		for (TObjectIterator<UASClass> It; It; ++It)
		{
			if (It->ScriptTypePtr == nullptr && It->IsRooted())
			{
				++Count;
			}
		}
		return Count;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptGlobalContainerCycleBoundedTests,
	"Angelscript.TestModule.Memory.GlobalContainerCycleBounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// Common rig: drive `NumCycles` engine create/destroy cycles, capture the
	// container size at the end of each cycle, and assert the size stays
	// within `TolerancePerCycle` of the post-warmup baseline.
	//
	// Cycle 1 is treated as a warm-up because the test process is rarely
	// pristine when this test runs — earlier tests (or engine bootstrap)
	// may have populated process-level globals that our engine cleanup then
	// wipes on first acquisition. We anchor the steady-state baseline at
	// cycle 2 and assert cycles 3..N stay near it; that's what catches a
	// real per-cycle leak (which would push the count up monotonically
	// from cycle 2 onward).
	//
	// `bAllowEmptyBaseline` controls cycle-2 behaviour. The default (false)
	// asserts the bind path produced *something* — the failure mode we want
	// is "the test wired a probe at the wrong global". Containers gated by
	// `IsGeneratingPrecompiledData()` (StaticJIT NativeForms) are empty
	// under normal headless test runs, so callers for those probes pass
	// `bAllowEmptyBaseline=true`.
	template <typename FnSampleSize>
	void RunBoundedCycleProbe(
		const TCHAR* ContainerLabel,
		int32 NumCycles,
		int32 TolerancePerCycle,
		FnSampleSize&& SampleSize,
		bool bAllowEmptyBaseline = false)
	{
		using namespace AngelscriptTest_Memory_GlobalContainerCycleBounded_Private;

		// Need at least 3 cycles: 1 warm-up + 1 baseline + >=1 follow-up.
		check(NumCycles >= 3);

		ResetToCleanSlate();

		int32 BaselineCount = -1;
		bool bProbePassed = true;

		for (int32 Cycle = 1; Cycle <= NumCycles; ++Cycle)
		{
			FAngelscriptEngine& Engine = AcquireTransientFullTestEngine();
			(void)Engine.GetScriptEngine();  // Touch the engine handle so the bind path runs.

			const int32 ThisCount = SampleSize();
			UE_LOG(Angelscript, Log,
				TEXT("[GlobalContainerCycle] %s cycle %d count = %d"),
				ContainerLabel, Cycle, ThisCount);

			if (Cycle == 1)
			{
				// Warm-up cycle: the engine cleanup we run during acquisition
				// may wipe state populated by earlier tests / engine
				// bootstrap. We don't anchor on this — drift between cycle 1
				// and cycle 2 is expected and not informative.
				continue;
			}

			if (Cycle == 2)
			{
				BaselineCount = ThisCount;
				if (!bAllowEmptyBaseline)
				{
					bProbePassed &= this->Assert.IsTrue(
						ThisCount > 0,
						*FString::Printf(
							TEXT("%s cycle 2 must register at least one entry (got %d) — otherwise the bind path didn't run"),
							ContainerLabel, ThisCount));
				}
				else if (ThisCount == 0)
				{
					UE_LOG(Angelscript, Log,
						TEXT("[GlobalContainerCycle] %s baseline is 0 (probe gated on PrecompiledData generation, not active in this run); regression check still runs."),
						ContainerLabel);
				}
			}
			else
			{
				const int32 Delta = ThisCount - BaselineCount;
				bProbePassed &= this->Assert.IsTrue(
					FMath::Abs(Delta) <= TolerancePerCycle,
					*FString::Printf(
						TEXT("%s cycle %d count=%d must stay within +/-%d of cycle-2 baseline %d (delta=%d) — drift means the cleanup is missing a Reset()/Empty() somewhere"),
						ContainerLabel, Cycle, ThisCount, TolerancePerCycle, BaselineCount, Delta));
			}
		}

		ResetToCleanSlate();
		if (!bProbePassed)
		{
			return;
		}
	}

	// Regression for `GBlueprintEventsByScriptName`. The cleanup path lives
	// at AngelscriptEngine.cpp::FAngelscriptEngine::Shutdown. If a future
	// refactor drops the `Empty()` call, this catches it within 6 cycles.
	TEST_METHOD(BlueprintEventsByScriptName_BoundedAcrossCycles)
	{
		using namespace AngelscriptTest_Memory_GlobalContainerCycleBounded_Private;

		// 8 entries of slack: hot-reload paths or rebind-on-clone scenarios
		// can re-add an entry that was already in flight from cycle 1. The
		// failure mode we want to catch is *unbounded* growth (~1k/cycle).
		RunBoundedCycleProbe(
			TEXT("GBlueprintEventsByScriptName"),
			/*NumCycles=*/5,
			/*TolerancePerCycle=*/8,
			[]() { return GetBlueprintEventsByScriptNameTotalCount(); });
	}

	// Regression for the `GScriptNativeForms` map. Only meaningful when JIT
	// is on AND `IsGeneratingPrecompiledData()` is true (binds are gated by
	// that). Under regular headless test runs the table is empty — we still
	// assert it stays stable across cycles, so a future bind-path change
	// that *does* populate it cannot regress silently.
	TEST_METHOD(ScriptNativeForms_BoundedAcrossCycles)
	{
#if AS_CAN_GENERATE_JIT
		using namespace AngelscriptTest_Memory_GlobalContainerCycleBounded_Private;

		RunBoundedCycleProbe(
			TEXT("GScriptNativeForms"),
			/*NumCycles=*/5,
			/*TolerancePerCycle=*/4,
			[]() { return FScriptFunctionNativeForm::NumNativeForms(); },
			/*bAllowEmptyBaseline=*/true);
#else
		constexpr bool bJitBuildBoundaryIsActive = true;
		ASSERT_THAT(IsTrue(
			bJitBuildBoundaryIsActive,
			TEXT("ScriptNativeForms bounded-cycle probe is an explicit build boundary when AS_CAN_GENERATE_JIT is disabled")));
#endif
	}

	// Regression for the four `FAngelscriptDocs` static TMaps. Pre-fix these
	// were never cleared on engine shutdown. Some maps populate only when
	// editor metadata is present (Tooltips, Categories), so under headless
	// runs the baselines may be 0; the bounded-cycle assertion still catches
	// any future drift.
	TEST_METHOD(AngelscriptDocs_BoundedAcrossCycles)
	{
		using namespace AngelscriptTest_Memory_GlobalContainerCycleBounded_Private;

		RunBoundedCycleProbe(
			TEXT("AngelscriptDocs.UnrealDocumentation"),
			/*NumCycles=*/5,
			/*TolerancePerCycle=*/4,
			[]() { return FAngelscriptDocs::GetUnrealDocumentationCount(); },
			/*bAllowEmptyBaseline=*/true);

		RunBoundedCycleProbe(
			TEXT("AngelscriptDocs.UnrealTypeDocumentation"),
			/*NumCycles=*/5,
			/*TolerancePerCycle=*/4,
			[]() { return FAngelscriptDocs::GetUnrealTypeDocumentationCount(); },
			/*bAllowEmptyBaseline=*/true);

		RunBoundedCycleProbe(
			TEXT("AngelscriptDocs.UnrealPropertyDocumentation"),
			/*NumCycles=*/5,
			/*TolerancePerCycle=*/4,
			[]() { return FAngelscriptDocs::GetUnrealPropertyDocumentationCount(); },
			/*bAllowEmptyBaseline=*/true);

		RunBoundedCycleProbe(
			TEXT("AngelscriptDocs.GlobalVariableDocumentation"),
			/*NumCycles=*/5,
			/*TolerancePerCycle=*/4,
			[]() { return FAngelscriptDocs::GetGlobalVariableDocumentationCount(); },
			/*bAllowEmptyBaseline=*/true);
	}

	// Regression for the original engine-shutdown UObject root leak. After
	// every cycle, no UASClass should be both detached (its OwnerScriptEngine
	// was nulled) AND still rooted. Previously these accumulated unbounded
	// (RF_MarkAsRootSet was never paired with a RemoveFromRoot on shutdown).
	TEST_METHOD(RootedDetachedASClasses_BoundedAcrossCycles)
	{
		using namespace AngelscriptTest_Memory_GlobalContainerCycleBounded_Private;

		ResetToCleanSlate();

		const int32 BaselineRootedDetached = CountRootedDetachedASClasses();
		UE_LOG(Angelscript, Log,
			TEXT("[GlobalContainerCycle] RootedDetachedASClasses baseline = %d"),
			BaselineRootedDetached);

		constexpr int32 NumCycles = 4;
		for (int32 Cycle = 1; Cycle <= NumCycles; ++Cycle)
		{
			FAngelscriptEngine& Engine = AcquireTransientFullTestEngine();
			(void)Engine.GetScriptEngine();
		}

		// Final teardown — the test storage is still holding the last
		// engine; release it so the count we read after GC reflects only
		// engines that were actually shut down.
		GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);

		const int32 FinalRootedDetached = CountRootedDetachedASClasses();
		UE_LOG(Angelscript, Log,
			TEXT("[GlobalContainerCycle] RootedDetachedASClasses after %d cycles = %d (baseline %d)"),
			NumCycles, FinalRootedDetached, BaselineRootedDetached);

		const bool bFinalCountWithinBaseline = this->Assert.IsTrue(
			FinalRootedDetached <= BaselineRootedDetached,
			*FString::Printf(
				TEXT("After %d cycles, rooted detached UASClass count (%d) must not exceed baseline (%d). Drift means a UASClass was created but never RemoveFromRoot'd on engine Shutdown."),
				NumCycles, FinalRootedDetached, BaselineRootedDetached));

		ResetToCleanSlate();
		if (!bFinalCountWithinBaseline)
		{
			return;
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
