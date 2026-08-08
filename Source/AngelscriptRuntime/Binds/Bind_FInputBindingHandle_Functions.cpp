#include "Bind_FInputBindingHandle.h"

uint32 FAngelscriptFInputBindingHandleBinds::GetActionEventHandle(const FEnhancedInputActionEventBinding& Binding)
{
	return Binding.GetHandle();
}

void FAngelscriptFInputBindingHandleBinds::ConstructActionValueDefault(FEnhancedInputActionValueBinding* Address)
{
	new (Address) FEnhancedInputActionValueBinding();
}

void FAngelscriptFInputBindingHandleBinds::ConstructActionValueFromAction(
	FEnhancedInputActionEventBinding* Address,
	const UInputAction* Action)
{
	new (Address) FEnhancedInputActionValueBinding(Action);
}
