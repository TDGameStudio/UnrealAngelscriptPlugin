#include "Bind_FPlane4f.h"

void FAngelscriptFPlane4fBinds::ConstructFromLocationAndNormal(
	FPlane4f* Address,
	const FVector3f& Location,
	const FVector3f& Normal)
{
	new (Address) FPlane4f(Location, Normal);
}

void FAngelscriptFPlane4fBinds::ConstructFromPoints(
	FPlane4f* Address,
	const FVector3f& PointA,
	const FVector3f& PointB,
	const FVector3f& PointC)
{
	new (Address) FPlane4f(PointA, PointB, PointC);
}

void FAngelscriptFPlane4fBinds::ConstructFromPlane(FPlane4f* Address, const FPlane& Plane)
{
	new (Address) FPlane4f(Plane);
}
