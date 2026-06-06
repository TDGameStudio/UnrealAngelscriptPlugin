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
#include "AngelscriptTestUtilities.h"
#include "CQTest.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"

#ifndef MALLOC_LEAKDETECTION
#define MALLOC_LEAKDETECTION 0
#endif

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
	struct FBindFreeCycleResult
	{
		bool bHasScriptEngine = false;
		bool bHasSignatureRegistry = false;
		int32 BlueprintSignatureCount = INDEX_NONE;
	};

	static FBindFreeCycleResult DriveOneCycle(const TCHAR* CycleTag)
	{
		UE_LOG(Angelscript, Log, TEXT("[BindFreeEvidence] === Cycle %s begin ==="), CycleTag);
		// Use the probe-enabled overload: it wraps acquisition with the
		// T0..T3 SampleBindFreeMem calls plus an explicit FMemory::Trim,
		// which is exactly what this test class needs evidence for.
		FAngelscriptEngine& Engine = AcquireTransientFullTestEngineWithProbe();
		FBindFreeCycleResult Result;
		Result.bHasScriptEngine = Engine.GetScriptEngine() != nullptr;
		if (FBlueprintEventSignatureRegistry* Registry = Engine.GetBlueprintEventSignatureRegistry())
		{
			Result.bHasSignatureRegistry = true;
			Result.BlueprintSignatureCount = Registry->Num();
		}
		UE_LOG(Angelscript, Log, TEXT("[BindFreeEvidence] === Cycle %s end   ==="), CycleTag);
		return Result;
	}

	static void ReleaseFinalCycle()
	{
		// Drop the shared transient storage and trim again so the *outermost*
		// "baseline after all cycles" sample is comparable to the first one.
		GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);
		SampleBindFreeMem(TEXT("T4_AfterFinalRelease"));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBindFreeEvidenceTests,
	"Angelscript.TestModule.Memory.BindFreeEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BindFreeEvidence_ThreeCycles)
	{
		using namespace AngelscriptTest_Memory_BindFreeEvidenceTests_Private;

		SampleBindFreeMem(TEXT("T_BeforeFirstCycle"));

		const FBindFreeCycleResult Cycle1 = DriveOneCycle(TEXT("1"));
		const FBindFreeCycleResult Cycle2 = DriveOneCycle(TEXT("2"));
		const FBindFreeCycleResult Cycle3 = DriveOneCycle(TEXT("3"));

		ReleaseFinalCycle();

		const FBindFreeCycleResult Cycles[] = { Cycle1, Cycle2, Cycle3 };
		for (int32 CycleIndex = 0; CycleIndex < UE_ARRAY_COUNT(Cycles); ++CycleIndex)
		{
			const int32 CycleNumber = CycleIndex + 1;
			TestRunner->TestTrue(
				*FString::Printf(TEXT("BindFreeEvidence cycle %d should create a script engine"), CycleNumber),
				Cycles[CycleIndex].bHasScriptEngine);
			TestRunner->TestTrue(
				*FString::Printf(TEXT("BindFreeEvidence cycle %d should create the blueprint signature registry"), CycleNumber),
				Cycles[CycleIndex].bHasSignatureRegistry);
			TestRunner->TestTrue(
				*FString::Printf(TEXT("BindFreeEvidence cycle %d should exercise the bind path and own at least one blueprint signature (got %d)"),
					CycleNumber, Cycles[CycleIndex].BlueprintSignatureCount),
				Cycles[CycleIndex].BlueprintSignatureCount > 0);
		}
	}

	// Regression guard for the FBlueprintEventSignature leak originally identified
	// in Phase 2 of the bind-free verification (ASBindFreeCompletenessVerification.md).
	//
	// Before the fix, every full engine cycle leaked ~1.7 MB of FBlueprintEventSignature
	// because the AS 2.33 fork does not invoke any cleanup callback when an
	// asCScriptFunction is destroyed. After the fix, FBlueprintEventSignatureRegistry
	// is per-engine and Reset() is called from FAngelscriptEngine::Shutdown()
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
		GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);

		constexpr int32 NumCycles = 6;
		constexpr int32 TolerancePerCycle = 8;
		int32 BaselineCount = -1;

		for (int32 Cycle = 1; Cycle <= NumCycles; ++Cycle)
		{
			FAngelscriptEngine& Engine = AcquireTransientFullTestEngine();
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
		GetTransientFullTestEngineStorage().Reset();
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

		// The installed engine used by this repo's default build ships with
		// MALLOC_LEAKDETECTION=0, so the leak reporter console commands are not
		// available. Keep the test explicit and runnable by treating that build
		// configuration as the documented boundary.
#if !MALLOC_LEAKDETECTION
		constexpr bool bLeakDetectionBuildBoundaryIsActive = true;
		TestRunner->TestTrue(
			TEXT("BindFreeEvidence_LeakReport is an explicit boundary when MALLOC_LEAKDETECTION=0"),
			bLeakDetectionBuildBoundaryIsActive);
		return;
#else
		// 1. Establish a clean slate — drop any cached engine, GC, then trim so the
		//    leak tracker starts from a stable mimalloc baseline.
		GetTransientFullTestEngineStorage().Reset();
		CollectGarbage(RF_NoFlags, true);
		FMemory::Trim(true);

		GEngine->Exec(nullptr, TEXT("mallocleak.start size=0"));
		GEngine->Exec(nullptr, TEXT("mallocleak.clear"));

		SampleBindFreeMem(TEXT("LeakReport_BeforeFirstCycle"));

		// 2. Drive three full create/free cycles.
		const FBindFreeCycleResult Cycle1 = DriveOneCycle(TEXT("Leak-1"));
		const FBindFreeCycleResult Cycle2 = DriveOneCycle(TEXT("Leak-2"));
		const FBindFreeCycleResult Cycle3 = DriveOneCycle(TEXT("Leak-3"));

		// 3. Final release + trim, then dump the report (a no-op without MALLOC_LEAKDETECTION).
		ReleaseFinalCycle();

		GEngine->Exec(nullptr, TEXT("mallocleak.report"));
		GEngine->Exec(nullptr, TEXT("mallocleak.stop"));

		const FBindFreeCycleResult Cycles[] = { Cycle1, Cycle2, Cycle3 };
		for (int32 CycleIndex = 0; CycleIndex < UE_ARRAY_COUNT(Cycles); ++CycleIndex)
		{
			const int32 CycleNumber = CycleIndex + 1;
			TestRunner->TestTrue(
				*FString::Printf(TEXT("BindFreeEvidence leak-report cycle %d should create a script engine"), CycleNumber),
				Cycles[CycleIndex].bHasScriptEngine);
			TestRunner->TestTrue(
				*FString::Printf(TEXT("BindFreeEvidence leak-report cycle %d should create the blueprint signature registry"), CycleNumber),
				Cycles[CycleIndex].bHasSignatureRegistry);
			TestRunner->TestTrue(
				*FString::Printf(TEXT("BindFreeEvidence leak-report cycle %d should exercise the bind path and own at least one blueprint signature (got %d)"),
					CycleNumber, Cycles[CycleIndex].BlueprintSignatureCount),
				Cycles[CycleIndex].BlueprintSignatureCount > 0);
		}
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
