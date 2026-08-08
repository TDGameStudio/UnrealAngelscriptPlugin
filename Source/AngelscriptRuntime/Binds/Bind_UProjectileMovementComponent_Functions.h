#pragma once

class UProjectileMovementComponent;
class USceneComponent;

struct FAngelscriptUProjectileMovementComponentBinds
{
	static const USceneComponent* GetHomingTargetComponent(const UProjectileMovementComponent* Component);
	static void SetHomingTargetComponent(UProjectileMovementComponent* Component, USceneComponent* HomingTargetComponent);
};
