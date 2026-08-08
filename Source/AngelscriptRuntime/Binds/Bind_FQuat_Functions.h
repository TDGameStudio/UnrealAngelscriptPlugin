#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFQuatBinds
{
	static void ConstructDefault(FQuat* Address);
	static void ConstructCopy(FQuat* Address, const FQuat& Quat);
	static void ConstructComponents(FQuat* Address, double X, double Y, double Z, double W);
	static void ConstructFromRotator(FQuat* Address, const FRotator& Rotator);
	static void ConstructAxisAngle(FQuat* Address, FVector Axis, double AngleRadians);
	static void ConstructFromQuat4f(FQuat* Address, const FQuat4f& Quat);
	static void AppendToString(void* Address, FString& OutString);
};
