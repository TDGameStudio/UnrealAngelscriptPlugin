#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFIntVector4Binds
{
	static void ConstructXYZW(FIntVector4* Address, int32 X, int32 Y, int32 Z, int32 W);
	static void ConstructZero(FIntVector4* Address);
	static void ConstructScalar(FIntVector4* Address, int32 Scalar);
	static void ConstructCopy(FIntVector4* Address, const FIntVector4& Other);
	static FIntVector4 Negate(const FIntVector4* Vector);
	static void AppendToString(void* Ptr, FString& Str);
};
