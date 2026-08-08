#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFBoxSphereBounds3fBinds
{
	static void ConstructDefault(FBoxSphereBounds3f* Address);
	static void ConstructOriginExtentRadius(FBoxSphereBounds3f* Address, const FVector3f& Origin, const FVector3f& BoxExtent, float SphereRadius);
	static void ConstructFromBounds(FBoxSphereBounds3f* Address, const FBoxSphereBounds& Bounds);
	static void ConstructBoxSphere(FBoxSphereBounds3f* Address, const FBox3f& Box, const FSphere3f& Sphere);
	static void ConstructFromBox(FBoxSphereBounds3f* Address, const FBox3f& Box);
	static void ConstructFromSphere(FBoxSphereBounds3f* Address, const FSphere3f& Sphere);
	static void ConstructFromPoints(FBoxSphereBounds3f* Address, TArray<FVector3f>& Points);
	static void AppendToString(void* Ptr, FString& Str);
};
