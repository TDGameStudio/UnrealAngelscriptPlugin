#include "AngelscriptTestEngine.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Containers/StringConv.h"
#include "UObject/GarbageCollection.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

TUniquePtr<FAngelscriptEngine> FAngelscriptTestEngine::Create(
	const FAngelscriptEngineConfig& Config,
	const FAngelscriptEngineDependencies& Dependencies)
{
	// Test-only Full engines bind UE/AS types and mark the initial compile
	// gate complete, but intentionally do not scan project/plugin Script
	// roots or compile disk .as files. Test scripts are supplied explicitly
	// through in-memory helpers.
	//
	// We force the "skip initial compile" branch via FAngelscriptEngineConfig.
	// `FAngelscriptEngine::Create` dispatches on this flag (see OpenSpec
	// `refactor-as-engine-clone-removal` D8); the flag is owned by this
	// wrapper so test call sites never set it manually.
	FAngelscriptEngineConfig LocalConfig = Config;
	LocalConfig.bSkipInitialCompile = true;
	// Unit-test Engines must not populate the project's real Saved cache during
	// destructor shutdown. Tests that provide an explicit isolated root opt into
	// the production shutdown publication path.
	LocalConfig.bDisableCacheV2Persistence =
		LocalConfig.CacheV2RootOverride.IsEmpty();
	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptEngine::Create(LocalConfig, Dependencies);
	UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::Create produced engine %p"), Engine.Get());
	return Engine;
}

FAngelscriptEngine& FAngelscriptTestEngine::GetSharedEngine()
{
	TUniquePtr<FAngelscriptEngine>& SharedEngineStorage = GetSharedTestEngineStorage();
	TUniquePtr<FAngelscriptEngineScope>& SharedScopeStorage = GetSharedTestEngineScopeStorage();

	if (!SharedEngineStorage.IsValid())
	{
		SharedScopeStorage.Reset();

		FAngelscriptEngineConfig Config;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		SharedEngineStorage = FAngelscriptTestEngine::Create(Config, Dependencies);
		check(SharedEngineStorage.IsValid());

		SharedScopeStorage = MakeUnique<FAngelscriptEngineScope>(*SharedEngineStorage);
		UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::GetSharedEngine created shared engine %p with persistent scope"),
			SharedEngineStorage.Get());
	}
	else if (!SharedScopeStorage.IsValid())
	{
		SharedScopeStorage = MakeUnique<FAngelscriptEngineScope>(*SharedEngineStorage);
		UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::GetSharedEngine re-established persistent scope for shared engine %p"),
			SharedEngineStorage.Get());
	}

	check(SharedEngineStorage.IsValid());
	return *SharedEngineStorage;
}

void FAngelscriptTestEngine::DestroySharedEngine()
{
	TUniquePtr<FAngelscriptEngine>& SharedEngineStorage = GetSharedTestEngineStorage();
	TUniquePtr<FAngelscriptEngineScope>& SharedScopeStorage = GetSharedTestEngineScopeStorage();

	// Drop the persistent scope first so the engine is no longer the
	// "current" engine while it tears down.
	SharedScopeStorage.Reset();

	if (SharedEngineStorage.IsValid())
	{
		UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::DestroySharedEngine releasing engine %p"),
			SharedEngineStorage.Get());
	}
	SharedEngineStorage.Reset();
}

void FAngelscriptTestEngine::ResetModules(FAngelscriptEngine& Engine)
{
	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::ResetModules engine=%p activeModules=%d"),
		&Engine, ActiveModules.Num());

	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		Engine.DiscardModule(*Module->ModuleName);
	}

	if (asCScriptEngine* ScriptEngine = reinterpret_cast<asCScriptEngine*>(Engine.GetScriptEngine()))
	{
		TArray<FString> RemainingModuleNames;
		const asUINT ModuleCount = ScriptEngine->GetModuleCount();
		RemainingModuleNames.Reserve(static_cast<int32>(ModuleCount));
		for (asUINT ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
		{
			if (asIScriptModule* Module = ScriptEngine->GetModuleByIndex(ModuleIndex))
			{
				RemainingModuleNames.Add(UTF8_TO_TCHAR(Module->GetName()));
			}
		}

		if (RemainingModuleNames.Num() > 0)
		{
			UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::ResetModules discarding %d raw AS modules"),
				RemainingModuleNames.Num());
		}

		for (const FString& ModuleName : RemainingModuleNames)
		{
			const auto ModuleNameAnsi = StringCast<ANSICHAR>(*ModuleName);
			ScriptEngine->DiscardModule(ModuleNameAnsi.Get());
		}

		ScriptEngine->DeleteDiscardedModules();
	}

	const FDetachedASTypeCleanupResult DetachedTypeResult = CleanupDetachedASTypesForGarbageCollection(&ActiveModules);
	if (DetachedTypeResult.DetachedClassCount > 0
		|| DetachedTypeResult.DetachedStructCount > 0
		|| DetachedTypeResult.DiscardedEnumCount > 0
		|| DetachedTypeResult.DiscardedDelegateFunctionCount > 0)
	{
		UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::ResetModules cleaned %d detached UASClass (%d unrooted), %d detached UASStruct (%d unrooted), %d discarded UEnum (%d unrooted), %d discarded delegate functions (%d unrooted, %d blueprint action entries cleared)"),
			DetachedTypeResult.DetachedClassCount,
			DetachedTypeResult.RootedDetachedClassCount,
			DetachedTypeResult.DetachedStructCount,
			DetachedTypeResult.RootedDetachedStructCount,
			DetachedTypeResult.DiscardedEnumCount,
			DetachedTypeResult.RootedDiscardedEnumCount,
			DetachedTypeResult.DiscardedDelegateFunctionCount,
			DetachedTypeResult.RootedDiscardedDelegateFunctionCount,
			DetachedTypeResult.BlueprintActionCacheClearedCount);
	}
	if (DetachedTypeResult.BlueprintSubsystemNodeActionCacheClearedCount > 0)
	{
		UE_LOG(Angelscript, Verbose, TEXT("[TestEngine] FAngelscriptTestEngine::ResetModules cleared %d subsystem Blueprint node action entries after script subsystem cleanup"),
			DetachedTypeResult.BlueprintSubsystemNodeActionCacheClearedCount);
	}

	CollectGarbage(RF_NoFlags, true);
}
