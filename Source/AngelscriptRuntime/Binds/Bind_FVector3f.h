#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FVector3fType : TAngelscriptVariantStructType<FVector3f>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFVector3fBinds
{
	static void ConstructXYZ(FVector3f* Address, float X, float Y, float Z);
	static void ConstructZero(FVector3f* Address);
	static void ConstructScalar(FVector3f* Address, float Scalar);
	static void ConstructCopy(FVector3f* Address, const FVector3f& Other);
	static void ConstructFromVector(FVector3f* Address, const FVector& Other);
	static void AppendToString(void* Address, FString& OutString);
};
