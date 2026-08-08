#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFColorBinds
{
	static void ConstructRGBA(FColor* Address, uint8 R, uint8 G, uint8 B, uint8 A);
	static void ConstructPacked(FColor* Address, uint32 PackedColor);
	static void AppendToString(void* Address, FString& OutString);
};
