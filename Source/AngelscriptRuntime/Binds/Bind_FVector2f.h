#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FVector2fType : TAngelscriptVariantStructType<FVector2f>
{
	FString GetAngelscriptTypeName() const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFVector2fBinds
{
	static void Construct(FVector2f* Address, float X, float Y);
	static void ConstructZero(FVector2f* Address);
	static void ConstructCopy(FVector2f* Address, const FVector2f& Other);
	static void ConstructFromVector3f(FVector2f* Address, const FVector3f& Other);
	static void ConstructFromVector2D(FVector2f* Address, const FVector2D& Other);
	static FVector2f GetClampedToMaxSize(FVector2f& Vector, float MaxSize);
	static void AppendToString(void* Ptr, FString& Str);
};
