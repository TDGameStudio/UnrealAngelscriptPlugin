#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFTimespanBinds
{
	static void ConstructTicks(FTimespan* Address, int64 Ticks);
	static void ConstructTime(FTimespan* Address, int32 Hours, int32 Minutes, int32 Seconds);
	static void ConstructDays(FTimespan* Address, int32 Days, int32 Hours, int32 Minutes, int32 Seconds);
	static void ConstructNanoseconds(FTimespan* Address, int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano);
	static int32 Compare(const FTimespan& Timespan, const FTimespan& Other);
	static FString ToStringFormat(FTimespan* Timespan, FString Format);
};
