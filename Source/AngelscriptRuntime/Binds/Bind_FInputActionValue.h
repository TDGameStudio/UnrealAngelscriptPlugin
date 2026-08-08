#pragma once

#include "InputActionValue.h"

struct FAngelscriptFInputActionValueBinds
{
	static void ConstructAxis1D(FInputActionValue* Address, float Value);
	static void ConstructAxis2D(FInputActionValue* Address, FVector2D Value);
	static void ConstructAxis3D(FInputActionValue* Address, FVector Value);
	static void ConstructTyped(FInputActionValue* Address, EInputActionValueType ValueType, FVector Value);
	static FInputActionValue& ConvertToType(FInputActionValue* Value, EInputActionValueType Type);
	static FInputActionValue& ConvertToOtherType(FInputActionValue* Value, const FInputActionValue& Other);
};
