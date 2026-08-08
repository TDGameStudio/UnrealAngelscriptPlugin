#include "Bind_FBoxSphereBounds3f.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FBoxSphereBounds3f construction, fields, bounds operations, intersection tests, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds();                                                                         | Constructs zero-sized bounds at the origin.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds(const FVector3f& InOrigin, const FVector3f& InBoxExtent,                   | Constructs bounds from center, positive box half-extents, and sphere radius.                                     |
 * |     float32 InSphereRadius);                                                                         | @param InOrigin Center in the caller's coordinate space.                                                         |
 * |                                                                                                      | @param InBoxExtent Positive half-size on each axis.                                                              |
 * |                                                                                                      | @param InSphereRadius Radius in the same units as the origin.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds(const FBoxSphereBounds& Bounds64);                                         | Converts double-precision bounds to single precision.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds(const FBox3f& Box, const FSphere3f& Sphere);                               | Builds bounds enclosing both supplied primitives.                                                                |
 * |                                                                                                      | @param Box Axis-aligned box in the same coordinate space.                                                        |
 * |                                                                                                      | @param Sphere Sphere in the same coordinate space.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds(const FBox3f& Box);                                                        | Builds box/sphere bounds from an axis-aligned box.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds(const FSphere3f& Sphere);                                                  | Builds box/sphere bounds from a sphere.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Bounds(const TArray<FVector3f>& Points);                                          | Builds bounds enclosing all points.                                                                              |
 * |                                                                                                      | @param Points Positions expressed in one coordinate space.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Bounds.Origin;                                                                             | Exposes the bounds center.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Bounds.BoxExtent;                                                                          | Exposes positive axis-aligned box half-extents.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Bounds.SphereRadius;                                                                         | Exposes the enclosing sphere radius.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Combined = Left + Right;                                                          | Returns bounds enclosing both operands.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares origin, extents, and radius exactly.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 DistanceSquared = Bounds.ComputeSquaredDistanceFromBoxToPoint(const FVector3f& Point) const; | Returns squared distance from the box to a point, in squared coordinate units.                                   |
 * |                                                                                                      | @param Point Position in the bounds' coordinate space.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBox3f Box = Bounds.GetBox() const;                                                                  | Returns the represented axis-aligned box.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Corner = Bounds.GetBoxExtrema(uint32 Extrema) const;                                       | Returns the selected minimum or maximum box corner.                                                              |
 * |                                                                                                      | @param Extrema Selects the lower or upper extrema using Unreal's bounds convention.                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Sphere = Bounds.GetSphere() const;                                                         | Returns the represented enclosing sphere.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Expanded = Bounds.ExpandBy(float32 ExpandAmount) const;                           | Expands box extents and sphere radius by a distance.                                                             |
 * |                                                                                                      | @param ExpandAmount Signed distance in bounds units.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds3f Transformed = Bounds.TransformBy(const FTransform3f& M) const;                    | Transforms the bounds and recomputes conservative extents.                                                       |
 * |                                                                                                      | @param M Transform from the current coordinate space to the destination space.                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bIntersects = FBoxSphereBounds3f::SpheresIntersect(const FBoxSphereBounds3f& A,                 | Tests the enclosing spheres with an additive distance tolerance.                                                 |
 * |     const FBoxSphereBounds3f& B, float32 Tolerance = KINDA_SMALL_NUMBER);                            |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bIntersects = FBoxSphereBounds3f::BoxesIntersect(const FBoxSphereBounds3f& A,                   | Tests the axis-aligned boxes for overlap.                                                                        |
 * |     const FBoxSphereBounds3f& B);                                                                    |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Bounds}";                                                                          | Formats origin, box extent, and sphere radius through the shared formatter.                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */


namespace
{
	void BindFBoxSphereBounds3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FBoxSphereBounds3f>("FBoxSphereBounds3f", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FBoxSphereBounds3fType>());
	}

	void BindFBoxSphereBounds3fToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FBoxSphereBounds3f"), &FAngelscriptFBoxSphereBounds3fBinds::AppendToString);
	}

	void BindFBoxSphereBounds3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FBoxSphereBounds3f_ = Binds.ExistingClassForTarget("FBoxSphereBounds3f");
		FBoxSphereBounds3f_.Constructor("void f()", &FAngelscriptFBoxSphereBounds3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FBoxSphereBounds3f", true, "ForceInit");
		FBoxSphereBounds3f_.Constructor(
			"void f(const FVector3f& InOrigin, const FVector3f& InBoxExtent, float32 InSphereRadius)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructOriginExtentRadius,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FBoxSphereBounds& Bounds)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromBounds,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FBox3f& Box, const FSphere3f& Sphere)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructBoxSphere,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FBox3f& Box)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromBox,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FSphere3f& Sphere)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromSphere,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const TArray<FVector3f>& Points)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromPoints)
			.NoDiscard();
		FBoxSphereBounds3f_.Property("FVector3f Origin", &FBoxSphereBounds3f::Origin);
		FBoxSphereBounds3f_.Property("FVector3f BoxExtent", &FBoxSphereBounds3f::BoxExtent);
		FBoxSphereBounds3f_.Property("float32 SphereRadius", &FBoxSphereBounds3f::SphereRadius);
		FBoxSphereBounds3f_.Method("FBoxSphereBounds3f opAdd(const FBoxSphereBounds3f& Other) const", METHODPR_TRIVIAL(FBoxSphereBounds3f, FBoxSphereBounds3f, operator+, (const FBoxSphereBounds3f&) const));
		FBoxSphereBounds3f_.Method("bool opEquals(const FBoxSphereBounds3f& Other) const", METHODPR_TRIVIAL(bool, FBoxSphereBounds3f, operator==, (const FBoxSphereBounds3f&) const));
		FBoxSphereBounds3f_.Method("float32 ComputeSquaredDistanceFromBoxToPoint( const FVector3f& Point ) const", METHODPR_TRIVIAL(float, FBoxSphereBounds3f, ComputeSquaredDistanceFromBoxToPoint, (const FVector3f&) const));
		FBoxSphereBounds3f_.Method("FBox3f GetBox() const", METHOD_TRIVIAL(FBoxSphereBounds3f, GetBox));
		FBoxSphereBounds3f_.Method("FVector3f GetBoxExtrema( uint32 Extrema ) const", METHODPR_TRIVIAL(FVector3f, FBoxSphereBounds3f, GetBoxExtrema, (uint32) const));
		FBoxSphereBounds3f_.Method("FSphere3f GetSphere() const", METHOD_TRIVIAL(FBoxSphereBounds3f, GetSphere));
		FBoxSphereBounds3f_.Method("FBoxSphereBounds3f ExpandBy( float32 ExpandAmount ) const", METHODPR_TRIVIAL(FBoxSphereBounds3f, FBoxSphereBounds3f, ExpandBy, (float) const));
		FBoxSphereBounds3f_.Method("FBoxSphereBounds3f TransformBy( const FTransform3f& M ) const", METHODPR_TRIVIAL(FBoxSphereBounds3f, FBoxSphereBounds3f, TransformBy, (const FTransform3f&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FBoxSphereBounds3f");
		Binds.BindGlobalFunctionForTarget("bool SpheresIntersect(const FBoxSphereBounds3f& A, const FBoxSphereBounds3f& B, float32 Tolerance = KINDA_SMALL_NUMBER) no_discard", FUNC_TRIVIAL(FBoxSphereBounds3f::SpheresIntersect));
		Binds.BindGlobalFunctionForTarget("bool BoxesIntersect(const FBoxSphereBounds3f& A, const FBoxSphereBounds3f& B) no_discard", FUNC_TRIVIAL(FBoxSphereBounds3f::BoxesIntersect));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds3f_Type(
	TEXT("FBoxSphereBounds3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFBoxSphereBounds3fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds3f_ToStringContribution(
	TEXT("FBoxSphereBounds3f.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFBoxSphereBounds3fToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds3f(
	TEXT("FBoxSphereBounds3f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFBoxSphereBounds3fFunctions);
