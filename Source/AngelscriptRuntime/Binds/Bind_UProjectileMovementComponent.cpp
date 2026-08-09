#include "Bind_UProjectileMovementComponent.h"

#include "AngelscriptBinds.h"

/**
 * UProjectileMovementComponent manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const USceneComponent UProjectileMovementComponent.GetHomingTargetComponent() const;       | Returns the current homing target component while its weak reference remains valid.                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UProjectileMovementComponent.SetHomingTargetComponent(                                | Sets the weak homing target component used when homing acceleration is enabled.                                      |
 * |     USceneComponent HomingTargetComponent);                                                | Registration is skipped only when UProjectileMovementComponent is unavailable.                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UProjectileMovementComponent(
	TEXT("UProjectileMovementComponent"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});
