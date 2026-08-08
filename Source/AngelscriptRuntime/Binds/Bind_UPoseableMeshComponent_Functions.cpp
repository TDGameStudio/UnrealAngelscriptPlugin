#include "Bind_UPoseableMeshComponent.h"

#include "Components/PoseableMeshComponent.h"

void FAngelscriptUPoseableMeshComponentBinds::AllocateTransformData(UPoseableMeshComponent* Component)
{
	Component->AllocateTransformData();
}

void FAngelscriptUPoseableMeshComponentBinds::RefreshBoneTransforms(UPoseableMeshComponent* Component)
{
	Component->RefreshBoneTransforms();
}
