#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "GameFramework/PlayerInput.h"

namespace
{
	void BindFInputActionKeyMapping(FAngelscriptBinds& Binds)
	{
		auto FInputActionKeyMapping_ = Binds.ExistingClassForTarget("FInputActionKeyMapping");

		FInputActionKeyMapping_.Method("bool opEquals(const FInputActionKeyMapping& Other) const", METHOD_TRIVIAL(FInputActionKeyMapping, operator==));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FInputActionKeyMapping(
	TEXT("FInputActionKeyMapping"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFInputActionKeyMapping);
