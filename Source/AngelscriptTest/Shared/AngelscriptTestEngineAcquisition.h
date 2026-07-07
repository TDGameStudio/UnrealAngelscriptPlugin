#pragma once

// ============================================================================
// AngelscriptTestEngineAcquisition
// ============================================================================
//
// Themed sub-header split out of `AngelscriptTestUtilities.h` (Phase 1 of
// OpenSpec change `refactor-as-test-shared-layout-and-naming`).
//
// Responsibility:
//   - Top-level `FAngelscriptTestEngineScopeAccess` shim that exposes a
//     friend-only view of `FAngelscriptEngine` singleton lifecycle.
//   - `` namespace covering every entry point that
//     **acquires**, **creates**, **resets**, **logs**, or **destroys** the
//     shared/isolated/production-like test engines:
//       * Production-runtime engine discovery
//         (`TryGetRunningProductionSubsystem`, `TryGetRunningProductionEngine`,
//          `TryGetRunningProductionDebuggerEngine`).
//       * Engine factories
//         (`CreateBareScriptEngine`, `CreateScriptScanFreeFullEngineForTesting`,
//          `CreateScriptScanFreeEngineForTesting`, `CreateIsolatedFullEngine`,
//          `CreateIsolatedCloneEngine`, `CreateFullTestEngine`).
//       * Shared-engine singletons
//         (`GetSharedTestEngineStorage`, `GetTransientFullTestEngineStorage`,
//          `GetOrCreateSharedCloneEngine`, `AcquireCleanSharedCloneEngine`).
//       * Reset / debug logging / destruction
//         (`ResetSharedCloneEngine`, `LogSharedEngineDebugState`,
//          `DestroySharedTestEngine`, `DestroyStrayLegacyGlobalTestEngine`,
//          `DestroySharedAndStrayGlobalTestEngine`).
//       * Production-like resolver
//         (`FResolvedProductionLikeEngine`, `AcquireProductionLikeEngine`,
//          `RequireRunningProductionEngine`).
//       * Scoped world-context helper (`FScopedTestWorldContextScope`).
//
// History:
//   - The four pure-forward aliases `GetSharedTestEngine`,
//     `ResetSharedInitializedTestEngine`, `GetResetSharedTestEngine`,
//     and `AcquireFreshSharedCloneEngine` were retired in Phase 1
//     task 1.8 of the same OpenSpec change. All 19 prior call sites
//     inside `AngelscriptTest/` were migrated to the canonical entry
//     points (`GetOrCreateSharedCloneEngine`, `ResetSharedCloneEngine`,
//     `AcquireCleanSharedCloneEngine`, or the explicit
//     `DestroySharedAndStrayGlobalTestEngine()` +
//     `AcquireCleanSharedCloneEngine()` pair).
//
// Scope guard:
//   - This header does **not** include the editor-only Blueprint headers
//     (`BlueprintActionDatabase.h`, `K2Node_GetSubsystem.h`); those stay
//     quarantined in `AngelscriptTestEngineCleanup.h`.
//   - `AcquireTransientFullTestEngine` / `*WithProbe` live in
//     `AngelscriptTestMemoryProbe.h` because their bodies depend on
//     `CleanupDetachedASTypesForGarbageCollection` (Cleanup.h) and the
//     `SampleBindFreeMem` probe (MemoryProbe.h). The legacy forward
//     declaration at original line 191 is intentionally **not** carried
//     over; no caller in this header depends on it.
//
// Original location: AngelscriptTestUtilities.h lines 32-247 + 468-691.
// ============================================================================

#include "AngelscriptEngine.h"
#include "AngelscriptSubsystem.h"
#include "AngelscriptTestEngine.h"
#include "ClassGenerator/ASClass.h"
#include "Containers/UnrealString.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

struct FAngelscriptTestEngineScopeAccess
{
	static FAngelscriptEngine* GetCurrentEngine()
	{
		return FAngelscriptEngine::TryGetCurrentEngine();
	}

	static FAngelscriptEngine* GetGlobalEngine()
	{
		return FAngelscriptEngine::TryGetGlobalEngine();
	}

	static bool DestroyGlobalEngine()
	{
		return FAngelscriptEngine::DestroyGlobal();
	}
};

using FAngelscriptTestEngineScopeAccess = ::FAngelscriptTestEngineScopeAccess;

inline UAngelscriptSubsystem* TryGetRunningProductionSubsystem()
{
	if (UAngelscriptSubsystem* Subsystem = UAngelscriptSubsystem::GetCurrent())
	{
		return Subsystem;
	}

	if (GEngine == nullptr)
	{
		return nullptr;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (World == nullptr)
		{
			continue;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		if (GameInstance == nullptr)
		{
			continue;
		}

		if (UAngelscriptSubsystem* Subsystem = GameInstance->GetSubsystem<UAngelscriptSubsystem>())
		{
			return Subsystem;
		}
	}

	return nullptr;
}

inline TUniquePtr<FAngelscriptEngine>& GetSharedTestEngineStorage()
{
	static TUniquePtr<FAngelscriptEngine> Storage;
	return Storage;
}

inline TUniquePtr<FAngelscriptEngineScope>& GetSharedTestEngineScopeStorage()
{
	static TUniquePtr<FAngelscriptEngineScope> Storage;
	return Storage;
}

struct FScopedTestWorldContextScope
{
	explicit FScopedTestWorldContextScope(UObject* WorldContextObject)
	{
		if (WorldContextObject != nullptr)
		{
			Scope = MakeUnique<FAngelscriptGameThreadScopeWorldContext>(WorldContextObject);
		}
	}

private:
	TUniquePtr<FAngelscriptGameThreadScopeWorldContext> Scope;
};

inline FAngelscriptEngine* TryGetRunningProductionEngine()
{
	if (UAngelscriptSubsystem* Subsystem = TryGetRunningProductionSubsystem())
	{
		if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
		{
			return AttachedEngine;
		}
	}

	if (FAngelscriptEngine::IsInitialized())
	{
		return &FAngelscriptEngine::Get();
	}

	return nullptr;
}

/**
 * Creates a bare asCScriptEngine with minimal AngelScript SDK configuration.
 * Does NOT register any UE type bindings, script class generators, or reflection hooks.
 * Intended for AngelScriptSDK tests that need a pure script engine sandbox.
 */
inline asCScriptEngine* CreateBareScriptEngine()
{
	asIScriptEngine* RawEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	return static_cast<asCScriptEngine*>(RawEngine);
}

inline TUniquePtr<FAngelscriptEngine> CreateScriptScanFreeFullEngineForTesting(
	const FAngelscriptEngineConfig& Config,
	const FAngelscriptEngineDependencies& Dependencies)
{
	// Test-only full engines bind UE/AS types and mark the initial compile gate
	// complete, but intentionally do not scan project/plugin Script roots or
	// compile disk .as files. Test scripts are supplied explicitly through the
	// in-memory helpers below, so the shared engine starts without
	// Script/Examples modules or generated classes. Routes through
	// `FAngelscriptTestEngine::Create` so the `bSkipInitialCompile` flag is
	// owned by the test wrapper rather than set inline here.
	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
	UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] Created script-scan-free Full engine %p"),
		Engine.Get());
	return Engine;
}

inline TUniquePtr<FAngelscriptEngine> CreateScriptScanFreeFullEngineForTesting()
{
	FAngelscriptEngineConfig Config;
	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
}

inline TUniquePtr<FAngelscriptEngine> CreateScriptScanFreeEngineForTesting(
	const FAngelscriptEngineConfig& Config,
	const FAngelscriptEngineDependencies& Dependencies)
{
	// Phase 2: with the Clone mechanism removed, every test engine is a
	// fresh Full engine. Forward to FAngelscriptTestEngine::Create so
	// the new helper is the single creation entry point for tests.
	return FAngelscriptTestEngine::Create(Config, Dependencies);
}

inline TUniquePtr<FAngelscriptEngine> CreateIsolatedFullEngine()
{
	return CreateScriptScanFreeFullEngineForTesting();
}

inline TUniquePtr<FAngelscriptEngine>& GetTransientFullTestEngineStorage()
{
	static thread_local TUniquePtr<FAngelscriptEngine> TransientFullEngine;
	return TransientFullEngine;
}

inline TUniquePtr<FAngelscriptEngine> CreateIsolatedCloneEngine()
{
	FAngelscriptEngineConfig Config;
	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
	UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] Created isolated test engine %p"),
		Engine.Get());
	return Engine;
}

inline FAngelscriptEngine& GetOrCreateSharedCloneEngine()
{
	// Phase 2: forward to FAngelscriptTestEngine, which now owns the
	// shared-engine singleton. The legacy name is retained as a thin
	// wrapper to keep existing test sites compiling unchanged; new code
	// should call FAngelscriptTestEngine::GetSharedEngine() directly.
	return FAngelscriptTestEngine::GetSharedEngine();
}

inline FAngelscriptEngine* TryGetRunningProductionDebuggerEngine()
{
	if (UAngelscriptSubsystem* Subsystem = TryGetRunningProductionSubsystem())
	{
		if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
		{
			if (AttachedEngine->DebugServer != nullptr)
			{
				return AttachedEngine;
			}
		}
	}

#if WITH_ANGELSCRIPT_UNITTESTS
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	FAngelscriptEngine* MatchingEngine = nullptr;
	for (int32 Index = SavedStack.Num() - 1; Index >= 0; --Index)
	{
		FAngelscriptEngine* Candidate = SavedStack[Index];
		if (Candidate != nullptr && Candidate->DebugServer != nullptr)
		{
			MatchingEngine = Candidate;
			break;
		}
	}

	FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	return MatchingEngine;
#else
	return nullptr;
#endif
}

inline void ResetSharedCloneEngine(FAngelscriptEngine& Engine)
{
	// Phase 2: forward to FAngelscriptTestEngine, which now owns the
	// module-reset semantics. The legacy name is retained as a thin
	// wrapper to keep existing test sites compiling unchanged; new code
	// should call FAngelscriptTestEngine::ResetModules() directly.
	FAngelscriptTestEngine::ResetModules(Engine);
}

inline void LogSharedEngineDebugState(const TCHAR* Phase, FAngelscriptEngine& Engine)
{
	asCScriptEngine* ScriptEngine = reinterpret_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	if (ScriptEngine == nullptr)
	{
		UE_LOG(Angelscript, Log, TEXT("[TestDebug] %s ScriptEngine=<null>"), Phase);
		return;
	}

	int32 LiveASClasses = 0;
	int32 DetachedASClasses = 0;
	int32 RootedDetachedASClasses = 0;
	TArray<FString> DetachedClassNames;
	for (TObjectIterator<UASClass> It; It; ++It)
	{
		if (It->ScriptTypePtr != nullptr)
		{
			++LiveASClasses;
		}
		else
		{
			++DetachedASClasses;
			if (It->IsRooted())
			{
				++RootedDetachedASClasses;
			}
			if (DetachedClassNames.Num() < 16)
			{
				DetachedClassNames.Add(FString::Printf(
					TEXT("%s Rooted=%s Standalone=%s"),
					*It->GetPathName(),
					It->IsRooted() ? TEXT("true") : TEXT("false"),
					It->HasAnyFlags(RF_Standalone) ? TEXT("true") : TEXT("false")));
			}
		}
	}

	int32 LiveASFunctions = 0;
	int32 DetachedASFunctions = 0;
	TArray<FString> DetachedFunctionNames;
	for (TObjectIterator<UASFunction> It; It; ++It)
	{
		if (It->ScriptFunction != nullptr)
		{
			++LiveASFunctions;
		}
		else
		{
			++DetachedASFunctions;
			if (DetachedFunctionNames.Num() < 16)
			{
				DetachedFunctionNames.Add(FString::Printf(
					TEXT("%s Outer=%s Validate=%s"),
					*It->GetPathName(),
					It->GetOuter() != nullptr ? *It->GetOuter()->GetPathName() : TEXT("<null>"),
					It->ValidateFunction != nullptr ? TEXT("true") : TEXT("false")));
			}
		}
	}

	TArray<FString> RawModuleNames;
	const asUINT RawModuleCount = ScriptEngine->GetModuleCount();
	RawModuleNames.Reserve(static_cast<int32>(RawModuleCount));
	for (asUINT ModuleIndex = 0; ModuleIndex < RawModuleCount; ++ModuleIndex)
	{
		if (asIScriptModule* Module = ScriptEngine->GetModuleByIndex(ModuleIndex))
		{
			RawModuleNames.Add(UTF8_TO_TCHAR(Module->GetName()));
		}
	}

	UE_LOG(
		Angelscript,
		Log,
		TEXT("[TestDebug] %s ActiveModules=%d RawModules=%u ScriptFunctionSlots=%d FreeScriptFunctionIds=%d LiveASClasses=%d DetachedASClasses=%d RootedDetachedASClasses=%d LiveASFunctions=%d DetachedASFunctions=%d"),
		Phase,
		Engine.GetActiveModules().Num(),
		ScriptEngine->GetModuleCount(),
		ScriptEngine->scriptFunctions.GetLength(),
		ScriptEngine->freeScriptFunctionIds.GetLength(),
		LiveASClasses,
		DetachedASClasses,
		RootedDetachedASClasses,
		LiveASFunctions,
		DetachedASFunctions);

	if (RawModuleNames.Num() > 0)
	{
		UE_LOG(Angelscript, Log, TEXT("[TestDebug] %s RawModuleNames=%s"), Phase, *FString::Join(RawModuleNames, TEXT(", ")));
	}

	if (DetachedClassNames.Num() > 0)
	{
		UE_LOG(Angelscript, Log, TEXT("[TestDebug] %s DetachedClasses=%s"), Phase, *FString::Join(DetachedClassNames, TEXT(" | ")));
	}

	if (DetachedFunctionNames.Num() > 0)
	{
		UE_LOG(Angelscript, Log, TEXT("[TestDebug] %s DetachedFunctions=%s"), Phase, *FString::Join(DetachedFunctionNames, TEXT(" | ")));
	}
}

inline FAngelscriptEngine& AcquireCleanSharedCloneEngine()
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] AcquireClean: resetting shared engine %p"),
		&Engine);
	ResetSharedCloneEngine(Engine);
	return Engine;
}

inline void DestroySharedTestEngine()
{
	TUniquePtr<FAngelscriptEngine>& SharedEngineStorage = GetSharedTestEngineStorage();
	TUniquePtr<FAngelscriptEngineScope>& SharedScope = GetSharedTestEngineScopeStorage();
	if (SharedEngineStorage.IsValid())
	{
		UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] DestroyShared: tearing down engine %p hasScope=%s"),
			SharedEngineStorage.Get(),
			SharedScope.IsValid() ? TEXT("true") : TEXT("false"));
		LogSharedEngineDebugState(TEXT("DestroySharedTestEngine.PreReset"), *SharedEngineStorage);
		ResetSharedCloneEngine(*SharedEngineStorage);
		LogSharedEngineDebugState(TEXT("DestroySharedTestEngine.PostReset"), *SharedEngineStorage);
	}

	SharedScope.Reset();
	SharedEngineStorage.Reset();
}

inline void DestroyStrayLegacyGlobalTestEngine()
{
	if (!UAngelscriptSubsystem::HasAnyTickOwner())
	{
		if (FAngelscriptEngine* GlobalEngine = FAngelscriptTestEngineScopeAccess::GetGlobalEngine())
		{
			ResetSharedCloneEngine(*GlobalEngine);
		}

		FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
	}
}

inline void DestroySharedAndStrayGlobalTestEngine()
{
	DestroySharedTestEngine();
	DestroyStrayLegacyGlobalTestEngine();
}

inline TUniquePtr<FAngelscriptEngine> CreateFullTestEngine()
{
	return CreateIsolatedFullEngine();
}

struct FResolvedProductionLikeEngine
{
	TUniquePtr<FAngelscriptEngine> OwnedEngine;
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FAngelscriptEngine* Engine = nullptr;

	FAngelscriptEngine& Get() const
	{
		check(Engine != nullptr);
		return *Engine;
	}
};

inline bool AcquireProductionLikeEngine(FAutomationTestBase& Test, const TCHAR* ErrorContext, FResolvedProductionLikeEngine& OutResolved)
{
	if (FAngelscriptEngine* ProductionEngine = TryGetRunningProductionEngine())
	{
		OutResolved.Engine = ProductionEngine;
		OutResolved.EngineScope = MakeUnique<FAngelscriptEngineScope>(*ProductionEngine);
		return true;
	}

	// Shared test storage may currently own the single live Full epoch.
	DestroySharedTestEngine();

	OutResolved.OwnedEngine = CreateFullTestEngine();
	if (!Test.TestNotNull(ErrorContext, OutResolved.OwnedEngine.Get()))
	{
		return false;
	}

	OutResolved.Engine = OutResolved.OwnedEngine.Get();
	OutResolved.EngineScope = MakeUnique<FAngelscriptEngineScope>(*OutResolved.Engine);
	return true;
}

inline FAngelscriptEngine* RequireRunningProductionEngine(FAutomationTestBase& Test, const TCHAR* ErrorContext)
{
	if (FAngelscriptEngine* ProductionEngine = TryGetRunningProductionEngine())
	{
		return ProductionEngine;
	}

	Test.AddError(ErrorContext);
	return nullptr;
}

