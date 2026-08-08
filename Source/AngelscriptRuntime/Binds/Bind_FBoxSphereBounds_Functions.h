#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFBoxSphereBoundsBinds
{
	static void ConstructDefault(FBoxSphereBounds* Address);
	static void ConstructOriginExtentRadius(FBoxSphereBounds* Address, const FVector& Origin, const FVector& BoxExtent, double SphereRadius);
	static void ConstructBoxSphere(FBoxSphereBounds* Address, const FBox& Box, const FSphere& Sphere);
	static void ConstructFromBounds3f(FBoxSphereBounds* Address, const FBoxSphereBounds3f& Bounds);
	static void ConstructFromBox(FBoxSphereBounds* Address, const FBox& Box);
	static void ConstructFromSphere(FBoxSphereBounds* Address, const FSphere& Sphere);
	static void ConstructFromPoints(FBoxSphereBounds* Address, TArray<FVector>& Points);
	static void AppendToString(void* Ptr, FString& Str);
};
