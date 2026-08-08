#include "AngelscriptBinds.h"

#include "EnhancedInputComponent.h"

#include "Bind_UEnhancedInputComponent_Functions.h"

namespace
{
	void BindUEnhancedInputComponent(FAngelscriptBinds& Binds)
	{
		auto InputComponent_ = Binds.ExistingClassForTarget("UEnhancedInputComponent");
		InputComponent_.Method("void SetShouldFireDelegatesInEditor(const bool bInNewValue)", METHODPR_TRIVIAL(void, UEnhancedInputComponent, SetShouldFireDelegatesInEditor, (const bool)));
		InputComponent_.Method("bool ShouldFireDelegatesInEditor() const", METHOD_TRIVIAL(UEnhancedInputComponent, ShouldFireDelegatesInEditor));
		InputComponent_.Method("bool HasBindings() const", METHOD_TRIVIAL(UEnhancedInputComponent, HasBindings));
		InputComponent_.Method("void ClearActionEventBindings()", METHOD_TRIVIAL(UEnhancedInputComponent, ClearActionEventBindings));
		InputComponent_.Method("void ClearActionValueBindings()", METHOD_TRIVIAL(UEnhancedInputComponent, ClearActionValueBindings));
		InputComponent_.Method("void ClearDebugKeyBindings()", METHOD_TRIVIAL(UEnhancedInputComponent, ClearDebugKeyBindings));
		InputComponent_.Method("void ClearActionBindings()", METHOD_TRIVIAL(UEnhancedInputComponent, ClearActionBindings));
		InputComponent_.Method("void ClearBindingsForObject(UObject InOwner)", METHODPR_TRIVIAL(void, UEnhancedInputComponent, ClearBindingsForObject, (UObject*)));
		InputComponent_.Method("bool RemoveActionEventBinding(const int32 BindingIndex)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveActionEventBinding, (const int32)));
		InputComponent_.Method("bool RemoveDebugKeyBinding(const int32 BindingIndex)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveDebugKeyBinding, (const int32)));
		InputComponent_.Method("bool RemoveActionValueBinding(const int32 BindingIndex)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveActionValueBinding, (const int32)));
		InputComponent_.Method("bool RemoveBindingByHandle(const uint32 BindingIndex)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveBindingByHandle, (const uint32)));
		InputComponent_.Method("bool RemoveBinding(const FInputBindingHandle& BindingToRemove)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveBinding, (const FInputBindingHandle&)));
		InputComponent_.Method("bool RemoveBinding(const FEnhancedInputActionEventBinding& BindingToRemove)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveBinding, (const FInputBindingHandle&)));
		InputComponent_.Method("bool RemoveBinding(const FEnhancedInputActionValueBinding& BindingToRemove)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveBinding, (const FInputBindingHandle&)));
		InputComponent_.Method("bool RemoveBinding(const FInputDebugKeyBinding& BindingToRemove)", METHODPR_TRIVIAL(bool, UEnhancedInputComponent, RemoveBinding, (const FInputBindingHandle&)));
		InputComponent_.Method(
			"FEnhancedInputActionEventBinding& BindAction(const UInputAction Action, ETriggerEvent TriggerEvent, FEnhancedInputActionHandlerDynamicSignature Delegate)",
			&FAngelscriptUEnhancedInputComponentBinds::BindAction);
		InputComponent_.Method("FEnhancedInputActionValueBinding& BindActionValue(const UInputAction Action)", METHODPR_TRIVIAL(FEnhancedInputActionValueBinding&, UEnhancedInputComponent, BindActionValue, (const UInputAction*)));
		InputComponent_.Method(
			"FInputDebugKeyBinding& BindDebugKey(const FInputChord Chord, const EInputEvent KeyEvent, FInputDebugKeyHandlerDynamicSignature Delegate, bool bExecuteWhenPaused = true)",
			&FAngelscriptUEnhancedInputComponentBinds::BindDebugKey);
		InputComponent_.Method("FInputActionValue GetBoundActionValue(const UInputAction Action)", METHODPR_TRIVIAL(FInputActionValue, UEnhancedInputComponent, GetBoundActionValue, (const UInputAction*)));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UEnhancedInputComponent(
	TEXT("UEnhancedInputComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUEnhancedInputComponent);
