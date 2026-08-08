#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFParseBinds
{
	static bool ValueString(const FString& Stream, const FString& Match, FString& Value);
	static bool ValueFloat(const FString& Stream, const FString& Match, float& Value);
	static bool ValueInt(const FString& Stream, const FString& Match, int& Value);
	static bool Bool(const FString& Stream, const FString& Match, bool& bValue);
};
