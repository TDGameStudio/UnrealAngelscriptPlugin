#include "AngelscriptBinds.h"

#include "Bind_UPoseableMeshComponent_Functions.h"

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
