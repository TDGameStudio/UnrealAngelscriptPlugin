#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFVector4Binds
{
	static void Construct(FVector4* Address, double X, double Y, double Z, double W);
	static void ConstructZero(FVector4* Address);
	static void ConstructCopy(FVector4* Address, const FVector4& Other);
	static void ConstructFromVector(FVector4* Address, FVector InVector, double InW);
	static void ConstructFromVector4f(FVector4* Address, const FVector4f& Other);
	static void AppendToString(void* Ptr, FString& Str);
};
