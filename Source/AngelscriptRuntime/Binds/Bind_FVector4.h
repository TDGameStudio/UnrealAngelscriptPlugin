#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FVector4Type : TAngelscriptBaseStructType<FVector4>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFVector4Binds
{
	static void Construct(FVector4* Address, double X, double Y, double Z, double W);
	static void ConstructZero(FVector4* Address);
	static void ConstructCopy(FVector4* Address, const FVector4& Other);
	static void ConstructFromVector(FVector4* Address, FVector InVector, double InW);
	static void ConstructFromVector4f(FVector4* Address, const FVector4f& Other);
	static void AppendToString(void* Ptr, FString& Str);
};
