#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFVectorBinds
{
	static void ConstructXYZ(FVector* Address, double X, double Y, double Z);
	static void ConstructZero(FVector* Address);
	static void ConstructScalar(FVector* Address, double Scalar);
	static void ConstructCopy(FVector* Address, const FVector& Other);
	static void ConstructFromVector3f(FVector* Address, const FVector3f& Other);
	static void AppendToString(void* Address, FString& OutString);
};
