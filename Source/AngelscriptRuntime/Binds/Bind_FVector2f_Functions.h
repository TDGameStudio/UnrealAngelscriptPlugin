#pragma once

#include "CoreMinimal.h"

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
