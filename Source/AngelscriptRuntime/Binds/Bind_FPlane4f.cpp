#include "Bind_FPlane4f.h"

#include "AngelscriptBinds.h"

/**
 * FPlane4f manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FPlane4f Plane(const FVector3f& InLocation, const FVector3f& InNormal);                    | Constructs a plane from a point and a normal.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FPlane4f Plane(const FVector3f& PointA, const FVector3f& PointB,                           | Constructs a plane through three points.                                                                             |
 * |     const FVector3f& PointC);                                                              |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FPlane4f Plane(const FPlane& Plane);                                                       | Constructs a single-precision plane from an FPlane.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FPlane4f.PlaneDot(const FVector3f& Location) const;                                | Returns the signed plane equation value at the location.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FPlane4f.GetOrigin() const;                                                      | Returns a point on the plane.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FVector3f& FPlane4f.GetNormal() const;                                               | Returns the plane normal.                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FPlane4f(
	TEXT("FPlane4f"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FPlane4f_ = Binds.ExistingClassForTarget("FPlane4f");
		FPlane4f_.Constructor(
			"void f(const FVector3f& InLocation, const FVector3f& InNormal)",
			&FAngelscriptFPlane4fBinds::ConstructFromLocationAndNormal)
			.NoDiscard();
		FPlane4f_.Constructor(
			"void f(const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC)",
			&FAngelscriptFPlane4fBinds::ConstructFromPoints)
			.NoDiscard();
		FPlane4f_.Constructor(
			"void f(const FPlane& Plane)",
			&FAngelscriptFPlane4fBinds::ConstructFromPlane)
			.NoDiscard();
		FPlane4f_.Method("float32 PlaneDot(const FVector3f& Location) const", METHODPR_TRIVIAL(float, FPlane4f, PlaneDot, (const FVector3f&)));
		FPlane4f_.Method("FVector3f GetOrigin() const", METHOD_TRIVIAL(FPlane4f, GetOrigin));
		FPlane4f_.Method("const FVector3f& GetNormal() const", METHOD_TRIVIAL(FPlane4f, GetNormal));
	});
