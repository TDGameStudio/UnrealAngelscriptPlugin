#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFRotator3fBinds
{
	static void ConstructComponents(FRotator3f* Address, float Pitch, float Yaw, float Roll);
	static void ConstructDefault(FRotator3f* Address);
	static void ConstructScalar(FRotator3f* Address, float Value);
	static void ConstructCopy(FRotator3f* Address, const FRotator3f& Other);
	static void ConstructFromQuat4f(FRotator3f* Address, const FQuat4f& Quat);
	static void ConstructFromRotator(FRotator3f* Address, const FRotator& Rotator);
	static FString ToColorString(const FRotator3f& Rotator);
	static void AppendToString(void* Address, FString& OutString);
};
