#include "Bind_FBox.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"


/**
 * FBox construction, fields, operators, geometry, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box();                                                                                          | Constructs an invalid force-initialized box.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box(const FVector& InMin, const FVector& InMax);                                                | Constructs a box from minimum and maximum corners.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box(const FBox3f& Box);                                                                         | Converts a single-precision box.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Box.Min;                                                                                     | Exposes the minimum corner.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Box.Max;                                                                                     | Exposes the maximum corner.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Union = Left + Right;                                                                           | Returns the union of two boxes.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left += Right;                                                                                       | Expands the left box to include another box.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares box bounds exactly.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Expanded = Box + Point;                                                                         | Expands a copy to include a point.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Box += Point;                                                                                        | Expands the box in place to include a point.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector& Corner = Box[Index];                                                                        | Returns Min for index 0 or Max for index 1.                                                                      |
 * |                                                                                                      | @param Index Corner index; native bounds behavior applies.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Box.GetCenter() const;                                                                       | Returns the box center.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Box.GetExtent() const;                                                                       | Returns box half extents.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Box.GetVolume() const;                                                                       | Returns box volume.                                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Box.GetCenterAndExtents(FVector& Center, FVector& Extents) const;                               | Returns center and half extents.                                                                                 |
 * |                                                                                                      | @param Center Receives the center.                                                                               |
 * |                                                                                                      | @param Extents Receives the half extents.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Box.GetClosestPointTo(const FVector& In) const;                                              | Returns the closest point on or in the box.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.InverseTransformBy(const FTransform& M) const;                                              | Applies the inverse transform to the box.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.TransformBy(const FTransform& M) const;                                                     | Transforms the box.                                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.Equals(const FBox& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const;                    | Compares bounds within a tolerance.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.Intersect(const FBox& Other) const;                                                         | Reports three-dimensional overlap.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IntersectXY(const FBox& Other) const;                                                       | Reports overlap in X and Y.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.Overlap(const FBox& Other) const;                                                           | Returns the overlapping box.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.ExpandBy(float64 W) const;                                                                  | Expands all axes by a scalar amount.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.ExpandBy(const FVector& V) const;                                                           | Expands each axis by the supplied amount.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.ShiftBy(const FVector& Offset) const;                                                       | Translates the box.                                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox Box.MoveTo(const FVector& Destination) const;                                                   | Moves the box center to a destination.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IsInside(const FVector& In) const;                                                          | Reports strict point containment.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IsInsideOrOn(const FVector& In) const;                                                      | Reports inclusive point containment.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IsInside(const FBox& In) const;                                                             | Reports strict box containment.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IsInsideXY(const FVector& In) const;                                                        | Reports strict point containment in X and Y.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IsInsideOrOnXY(const FVector& In) const;                                                    | Reports inclusive point containment in X and Y.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Box.IsInsideXY(const FBox& In) const;                                                           | Reports box containment in X and Y.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox FBox::BuildAABB(const FVector& Origin, const FVector& Extent);                                  | Builds an axis-aligned box from center and half extents.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Box}";                                                                             | Formats the box through the shared string formatter contribution.                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FBox_Type(
	TEXT("FBox.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FBox>("FBox", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FBoxType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FBox_ToStringContribution(
	TEXT("FBox.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FBox"), &FAngelscriptFBoxBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FBox(
	TEXT("FBox.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FBox_ = Binds.ExistingClassForTarget("FBox");
		FBox_.Constructor("void f()", &FAngelscriptFBoxBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FBox", true, "ForceInit");
		FBox_.Constructor(
			"void f(const FVector& InMin, const FVector& InMax)",
			&FAngelscriptFBoxBinds::ConstructMinMax,
			"FBox",
			true)
			.NoDiscard();
		FBox_.Constructor("void f(const FBox3f& Box)", &FAngelscriptFBoxBinds::ConstructFromBox3f, "FBox", true).NoDiscard();
		FBox_.Property("FVector Min", &FBox::Min);
		FBox_.Property("FVector Max", &FBox::Max);
		FBox_.Method("FBox opAdd(const FBox& Other) const", METHODPR_TRIVIAL(FBox, FBox, operator+, (const FBox&) const));
		FBox_.Method("FBox& opAddAssign(const FBox& Other)", METHODPR_TRIVIAL(FBox&, FBox, operator+=, (const FBox&)));
		FBox_.Method("bool opEquals(const FBox& Other) const", METHODPR_TRIVIAL(bool, FBox, operator==, (const FBox&) const));
		FBox_.Method("FBox opAdd(const FVector& Other) const", METHODPR_TRIVIAL(FBox, FBox, operator+, (const FVector&) const));
		FBox_.Method("FBox& opAddAssign(const FVector& Other)", METHODPR_TRIVIAL(FBox&, FBox, operator+=, (const FVector&)));
		FBox_.Method("FVector& opIndex(int32 Index)", METHODPR_TRIVIAL(FVector&, FBox, operator[], (int32)));
		FBox_.Method("FVector GetCenter() const", METHOD_TRIVIAL(FBox, GetCenter));
		FBox_.Method("FVector GetExtent() const", METHOD_TRIVIAL(FBox, GetExtent));
		FBox_.Method("float64 GetVolume() const", METHOD_TRIVIAL(FBox, GetVolume));
		FBox_.Method("void GetCenterAndExtents(FVector& Center, FVector& Extents) const", METHOD_TRIVIAL(FBox, GetCenterAndExtents));
		FBox_.Method("FVector GetClosestPointTo( const FVector& In ) const", METHOD_TRIVIAL(FBox, GetClosestPointTo));
		FBox_.Method("FBox InverseTransformBy( const FTransform& M ) const", METHOD_TRIVIAL(FBox, InverseTransformBy));
		FBox_.Method("FBox TransformBy( const FTransform& M ) const", METHODPR_TRIVIAL(FBox, FBox, TransformBy, (const FTransform&) const));
		FBox_.Method("bool Equals(const FBox& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FBox, Equals));
		FBox_.Method("bool Intersect(const FBox& Other) const", METHOD_TRIVIAL(FBox, Intersect));
		FBox_.Method("bool IntersectXY(const FBox& Other) const", METHOD_TRIVIAL(FBox, IntersectXY));
		FBox_.Method("FBox Overlap(const FBox& Other) const", METHOD_TRIVIAL(FBox, Overlap));
		FBox_.Method("FBox ExpandBy(float64 W) const", METHODPR_TRIVIAL(FBox, FBox, ExpandBy, (double) const));
		FBox_.Method("FBox ExpandBy(const FVector& V) const", METHODPR_TRIVIAL(FBox, FBox, ExpandBy, (const FVector&) const));
		FBox_.Method("FBox ShiftBy(const FVector& Offset) const", METHODPR_TRIVIAL(FBox, FBox, ShiftBy, (const FVector&) const));
		FBox_.Method("FBox MoveTo(const FVector& Destination) const", METHODPR_TRIVIAL(FBox, FBox, MoveTo, (const FVector&) const));
		FBox_.Method("bool IsInside( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInside, (const FVector&) const));
		FBox_.Method("bool IsInsideOrOn( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideOrOn, (const FVector&) const));
		FBox_.Method("bool IsInside( const FBox& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInside, (const FBox&) const));
		FBox_.Method("bool IsInsideXY( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideXY, (const FVector&) const));
		FBox_.Method("bool IsInsideOrOnXY( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideOrOnXY, (const FVector&) const));
		FBox_.Method("bool IsInsideXY( const FBox& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideXY, (const FBox&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FBox");
		Binds.BindGlobalFunctionForTarget("FBox BuildAABB( const FVector& Origin, const FVector& Extent) no_discard", FUNC_TRIVIAL(FBox::BuildAABB));
	});
