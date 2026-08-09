#pragma once
#include "CoreMinimal.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputComponentScriptMixinLibrary.generated.h"

/**
 * ScriptMixin library to bind functions on UInputComponent
 * that are not BlueprintCallable by default.
 */
UCLASS(Meta = (ScriptMixin = "UInputComponent"))
class UInputComponentScriptMixinLibrary : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Bind a function to be called when a key bound to this action triggers a specific keyevent.
	 * Specified function must be a UFUNCTION() and takes a single FKey as its argument.
	 */
	UFUNCTION(BlueprintCallable)
	static void BindAction(UInputComponent* Component, const FName& ActionName, EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate)
	{
		FInputActionBinding AB( ActionName, KeyEvent );
		AB.ActionDelegate = Delegate;
		Component->AddActionBinding(AB);
	}

	/**
	 * Bind a specific key to a delegate. This bypasses any action bindings setup in project settings.
	 * Specified function must be a UFUNCTION() and takes a single FKey as its argument.
	 */
	UFUNCTION(BlueprintCallable)
	static void BindKey(UInputComponent* Component, const FKey& Key, EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate, bool bConsumeInput = true)
	{
		FInputKeyBinding KB(FInputChord(Key, false, false, false, false), KeyEvent);
		KB.bConsumeInput = bConsumeInput;
		KB.KeyDelegate = FInputActionUnifiedDelegate(Delegate);
		Component->KeyBindings.Emplace(MoveTemp(KB));
	}

	/**
	 * Bind a specific key chord to a delegate. This bypasses any action bindings setup in project settings.
	 * Specified function must be a UFUNCTION() and takes a single FKey as its argument.
	 */
	UFUNCTION(BlueprintCallable)
	static void BindChord(UInputComponent* Component, const FInputChord& Chord, EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate)
	{
		FInputKeyBinding KB(Chord, KeyEvent);
		KB.KeyDelegate = FInputActionUnifiedDelegate(Delegate);
		Component->KeyBindings.Emplace(MoveTemp(KB));
	}

	/**
	 * Bind a function to be called whenever a float axis bound to the specified axis name is changed.
	 * Specified function must be a UFUNCTION() and takes a single float as its argument.
	 */
	UFUNCTION(BlueprintCallable)
	static void BindAxis(UInputComponent* Component, const FName& AxisName, const FInputAxisHandlerDynamicSignature& Delegate)
	{
		FInputAxisBinding AB( AxisName );

		// Why this ugly cast you ask? FInputAxisUnifiedDelegate doesn't actually expose any
		// way of setting a dynamic delegate to it, even though it has one.
		*(TInputUnifiedDelegate<FInputAxisHandlerSignature, FInputAxisHandlerDynamicSignature>*)&AB.AxisDelegate = TInputUnifiedDelegate<FInputAxisHandlerSignature, FInputAxisHandlerDynamicSignature>(Delegate);

		Component->AxisBindings.Emplace(MoveTemp(AB));
	}

	/**
	 * Bind a function to be called whenever an axis specified by the given key changes. This bypasses any action bindings setup in project settings.
	 * Specified function must be a UFUNCTION() and takes a single float as its argument.
	 */
	UFUNCTION(BlueprintCallable)
	static void BindAxisKey(UInputComponent* Component, const FName& AxisKey, const FInputAxisHandlerDynamicSignature& Delegate)
	{
		FInputAxisKeyBinding AB(AxisKey);

		// Why this ugly cast you ask? FInputAxisUnifiedDelegate doesn't actually expose any
		// way of setting a dynamic delegate to it, even though it has one.
		*(TInputUnifiedDelegate<FInputAxisHandlerSignature, FInputAxisHandlerDynamicSignature>*)&AB.AxisDelegate = TInputUnifiedDelegate<FInputAxisHandlerSignature, FInputAxisHandlerDynamicSignature>(Delegate);

		Component->AxisKeyBindings.Emplace(MoveTemp(AB));
	}

	/**
	 * Bind a function to be called whenever a vector axis specified by the given key changes.
	 * Specified function must be a UFUNCTION() and takes a single FVector as its argument.
		GB.GestureDelegate = FInputGestureUnifiedDelegate(Delegate);
	 */
	UFUNCTION(BlueprintCallable)
	static void BindVectorAxis(UInputComponent* Component, const FKey& AxisKey, const FInputVectorAxisHandlerDynamicSignature& Delegate)
	{
		FInputVectorAxisBinding AB(AxisKey);

		// Why this ugly cast you ask? FInputVectorAxisUnifiedDelegate doesn't actually expose any
		// way of setting a dynamic delegate to it, even though it has one.
		*(TInputUnifiedDelegate<FInputVectorAxisHandlerSignature, FInputVectorAxisHandlerDynamicSignature>*)&AB.AxisDelegate = TInputUnifiedDelegate<FInputVectorAxisHandlerSignature, FInputVectorAxisHandlerDynamicSignature>(Delegate);

		Component->VectorAxisBindings.Emplace(MoveTemp(AB));
	}

};

/**
 * ScriptMixin library to bind functions on APlayerController for handling input.
 */
UCLASS(Meta = (ScriptMixin = "APlayerController"))
class UPlayerControllerInputScriptMixinLibrary : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Push an input component to handle input from this player controller.
	 */
	UFUNCTION(BlueprintCallable)
	static void PushInputComponent(APlayerController* PlayerController, UInputComponent* Component)
	{
		if (PlayerController != nullptr)
		{
			PlayerController->PushInputComponent(Component);
		}
	}

	/**
	 * Remove an input component so it no longer handles input from this player controller.
	 */
	UFUNCTION(BlueprintCallable)
	static void PopInputComponent(APlayerController* PlayerController, UInputComponent* Component)
	{
		if (PlayerController != nullptr)
		{
			PlayerController->PopInputComponent(Component);
		}
	}

	UFUNCTION(BlueprintCallable)
	static UPlayerInput* GetPlayerInput(APlayerController* PlayerController)
	{
		return PlayerController != nullptr ? PlayerController->PlayerInput : nullptr;
	}

};

UCLASS(Meta = (ScriptMixin = "UPlayerInput"))
class UPlayerInputScriptMixinLibrary : public UObject
{
	GENERATED_BODY()

public:
	static const TArray<FInputActionKeyMapping>& GetEmptyActionMappings()
	{
		static const TArray<FInputActionKeyMapping> EmptyMappings;
		return EmptyMappings;
	}

	static const TArray<FInputAxisKeyMapping>& GetEmptyAxisMappings()
	{
		static const TArray<FInputAxisKeyMapping> EmptyMappings;
		return EmptyMappings;
	}

	UFUNCTION(BlueprintCallable)
	static void AddActionMapping(UPlayerInput* PlayerInput, const FInputActionKeyMapping& KeyMapping)
	{
		if (PlayerInput != nullptr)
		{
			PlayerInput->AddActionMapping(KeyMapping);
		}
	}

	UFUNCTION(BlueprintCallable)
	static void RemoveActionMapping(UPlayerInput* PlayerInput, const FInputActionKeyMapping& KeyMapping)
	{
		if (PlayerInput != nullptr)
		{
			PlayerInput->RemoveActionMapping(KeyMapping);
		}
	}

	UFUNCTION(BlueprintCallable)
	static void AddAxisMapping(UPlayerInput* PlayerInput, const FInputAxisKeyMapping& KeyMapping)
	{
		if (PlayerInput != nullptr)
		{
			PlayerInput->AddAxisMapping(KeyMapping);
		}
	}

	UFUNCTION(BlueprintCallable)
	static void RemoveAxisMapping(UPlayerInput* PlayerInput, const FInputAxisKeyMapping& KeyMapping)
	{
		if (PlayerInput != nullptr)
		{
			PlayerInput->RemoveAxisMapping(KeyMapping);
		}
	}

	UFUNCTION(BlueprintCallable)
	static void ForceRebuildingKeyMaps(UPlayerInput* PlayerInput, const bool bRestoreDefaults = false)
	{
		if (PlayerInput != nullptr)
		{
			PlayerInput->ForceRebuildingKeyMaps(bRestoreDefaults);
		}
	}

	UFUNCTION(BlueprintCallable)
	static const TArray<FInputActionKeyMapping>& GetKeysForAction(UPlayerInput* PlayerInput, const FName ActionName)
	{
		return PlayerInput != nullptr ? PlayerInput->GetKeysForAction(ActionName) : GetEmptyActionMappings();
	}

	UFUNCTION(BlueprintCallable)
	static const TArray<FInputAxisKeyMapping>& GetKeysForAxis(UPlayerInput* PlayerInput, const FName AxisName)
	{
		return PlayerInput != nullptr ? PlayerInput->GetKeysForAxis(AxisName) : GetEmptyAxisMappings();
	}

	UFUNCTION(BlueprintCallable)
	static const TArray<FInputActionKeyMapping>& GetEngineDefinedActionMappings(UPlayerInput* PlayerInput)
	{
		return PlayerInput->GetEngineDefinedActionMappings();
	}

	UFUNCTION(BlueprintCallable)
	static const TArray<FInputAxisKeyMapping>& GetEngineDefinedAxisMappings(UPlayerInput* PlayerInput)
	{
		return PlayerInput->GetEngineDefinedAxisMappings();
	}

	UFUNCTION(BlueprintCallable)
	static void InvertAxis(UPlayerInput* PlayerInput, const FName AxisName)
	{
		PlayerInput->InvertAxis(AxisName);
	}

	UFUNCTION(BlueprintCallable)
	static void SetMouseSensitivity(UPlayerInput* PlayerInput, const float Sensitivity)
	{
		if (PlayerInput != nullptr)
		{
			PlayerInput->SetMouseSensitivity(Sensitivity);
		}
	}

	UFUNCTION(BlueprintCallable)
	static float GetMouseSensitivityX(UPlayerInput* PlayerInput)
	{
		return PlayerInput != nullptr ? PlayerInput->GetMouseSensitivityX() : 0.f;
	}

	UFUNCTION(BlueprintCallable)
	static float GetMouseSensitivityY(UPlayerInput* PlayerInput)
	{
		return PlayerInput != nullptr ? PlayerInput->GetMouseSensitivityY() : 0.f;
	}
};
