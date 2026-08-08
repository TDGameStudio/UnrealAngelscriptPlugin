#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFIntVector2Binds
{
	static void ConstructXY(FIntVector2* Address, int32 X, int32 Y);
	static void ConstructZero(FIntVector2* Address);
	static void ConstructScalar(FIntVector2* Address, int32 Scalar);
	static void ConstructCopy(FIntVector2* Address, const FIntVector2& Other);
	static void AppendToString(void* Ptr, FString& Str);
};
