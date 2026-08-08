#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFPlaneBinds
{
	static void ConstructFromLocationAndNormal(FPlane* Address, const FVector& Location, const FVector& Normal);
	static void ConstructFromPoints(FPlane* Address, const FVector& PointA, const FVector& PointB, const FVector& PointC);
	static void ConstructFromPlane4f(FPlane* Address, const FPlane4f& Plane);
	static FVector RayPlaneIntersection(const FPlane& Plane, const FVector& RayOrigin, const FVector& RayDirection);
	static bool SegmentPlaneIntersection(const FPlane& Plane, const FVector& StartPoint, const FVector& EndPoint, FVector& OutIntersectionPoint);
};
