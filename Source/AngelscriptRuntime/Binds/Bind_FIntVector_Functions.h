#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFIntVectorBinds
{
	static void ConstructXYZ(FIntVector* Address, int32 X, int32 Y, int32 Z);
	static void ConstructZero(FIntVector* Address);
	static void ConstructScalar(FIntVector* Address, int32 Scalar);
	static void ConstructCopy(FIntVector* Address, const FIntVector& Other);
	static FIntVector Negate(const FIntVector* Vector);
	static void AppendToString(void* Ptr, FString& Str);
};
