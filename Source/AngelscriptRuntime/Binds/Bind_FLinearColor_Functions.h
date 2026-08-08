#pragma once

#include "CoreMinimal.h"

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
