#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FLinearColorType : TAngelscriptBaseStructType<FLinearColor>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFLinearColorBinds
{
	static void ConstructDefault(FLinearColor* Address);
	static void ConstructRGBA(FLinearColor* Address, float R, float G, float B, float A);
	static void ConstructCopy(FLinearColor* Address, const FLinearColor& Other);
	static FLinearColor* Assign(FLinearColor* Color, const FLinearColor& Other);
	static FLinearColor MakeFromHex(uint32 HexColor, bool bSRGB);
	static void ConstructFromVector(FLinearColor* Address, const FVector& Other, float A);
	static void ConstructFromColor(FLinearColor* Address, const FColor& Other);
	static void AppendToString(void* Address, FString& OutString);
};
