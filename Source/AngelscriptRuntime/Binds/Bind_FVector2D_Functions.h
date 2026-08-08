#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFVector2DBinds
{
	static void Construct(FVector2D* Address, double X, double Y);
	static void ConstructZero(FVector2D* Address);
	static void ConstructCopy(FVector2D* Address, const FVector2D& Other);
	static void ConstructFromVector2f(FVector2D* Address, const FVector2f& Other);
	static FVector2D GetClampedToMaxSize(FVector2D& Vector, double MaxSize);
	static void AppendToString(void* Ptr, FString& Str);
};
