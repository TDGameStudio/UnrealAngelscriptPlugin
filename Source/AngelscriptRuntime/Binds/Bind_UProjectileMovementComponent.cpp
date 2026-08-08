#include "AngelscriptBinds.h"

#include "Bind_UProjectileMovementComponent_Functions.h"

namespace
{
	void BindUProjectileMovementComponent(FAngelscriptBinds& Binds)
	{
		auto ProjectileMovement_ = Binds.ExistingClassForTarget("UProjectileMovementComponent");
		if (ProjectileMovement_.GetTypeInfo() == nullptr)
		{
			return;
		}

		ProjectileMovement_.Method(
			"const USceneComponent GetHomingTargetComponent() const",
			&FAngelscriptUProjectileMovementComponentBinds::GetHomingTargetComponent);
		ProjectileMovement_.Method(
			"void SetHomingTargetComponent(USceneComponent HomingTargetComponent)",
			&FAngelscriptUProjectileMovementComponentBinds::SetHomingTargetComponent);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UProjectileMovementComponent(
	TEXT("UProjectileMovementComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUProjectileMovementComponent);
