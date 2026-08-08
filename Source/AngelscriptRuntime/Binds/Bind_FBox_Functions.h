#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFBoxBinds
{
	static void ConstructDefault(FBox* Address);
	static void ConstructMinMax(FBox* Address, const FVector& Min, const FVector& Max);
	static void ConstructFromBox3f(FBox* Address, const FBox3f& Box);
	static void AppendToString(void* Ptr, FString& Str);
};
