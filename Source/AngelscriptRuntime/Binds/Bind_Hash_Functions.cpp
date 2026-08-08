#include "Bind_Hash_Functions.h"

#include "Hash/CityHash.h"

uint32 FAngelscriptHashBinds::CityHash32String(const FString& Buffer)
{
	return ::CityHash32(reinterpret_cast<const char*>(*Buffer), static_cast<uint32>(Buffer.Len() * sizeof(TCHAR)));
}

uint32 FAngelscriptHashBinds::CityHash32Bytes(const TArray<int8>& Buffer)
{
	return ::CityHash32(reinterpret_cast<const char*>(Buffer.GetData()), static_cast<uint32>(Buffer.Num()));
}

uint64 FAngelscriptHashBinds::CityHash64String(const FString& Buffer)
{
	return ::CityHash64(reinterpret_cast<const char*>(*Buffer), static_cast<uint32>(Buffer.Len() * sizeof(TCHAR)));
}

uint64 FAngelscriptHashBinds::CityHash64Bytes(const TArray<int8>& Buffer)
{
	return ::CityHash64(reinterpret_cast<const char*>(Buffer.GetData()), static_cast<uint32>(Buffer.Num()));
}

uint64 FAngelscriptHashBinds::CityHash64StringWithSeed(const FString& Buffer, uint64 Seed)
{
	return ::CityHash64WithSeed(reinterpret_cast<const char*>(*Buffer), static_cast<uint32>(Buffer.Len() * sizeof(TCHAR)), Seed);
}

uint64 FAngelscriptHashBinds::CityHash64BytesWithSeed(const TArray<int8>& Buffer, uint64 Seed)
{
	return ::CityHash64WithSeed(reinterpret_cast<const char*>(Buffer.GetData()), static_cast<uint32>(Buffer.Num()), Seed);
}

uint64 FAngelscriptHashBinds::CityHash64StringWithSeeds(const FString& Buffer, uint64 Seed0, uint64 Seed1)
{
	return ::CityHash64WithSeeds(
		reinterpret_cast<const char*>(*Buffer),
		static_cast<uint32>(Buffer.Len() * sizeof(TCHAR)),
		Seed0,
		Seed1);
}

uint64 FAngelscriptHashBinds::CityHash64BytesWithSeeds(const TArray<int8>& Buffer, uint64 Seed0, uint64 Seed1)
{
	return ::CityHash64WithSeeds(
		reinterpret_cast<const char*>(Buffer.GetData()),
		static_cast<uint32>(Buffer.Num()),
		Seed0,
		Seed1);
}
