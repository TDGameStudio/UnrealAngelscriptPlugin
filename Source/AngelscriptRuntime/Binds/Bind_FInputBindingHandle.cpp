#include "AngelscriptBinds.h"

#include "EnhancedInputComponent.h"

#include "Bind_FInputBindingHandle_Functions.h"

namespace
{
	void BindInputBindingHandleTypes(FAngelscriptBinds& Binds)
	{
		const FBindFlags Flags;
		Binds.ValueClassForTarget<FInputBindingHandle>("FInputBindingHandle", Flags);
		Binds.ValueClassForTarget<FEnhancedInputActionEventBinding>("FEnhancedInputActionEventBinding", Flags);
		Binds.ValueClassForTarget<FEnhancedInputActionValueBinding>("FEnhancedInputActionValueBinding", Flags);
		Binds.ValueClassForTarget<FInputDebugKeyBinding>("FInputDebugKeyBinding", Flags);
	}

	void BindInputBindingHandleFunctions(FAngelscriptBinds& Binds)
	{
		auto FInputBindingHandle_ = Binds.ExistingClassForTarget("FInputBindingHandle");
		FInputBindingHandle_.Method("bool opEquals(const FInputBindingHandle& Other) const", METHODPR_TRIVIAL(bool, FInputBindingHandle, operator==, (const FInputBindingHandle&) const));
		FInputBindingHandle_.Method("uint32 GetHandle() const", METHOD_TRIVIAL(FInputBindingHandle, GetHandle));

		auto FEnhancedInputActionEventBinding_ = Binds.ExistingClassForTarget("FEnhancedInputActionEventBinding");
		FEnhancedInputActionEventBinding_.Method("bool opEquals(const FEnhancedInputActionEventBinding& Other) const", METHODPR_TRIVIAL(bool, FEnhancedInputActionEventBinding, operator==, (const FEnhancedInputActionEventBinding&) const));
		FEnhancedInputActionEventBinding_.Method("uint32 GetHandle() const", &FAngelscriptFInputBindingHandleBinds::GetActionEventHandle);
		FEnhancedInputActionEventBinding_.Method("const UInputAction GetAction() const", METHOD_TRIVIAL(FEnhancedInputActionEventBinding, GetAction));
		FEnhancedInputActionEventBinding_.Method("ETriggerEvent GetTriggerEvent() const", METHOD_TRIVIAL(FEnhancedInputActionEventBinding, GetTriggerEvent));
		FEnhancedInputActionEventBinding_.Method("void Execute(const FInputActionInstance& ActionData) const", METHODPR_TRIVIAL(void, FEnhancedInputActionEventBinding, Execute, (const FInputActionInstance&) const));
		FEnhancedInputActionEventBinding_.Method("void SetShouldFireWithEditorScriptGuard(const bool bNewValue)", METHODPR_TRIVIAL(void, FEnhancedInputActionEventBinding, SetShouldFireWithEditorScriptGuard, (const bool)));
		FEnhancedInputActionEventBinding_.Method("bool IsBoundToObject(const UObject Object) const", METHODPR_TRIVIAL(bool, FEnhancedInputActionEventBinding, IsBoundToObject, (const UObject*) const));

		auto FEnhancedInputActionValueBinding_ = Binds.ExistingClassForTarget("FEnhancedInputActionValueBinding");
		FEnhancedInputActionValueBinding_.Constructor(
			"void f()",
			&FAngelscriptFInputBindingHandleBinds::ConstructActionValueDefault,
			"FEnhancedInputActionValueBinding",
			true);
		FEnhancedInputActionValueBinding_.Method("bool opEquals(const FEnhancedInputActionValueBinding& Other) const", METHODPR_TRIVIAL(bool, FEnhancedInputActionValueBinding, operator==, (const FEnhancedInputActionValueBinding&) const));
		FEnhancedInputActionValueBinding_.Constructor(
			"void f(const UInputAction InAction)",
			&FAngelscriptFInputBindingHandleBinds::ConstructActionValueFromAction,
			"FEnhancedInputActionValueBinding",
			true);
		FEnhancedInputActionValueBinding_.Method("uint32 GetHandle() const", METHOD_TRIVIAL(FEnhancedInputActionValueBinding, GetHandle));
		FEnhancedInputActionValueBinding_.Method("const UInputAction GetAction() const", METHOD_TRIVIAL(FEnhancedInputActionValueBinding, GetAction));
		FEnhancedInputActionValueBinding_.Method("FInputActionValue GetValue() const", METHOD_TRIVIAL(FEnhancedInputActionValueBinding, GetValue));

		auto FInputDebugKeyBinding_ = Binds.ExistingClassForTarget("FInputDebugKeyBinding");
		FInputDebugKeyBinding_.Method("bool opEquals(const FInputDebugKeyBinding& Other) const", METHODPR_TRIVIAL(bool, FInputDebugKeyBinding, operator==, (const FInputDebugKeyBinding&) const));
		FInputDebugKeyBinding_.Method("uint32 GetHandle() const", METHOD_TRIVIAL(FInputDebugKeyBinding, GetHandle));
		FInputDebugKeyBinding_.Method("void Execute(const FInputActionValue& ActionValue) const", METHODPR_TRIVIAL(void, FEnhancedInputActionEventBinding, Execute, (const FInputActionValue&) const));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FInputBindingHandle_Types(
	TEXT("FInputBindingHandle.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindInputBindingHandleTypes);

AS_FORCE_LINK const FAngelscriptBind Bind_FInputBindingHandle(
	TEXT("FInputBindingHandle.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindInputBindingHandleFunctions);
