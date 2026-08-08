#include "AngelscriptBinds.h"

#include "CollisionShape.h"
#include "Helper_CppType.h"

#include "Bind_FCollisionShape_Functions.h"

struct FCollisionShapeType : TAngelscriptCppType<FCollisionShape>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FCollisionShape");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

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
