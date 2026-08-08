#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFDateTimeBinds
{
	static void Construct(
		FDateTime* Address,
		int32 Year,
		int32 Month,
		int32 Day,
		int32 Hour,
		int32 Minute,
		int32 Second,
		int32 Millisecond);
	static void AppendToString(void* Ptr, FString& Str);
	static FString ToStringFormat(const FDateTime& DateTime, const FString& Format);
	static int32 Compare(const FDateTime& DateTime, const FDateTime& Other);
	static bool ParseIso8601(const FString& DateTimeString, FDateTime& OutDateTime);
};
