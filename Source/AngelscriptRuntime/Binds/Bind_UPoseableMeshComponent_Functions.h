#pragma once

class UPoseableMeshComponent;

struct FAngelscriptUPoseableMeshComponentBinds
{
	static void AllocateTransformData(UPoseableMeshComponent* Component);
	static void RefreshBoneTransforms(UPoseableMeshComponent* Component);
};
