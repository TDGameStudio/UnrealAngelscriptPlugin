#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFGuidBinds
{
	static void ConstructParts(FGuid* Address, uint32 A, uint32 B, uint32 C, uint32 D);
	static void ConstructString(FGuid* Address, const FString& GuidString);
	static bool Equals(const FGuid& Guid, const FGuid& Other);
	static int Compare(const FGuid& Guid, const FGuid& Other);
	static FString ToString(const FGuid& Guid);
	static uint32 Hash(const FGuid& Guid);
};
