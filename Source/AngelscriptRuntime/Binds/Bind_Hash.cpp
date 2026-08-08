#include "Bind_Hash.h"

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

namespace
{
	void BindHash(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Hash(
	TEXT("Hash"),
	EAngelscriptBindPhase::ManualBindings,
	&BindHash);
