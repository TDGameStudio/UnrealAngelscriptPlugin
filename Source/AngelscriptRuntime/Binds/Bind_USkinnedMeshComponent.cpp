#include "Components/SkinnedMeshComponent.h"

#include "AngelscriptBinds.h"

/**
 * USkinnedMeshComponent manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void USkinnedMeshComponent.UpdateLODStatus();                                              | Refreshes the component LOD status.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void USkinnedMeshComponent.InvalidateCachedBounds();                                       | Invalidates cached bounds so they are recalculated.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindUSkinnedMeshComponent(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds USkinnedMeshComponent_ = Binds.ExistingClassForTarget("USkinnedMeshComponent");

		USkinnedMeshComponent_.Method("void UpdateLODStatus()", METHOD_TRIVIAL(USkinnedMeshComponent, UpdateLODStatus));
		USkinnedMeshComponent_.Method("void InvalidateCachedBounds()", METHOD_TRIVIAL(USkinnedMeshComponent, InvalidateCachedBounds));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_USkinnedMeshComponent(
	TEXT("USkinnedMeshComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUSkinnedMeshComponent);
