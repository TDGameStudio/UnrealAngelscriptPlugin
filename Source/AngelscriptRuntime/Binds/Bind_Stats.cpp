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

#include "AngelscriptBinds.h"

/**
 * Stats binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FStatID;                                                                            | Declares the scoped-stat identifier value type.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FScopeCycleCounter;                                                                 | Declares the scoped cycle-counter value type.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FStatID Stat(const FName& Name);                                                           | Constructs a stat identifier; its native lifetime is managed automatically.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FScopeCycleCounter Scope(const FStatID& Stat);                                             | Begins a scoped cycle counter for a stat identifier.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FScopeCycleCounter Scope(const UObject Object);                                            | Begins a scoped cycle counter attributed to an object.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_Stats_Types(
	TEXT("Stats.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags StatFlags;
		Binds.ValueClassForTarget<FScriptStatID>("FStatID", StatFlags);

		FBindFlags CounterFlags;
		Binds.ValueClassForTarget<FScriptScopeCycleCounter>("FScopeCycleCounter", CounterFlags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_Stats(
	TEXT("Stats.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FStatID_ = Binds.ExistingClassForTarget("FStatID");
		FStatID_.Constructor("void f(const FName& Name)", &FAngelscriptStatsBinds::ConstructStatID).NoDiscard();
		FStatID_.Destructor("void f()", &FAngelscriptStatsBinds::DestructStatID);

		auto FScopeCycleCounter_ = Binds.ExistingClassForTarget("FScopeCycleCounter");
		FScopeCycleCounter_.Constructor(
			"void f(const FStatID& Stat)",
			&FAngelscriptStatsBinds::ConstructScopeFromStat)
			.NoDiscard();
		FScopeCycleCounter_.Constructor(
			"void f(const UObject Object)",
			&FAngelscriptStatsBinds::ConstructScopeFromObject)
			.NoDiscard();
		FScopeCycleCounter_.Destructor("void f()", &FAngelscriptStatsBinds::DestructScope);
	});

#include "AngelscriptPerformanceStats.h"

#if STATS || ENABLE_STATNAMEDEVENTS
FScriptStatID::FScriptStatID(const FName& Name)
{
	FString NameStr = Name.ToString();

#if STATS
	StatID = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_Angelscript>(NameStr);
#else
	const auto& ConversionData = StringCast<PROFILER_CHAR>(*NameStr);
	const int32 NumStorageChars = ConversionData.Length() + 1;

	auto* StoragePtr = new PROFILER_CHAR[NumStorageChars];
	FMemory::Memcpy(StoragePtr, ConversionData.Get(), NumStorageChars * sizeof(PROFILER_CHAR));
	StatID = TStatId(StoragePtr);
#endif
}
#else
FScriptStatID::FScriptStatID(const FName& Name)
{
}
#endif

FScriptScopeCycleCounter::FScriptScopeCycleCounter(const FScriptStatID& StatID)
	: Counter(StatID.StatID)
{
}

FScriptScopeCycleCounter::FScriptScopeCycleCounter(const UObject* Object)
	: Counter(Object != nullptr ? Object->GetStatID() : TStatId())
{
}

void FAngelscriptStatsBinds::ConstructStatID(FScriptStatID* Counter, const FName& Name)
{
	new (Counter) FScriptStatID(Name);
}

void FAngelscriptStatsBinds::DestructStatID(FScriptStatID* Counter)
{
	Counter->~FScriptStatID();
}

void FAngelscriptStatsBinds::ConstructScopeFromStat(FScriptScopeCycleCounter* Counter, const FScriptStatID& Stat)
{
	new (Counter) FScriptScopeCycleCounter(Stat);
}

void FAngelscriptStatsBinds::ConstructScopeFromObject(FScriptScopeCycleCounter* Counter, const UObject* Object)
{
	new (Counter) FScriptScopeCycleCounter(Object);
}

void FAngelscriptStatsBinds::DestructScope(FScriptScopeCycleCounter* Counter)
{
	Counter->~FScriptScopeCycleCounter();
}
