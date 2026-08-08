#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Particles/ParticleSystemComponent.h"

namespace
{
	void BindUFXSystemComponent(FAngelscriptBinds& Binds)
	{
		auto FXSystemComponent = Binds.ExistingClassForTarget("UFXSystemComponent");

		FXSystemComponent.Method("void DeactivateImmediate()", METHODPR_TRIVIAL(void, UFXSystemComponent, DeactivateImmediate, ()));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UFXSystemComponent(
	TEXT("UFXSystemComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUFXSystemComponent);
