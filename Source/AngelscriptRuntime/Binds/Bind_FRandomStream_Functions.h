#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFRandomStreamBinds
{
	static void ConstructDefault(FRandomStream* Address);
	static void ConstructIntSeed(FRandomStream* Address, int32 Seed);
	static void ConstructUIntSeed(FRandomStream* Address, uint32 Seed);
	static void AppendToString(void* Address, FString& String);
};
