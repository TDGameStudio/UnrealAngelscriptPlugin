#include "Bind_FPlane_Functions.h"

void FAngelscriptFPlaneBinds::ConstructFromLocationAndNormal(
	FPlane* Address,
	const FVector& Location,
	const FVector& Normal)
{
	new (Address) FPlane(Location, Normal.GetSafeNormal());
}

void FAngelscriptFPlaneBinds::ConstructFromPoints(
	FPlane* Address,
	const FVector& PointA,
	const FVector& PointB,
	const FVector& PointC)
{
	new (Address) FPlane(PointA, PointB, PointC);
}

void FAngelscriptFPlaneBinds::ConstructFromPlane4f(FPlane* Address, const FPlane4f& Plane)
{
	new (Address) FPlane(Plane);
}

FVector FAngelscriptFPlaneBinds::RayPlaneIntersection(
	const FPlane& Plane,
	const FVector& RayOrigin,
	const FVector& RayDirection)
{
	return FMath::RayPlaneIntersection(RayOrigin, RayDirection, Plane);
}

bool FAngelscriptFPlaneBinds::SegmentPlaneIntersection(
	const FPlane& Plane,
	const FVector& StartPoint,
	const FVector& EndPoint,
	FVector& OutIntersectionPoint)
{
	return FMath::SegmentPlaneIntersection(StartPoint, EndPoint, Plane, OutIntersectionPoint);
}
