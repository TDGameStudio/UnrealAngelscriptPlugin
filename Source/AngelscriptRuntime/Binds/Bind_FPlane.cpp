#include "CoreMinimal.h"

struct FAngelscriptFPlaneBinds
{
	static void ConstructFromLocationAndNormal(FPlane* Address, const FVector& Location, const FVector& Normal);
	static void ConstructFromPoints(FPlane* Address, const FVector& PointA, const FVector& PointB, const FVector& PointC);
	static void ConstructFromPlane4f(FPlane* Address, const FPlane4f& Plane);
	static FVector RayPlaneIntersection(const FPlane& Plane, const FVector& RayOrigin, const FVector& RayDirection);
	static bool SegmentPlaneIntersection(const FPlane& Plane, const FVector& StartPoint, const FVector& EndPoint, FVector& OutIntersectionPoint);
};

#include "AngelscriptBinds.h"

/**
 * FPlane construction and geometric queries.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPlane Plane(const FVector& InLocation, const FVector& InNormal);                                    | Constructs a plane from a point and normal.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPlane Plane(const FVector& PointA, const FVector& PointB, const FVector& PointC);                   | Constructs a plane through three points.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPlane Plane(const FPlane4f& Plane);                                                                 | Converts a single-precision plane.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Plane.PlaneDot(const FVector& Location) const;                                               | Returns the signed plane equation value at the location.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Plane.GetOrigin() const;                                                                     | Returns a point on the plane.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const FVector& Plane.GetNormal() const;                                                              | Returns the plane normal.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Plane.RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection) const;     | Returns the intersection of an infinite ray and this plane; the caller must avoid parallel inputs.               |
 * |                                                                                                      | @param RayOrigin Start point of the ray.                                                                         |
 * |                                                                                                      | @param RayDirection Normalized ray direction.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Plane.SegmentPlaneIntersection(const FVector& StartPoint,                                       | Reports whether a finite segment intersects this plane.                                                          |
 * |     const FVector& EndPoint,                                                                         | @param StartPoint Segment start.                                                                                 |
 * |     FVector& OutIntersectionPoint) const;                                                            | @param EndPoint Segment end.                                                                                     |
 * |                                                                                                      | @param OutIntersectionPoint Receives the intersection when the function returns true.                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FPlane(
	TEXT("FPlane"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FPlane_ = Binds.ExistingClassForTarget("FPlane");

		FPlane_.Constructor(
			"void f(const FVector& InLocation, const FVector& InNormal)",
			&FAngelscriptFPlaneBinds::ConstructFromLocationAndNormal)
			.NoDiscard();
		FPlane_.Constructor(
			"void f(const FVector& PointA, const FVector& PointB, const FVector& PointC)",
			&FAngelscriptFPlaneBinds::ConstructFromPoints)
			.NoDiscard();
		FPlane_.Constructor(
			"void f(const FPlane4f& Plane)",
			&FAngelscriptFPlaneBinds::ConstructFromPlane4f)
			.NoDiscard();

		FPlane_.Method("float64 PlaneDot(const FVector& Location) const", METHODPR_TRIVIAL(double, FPlane, PlaneDot, (const FVector&)));
		FPlane_.Method("FVector GetOrigin() const", METHOD_TRIVIAL(FPlane, GetOrigin));
		FPlane_.Method("const FVector& GetNormal() const", METHOD_TRIVIAL(FPlane, GetNormal));

		FPlane_.Method(
			"FVector RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection) const",
			&FAngelscriptFPlaneBinds::RayPlaneIntersection)
			.Documentation(TEXT(
				"Find the intersection of a ray and a plane.  The ray has a start point with an infinite length.  Assumes that the"
				"line and plane do indeed intersect; you must make sure they're not parallel before calling."
				"@param RayOrigin\tThe start point of the ray"
				"@param RayDirection\tThe direction the ray is pointing (normalized vector)"
				"@return The point of intersection between the ray and the plane."));

		FPlane_.Method(
			"bool SegmentPlaneIntersection(const FVector& StartPoint, const FVector& EndPoint, FVector& OutIntersectionPoint) const",
			&FAngelscriptFPlaneBinds::SegmentPlaneIntersection)
			.Documentation(TEXT(
				"Returns true if there is an intersection between the segment specified by StartPoint and Endpoint, and"
				"the plane on which polygon Plane lies. If there is an intersection, the point is placed in out_IntersectionPoint"
				"@param StartPoint - start point of segment"
				"@param EndPoint   - end point of segment"
				"@param OutIntersectionPoint - the point on the segment that intersects the mesh (if any)"
				"@return true if intersection occurred"));
	});

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
