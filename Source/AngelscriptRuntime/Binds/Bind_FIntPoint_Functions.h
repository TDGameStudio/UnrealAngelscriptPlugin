#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFIntPointBinds
{
	static void ConstructXY(FIntPoint* Address, int32 X, int32 Y);
	static void ConstructZero(FIntPoint* Address);
	static void ConstructScalar(FIntPoint* Address, int32 Scalar);
	static void ConstructCopy(FIntPoint* Address, const FIntPoint& Other);
	static FIntPoint Negate(FIntPoint* Point);
	static void AppendToString(void* Address, FString& String);
};
