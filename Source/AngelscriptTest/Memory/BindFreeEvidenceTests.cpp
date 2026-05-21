// Targeted test that drives FAngelscriptEngine through several full create/destroy
// cycles. The work performed inside the cycles is deliberately minimal — the
// memory-of-interest is the bind phase itself plus the *residual* memory that
// remains after Reset() + GC + FMemory::Trim(true).
//
// The probe is implemented inside AcquireTransientFullTestEngineWithProbe()
// (see AngelscriptTestUtilities.h) and prints four samples per cycle:
//   T0 BeforeReset       — residual heap right before destruction
//   T1 AfterResetAndGC   — after engine destruct + UObject GC
//   T2 AfterTrim         — after FMemory::Trim(true) (mimalloc returns pages to OS)
//   T3 AfterNewCreate    — once the next engine is fully bound
//
// The non-probe `AcquireTransientFullTestEngine()` overload is the one the
// rest of the test suite uses; this file is the *only* caller of the
// `WithProbe` overload, so SampleBindFreeMem doesn't pollute the common path.
//
// To collapse the mimalloc reset_delay, run the test with -ini:
//   -ini:Engine:[ConsoleVariables]:mi.MemoryResetDelay=0
// otherwise the T2 sample will lag behind by ~10 seconds.

#include "AngelscriptEngine.h"
#include "Binds/BlueprintEventSignatureRegistry.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "CQTest.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"

// NOTE: We intentionally do NOT include HAL/MallocLeakDetection.h or
// ProfilingDebugging/MallocLeakReporter.h here. Doing so would force the
// test module to reference symbols (e.g. FMallocLeakDetection::DumpOpenCallstacks)
// that the installed Engine's Core.dll does not export when it was built with
// MALLOC_LEAKDETECTION=0. The leak-report path is driven entirely through
// `GEngine->Exec("mallocleak.*")` so it degrades to a logged warning when the
// engine binary lacks leak-detection support.

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Memory_BindFreeEvidenceTests_Private
{
	static void DriveOneCycle(const TCHAR* CycleTag)
	{
		UE_LOG(Angelscript, Log, TEXT("[BindFreeEvidence] === Cycle %s begin ==="), CycleTag);
		// Use the probe-enabled overload: it wraps acquisition with the
		// T0..T3 SampleBindFreeMem calls plus an explicit FMemory::Trim,
		// which is exactly what this test class needs evidence for.
		FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireTransientFullTestEngineWithProbe();
		// We intentionally touch only the engine handle. We don't compile any
		// scripts — we want the bind phase memory to dominate the cycle output.
		(void)Engine.GetScriptEngine();
		UE_LOG(Angelscript, Log, TEXT("[BindFreeEvidence] === Cycle %s end   ==="), CycleTag);
	}

	static void ReleaseFinalCycle()
	{
		// Drop the shared transient storage and trim again so the *outermost*
		// "baseline after all cycles" sample is comparable to the first one.
		AngelscriptTestSupport::GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);
		AngelscriptTestSupport::SampleBindFreeMem(TEXT("T4_AfterFinalRelease"));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBindFreeEvidenceTests,
	"Angelscript.TestModule.Memory.BindFreeEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BindFreeEvidence_ThreeCycles)
	{
		using namespace AngelscriptTest_Memory_BindFreeEvidenceTests_Private;

		AngelscriptTestSupport::SampleBindFreeMem(TEXT("T_BeforeFirstCycle"));

		DriveOneCycle(TEXT("1"));
		DriveOneCycle(TEXT("2"));
		DriveOneCycle(TEXT("3"));

		ReleaseFinalCycle();

		TestRunner->TestTrue(TEXT("BindFreeEvidence_ThreeCycles completed (see log for T0..T4)"), true);
	}

	// Regression guard for the FBlueprintEventSignature leak originally identified
	// in Phase 2 of the bind-free verification (ASBindFreeCompletenessVerification.md).
	//
	// Before the fix, every full engine cycle leaked ~1.7 MB of FBlueprintEventSignature
	// because the AS 2.33 fork does not invoke any cleanup callback when an
	// asCScriptFunction is destroyed. After the fix, FBlueprintEventSignatureRegistry
	// is per-engine and Reset() is called from FAngelscriptEngine::Shutdown()
	// (post-flatten; previously the helper was named ReleaseOwnedSharedStateResources)
	// right after ScriptEngine->ShutDownAndRelease().
	//
	// We assert two things:
	//   (a) Each fresh engine creates a *positive* number of signatures (otherwise
	//       we're either not exercising the bind path or the registry isn't wired).
	//   (b) The count for engine N+1 is within a small tolerance of the count for
	//       engine 1. If the leak had returned, ownership would accumulate across
	//       cycles and the count would drift upward monotonically.
	TEST_METHOD(BindFreeEvidence_BlueprintEventSignatureBounded)
	{
		using namespace AngelscriptTest_Memory_BindFreeEvidenceTests_Private;

		// Clean slate.
		AngelscriptTestSupport::GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);

		constexpr int32 NumCycles = 6;
		constexpr int32 TolerancePerCycle = 8;
		int32 BaselineCount = -1;

		for (int32 Cycle = 1; Cycle <= NumCycles; ++Cycle)
		{
			FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireTransientFullTestEngine();
			FBlueprintEventSignatureRegistry* Registry = Engine.GetBlueprintEventSignatureRegistry();
			if (Registry == nullptr)
			{
				TestRunner->AddError(TEXT("FBlueprintEventSignatureRegistry was not initialised for the current engine."));
				return;
			}

			const int32 ThisCount = Registry->Num();
			UE_LOG(Angelscript, Log, TEXT("[BindFreeEvidence] Cycle %d Registry->Num() = %d"), Cycle, ThisCount);

			if (Cycle == 1)
			{
				BaselineCount = ThisCount;
				TestRunner->TestTrue(
					*FString::Printf(TEXT("Cycle 1 must register at least one FBlueprintEventSignature (got %d)"), ThisCount),
					ThisCount > 0);
			}
			else
			{
				const int32 Delta = ThisCount - BaselineCount;
				TestRunner->TestTrue(
					*FString::Printf(TEXT("Cycle %d Registry->Num()=%d should stay within +/-%d of baseline %d (delta=%d) — drift indicates the leak has returned"),
						Cycle, ThisCount, TolerancePerCycle, BaselineCount, Delta),
					FMath::Abs(Delta) <= TolerancePerCycle);
			}
		}

		// Final teardown so subsequent tests start clean.
		AngelscriptTestSupport::GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);
	}

	TEST_METHOD(BindFreeEvidence_LeakReport)
	{
		using namespace AngelscriptTest_Memory_BindFreeEvidenceTests_Private;

		if (GEngine == nullptr)
		{
			TestRunner->AddError(TEXT("BindFreeEvidence_LeakReport requires GEngine to be initialised."));
			return;
		}

		// 1. Establish a clean slate — drop any cached engine, GC, then trim so the
		//    leak tracker starts from a stable mimalloc baseline.
		AngelscriptTestSupport::GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);

		// mallocleak.start: also doubles as a feature probe — if Core.dll was built
		// with MALLOC_LEAKDETECTION=0 the engine logs an error and the rest of the
		// mallocleak.* calls become no-ops. We still want the rest of the cycle to
		// run so the LLM CSV captures the same pattern as the ThreeCycles test.
		GEngine->Exec(nullptr, TEXT("mallocleak.start size=0"));
		GEngine->Exec(nullptr, TEXT("mallocleak.clear"));

		AngelscriptTestSupport::SampleBindFreeMem(TEXT("LeakReport_BeforeFirstCycle"));

		// 2. Drive three full create/free cycles.
		DriveOneCycle(TEXT("Leak-1"));
		DriveOneCycle(TEXT("Leak-2"));
		DriveOneCycle(TEXT("Leak-3"));

		// 3. Final release + trim, then dump the report (a no-op without MALLOC_LEAKDETECTION).
		ReleaseFinalCycle();

		GEngine->Exec(nullptr, TEXT("mallocleak.report"));
		GEngine->Exec(nullptr, TEXT("mallocleak.stop"));

		TestRunner->TestTrue(TEXT("BindFreeEvidence_LeakReport completed (see Profiling/MemReports/*Leaks.txt if MALLOC_LEAKDETECTION=1)"), true);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
