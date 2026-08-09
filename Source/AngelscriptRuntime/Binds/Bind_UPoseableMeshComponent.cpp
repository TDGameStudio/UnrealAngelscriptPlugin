class UPoseableMeshComponent;

struct FAngelscriptUPoseableMeshComponentBinds
{
	static void AllocateTransformData(UPoseableMeshComponent* Component);
	static void RefreshBoneTransforms(UPoseableMeshComponent* Component);
};

#include "AngelscriptBinds.h"

/**
 * UPoseableMeshComponent manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UPoseableMeshComponent.AllocateTransformData();                                       | Allocates per-bone transform storage for the current skeletal mesh.                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UPoseableMeshComponent.RefreshBoneTransforms();                                       | Rebuilds component-space bone transforms from the poseable transform data.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UPoseableMeshComponent(
	TEXT("UPoseableMeshComponent"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UPoseableMeshComponent_ = Binds.ExistingClassForTarget("UPoseableMeshComponent");
		UPoseableMeshComponent_.Method("void AllocateTransformData()", &FAngelscriptUPoseableMeshComponentBinds::AllocateTransformData);
		UPoseableMeshComponent_.Method("void RefreshBoneTransforms()", &FAngelscriptUPoseableMeshComponentBinds::RefreshBoneTransforms);
	});

#include "Components/PoseableMeshComponent.h"

void FAngelscriptUPoseableMeshComponentBinds::AllocateTransformData(UPoseableMeshComponent* Component)
{
	Component->AllocateTransformData();
}

void FAngelscriptUPoseableMeshComponentBinds::RefreshBoneTransforms(UPoseableMeshComponent* Component)
{
	Component->RefreshBoneTransforms();
}
