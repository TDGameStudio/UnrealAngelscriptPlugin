#pragma once

// ============================================================================
// AngelscriptTestEngineCleanup
// ============================================================================
//
// Themed sub-header split out of `AngelscriptTestUtilities.h` (Phase 1 of
// OpenSpec change `refactor-as-test-shared-layout-and-naming`).
//
// Responsibility:
//   - Sweep detached UASClass / UASStruct objects + enum / delegate functions
//     belonging to discarded modules so that the next GC cycle can reclaim
//     them, returning per-bucket statistics for diagnostics.
//   - Editor-only: invalidate Blueprint action database entries that may
//     pin generated UClasses/UStructs before they are unrooted.
//
// Scope guard:
//   - This is the **only** sub-header in `Shared/` allowed to depend on
//     editor-only Blueprint headers (`BlueprintActionDatabase.h`,
//     `K2Node_GetSubsystem.h`). All other Shared/* headers must stay
//     editor-include-free; consumers needing the cleanup helper from a
//     runtime context should branch on `WITH_EDITOR` themselves.
//
// Original location: AngelscriptTestUtilities.h lines 250-401 (+ the
// WITH_EDITOR include block at lines 26-30).
// ============================================================================

#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "BlueprintActionDatabase.h"
#include "K2Node_GetSubsystem.h"
#include "Subsystems/Subsystem.h"
#endif

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

