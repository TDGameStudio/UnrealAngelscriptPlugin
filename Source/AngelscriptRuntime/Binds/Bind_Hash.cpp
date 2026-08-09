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

#include "AngelscriptBinds.h"

/**
 * Hash namespace hashing helpers.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Hash::CityHash32(const FString& buf);                                                         | Computes a 32-bit CityHash for the string bytes.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Hash::CityHash32(const TArray<int8>& buf);                                                    | Computes a 32-bit CityHash for the byte array.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Hash::CityHash64(const FString& buf);                                                         | Computes a 64-bit CityHash for the string bytes.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Hash::CityHash64(const TArray<int8>& buf);                                                    | Computes a 64-bit CityHash for the byte array.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Hash::CityHash64WithSeed(const FString& buf, uint64 seed);                                    | Computes a seeded 64-bit CityHash for the string bytes.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Hash::CityHash64WithSeed(const TArray<int8>& buf, uint64 seed);                               | Computes a seeded 64-bit CityHash for the byte array.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Hash::CityHash64WithSeeds(const FString& buf, uint64 seed0, uint64 seed1);                    | Computes a 64-bit CityHash with two seeds for the string bytes.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Hash::CityHash64WithSeeds(const TArray<int8>& buf, uint64 seed0, uint64 seed1);               | Computes a 64-bit CityHash with two seeds for the byte array.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_Hash(
	TEXT("Hash"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "Hash");
		Binds.BindGlobalFunctionForTarget("uint32 CityHash32(const FString& buf)", &FAngelscriptHashBinds::CityHash32String);
		Binds.BindGlobalFunctionForTarget("uint32 CityHash32(const TArray<int8>& buf)", &FAngelscriptHashBinds::CityHash32Bytes);
		Binds.BindGlobalFunctionForTarget("uint64 CityHash64(const FString& buf)", &FAngelscriptHashBinds::CityHash64String);
		Binds.BindGlobalFunctionForTarget("uint64 CityHash64(const TArray<int8>& buf)", &FAngelscriptHashBinds::CityHash64Bytes);
		Binds.BindGlobalFunctionForTarget("uint64 CityHash64WithSeed(const FString& buf, uint64 seed)", &FAngelscriptHashBinds::CityHash64StringWithSeed);
		Binds.BindGlobalFunctionForTarget("uint64 CityHash64WithSeed(const TArray<int8>& buf, uint64 seed)", &FAngelscriptHashBinds::CityHash64BytesWithSeed);
		Binds.BindGlobalFunctionForTarget("uint64 CityHash64WithSeeds(const FString& buf, uint64 seed0, uint64 seed1)", &FAngelscriptHashBinds::CityHash64StringWithSeeds);
		Binds.BindGlobalFunctionForTarget("uint64 CityHash64WithSeeds(const TArray<int8>& buf, uint64 seed0, uint64 seed1)", &FAngelscriptHashBinds::CityHash64BytesWithSeeds);
	});

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
