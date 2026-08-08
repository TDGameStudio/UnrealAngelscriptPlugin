#include "Bind_FDateTime_Functions.h"

void FAngelscriptFDateTimeBinds::Construct(
	FDateTime* Address,
	const int32 Year,
	const int32 Month,
	const int32 Day,
	const int32 Hour,
	const int32 Minute,
	const int32 Second,
	const int32 Millisecond)
{
	new (Address) FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}

void FAngelscriptFDateTimeBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FDateTime*>(Ptr)->ToString();
}

FString FAngelscriptFDateTimeBinds::ToStringFormat(const FDateTime& DateTime, const FString& Format)
{
	return DateTime.ToString(*Format);
}

int32 FAngelscriptFDateTimeBinds::Compare(const FDateTime& DateTime, const FDateTime& Other)
{
	if (DateTime < Other)
	{
		return -1;
	}
	if (DateTime > Other)
	{
		return 1;
	}
	return 0;
}

bool FAngelscriptFDateTimeBinds::ParseIso8601(const FString& DateTimeString, FDateTime& OutDateTime)
{
	return FDateTime::ParseIso8601(*DateTimeString, OutDateTime);
}
