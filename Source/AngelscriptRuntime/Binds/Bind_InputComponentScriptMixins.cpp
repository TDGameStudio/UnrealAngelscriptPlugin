#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/InputComponentScriptMixinLibrary.h"

/**
 * UPlayerInput generated-binding overrides.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.AddActionMapping(const FInputActionKeyMapping& Mapping);                | Adds one action-key mapping. @param Mapping Action name, key, and modifier-key settings.                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.AddAxisMapping(const FInputAxisKeyMapping& Mapping);                    | Adds one axis-key mapping. @param Mapping Axis name, key, and scale.                                               |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.RemoveActionMapping(const FInputActionKeyMapping& Mapping);             | Removes the matching action-key mapping.                                                                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void PlayerInput.RemoveAxisMapping(const FInputAxisKeyMapping& Mapping);                 | Removes the matching axis-key mapping.                                                                             |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindInputComponentScriptMixins(FAngelscriptBinds& Binds)
	{
		// UHT marks these wrappers overloaded-unresolved. Install the exact signatures
		// in the selected engine before GeneratedBindings falls back to reflection.
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"AddActionMapping",
			{ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::AddActionMapping, (UPlayerInput*, const FInputActionKeyMapping&), ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"AddAxisMapping",
			{ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::AddAxisMapping, (UPlayerInput*, const FInputAxisKeyMapping&), ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"RemoveActionMapping",
			{ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::RemoveActionMapping, (UPlayerInput*, const FInputActionKeyMapping&), ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UPlayerInputScriptMixinLibrary::StaticClass(),
			"RemoveAxisMapping",
			{ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::RemoveAxisMapping, (UPlayerInput*, const FInputAxisKeyMapping&), ERASE_ARGUMENT_PACK(void))});
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_InputComponentScriptMixins(
	TEXT("InputComponentScriptMixins.GeneratedOverrides"),
	EAngelscriptBindPhase::ManualBindings,
	&BindInputComponentScriptMixins);
