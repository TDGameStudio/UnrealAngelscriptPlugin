#include "Bind_UEnhancedInputComponent_Functions.h"

FEnhancedInputActionEventBinding& FAngelscriptUEnhancedInputComponentBinds::BindAction(
	UEnhancedInputComponent& InputComponent,
	const UInputAction* Action,
	ETriggerEvent TriggerEvent,
	FEnhancedInputActionHandlerDynamicSignature Delegate)
{
	return InputComponent.BindAction(Action, TriggerEvent, Delegate.GetUObject(), Delegate.GetFunctionName());
}

FInputDebugKeyBinding& FAngelscriptUEnhancedInputComponentBinds::BindDebugKey(
	UEnhancedInputComponent& InputComponent,
	FInputChord Chord,
	EInputEvent KeyEvent,
	FInputDebugKeyHandlerDynamicSignature Delegate,
	bool bExecuteWhenPaused)
{
	return InputComponent.BindDebugKey(
		Chord,
		KeyEvent,
		Delegate.GetUObject(),
		Delegate.GetFunctionName(),
		bExecuteWhenPaused);
}
