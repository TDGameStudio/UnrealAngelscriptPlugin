#include "Bind_UEnhancedInputComponent.h"

#include "AngelscriptBinds.h"

#include "EnhancedInputComponent.h"

/**
 * UEnhancedInputComponent manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UEnhancedInputComponent.SetShouldFireDelegatesInEditor(                               | Sets whether enhanced-input delegates may fire in editor worlds.                                                     |
 * |     const bool bInNewValue);                                                               |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.ShouldFireDelegatesInEditor() const;                          | Returns whether enhanced-input delegates may fire in editor worlds.                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.HasBindings() const;                                          | Returns whether the component owns any input bindings.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UEnhancedInputComponent.ClearActionEventBindings();                                   | Removes all enhanced action-event bindings.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UEnhancedInputComponent.ClearActionValueBindings();                                   | Removes all enhanced action-value bindings.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UEnhancedInputComponent.ClearDebugKeyBindings();                                      | Removes all debug-key bindings.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UEnhancedInputComponent.ClearActionBindings();                                        | Removes all action bindings owned by the component.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UEnhancedInputComponent.ClearBindingsForObject(UObject InOwner);                      | Removes every binding whose delegate is owned by InOwner.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveActionEventBinding(const int32 BindingIndex);           | Removes an action-event binding by array index.                                                                      |
 * |                                                                                            | @param BindingIndex Index in the action-event binding array.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveDebugKeyBinding(const int32 BindingIndex);              | Removes a debug-key binding by array index.                                                                          |
 * |                                                                                            | @param BindingIndex Index in the debug-key binding array.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveActionValueBinding(const int32 BindingIndex);           | Removes an action-value binding by array index.                                                                      |
 * |                                                                                            | @param BindingIndex Index in the action-value binding array.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveBindingByHandle(const uint32 BindingIndex);             | Removes the binding identified by its numeric binding handle.                                                        |
 * |                                                                                            | @param BindingIndex Registered binding-handle value, despite the exposed parameter name.                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveBinding(                                                | Removes the binding identified by the generic input-binding handle.                                                  |
 * |     const FInputBindingHandle& BindingToRemove);                                           |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveBinding(                                                | Removes the supplied enhanced action-event binding.                                                                  |
 * |     const FEnhancedInputActionEventBinding& BindingToRemove);                              |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveBinding(                                                | Removes the supplied enhanced action-value binding.                                                                  |
 * |     const FEnhancedInputActionValueBinding& BindingToRemove);                              |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnhancedInputComponent.RemoveBinding(                                                | Removes the supplied debug-key binding.                                                                              |
 * |     const FInputDebugKeyBinding& BindingToRemove);                                         |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedInputActionEventBinding& UEnhancedInputComponent.BindAction(                      | Binds a dynamic delegate to an input action trigger event.                                                           |
 * |     const UInputAction Action, ETriggerEvent TriggerEvent,                                 | @param Action Input action asset to observe.                                                                         |
 * |     FEnhancedInputActionHandlerDynamicSignature Delegate);                                 | @param TriggerEvent Trigger phase that invokes Delegate.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedInputActionValueBinding& UEnhancedInputComponent.BindActionValue(                 | Binds value polling for an input action and returns the stable binding record.                                       |
 * |     const UInputAction Action);                                                            |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputDebugKeyBinding& UEnhancedInputComponent.BindDebugKey(                               | Binds a dynamic delegate to a debug chord and input event.                                                           |
 * |     const FInputChord Chord, const EInputEvent KeyEvent,                                   | @param KeyEvent Input transition that invokes Delegate.                                                              |
 * |     FInputDebugKeyHandlerDynamicSignature Delegate, bool bExecuteWhenPaused = true);       | @param bExecuteWhenPaused Allows invocation while gameplay is paused.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue UEnhancedInputComponent.GetBoundActionValue(                             | Returns the current accumulated value for a bound input action.                                                      |
 * |     const UInputAction Action);                                                            |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

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
