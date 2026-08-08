#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFSphereBinds
{
	static void ConstructDefault(FSphere* Address);
	static void ConstructCenterRadius(FSphere* Address, FVector Center, float Radius);
	static void ConstructCopy(FSphere* Address, const FSphere& Sphere);
	static void ConstructFromSphere3f(FSphere* Address, const FSphere3f& Sphere);
	static void ConstructFromPoints(FSphere* Address, TArray<FVector>& Points);
};
