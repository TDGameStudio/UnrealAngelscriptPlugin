#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FVectorType : TAngelscriptBaseStructType<FVector>
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

struct FAngelscriptFVectorBinds
{
	static void ConstructXYZ(FVector* Address, double X, double Y, double Z);
	static void ConstructZero(FVector* Address);
	static void ConstructScalar(FVector* Address, double Scalar);
	static void ConstructCopy(FVector* Address, const FVector& Other);
	static void ConstructFromVector3f(FVector* Address, const FVector3f& Other);
	static void AppendToString(void* Address, FString& OutString);
};
