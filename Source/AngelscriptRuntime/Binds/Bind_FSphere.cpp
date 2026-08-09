#include "Bind_FSphere.h"

#include "AngelscriptBinds.h"


/**
 * FSphere construction, fields, operators, and geometry.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Sphere();                                                                                    | Constructs a zero-initialized sphere.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Sphere(FVector InV, float32 InW);                                                            | Constructs a sphere from center and radius.                                                                      |
 * |                                                                                                      | @param InV Center in Unreal units.                                                                               |
 * |                                                                                                      | @param InW Radius in Unreal units.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Sphere(const FSphere& Sphere);                                                               | Copy-constructs a sphere.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Sphere(const FSphere3f& Sphere);                                                             | Converts a single-precision sphere.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Sphere(const TArray<FVector>& Points);                                                       | Constructs the bounding sphere of the supplied points.                                                           |
 * |                                                                                                      | @param Points World-space positions to enclose.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Sphere.W;                                                                                    | Exposes the sphere radius.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Sphere.Center;                                                                               | Exposes the sphere center.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Combined = Left + Right;                                                                     | Returns the sphere enclosing both operands.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left += Right;                                                                                       | Expands the left sphere to enclose the right sphere.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Sphere.Equals(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const;              | Compares sphere center and radius within a tolerance.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Sphere.IsInside(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const;            | Reports whether this sphere lies inside the other sphere.                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Sphere.IsInside(const FVector& Point, float64 Tolerance = KINDA_SMALL_NUMBER) const;            | Reports whether the point lies inside this sphere.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Sphere.Intersects(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const;          | Reports whether the spheres intersect.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere Sphere.TransformBy(const FTransform& M) const;                                               | Transforms the sphere.                                                                                           |
 * |                                                                                                      | @param M Transform applied to the center and radius.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Sphere.GetVolume() const;                                                                    | Returns the sphere volume.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere_Type(
	TEXT("FSphere.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FSphere>("FSphere", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FSphereType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere(
	TEXT("FSphere.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FSphere_ = Binds.ExistingClassForTarget("FSphere");
		FSphere_.Constructor("void f()", &FAngelscriptFSphereBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FSphere", true, "ForceInit");
		FSphere_.Constructor(
			"void f(FVector InV, float32 InW)",
			&FAngelscriptFSphereBinds::ConstructCenterRadius,
			"FSphere",
			true)
			.NoDiscard();
		FSphere_.Constructor(
			"void f(const FSphere& Sphere)",
			&FAngelscriptFSphereBinds::ConstructCopy,
			"FSphere",
			true)
			.NoDiscard();
		FSphere_.Constructor(
			"void f(const FSphere3f& Sphere)",
			&FAngelscriptFSphereBinds::ConstructFromSphere3f,
			"FSphere",
			true)
			.NoDiscard();
		FSphere_.Constructor(
			"void f(const TArray<FVector>& Points)",
			&FAngelscriptFSphereBinds::ConstructFromPoints)
			.NoDiscard();
		FSphere_.Property("float64 W", &FSphere::W);
		FSphere_.Property("FVector Center", &FSphere::Center);
		FSphere_.Method("FSphere opAdd(const FSphere& Other) const", METHODPR_TRIVIAL(FSphere, FSphere, operator+, (const FSphere&) const));
		FSphere_.Method("FSphere& opAddAssign(const FSphere& Other)", METHODPR_TRIVIAL(FSphere&, FSphere, operator+=, (const FSphere&)));
		FSphere_.Method("bool Equals(const FSphere& Sphere, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, Equals, (const FSphere&, double) const));
		FSphere_.Method("bool IsInside(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, IsInside, (const FSphere&, double) const));
		FSphere_.Method("bool IsInside(const FVector& In, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, IsInside, (const FVector&, double) const));
		FSphere_.Method("bool Intersects(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, Intersects, (const FSphere&, double) const));
		FSphere_.Method("FSphere TransformBy( const FTransform& M ) const", METHODPR_TRIVIAL(FSphere, FSphere, TransformBy, (const FTransform&) const));
		FSphere_.Method("float32 GetVolume() const", METHOD_TRIVIAL(FSphere, GetVolume));
	});
