#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFPlane4fBinds
{
	static void ConstructFromLocationAndNormal(FPlane4f* Address, const FVector3f& Location, const FVector3f& Normal);
	static void ConstructFromPoints(FPlane4f* Address, const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC);
	static void ConstructFromPlane(FPlane4f* Address, const FPlane& Plane);
};
