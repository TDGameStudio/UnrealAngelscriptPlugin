#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

struct FScriptStatID
{
	TStatId StatID;

	explicit FScriptStatID(const FName& Name);
};

struct FScriptScopeCycleCounter
{
	FScopeCycleCounter Counter;

	explicit FScriptScopeCycleCounter(const FScriptStatID& StatID);
	explicit FScriptScopeCycleCounter(const UObject* Object);
};

struct FAngelscriptStatsBinds
{
	static void ConstructStatID(FScriptStatID* Counter, const FName& Name);
	static void DestructStatID(FScriptStatID* Counter);
	static void ConstructScopeFromStat(FScriptScopeCycleCounter* Counter, const FScriptStatID& Stat);
	static void ConstructScopeFromObject(FScriptScopeCycleCounter* Counter, const UObject* Object);
	static void DestructScope(FScriptScopeCycleCounter* Counter);
};
