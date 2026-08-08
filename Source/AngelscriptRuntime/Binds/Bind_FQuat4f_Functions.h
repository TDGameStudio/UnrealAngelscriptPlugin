#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFQuat4fBinds
{
	static void ConstructDefault(FQuat4f* Address);
	static void ConstructCopy(FQuat4f* Address, const FQuat4f& Quat);
	static void ConstructComponents(FQuat4f* Address, float X, float Y, float Z, float W);
	static void ConstructFromRotator3f(FQuat4f* Address, const FRotator3f& Rotator);
	static void ConstructAxisAngle(FQuat4f* Address, FVector3f Axis, float AngleRadians);
	static void ConstructFromQuat(FQuat4f* Address, const FQuat& Quat);
	static void AppendToString(void* Address, FString& OutString);
};
