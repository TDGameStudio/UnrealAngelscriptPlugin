#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFBox3fBinds
{
	static void ConstructDefault(FBox3f* Address);
	static void ConstructMinMax(FBox3f* Address, const FVector3f& Min, const FVector3f& Max);
	static void ConstructFromBox(FBox3f* Address, const FBox& Box);
	static void AppendToString(void* Ptr, FString& Str);
};
