#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFVector3fBinds
{
	static void ConstructXYZ(FVector3f* Address, float X, float Y, float Z);
	static void ConstructZero(FVector3f* Address);
	static void ConstructScalar(FVector3f* Address, float Scalar);
	static void ConstructCopy(FVector3f* Address, const FVector3f& Other);
	static void ConstructFromVector(FVector3f* Address, const FVector& Other);
	static void AppendToString(void* Address, FString& OutString);
};
