#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "GameFramework/PlayerInput.h"

/**
 * Input action mapping comparison.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Mapping == Other;                                                          | Compares the action name, key, and modifier settings.                                                              |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FInputActionKeyMapping(
	TEXT("FInputActionKeyMapping"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FInputActionKeyMapping_ = Binds.ExistingClassForTarget("FInputActionKeyMapping");

		FInputActionKeyMapping_.Method("bool opEquals(const FInputActionKeyMapping& Other) const", METHOD_TRIVIAL(FInputActionKeyMapping, operator==));
	});
