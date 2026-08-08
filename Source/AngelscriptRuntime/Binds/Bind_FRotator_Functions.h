#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFRotatorBinds
{
	static void ConstructComponents(FRotator* Address, double Pitch, double Yaw, double Roll);
	static void ConstructDefault(FRotator* Address);
	static void ConstructScalar(FRotator* Address, double Value);
	static void ConstructCopy(FRotator* Address, const FRotator& Other);
	static FVector GetRightVector(const FRotator& Rotator);
	static FVector GetUpVector(const FRotator& Rotator);
	static void ConstructFromQuat(FRotator* Address, const FQuat& Quat);
	static void ConstructFromRotator3f(FRotator* Address, const FRotator3f& Rotator);
	static FString ToColorString(const FRotator& Rotator);
	static void AppendToString(void* Address, FString& OutString);
};
