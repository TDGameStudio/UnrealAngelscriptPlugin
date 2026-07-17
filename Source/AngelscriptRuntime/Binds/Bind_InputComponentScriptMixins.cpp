#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/InputComponentScriptMixinLibrary.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_InputComponentScriptMixins((int32)FAngelscriptBinds::EOrder::Late + 49, []
{
	// UHT marks these wrappers overloaded-unresolved, so register the exact signatures
	// before the generated function table falls back to reflective dispatch.
	FAngelscriptBinds::RegisterFunctionBinding(
		UPlayerInputScriptMixinLibrary::StaticClass(),
		"AddActionMapping",
		{ ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::AddActionMapping, (UPlayerInput*, const FInputActionKeyMapping&), ERASE_ARGUMENT_PACK(void)) });
	FAngelscriptBinds::RegisterFunctionBinding(
		UPlayerInputScriptMixinLibrary::StaticClass(),
		"AddAxisMapping",
		{ ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::AddAxisMapping, (UPlayerInput*, const FInputAxisKeyMapping&), ERASE_ARGUMENT_PACK(void)) });
	FAngelscriptBinds::RegisterFunctionBinding(
		UPlayerInputScriptMixinLibrary::StaticClass(),
		"RemoveActionMapping",
		{ ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::RemoveActionMapping, (UPlayerInput*, const FInputActionKeyMapping&), ERASE_ARGUMENT_PACK(void)) });
	FAngelscriptBinds::RegisterFunctionBinding(
		UPlayerInputScriptMixinLibrary::StaticClass(),
		"RemoveAxisMapping",
		{ ERASE_FUNCTION_PTR(UPlayerInputScriptMixinLibrary::RemoveAxisMapping, (UPlayerInput*, const FInputAxisKeyMapping&), ERASE_ARGUMENT_PACK(void)) });
});
