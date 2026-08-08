#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FVector4fType : TAngelscriptVariantStructType<FVector4f>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFVector4fBinds
{
	static void Construct(FVector4f* Address, float X, float Y, float Z, float W);
	static void ConstructZero(FVector4f* Address);
	static void ConstructCopy(FVector4f* Address, const FVector4f& Other);
	static void ConstructFromVector3f(FVector4f* Address, FVector3f InVector, float InW);
	static void ConstructFromVector4(FVector4f* Address, const FVector4& Other);
	static void AppendToString(void* Ptr, FString& Str);
};
