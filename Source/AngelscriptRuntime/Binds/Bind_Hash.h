#pragma once

#include "CoreMinimal.h"

struct FAngelscriptHashBinds
{
	static uint32 CityHash32String(const FString& Buffer);
	static uint32 CityHash32Bytes(const TArray<int8>& Buffer);
	static uint64 CityHash64String(const FString& Buffer);
	static uint64 CityHash64Bytes(const TArray<int8>& Buffer);
	static uint64 CityHash64StringWithSeed(const FString& Buffer, uint64 Seed);
	static uint64 CityHash64BytesWithSeed(const TArray<int8>& Buffer, uint64 Seed);
	static uint64 CityHash64StringWithSeeds(const FString& Buffer, uint64 Seed0, uint64 Seed1);
	static uint64 CityHash64BytesWithSeeds(const TArray<int8>& Buffer, uint64 Seed0, uint64 Seed1);
};
