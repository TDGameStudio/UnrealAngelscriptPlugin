#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FVector2DType : TAngelscriptBaseStructType<FVector2D>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFVector2DBinds
{
	static void Construct(FVector2D* Address, double X, double Y);
	static void ConstructZero(FVector2D* Address);
	static void ConstructCopy(FVector2D* Address, const FVector2D& Other);
	static void ConstructFromVector2f(FVector2D* Address, const FVector2f& Other);
	static FVector2D GetClampedToMaxSize(FVector2D& Vector, double MaxSize);
	static void AppendToString(void* Ptr, FString& Str);
};
