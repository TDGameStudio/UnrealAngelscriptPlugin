#include "Components/SkeletalMeshComponent.h"

#include "AngelscriptBinds.h"

/**
 * USkeletalMeshComponent binding surface.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const TArray<UAnimInstance>& USkeletalMeshComponent.GetLinkedAnimInstances() const;                  | Returns animation instances linked to this skeletal mesh component.                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void USkeletalMeshComponent.SetSkeletalMeshAsset(USkeletalMesh NewMesh);                             | Assigns the skeletal mesh asset.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | USkeletalMesh USkeletalMeshComponent.GetSkeletalMeshAsset() const;                                   | Returns the assigned skeletal mesh asset.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_USkeletalMeshComponent(
	TEXT("USkeletalMeshComponent"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds USkeletalMeshComponent_ = Binds.ExistingClassForTarget("USkeletalMeshComponent");
		USkeletalMeshComponent_.Method("const TArray<UAnimInstance>& GetLinkedAnimInstances() const", METHODPR_TRIVIAL(const TArray<UAnimInstance*>&, USkeletalMeshComponent, GetLinkedAnimInstances, () const));
		USkeletalMeshComponent_.Method("void SetSkeletalMeshAsset(USkeletalMesh NewMesh)", METHOD_TRIVIAL(USkeletalMeshComponent, SetSkeletalMeshAsset));
		USkeletalMeshComponent_.Method("USkeletalMesh GetSkeletalMeshAsset() const", METHOD_TRIVIAL(USkeletalMeshComponent, GetSkeletalMeshAsset));
	});
