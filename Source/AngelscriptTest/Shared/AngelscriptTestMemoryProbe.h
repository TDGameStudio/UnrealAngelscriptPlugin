#pragma once

// ============================================================================
// AngelscriptTestMemoryProbe
// ============================================================================
//
// Themed sub-header split out of `AngelscriptTestUtilities.h` (Phase 1 of
// OpenSpec change `refactor-as-test-shared-layout-and-naming`).
//
// Responsibility:
//   - `SampleBindFreeMem` — coarse OS-level memory probe used **only** by
//     the `Memory.BindFreeEvidence` test family. Intentionally kept off
//     the common acquisition path so 400+ tests that just need a fresh
//     Full engine pay zero `FPlatformMemory::GetStats` + `FMemory::Trim`
//     overhead and do not pollute logs with `[BindFreeProbe]` lines.
//   - `AcquireTransientFullTestEngine` — default transient acquisition
//     entry point. Resets the previous transient engine, runs
//     `CleanupDetachedASTypesForGarbageCollection`, runs `CollectGarbage`,
//     and hands back a freshly-created isolated Full engine. No memory
//     probes by design.
//   - `AcquireTransientFullTestEngineWithProbe` — diagnostic overload that
//     wraps the same acquisition flow with the five-phase
//     `T0_BeforeReset` / `T1_AfterResetAndGC` / `T2_AfterTrim` /
//     `T3_AfterNewCreate` probe sequence (use with
//     `-ini:Engine:[ConsoleVariables]:mi.MemoryResetDelay=0` to collapse
//     mimalloc's 10s reset delay during measurement).
//
// Note: `AcquireTransientFullTestEngine` is the **real** definition; the
// historical forward declaration at original `AngelscriptTestUtilities.h`
// line 191 is intentionally not carried over because no in-scope caller
// from `AngelscriptTestEngineAcquisition.h` depends on it.
//
// Original location: AngelscriptTestUtilities.h lines 403-466.
// ============================================================================

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestEngineCleanup.h"
#include "AngelscriptEngine.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "UObject/GarbageCollection.h"

// Coarse OS-level memory probe used by the Memory.BindFreeEvidence tests.
// NOT called automatically by AcquireTransientFullTestEngine — those probe
// callers explicitly invoke it through AcquireTransientFullTestEngineWithProbe
// below. Keeping the probe out of the common acquire path means the 400+
// tests that just need a fresh Full engine pay zero `FPlatformMemory::GetStats`
// + `FMemory::Trim(true)` overhead and don't pollute logs with [BindFreeProbe].
inline void SampleBindFreeMem(const TCHAR* Phase)
{
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	UE_LOG(Angelscript, Log, TEXT("[BindFreeProbe] %s | OS_UsedPhys=%.1f MB | Peak=%.1f MB | UsedVirt=%.1f MB"),
		Phase,
		Stats.UsedPhysical / (1024.0 * 1024.0),
		Stats.PeakUsedPhysical / (1024.0 * 1024.0),
		Stats.UsedVirtual / (1024.0 * 1024.0));
}

// Default acquisition path. Reset the previous transient engine, GC, and
// hand back a freshly-created isolated Full engine. No memory probes, no
// `FMemory::Trim(true)` — those are diagnostic operations that belong to
// BindFreeEvidence tests, not to the 400+ tests that use ASTEST_CREATE_ENGINE_FULL.
inline FAngelscriptEngine& AcquireTransientFullTestEngine()
{
	TUniquePtr<FAngelscriptEngine>& TransientFullEngine = GetTransientFullTestEngineStorage();
	if (TransientFullEngine.IsValid())
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesBeforeReset = TransientFullEngine->GetActiveModules();
		TransientFullEngine.Reset();
		CleanupDetachedASTypesForGarbageCollection(&ModulesBeforeReset);
		CollectGarbage(RF_NoFlags, true);
	}
	TransientFullEngine = CreateIsolatedFullEngine();
	check(TransientFullEngine.IsValid());
	return *TransientFullEngine;
}

// Diagnostic overload for the Memory.BindFreeEvidence test family. Wraps
// the same acquisition flow with five SampleBindFreeMem probes (T0..T3,
// plus an explicit FMemory::Trim(true)) so each cycle's residual heap
// shows up before/after the allocator gets a chance to release pages.
// Run with `-ini:Engine:[ConsoleVariables]:mi.MemoryResetDelay=0` to
// collapse mimalloc's 10s reset delay during measurement.
inline FAngelscriptEngine& AcquireTransientFullTestEngineWithProbe()
{
	TUniquePtr<FAngelscriptEngine>& TransientFullEngine = GetTransientFullTestEngineStorage();
	if (TransientFullEngine.IsValid())
	{
		SampleBindFreeMem(TEXT("T0_BeforeReset"));

		const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesBeforeReset = TransientFullEngine->GetActiveModules();
		TransientFullEngine.Reset();
		CleanupDetachedASTypesForGarbageCollection(&ModulesBeforeReset);
		CollectGarbage(RF_NoFlags, true);
		SampleBindFreeMem(TEXT("T1_AfterResetAndGC"));

		// Force mimalloc to return free pages to the OS so the residual we observe
		// after this point is real (i.e. not pages held by the allocator's reset_delay).
		FMemory::Trim(true);
		SampleBindFreeMem(TEXT("T2_AfterTrim"));
	}
	TransientFullEngine = CreateIsolatedFullEngine();
	SampleBindFreeMem(TEXT("T3_AfterNewCreate"));
	check(TransientFullEngine.IsValid());
	return *TransientFullEngine;
}

