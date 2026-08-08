#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FGetBox3f
{
	static UScriptStruct* Get();
};

struct FBox3fType : TAngelscriptCoreStructType<FBox3f, FGetBox3f>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFBox3fBinds
{
	static void ConstructDefault(FBox3f* Address);
	static void ConstructMinMax(FBox3f* Address, const FVector3f& Min, const FVector3f& Max);
	static void ConstructFromBox(FBox3f* Address, const FBox& Box);
	static void AppendToString(void* Ptr, FString& Str);
};
