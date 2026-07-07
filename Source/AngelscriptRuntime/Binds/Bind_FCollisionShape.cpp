#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Helper_CppType.h"

#include "CollisionShape.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_CollisionShape((int32)FAngelscriptBinds::EOrder::Early - 1, []
{
	auto CollisionShape_ = FAngelscriptBinds::Enum("ECollisionShape");
	CollisionShape_["Line"]    = ECollisionShape::Line;
	CollisionShape_["Box"]     = ECollisionShape::Box;
	CollisionShape_["Sphere"]  = ECollisionShape::Sphere;
	CollisionShape_["Capsule"] = ECollisionShape::Capsule;
});

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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionShape_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bPOD = true;

	auto FCollisionShape_ = FAngelscriptBinds::ValueClass<FCollisionShape>("FCollisionShape", Flags);
	FAngelscriptType::Register(MakeShared<FCollisionShapeType>());

	FCollisionShape_.Constructor("void f()", [](FCollisionShape* Address)
	{
		new(Address) FCollisionShape();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionShape_, "FCollisionShape");
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionShape_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FCollisionShape_ = FAngelscriptBinds::ExistingClass("FCollisionShape");
	FCollisionShape_.Property("ECollisionShape ShapeType", &FCollisionShape::ShapeType);

	FCollisionShape_.Method("bool IsLine() const", METHOD_TRIVIAL(FCollisionShape, IsLine));
	FCollisionShape_.Method("bool IsBox() const", METHOD_TRIVIAL(FCollisionShape, IsBox));
	FCollisionShape_.Method("bool IsCapsule() const", METHOD_TRIVIAL(FCollisionShape, IsCapsule));
	FCollisionShape_.Method("bool IsSphere() const", METHOD_TRIVIAL(FCollisionShape, IsSphere));
	FCollisionShape_.Method("void SetBox(const FVector& HalfExtent)", [](FCollisionShape* Shape, const FVector& HalfExtent)
	{
		Shape->SetBox(FVector3f(HalfExtent));
	});
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

	{
		FAngelscriptBinds::FNamespace ns("FCollisionShape");
		FAngelscriptBinds::BindGlobalFunction("float32 MinBoxExtent() no_discard", FUNC_TRIVIAL(FCollisionShape::MinBoxExtent));
		FAngelscriptBinds::BindGlobalFunction("float32 MinSphereRadius() no_discard", FUNC_TRIVIAL(FCollisionShape::MinSphereRadius));
		FAngelscriptBinds::BindGlobalFunction("float32 MinCapsuleRadius() no_discard", FUNC_TRIVIAL(FCollisionShape::MinCapsuleRadius));
		FAngelscriptBinds::BindGlobalFunction("float32 MinCapsuleAxisHalfHeight() no_discard", FUNC_TRIVIAL(FCollisionShape::MinCapsuleAxisHalfHeight));

		FAngelscriptBinds::BindGlobalFunction("FCollisionShape MakeBox(const FVector& BoxHalfExtent) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeBox, (const FVector&)));
		FAngelscriptBinds::BindGlobalFunction("FCollisionShape MakeBox(const FVector3f& BoxHalfExtent) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeBox, (const FVector3f&)));
		FAngelscriptBinds::BindGlobalFunction("FCollisionShape MakeSphere(const float32 SphereRadius) no_discard", FUNC_TRIVIAL(FCollisionShape::MakeSphere));
		FAngelscriptBinds::BindGlobalFunction("FCollisionShape MakeCapsule(const float32 CapsuleRadius, const float32 CapsuleHalfHeight) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeCapsule, (const float, const float)));
		FAngelscriptBinds::BindGlobalFunction("FCollisionShape MakeCapsule(const FVector& Extent) no_discard", FUNCPR_TRIVIAL(FCollisionShape, FCollisionShape::MakeCapsule, (const FVector&)));
	}
});
