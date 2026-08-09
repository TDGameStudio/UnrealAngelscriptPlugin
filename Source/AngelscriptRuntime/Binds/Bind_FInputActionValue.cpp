#include "Bind_FInputActionValue.h"

#include "AngelscriptBinds.h"

#include "InputActionValue.h"

/**
 * FInputActionValue manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue Value(float32 InValue);                                                  | Constructs a one-dimensional action value.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue Value(FVector2D InValue);                                                | Constructs a two-dimensional action value.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue Value(FVector InValue);                                                  | Constructs a three-dimensional action value.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue Value(EInputActionValueType InValueType, FVector InValue);               | Constructs an action value with an explicit value type.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Value += Other;                                                                            | Adds another action value in place.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Value *= Scalar;                                                                           | Scales the action value in place.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FInputActionValue.IsNonZero(float32 Tolerance = KINDA_SMALL_NUMBER) const;            | Returns whether any represented axis exceeds the tolerance.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue& FInputActionValue.ConvertToType(EInputActionValueType Type);            | Converts this value to the requested action-value type.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue& FInputActionValue.ConvertToType(const FInputActionValue& Other);        | Converts this value to the type represented by Other.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FInputActionValue.Get() const;                                                        | Returns the value as a digital action state.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FInputActionValue.GetAxis1D() const;                                               | Returns the one-dimensional axis value.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FInputActionValue.GetAxis2D() const;                                             | Returns the two-dimensional axis value.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FInputActionValue.GetAxis3D() const;                                               | Returns the three-dimensional axis value.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EInputActionValueType FInputActionValue::GetValueTypeFromKey(FKey Key);                    | Returns the action-value type implied by the key.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FInputActionValue(
	TEXT("FInputActionValue"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Value_ = Binds.ExistingClassForTarget("FInputActionValue");
		Value_.Constructor("void f(float32 InValue)", &FAngelscriptFInputActionValueBinds::ConstructAxis1D, "FInputActionValue", true);
		Value_.Constructor("void f(FVector2D InValue)", &FAngelscriptFInputActionValueBinds::ConstructAxis2D, "FInputActionValue", true);
		Value_.Constructor("void f(FVector InValue)", &FAngelscriptFInputActionValueBinds::ConstructAxis3D, "FInputActionValue", true);
		Value_.Constructor("void f(EInputActionValueType InValueType, FVector InValue)", &FAngelscriptFInputActionValueBinds::ConstructTyped, "FInputActionValue", true);
		Value_.Method("FInputActionValue& opAddAssign(const FInputActionValue& Other)", METHODPR_TRIVIAL(FInputActionValue&, FInputActionValue, operator+=, (const FInputActionValue&)));
		Value_.Method("FInputActionValue& opMulAssign(float32 Scalar)", METHODPR_TRIVIAL(FInputActionValue&, FInputActionValue, operator*=, (float)));
		Value_.Method("bool IsNonZero(float32 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FInputActionValue, IsNonZero));
		Value_.Method("FInputActionValue& ConvertToType(EInputActionValueType Type)", &FAngelscriptFInputActionValueBinds::ConvertToType);
		Value_.Method("FInputActionValue& ConvertToType(const FInputActionValue& Other)", &FAngelscriptFInputActionValueBinds::ConvertToOtherType);
		Value_.Method("bool Get() const", METHOD_TRIVIAL(FInputActionValue, Get<bool>));
		Value_.Method("float32 GetAxis1D() const", METHOD_TRIVIAL(FInputActionValue, Get<float>));
		Value_.Method("FVector2D GetAxis2D() const", METHOD_TRIVIAL(FInputActionValue, Get<FVector2D>));
		Value_.Method("FVector GetAxis3D() const", METHOD_TRIVIAL(FInputActionValue, Get<FVector>));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FInputActionValue");
		Binds.BindGlobalFunctionForTarget("EInputActionValueType GetValueTypeFromKey(FKey Key)", &FInputActionValue::GetValueTypeFromKey)
			.NativeFunction("FInputActionValue::GetValueTypeFromKey", true);
	});
