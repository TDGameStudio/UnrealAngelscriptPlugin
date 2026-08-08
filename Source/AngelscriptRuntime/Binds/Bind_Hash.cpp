#include "AngelscriptBinds.h"

#include "Bind_Hash_Functions.h"

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
