#include "Bind_FTimespan_Functions.h"

void FAngelscriptFTimespanBinds::ConstructTicks(FTimespan* Address, const int64 Ticks)
{
	new (Address) FTimespan(Ticks);
}

void FAngelscriptFTimespanBinds::ConstructTime(FTimespan* Address, const int32 Hours, const int32 Minutes, const int32 Seconds)
{
	new (Address) FTimespan(Hours, Minutes, Seconds);
}

void FAngelscriptFTimespanBinds::ConstructDays(FTimespan* Address, const int32 Days, const int32 Hours, const int32 Minutes, const int32 Seconds)
{
	new (Address) FTimespan(Days, Hours, Minutes, Seconds);
}

void FAngelscriptFTimespanBinds::ConstructNanoseconds(
	FTimespan* Address,
	const int32 Days,
	const int32 Hours,
	const int32 Minutes,
	const int32 Seconds,
	const int32 FractionNano)
{
	new (Address) FTimespan(Days, Hours, Minutes, Seconds, FractionNano);
}

int32 FAngelscriptFTimespanBinds::Compare(const FTimespan& Timespan, const FTimespan& Other)
{
	if (Timespan < Other)
	{
		return -1;
	}
	if (Timespan > Other)
	{
		return 1;
	}
	return 0;
}

FString FAngelscriptFTimespanBinds::ToStringFormat(FTimespan* Timespan, FString Format)
{
	return Timespan->ToString(*Format);
}
