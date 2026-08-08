#include "Bind_FInputActionValue_Functions.h"

void FAngelscriptFInputActionValueBinds::ConstructAxis1D(FInputActionValue* Address, float Value)
{
	new (Address) FInputActionValue(Value);
}

void FAngelscriptFInputActionValueBinds::ConstructAxis2D(FInputActionValue* Address, FVector2D Value)
{
	new (Address) FInputActionValue(Value);
}

void FAngelscriptFInputActionValueBinds::ConstructAxis3D(FInputActionValue* Address, FVector Value)
{
	new (Address) FInputActionValue(Value);
}

void FAngelscriptFInputActionValueBinds::ConstructTyped(
	FInputActionValue* Address,
	EInputActionValueType ValueType,
	FVector Value)
{
	new (Address) FInputActionValue(ValueType, Value);
}

FInputActionValue& FAngelscriptFInputActionValueBinds::ConvertToType(
	FInputActionValue* Value,
	EInputActionValueType Type)
{
	return Value->ConvertToType(Type);
}

FInputActionValue& FAngelscriptFInputActionValueBinds::ConvertToOtherType(
	FInputActionValue* Value,
	const FInputActionValue& Other)
{
	return Value->ConvertToType(Other);
}
