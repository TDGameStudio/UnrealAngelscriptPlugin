#include "Bind_UProjectileMovementComponent_Functions.h"

#include "GameFramework/ProjectileMovementComponent.h"

const USceneComponent* FAngelscriptUProjectileMovementComponentBinds::GetHomingTargetComponent(
	const UProjectileMovementComponent* Component)
{
	return Component->HomingTargetComponent.Get();
}

void FAngelscriptUProjectileMovementComponentBinds::SetHomingTargetComponent(
	UProjectileMovementComponent* Component,
	USceneComponent* HomingTargetComponent)
{
	Component->HomingTargetComponent = HomingTargetComponent;
}
