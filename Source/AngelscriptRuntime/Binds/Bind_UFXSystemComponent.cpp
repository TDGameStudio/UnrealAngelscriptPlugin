#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Particles/ParticleSystemComponent.h"

/**
 * FX system component control.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void FXSystemComponent.DeactivateImmediate();                                            | Stops the FX system immediately without waiting for normal completion.                                             |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UFXSystemComponent(
	TEXT("UFXSystemComponent"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FXSystemComponent = Binds.ExistingClassForTarget("UFXSystemComponent");

		FXSystemComponent.Method("void DeactivateImmediate()", METHODPR_TRIVIAL(void, UFXSystemComponent, DeactivateImmediate, ()));
	});
