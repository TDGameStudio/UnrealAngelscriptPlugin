#include "Bind_FCollisionShape.h"

#include "AngelscriptBinds.h"

#include "CollisionShape.h"

/**
 * FCollisionShape enum, construction, shape queries, and factories.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum ECollisionShape { Line, Box, Sphere, Capsule };                                                 | Identifies the collision primitive kind.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCollisionShape Shape();                                                                             | Constructs an empty line shape.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | ECollisionShape Shape.ShapeType;                                                                     | Exposes the current primitive kind.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Shape.IsLine() const;                                                                           | Reports whether the shape is a line.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Shape.IsBox() const;                                                                            | Reports whether the shape is a box.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Shape.IsCapsule() const;                                                                        | Reports whether the shape is a capsule.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Shape.IsSphere() const;                                                                         | Reports whether the shape is a sphere.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Shape.SetBox(const FVector& HalfExtent);                                                        | Sets box half extents.                                                                                           |
 * |                                                                                                      | @param HalfExtent Positive half-size along each axis in Unreal units.                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Shape.SetSphere(const float32 Radius);                                                          | Sets the sphere radius.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Shape.SetCapsule(const float32 Radius, const float32 HalfHeight);                               | Sets capsule dimensions.                                                                                         |
 * |                                                                                                      | @param Radius Capsule radius in Unreal units.                                                                    |
 * |                                                                                                      | @param HalfHeight Total capsule half height, including the hemispherical cap.                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Shape.SetCapsule(const FVector3f& Extent);                                                      | Sets capsule dimensions from an extent vector.                                                                   |
 * |                                                                                                      | @param Extent X is radius and Z is capsule half height.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Shape.IsNearlyZero() const;                                                                     | Reports whether the shape dimensions are nearly zero.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Shape.GetExtent() const;                                                                     | Returns a generic extent vector.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Shape.GetCapsuleAxisHalfLength() const;                                                      | Returns capsule cylindrical-axis half length.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Shape.GetBox() const;                                                                        | Returns box half extents.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Shape.GetSphereRadius() const;                                                               | Returns sphere radius.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Shape.GetCapsuleRadius() const;                                                              | Returns capsule radius.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Shape.GetCapsuleHalfHeight() const;                                                          | Returns capsule half height.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 FCollisionShape::MinBoxExtent();                                                             | Returns the minimum nonzero box extent.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 FCollisionShape::MinSphereRadius();                                                          | Returns the minimum nonzero sphere radius.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 FCollisionShape::MinCapsuleRadius();                                                         | Returns the minimum nonzero capsule radius.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 FCollisionShape::MinCapsuleAxisHalfHeight();                                                 | Returns the minimum capsule axis half height.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCollisionShape FCollisionShape::MakeBox(const FVector& BoxHalfExtent);                              | Creates a box shape from double-precision extents.                                                               |
 * |                                                                                                      | @param BoxHalfExtent Positive half-size along each axis in Unreal units.                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCollisionShape FCollisionShape::MakeBox(const FVector3f& BoxHalfExtent);                            | Creates a box shape from single-precision extents.                                                               |
 * |                                                                                                      | @param BoxHalfExtent Positive half-size along each axis in Unreal units.                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCollisionShape FCollisionShape::MakeSphere(const float32 SphereRadius);                             | Creates a sphere shape.                                                                                          |
 * |                                                                                                      | @param SphereRadius Radius in Unreal units.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCollisionShape FCollisionShape::MakeCapsule(const float32 CapsuleRadius,                            | Creates a capsule from radius and half height.                                                                   |
 * |     const float32 CapsuleHalfHeight);                                                                | @param CapsuleRadius Radius in Unreal units.                                                                     |
 * |                                                                                                      | @param CapsuleHalfHeight Total half height, including the cap.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCollisionShape FCollisionShape::MakeCapsule(const FVector& Extent);                                 | Creates a capsule from an extent vector.                                                                         |
 * |                                                                                                      | @param Extent X is radius and Z is capsule half height.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindCollisionShapeTypes(FAngelscriptBinds& Binds)
	{
		auto CollisionShape_ = Binds.EnumForTarget("ECollisionShape");
		CollisionShape_["Line"] = ECollisionShape::Line;
		CollisionShape_["Box"] = ECollisionShape::Box;
		CollisionShape_["Sphere"] = ECollisionShape::Sphere;
		CollisionShape_["Capsule"] = ECollisionShape::Capsule;

		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionShape>("FCollisionShape", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FCollisionShapeType>());
	}

	void BindCollisionShapeFunctions(FAngelscriptBinds& Binds)
	{
		auto FCollisionShape_ = Binds.ExistingClassForTarget("FCollisionShape");
		FCollisionShape_.Constructor(
			"void f()",
			&FAngelscriptFCollisionShapeBinds::ConstructDefault,
			"FCollisionShape",
			true)
			.NoDiscard();
		FCollisionShape_.Property("ECollisionShape ShapeType", &FCollisionShape::ShapeType);
		FCollisionShape_.Method("bool IsLine() const", METHOD_TRIVIAL(FCollisionShape, IsLine));
		FCollisionShape_.Method("bool IsBox() const", METHOD_TRIVIAL(FCollisionShape, IsBox));
		FCollisionShape_.Method("bool IsCapsule() const", METHOD_TRIVIAL(FCollisionShape, IsCapsule));
		FCollisionShape_.Method("bool IsSphere() const", METHOD_TRIVIAL(FCollisionShape, IsSphere));
		FCollisionShape_.Method("void SetBox(const FVector& HalfExtent)", &FAngelscriptFCollisionShapeBinds::SetBox);
		FCollisionShape_.Method("void SetSphere(const float32 Radius)", METHOD_TRIVIAL(FCollisionShape, SetSphere));
		FCollisionShape_.Method("void SetCapsule(const float32 Radius, const float32 HalfHeight)", METHODPR_TRIVIAL(void, FCollisionShape, SetCapsule, (const float, const float)));
		FCollisionShape_.Method("void SetCapsule(const FVector3f& Extent)", METHODPR_TRIVIAL(void, FCollisionShape, SetCapsule, (const FVector3f&)));
		FCollisionShape_.Method("bool IsNearlyZero() const", METHOD_TRIVIAL(FCollisionShape, IsNearlyZero));
		FCollisionShape_.Method("FVector GetExtent() const", METHOD_TRIVIAL(FCollisionShape, GetExtent));
		FCollisionShape_.Method("float32 GetCapsuleAxisHalfLength() const", METHOD_TRIVIAL(FCollisionShape, GetCapsuleAxisHalfLength));
		FCollisionShape_.Method("FVector GetBox() const", METHOD_TRIVIAL(FCollisionShape, GetBox));
		FCollisionShape_.Method("float32 GetSphereRadius() const", METHOD_TRIVIAL(FCollisionShape, GetSphereRadius));
		FCollisionShape_.Method("float32 GetCapsuleRadius() const", METHOD_TRIVIAL(FCollisionShape, GetCapsuleRadius));
		FCollisionShape_.Method("float32 GetCapsuleHalfHeight() const", METHOD_TRIVIAL(FCollisionShape, GetCapsuleHalfHeight));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionShape");
		Binds.BindGlobalFunctionForTarget("float32 MinBoxExtent() no_discard", FUNC_TRIVIAL(FCollisionShape::MinBoxExtent));
		Binds.BindGlobalFunctionForTarget("float32 MinSphereRadius() no_discard", FUNC_TRIVIAL(FCollisionShape::MinSphereRadius));
		Binds.BindGlobalFunctionForTarget("float32 MinCapsuleRadius() no_discard", FUNC_TRIVIAL(FCollisionShape::MinCapsuleRadius));
		Binds.BindGlobalFunctionForTarget("float32 MinCapsuleAxisHalfHeight() no_discard", FUNC_TRIVIAL(FCollisionShape::MinCapsuleAxisHalfHeight));
		Binds.BindGlobalFunctionForTarget("FCollisionShape MakeBox(const FVector& BoxHalfExtent) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeBox, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FCollisionShape MakeBox(const FVector3f& BoxHalfExtent) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeBox, (const FVector3f&)));
		Binds.BindGlobalFunctionForTarget("FCollisionShape MakeSphere(const float32 SphereRadius) no_discard", FUNC_TRIVIAL(FCollisionShape::MakeSphere));
		Binds.BindGlobalFunctionForTarget("FCollisionShape MakeCapsule(const float32 CapsuleRadius, const float32 CapsuleHalfHeight) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeCapsule, (const float, const float)));
		Binds.BindGlobalFunctionForTarget("FCollisionShape MakeCapsule(const FVector& Extent) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeCapsule, (const FVector&)));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionShape_Types(
	TEXT("FCollisionShape.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindCollisionShapeTypes);

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionShape(
	TEXT("FCollisionShape.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindCollisionShapeFunctions);
