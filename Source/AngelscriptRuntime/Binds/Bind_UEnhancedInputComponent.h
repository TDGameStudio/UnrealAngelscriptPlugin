#pragma once

#include "EnhancedInputComponent.h"

struct FAngelscriptUEnhancedInputComponentBinds
{
	static FEnhancedInputActionEventBinding& BindAction(
		UEnhancedInputComponent& InputComponent,
		const UInputAction* Action,
		ETriggerEvent TriggerEvent,
		FEnhancedInputActionHandlerDynamicSignature Delegate);
	static FInputDebugKeyBinding& BindDebugKey(
		UEnhancedInputComponent& InputComponent,
		FInputChord Chord,
		EInputEvent KeyEvent,
		FInputDebugKeyHandlerDynamicSignature Delegate,
		bool bExecuteWhenPaused);
};
