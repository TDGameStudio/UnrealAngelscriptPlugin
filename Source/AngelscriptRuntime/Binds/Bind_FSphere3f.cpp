#include "Bind_FSphere3f.h"

#include "AngelscriptBinds.h"

/**
 * FSphere3f construction, fields, combination, containment, intersection, and volume.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Sphere();                                                                                  | Constructs a sphere with zero center and radius.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Sphere(FVector3f InV, float32 InW);                                                        | Constructs from center and radius.                                                                               |
 * |                                                                                                      | @param InV Center in the caller's coordinate space.                                                              |
 * |                                                                                                      | @param InW Radius in the same units.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Sphere(const FSphere3f& Other);                                                            | Copy-constructs a single-precision sphere.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Sphere(const FSphere& Sphere64);                                                           | Converts a double-precision sphere.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Sphere(const TArray<FVector3f>& Points);                                                   | Constructs a sphere enclosing all points.                                                                        |
 * |                                                                                                      | @param Points Positions expressed in one coordinate space.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Sphere.W;                                                                                    | Exposes the radius.                                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Sphere.Center;                                                                             | Exposes the center.                                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSphere3f Combined = Left + Right;                                                                   | Returns a sphere enclosing both operands.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Sphere += Other;                                                                                     | Expands the sphere in place to enclose another sphere.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Sphere.Equals(const FSphere3f& Other, float32 Tolerance = KINDA_SMALL_NUMBER) const;   | Compares center and radius within a tolerance.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bInside = Sphere.IsInside(const FSphere3f& Other,                                               | Reports whether this sphere lies inside the other sphere within tolerance.                                       |
 * |     float32 Tolerance = KINDA_SMALL_NUMBER) const;                                                   |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bIntersects = Sphere.Intersects(const FSphere3f& Other,                                         | Reports whether the spheres overlap within tolerance.                                                            |
 * |     float32 Tolerance = KINDA_SMALL_NUMBER) const;                                                   |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Volume = Sphere.GetVolume() const;                                                           | Returns volume in cubic coordinate units.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */


AS_FORCE_LINK const FAngelscriptBind Bind_FSphere3f_Type(
	TEXT("FSphere3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FSphere3f>("FSphere3f", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FSphere3fType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere3f(
	TEXT("FSphere3f.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FSphere3f_ = Binds.ExistingClassForTarget("FSphere3f");
		FSphere3f_.Constructor("void f()", &FAngelscriptFSphere3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FSphere3f", true, "ForceInit");
		FSphere3f_.Constructor(
			"void f(FVector3f InV, float32 InW)",
			&FAngelscriptFSphere3fBinds::ConstructCenterRadius,
			"FSphere3f",
			true)
			.NoDiscard();
		FSphere3f_.Constructor(
			"void f(const FSphere3f& Sphere)",
			&FAngelscriptFSphere3fBinds::ConstructCopy,
			"FSphere3f",
			true)
			.NoDiscard();
		FSphere3f_.Constructor(
			"void f(const FSphere& Sphere)",
			&FAngelscriptFSphere3fBinds::ConstructFromSphere,
			"FSphere3f",
			true)
			.NoDiscard();
		FSphere3f_.Constructor(
			"void f(const TArray<FVector3f>& Points)",
			&FAngelscriptFSphere3fBinds::ConstructFromPoints)
			.NoDiscard();
		FSphere3f_.Property("float32 W", &FSphere3f::W);
		FSphere3f_.Property("FVector3f Center", &FSphere3f::Center);
		FSphere3f_.Method("FSphere3f opAdd(const FSphere3f& Other) const", METHODPR_TRIVIAL(FSphere3f, FSphere3f, operator+, (const FSphere3f&) const));
		FSphere3f_.Method("FSphere3f& opAddAssign(const FSphere3f& Other)", METHODPR_TRIVIAL(FSphere3f&, FSphere3f, operator+=, (const FSphere3f&)));
		FSphere3f_.Method("bool Equals(const FSphere3f& Sphere, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere3f, Equals, (const FSphere3f&, float) const));
		FSphere3f_.Method("bool IsInside(const FSphere3f& Other, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere3f, IsInside, (const FSphere3f&, float) const));
		FSphere3f_.Method("bool Intersects(const FSphere3f& Other, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere3f, Intersects, (const FSphere3f&, float) const));
		FSphere3f_.Method("float32 GetVolume() const", METHOD_TRIVIAL(FSphere3f, GetVolume));
	});

void FAngelscriptFSphere3fBinds::ConstructDefault(FSphere3f* Address)
{
	new (Address) FSphere3f(ForceInit);
}

void FAngelscriptFSphere3fBinds::ConstructCenterRadius(FSphere3f* Address, const FVector3f Center, const float Radius)
{
	new (Address) FSphere3f(Center, Radius);
}

void FAngelscriptFSphere3fBinds::ConstructCopy(FSphere3f* Address, const FSphere3f& Sphere)
{
	new (Address) FSphere3f(Sphere);
}

void FAngelscriptFSphere3fBinds::ConstructFromSphere(FSphere3f* Address, const FSphere& Sphere)
{
	new (Address) FSphere3f(Sphere);
}

void FAngelscriptFSphere3fBinds::ConstructFromPoints(FSphere3f* Address, TArray<FVector3f>& Points)
{
	new (Address) FSphere3f(Points.GetData(), Points.Num());
}
