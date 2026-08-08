#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FBoxType : TAngelscriptCoreStructType<FBox, FGetBox>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFBoxBinds
{
	static void ConstructDefault(FBox* Address);
	static void ConstructMinMax(FBox* Address, const FVector& Min, const FVector& Max);
	static void ConstructFromBox3f(FBox* Address, const FBox3f& Box);
	static void AppendToString(void* Ptr, FString& Str);
};
