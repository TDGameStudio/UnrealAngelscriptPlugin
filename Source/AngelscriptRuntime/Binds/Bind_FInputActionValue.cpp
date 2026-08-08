#include "AngelscriptBinds.h"

#include "InputActionValue.h"

#include "Bind_FInputActionValue_Functions.h"

namespace
{
	void BindFInputActionValue(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FInputActionValue(
	TEXT("FInputActionValue"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFInputActionValue);
