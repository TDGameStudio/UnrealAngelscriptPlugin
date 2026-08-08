#pragma once

#include "CoreMinimal.h"

struct FAngelscriptJsonObjectConverterBinds
{
	static bool UStructToJsonObjectString(
		const void* Data,
		int TypeId,
		FString& Result,
		int CheckFlags,
		int SkipFlags,
		int Indent,
		bool PrettyPrint);
	static bool AppendUStructToJsonObjectString(
		const void* Data,
		int TypeId,
		FString& InOutString,
		int CheckFlags,
		int SkipFlags,
		int Indent,
		bool PrettyPrint);
	static bool JsonObjectStringToUStruct(
		const FString& JsonString,
		void* Data,
		int TypeId,
		int CheckFlags,
		int SkipFlags);
};
