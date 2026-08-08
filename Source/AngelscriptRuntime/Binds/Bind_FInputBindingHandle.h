#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"

struct FAngelscriptFInputBindingHandleBinds
{
	static uint32 GetActionEventHandle(const FEnhancedInputActionEventBinding& Binding);
	static void ConstructActionValueDefault(FEnhancedInputActionValueBinding* Address);
	static void ConstructActionValueFromAction(FEnhancedInputActionEventBinding* Address, const UInputAction* Action);
};
