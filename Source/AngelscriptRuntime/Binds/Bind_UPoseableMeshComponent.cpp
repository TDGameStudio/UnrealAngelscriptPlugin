#include "Bind_UPoseableMeshComponent.h"

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

namespace
{
	void BindUPoseableMeshComponent(FAngelscriptBinds& Binds)
	{
		auto UPoseableMeshComponent_ = Binds.ExistingClassForTarget("UPoseableMeshComponent");
		UPoseableMeshComponent_.Method("void AllocateTransformData()", &FAngelscriptUPoseableMeshComponentBinds::AllocateTransformData);
		UPoseableMeshComponent_.Method("void RefreshBoneTransforms()", &FAngelscriptUPoseableMeshComponentBinds::RefreshBoneTransforms);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UPoseableMeshComponent(
	TEXT("UPoseableMeshComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUPoseableMeshComponent);
