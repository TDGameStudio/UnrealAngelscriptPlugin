#pragma once

#include "AngelscriptEngine.h"
#include "AngelscriptGameInstanceSubsystem.h"
#include "Shared/AngelscriptTestEngine.h"
#include "HAL/FileManager.h"
#include "Containers/StringConv.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/Crc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_EDITOR
#include "BlueprintActionDatabase.h"
#include "K2Node_GetSubsystem.h"
#include "Subsystems/Subsystem.h"
#endif

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

namespace AngelscriptTestSupport
{
	using FAngelscriptTestEngineScopeAccess = ::FAngelscriptTestEngineScopeAccess;

	inline UAngelscriptGameInstanceSubsystem* TryGetRunningProductionSubsystem()
	{
		if (UAngelscriptGameInstanceSubsystem* Subsystem = UAngelscriptGameInstanceSubsystem::GetCurrent())
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

			if (UAngelscriptGameInstanceSubsystem* Subsystem = GameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>())
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
		if (UAngelscriptGameInstanceSubsystem* Subsystem = TryGetRunningProductionSubsystem())
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

	// AcquireTransientFullTestEngine is defined after CleanupDetachedASTypesForGarbageCollection
	FAngelscriptEngine& AcquireTransientFullTestEngine();

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
		if (UAngelscriptGameInstanceSubsystem* Subsystem = TryGetRunningProductionSubsystem())
		{
			if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
			{
				if (AttachedEngine->DebugServer != nullptr)
				{
					return AttachedEngine;
				}
			}
		}

#if WITH_DEV_AUTOMATION_TESTS
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

	inline FAngelscriptEngine& GetSharedTestEngine()
	{
		return GetOrCreateSharedCloneEngine();
	}

	struct FDetachedASTypeCleanupResult
	{
		int32 DetachedClassCount = 0;
		int32 RootedDetachedClassCount = 0;
		int32 DetachedStructCount = 0;
		int32 RootedDetachedStructCount = 0;
		int32 DiscardedEnumCount = 0;
		int32 RootedDiscardedEnumCount = 0;
		int32 DiscardedDelegateFunctionCount = 0;
		int32 RootedDiscardedDelegateFunctionCount = 0;
		int32 BlueprintActionCacheClearedCount = 0;
		int32 BlueprintSubsystemNodeActionCacheClearedCount = 0;
	};

	inline FDetachedASTypeCleanupResult CleanupDetachedASTypesForGarbageCollection(const TArray<TSharedRef<FAngelscriptModuleDesc>>* DiscardedModules = nullptr)
	{
		FDetachedASTypeCleanupResult Result;
#if WITH_EDITOR
		// Editor Blueprint action entries can keep test-generated UASClass objects alive.
		// The action database references UBlueprintNodeSpawner objects during GC, and
		// variable node spawners can point at FProperty objects owned by the generated class.
		// Use TryGet() so reset only cleans an existing database instead of initializing one.
		FBlueprintActionDatabase* BlueprintActionDatabase = FBlueprintActionDatabase::TryGet();
		bool bClearSubsystemNodeActions = false;
		auto MarkSubsystemNodeActionsDirty = [&bClearSubsystemNodeActions](UObject* Object)
		{
			const UClass* Class = Cast<UClass>(Object);
			if (Class != nullptr && Class->IsChildOf(USubsystem::StaticClass()))
			{
				bClearSubsystemNodeActions = true;
			}
		};
#endif
		auto CleanupGeneratedObject = [&Result
#if WITH_EDITOR
			, BlueprintActionDatabase, &MarkSubsystemNodeActionsDirty
#endif
		](UObject* Object, int32& ObjectCount, int32& RootedObjectCount)
		{
			if (Object == nullptr)
			{
				return;
			}

			++ObjectCount;
#if WITH_EDITOR
			MarkSubsystemNodeActionsDirty(Object);
			if (BlueprintActionDatabase != nullptr && BlueprintActionDatabase->ClearAssetActions(Object))
			{
				++Result.BlueprintActionCacheClearedCount;
			}
#endif
			if (Object->IsRooted())
			{
				Object->RemoveFromRoot();
				++RootedObjectCount;
			}
			Object->ClearFlags(RF_Standalone);
		};

		for (TObjectIterator<UASClass> It; It; ++It)
		{
			if (It->ScriptTypePtr == nullptr)
			{
				++Result.DetachedClassCount;
#if WITH_EDITOR
				MarkSubsystemNodeActionsDirty(*It);
				// Drop cached actions before GC; otherwise Blueprint action spawners may
				// remain external strong references to this detached generated class.
				if (BlueprintActionDatabase != nullptr && BlueprintActionDatabase->ClearAssetActions(*It))
				{
					++Result.BlueprintActionCacheClearedCount;
				}
#endif
				if (It->IsRooted())
				{
					It->RemoveFromRoot();
					++Result.RootedDetachedClassCount;
				}
				It->ClearFlags(RF_Standalone);
			}
		}

		for (TObjectIterator<UASStruct> It; It; ++It)
		{
			if (It->ScriptType == nullptr)
			{
				++Result.DetachedStructCount;
#if WITH_EDITOR
				// Script structs are also rooted/standalone generated objects. If the
				// editor created struct actions, drop those cache entries before GC.
				if (BlueprintActionDatabase != nullptr && BlueprintActionDatabase->ClearAssetActions(*It))
				{
					++Result.BlueprintActionCacheClearedCount;
				}
#endif
				if (It->IsRooted())
				{
					It->RemoveFromRoot();
					++Result.RootedDetachedStructCount;
				}
				It->ClearFlags(RF_Standalone);
			}
		}

		if (DiscardedModules != nullptr)
		{
			// UEnum and UDelegateFunction do not carry an object-local "detached"
			// marker like UASClass::ScriptTypePtr or UASStruct::ScriptType. Limit
			// cleanup to objects recorded by modules we just discarded so reset will
			// not touch generated types that may still belong to another live engine.
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : *DiscardedModules)
			{
				for (const TSharedRef<FAngelscriptEnumDesc>& Enum : Module->Enums)
				{
					CleanupGeneratedObject(
						Enum->Enum,
						Result.DiscardedEnumCount,
						Result.RootedDiscardedEnumCount);
				}

				for (const TSharedRef<FAngelscriptDelegateDesc>& Delegate : Module->Delegates)
				{
					CleanupGeneratedObject(
						Delegate->Function,
						Result.DiscardedDelegateFunctionCount,
						Result.RootedDiscardedDelegateFunctionCount);
				}
			}
		}

#if WITH_EDITOR
		if (bClearSubsystemNodeActions && BlueprintActionDatabase != nullptr)
		{
			auto ClearNodeActions = [&Result, BlueprintActionDatabase](UClass* NodeClass)
			{
				if (NodeClass != nullptr && BlueprintActionDatabase->ClearAssetActions(NodeClass))
				{
					++Result.BlueprintActionCacheClearedCount;
					++Result.BlueprintSubsystemNodeActionCacheClearedCount;
				}
			};

			ClearNodeActions(UK2Node_GetSubsystem::StaticClass());
			ClearNodeActions(UK2Node_GetSubsystemFromPC::StaticClass());
			ClearNodeActions(UK2Node_GetEngineSubsystem::StaticClass());
			ClearNodeActions(UK2Node_GetEditorSubsystem::StaticClass());
		}
#endif

		return Result;
	}

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

	inline void ResetSharedCloneEngine(FAngelscriptEngine& Engine)
	{
		// Phase 2: forward to FAngelscriptTestEngine, which now owns the
		// module-reset semantics. The legacy name is retained as a thin
		// wrapper to keep existing test sites compiling unchanged; new code
		// should call FAngelscriptTestEngine::ResetModules() directly.
		FAngelscriptTestEngine::ResetModules(Engine);
	}

	inline void ResetSharedInitializedTestEngine(FAngelscriptEngine& Engine)
	{
		ResetSharedCloneEngine(Engine);
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

	inline FAngelscriptEngine& GetResetSharedTestEngine()
	{
		return AcquireCleanSharedCloneEngine();
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
		if (!UAngelscriptGameInstanceSubsystem::HasAnyTickOwner())
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

	inline FAngelscriptEngine& AcquireFreshSharedCloneEngine()
	{
		DestroySharedAndStrayGlobalTestEngine();
		return AcquireCleanSharedCloneEngine();
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

	inline void ReportCompileDiagnostics(FAutomationTestBase& Test, const FAngelscriptEngine& Engine, const FString& AbsoluteFilename)
	{
		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
		if (Diagnostics == nullptr || Diagnostics->Diagnostics.IsEmpty())
		{
			return;
		}

		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
		{
			if (!Diagnostic.bIsError)
			{
				continue;
			}

			Test.AddError(FString::Printf(
				TEXT("%s:%d:%d: %s"),
				*AbsoluteFilename,
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}
	}

	struct FScopedAutomaticImportsOverride
	{
		explicit FScopedAutomaticImportsOverride(asIScriptEngine* InScriptEngine)
			: ScriptEngine(InScriptEngine)
			, PreviousValue(InScriptEngine != nullptr ? InScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS) : 0)
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 0);
			}
		}

		~FScopedAutomaticImportsOverride()
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, PreviousValue);
			}
		}

		asIScriptEngine* ScriptEngine = nullptr;
		asPWORD PreviousValue = 0;
	};

	inline asIScriptModule* BuildModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName, const FString& Source)
	{
		const FString RequestedModuleName = ANSI_TO_TCHAR(ModuleName);
		const FString UniqueFilename = FString::Printf(TEXT("%s_%s.as"), *RequestedModuleName, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString RelativeFilename = FString::Printf(TEXT("%s.as"), *RequestedModuleName);
		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), UniqueFilename);
		const FString AutomationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
		TArray<FString> ExistingAutomationFiles;
		IFileManager::Get().FindFiles(ExistingAutomationFiles, *(AutomationDirectory / (RequestedModuleName + TEXT("*.as"))), true, false);
		for (const FString& ExistingFilename : ExistingAutomationFiles)
		{
			IFileManager::Get().Delete(*(AutomationDirectory / ExistingFilename), false, true, true);
		}

		if (!FFileHelper::SaveStringToFile(Source, *AbsoluteFilename))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write script module '%s' to '%s'"), *RequestedModuleName, *AbsoluteFilename));
			return nullptr;
		}

		FAngelscriptEngineScope EngineScope(Engine);

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			ReportCompileDiagnostics(Test, Engine, AbsoluteFilename);
			Test.AddError(FString::Printf(TEXT("Failed to preprocess script module '%s'"), *RequestedModuleName));
			return nullptr;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			ReportCompileDiagnostics(Test, Engine, AbsoluteFilename);
			Test.AddError(FString::Printf(TEXT("Failed to compile script module '%s'"), *RequestedModuleName));
			return nullptr;
		}

		if (!Test.TestTrue(TEXT("Exactly one Angelscript test module should compile"), CompiledModules.Num() == 1))
		{
			return nullptr;
		}

		asIScriptModule* Module = CompiledModules[0]->ScriptModule;
		const FString ModuleContext = FString::Printf(TEXT("Compiled script module '%s' should have a backing asIScriptModule"), *RequestedModuleName);
		if (!Test.TestNotNull(*ModuleContext, Module))
		{
			return nullptr;
		}

		return Module;
	}

	inline asIScriptFunction* GetFunctionByDecl(FAutomationTestBase& Test, asIScriptModule& Module, const FString& Declaration)
	{
		FString FunctionName;
		FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get());
		if (Function == nullptr)
		{
			int32 OpenParenIndex = INDEX_NONE;
			if (Declaration.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
					if (!FunctionName.IsEmpty())
					{
						FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
						Function = Module.GetFunctionByName(FunctionNameUtf8.Get());
					}
				}
			}
		}

		if (Function == nullptr && !FunctionName.IsEmpty())
		{
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
				{
					Function = CandidateFunction;
					break;
				}
			}
		}

		if (Function == nullptr)
		{
			FString AvailableFunctions;
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction == nullptr)
				{
					continue;
				}

				if (!AvailableFunctions.IsEmpty())
				{
					AvailableFunctions += TEXT(", ");
				}

				AvailableFunctions += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
			}

			if (AvailableFunctions.IsEmpty())
			{
				Test.AddError(FString::Printf(TEXT("Failed to find function declaration '%s'; module exposes no global functions"), *Declaration));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to find function declaration '%s'; available functions: %s"), *Declaration, *AvailableFunctions));
			}
		}

		return Function;
	}

	inline bool ExecuteIntFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, int32& OutValue)
	{
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Failed to create Angelscript execution context"));
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (PrepareResult != asSUCCESS)
		{
			Test.AddError(FString::Printf(TEXT("Failed to prepare function (code %d)"), PrepareResult));
		}
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			Test.AddError(FString::Printf(TEXT("Failed to execute function (code %d)"), ExecuteResult));
		}

		if (PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}

		Context->Release();
		return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
	}

	inline bool ExecuteIntFunctionExpectingScriptException(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptFunction& Function,
		const TCHAR* ContextLabel,
		const TCHAR* ExpectedExceptionContains,
		const TCHAR* ExpectedStackFrameContains = nullptr,
		const TCHAR* ExpectedOuterStackFrameContains = nullptr)
	{
		if (const ANSICHAR* ModuleNameAnsi = Function.GetModuleName())
		{
			if (ModuleNameAnsi[0] != '\0')
			{
				Test.AddExpectedErrorPlain(UTF8_TO_TCHAR(ModuleNameAnsi), EAutomationExpectedErrorFlags::Contains, 0);
			}
		}

		if (ExpectedStackFrameContains != nullptr && ExpectedStackFrameContains[0] != TEXT('\0'))
		{
			Test.AddExpectedErrorPlain(ExpectedStackFrameContains, EAutomationExpectedErrorFlags::Contains, 0);
		}
		else if (const ANSICHAR* DeclarationAnsi = Function.GetDeclaration(true, false, false, true))
		{
			if (DeclarationAnsi[0] != '\0')
			{
				Test.AddExpectedErrorPlain(
					FString::Printf(TEXT("%s | Line"), UTF8_TO_TCHAR(DeclarationAnsi)),
					EAutomationExpectedErrorFlags::Contains,
					0);
			}
		}

		if (ExpectedOuterStackFrameContains != nullptr && ExpectedOuterStackFrameContains[0] != TEXT('\0'))
		{
			Test.AddExpectedErrorPlain(ExpectedOuterStackFrameContains, EAutomationExpectedErrorFlags::Contains, 0);
		}

		Test.AddExpectedError(ExpectedExceptionContains, EAutomationExpectedErrorFlags::Contains, 0);

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create execution context"), ContextLabel), Context))
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(&Function);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		const char* ExceptionStringAnsi = Context->GetExceptionString();
		const FString ExceptionString = UTF8_TO_TCHAR(ExceptionStringAnsi != nullptr ? ExceptionStringAnsi : "");
		const int32 ExceptionLine = Context->GetExceptionLineNumber();

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should raise asEXECUTION_EXCEPTION"), ContextLabel),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_EXCEPTION));
		bPassed &= Test.TestFalse(
			*FString::Printf(TEXT("%s should expose a non-empty exception string"), ContextLabel),
			ExceptionString.IsEmpty());
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s exception '%s' should contain '%s'"), ContextLabel, *ExceptionString, ExpectedExceptionContains),
			ExceptionString.Contains(ExpectedExceptionContains));
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should report a positive exception line (got=%d)"), ContextLabel, ExceptionLine),
			ExceptionLine > 0);

		return bPassed;
	}

	inline bool ExecuteInt64Function(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, int64& OutValue)
	{
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Failed to create Angelscript execution context"));
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (PrepareResult != asSUCCESS)
		{
			Test.AddError(FString::Printf(TEXT("Failed to prepare int64 function (code %d)"), PrepareResult));
		}
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			Test.AddError(FString::Printf(TEXT("Failed to execute int64 function (code %d)"), ExecuteResult));
		}

		if (PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int64>(Context->GetReturnQWord());
		}

		Context->Release();
		return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
	}

	enum class ETestEngineMode : uint8
	{
		SharedClone,
		IsolatedFull,
		ProductionLike,
	};

	struct FAngelscriptTestFixture
	{
		FAngelscriptTestFixture(FAutomationTestBase& InTest, ETestEngineMode InMode = ETestEngineMode::SharedClone)
			: Test(InTest)
			, Mode(InMode)
		{
			switch (Mode)
			{
			case ETestEngineMode::SharedClone:
			{
				FAngelscriptEngine& SharedEngine = AcquireCleanSharedCloneEngine();
				Engine = &SharedEngine;
				EngineScope = MakeUnique<FAngelscriptEngineScope>(SharedEngine);
				break;
			}
			case ETestEngineMode::IsolatedFull:
			{
				OwnedEngine = CreateIsolatedFullEngine();
				if (OwnedEngine.IsValid())
				{
					Engine = OwnedEngine.Get();
					EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
				}
				break;
			}
			case ETestEngineMode::ProductionLike:
			{
				FResolvedProductionLikeEngine Resolved;
				if (AcquireProductionLikeEngine(InTest, TEXT("FAngelscriptTestFixture failed to acquire production-like engine"), Resolved))
				{
					OwnedEngine = MoveTemp(Resolved.OwnedEngine);
					EngineScope = MoveTemp(Resolved.EngineScope);
					Engine = Resolved.Engine;
				}
				break;
			}
			}
		}

		~FAngelscriptTestFixture()
		{
			EngineScope.Reset();
		}

		FAngelscriptTestFixture(const FAngelscriptTestFixture&) = delete;
		FAngelscriptTestFixture& operator=(const FAngelscriptTestFixture&) = delete;

		bool IsValid() const { return Engine != nullptr; }
		FAngelscriptEngine& GetEngine() const { check(Engine != nullptr); return *Engine; }
		FAutomationTestBase& GetTest() const { return Test; }

		asIScriptModule* BuildModule(const char* ModuleName, const FString& Source)
		{
			check(Engine != nullptr);
			return AngelscriptTestSupport::BuildModule(Test, *Engine, ModuleName, Source);
		}

		bool ExecuteInt(asIScriptFunction& Function, int32& OutResult)
		{
			check(Engine != nullptr);
			return AngelscriptTestSupport::ExecuteIntFunction(Test, *Engine, Function, OutResult);
		}

		bool ExecuteInt64(asIScriptFunction& Function, int64& OutResult)
		{
			check(Engine != nullptr);
			return AngelscriptTestSupport::ExecuteInt64Function(Test, *Engine, Function, OutResult);
		}

	private:
		FAutomationTestBase& Test;
		ETestEngineMode Mode;
		FAngelscriptEngine* Engine = nullptr;
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		TUniquePtr<FAngelscriptEngineScope> EngineScope;
	};

}
