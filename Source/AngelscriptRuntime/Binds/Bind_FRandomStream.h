#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FRandomStreamType : TAngelscriptBaseStructType<FRandomStream>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFRandomStreamBinds
{
	static void ConstructDefault(FRandomStream* Address);
	static void ConstructIntSeed(FRandomStream* Address, int32 Seed);
	static void ConstructUIntSeed(FRandomStream* Address, uint32 Seed);
	static void AppendToString(void* Address, FString& String);
};
