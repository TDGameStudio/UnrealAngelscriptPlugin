#include "Components/SkeletalMeshComponent.h"

#include "AngelscriptBinds.h"

namespace
{
	void BindUSkeletalMeshComponent(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds USkeletalMeshComponent_ = Binds.ExistingClassForTarget("USkeletalMeshComponent");
		USkeletalMeshComponent_.Method("const TArray<UAnimInstance>& GetLinkedAnimInstances() const", METHODPR_TRIVIAL(const TArray<UAnimInstance*>&, USkeletalMeshComponent, GetLinkedAnimInstances, () const));
		USkeletalMeshComponent_.Method("void SetSkeletalMeshAsset(USkeletalMesh NewMesh)", METHOD_TRIVIAL(USkeletalMeshComponent, SetSkeletalMeshAsset));
		USkeletalMeshComponent_.Method("USkeletalMesh GetSkeletalMeshAsset() const", METHOD_TRIVIAL(USkeletalMeshComponent, GetSkeletalMeshAsset));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_USkeletalMeshComponent(
	TEXT("USkeletalMeshComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUSkeletalMeshComponent);
