#include "Bind_FInputBindingHandle.h"

#include "AngelscriptBinds.h"

#include "EnhancedInputComponent.h"

/**
 * Enhanced Input binding handles, action bindings, value access, and dispatch helpers.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftHandle == RightHandle;                                                             | Compares base input-binding handles.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Handle = BindingHandle.GetHandle() const;                                                     | Returns the numeric base binding handle.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftEventBinding == RightEventBinding;                                                 | Compares enhanced action-event bindings.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Handle = EventBinding.GetHandle() const;                                                      | Returns the action-event binding handle.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const UInputAction Action = EventBinding.GetAction() const;                                          | Returns the input action observed by the event binding.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | ETriggerEvent Trigger = EventBinding.GetTriggerEvent() const;                                        | Returns the trigger phase that dispatches the event binding.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | EventBinding.Execute(const FInputActionInstance& ActionData) const;                                  | Dispatches the bound event callback with an action instance.                                                     |
 * |                                                                                                      | @param ActionData Current trigger state, value, timing, and source action.                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | EventBinding.SetShouldFireWithEditorScriptGuard(const bool bNewValue);                               | Controls whether editor script-execution guards permit this binding to fire.                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bBound = EventBinding.IsBoundToObject(const UObject Object) const;                              | Reports whether the callback targets an object.                                                                  |
 * |                                                                                                      | @param Object Object whose binding ownership is queried.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedInputActionValueBinding ValueBinding();                                                     | Constructs an unassociated action-value binding.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftValueBinding == RightValueBinding;                                                 | Compares enhanced action-value bindings.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedInputActionValueBinding ValueBinding(const UInputAction InAction);                          | Constructs a value binding for an input action.                                                                  |
 * |                                                                                                      | @param InAction Action whose latest value is exposed.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Handle = ValueBinding.GetHandle() const;                                                      | Returns the action-value binding handle.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const UInputAction Action = ValueBinding.GetAction() const;                                          | Returns the action associated with the value binding.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FInputActionValue Value = ValueBinding.GetValue() const;                                             | Returns the action's latest evaluated value.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftDebugBinding == RightDebugBinding;                                                 | Compares debug-key bindings.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Handle = DebugBinding.GetHandle() const;                                                      | Returns the debug-key binding handle.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | DebugBinding.Execute(const FInputActionValue& ActionValue) const;                                    | Dispatches the debug-key callback.                                                                               |
 * |                                                                                                      | @param ActionValue Value supplied to the callback.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FInputBindingHandle_Types(
	TEXT("FInputBindingHandle.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		const FBindFlags Flags;
		Binds.ValueClassForTarget<FInputBindingHandle>("FInputBindingHandle", Flags);
		Binds.ValueClassForTarget<FEnhancedInputActionEventBinding>("FEnhancedInputActionEventBinding", Flags);
		Binds.ValueClassForTarget<FEnhancedInputActionValueBinding>("FEnhancedInputActionValueBinding", Flags);
		Binds.ValueClassForTarget<FInputDebugKeyBinding>("FInputDebugKeyBinding", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FInputBindingHandle(
	TEXT("FInputBindingHandle.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
