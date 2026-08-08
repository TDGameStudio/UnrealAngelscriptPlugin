#include "Bind_Stats_Functions.h"

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
