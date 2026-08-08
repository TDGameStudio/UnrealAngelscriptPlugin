#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFSphere3fBinds
{
	static void ConstructDefault(FSphere3f* Address);
	static void ConstructCenterRadius(FSphere3f* Address, FVector3f Center, float Radius);
	static void ConstructCopy(FSphere3f* Address, const FSphere3f& Sphere);
	static void ConstructFromSphere(FSphere3f* Address, const FSphere& Sphere);
	static void ConstructFromPoints(FSphere3f* Address, TArray<FVector3f>& Points);
};
