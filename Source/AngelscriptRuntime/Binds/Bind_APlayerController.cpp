#include "Bind_APlayerController.h"

#include "AngelscriptBinds.h"
#include "GameFramework/PlayerController.h"

/**
 * Controller, player-controller, and pawn convenience surface.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | APawn Controller.GetPawn() const;                                                                    | Returns the possessed pawn.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | ACharacter Controller.GetCharacter() const;                                                          | Returns the possessed pawn as a character.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Controller.IsLocalController() const;                                                           | Reports whether the controller is local.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Controller.IsPlayerController() const;                                                          | Reports whether this is a player controller.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Controller.IsLocalPlayerController() const;                                                     | Reports whether this is a local player controller.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator Controller.GetControlRotation() const;                                                      | Returns the control rotation.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Controller.SetControlRotation(const FRotator& NewRotation);                                     | Sets the control rotation.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void PlayerController.SetPlayer(UPlayer InPlayer);                                                   | Assigns the player object.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer PlayerController.GetLocalPlayer() const;                                                | Returns the local player.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | APlayerState PlayerController.GetPlayerState() const;                                                | Returns the player state.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | APlayerCameraManager PlayerController.GetPlayerCameraManager() const;                                | Returns the player camera manager.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void PlayerController.SetViewTargetWithBlend(AActor NewViewTarget,                                   | Blends to a new camera view target.                                                                              |
 * |     float32 BlendTime = 0.0,                                                                         | @param BlendFunc Selects the interpolation curve.                                                                |
 * |     EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Linear,                   | @param BlendExp Controls exponent-based curves.                                                                  |
 * |     float32 BlendExp = 0.0,                                                                          | @param bLockOutgoing Locks the outgoing view during the blend.                                                   |
 * |     bool bLockOutgoing = false);                                                                     |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AController Pawn.GetController() const;                                                              | Returns the pawn controller.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | APlayerController Pawn.GetPlayerController() const;                                                  | Returns the controller as a player controller.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Pawn.IsLocallyControlled() const;                                                               | Reports whether the pawn is locally controlled.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Pawn.IsPlayerControlled() const;                                                                | Reports whether a player controls the pawn.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Pawn.IsBotControlled() const;                                                                   | Reports whether AI controls the pawn.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | APlayerState Pawn.GetPlayerState() const;                                                            | Returns the pawn player state.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Pawn.AddMovementInput(FVector WorldDirection, float32 ScaleValue = 1.0, bool bForce = false);   | Adds movement input in world space.                                                                              |
 * |                                                                                                      | @param bForce Bypasses the pawn input-disabled check when true.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Pawn.AddControllerYawInput(float32 Val);                                                        | Adds yaw input to the controller.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Pawn.AddControllerPitchInput(float32 Val);                                                      | Adds pitch input to the controller.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_AController(
	TEXT("AController.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto AController_ = Binds.ExistingClassForTarget("AController");
		AController_.Method("APawn GetPawn() const", &FAngelscriptAPlayerControllerBinds::GetPawn);
		AController_.Method("ACharacter GetCharacter() const", &FAngelscriptAPlayerControllerBinds::GetCharacter);
		AController_.Method("bool IsLocalController() const", &FAngelscriptAPlayerControllerBinds::IsLocalController);
		AController_.Method("bool IsPlayerController() const", &FAngelscriptAPlayerControllerBinds::IsPlayerController);
		AController_.Method("bool IsLocalPlayerController() const", &FAngelscriptAPlayerControllerBinds::IsLocalPlayerController);
		AController_.Method("FRotator GetControlRotation() const", &FAngelscriptAPlayerControllerBinds::GetControlRotation);
		AController_.Method("void SetControlRotation(const FRotator& NewRotation)", &FAngelscriptAPlayerControllerBinds::SetControlRotation);
	});
AS_FORCE_LINK const FAngelscriptBind Bind_APlayerController(
	TEXT("APlayerController.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto APlayerController_ = Binds.ExistingClassForTarget("APlayerController");
		APlayerController_.Method("void SetPlayer(UPlayer InPlayer)", METHOD_TRIVIAL(APlayerController, SetPlayer));
		APlayerController_.Method("ULocalPlayer GetLocalPlayer() const", METHOD_TRIVIAL(APlayerController, GetLocalPlayer));
		APlayerController_.Method("APlayerState GetPlayerState() const", &FAngelscriptAPlayerControllerBinds::GetPlayerState);
		APlayerController_.Method("APlayerCameraManager GetPlayerCameraManager() const", &FAngelscriptAPlayerControllerBinds::GetPlayerCameraManager);
		APlayerController_.Method(
			"void SetViewTargetWithBlend(AActor NewViewTarget, float32 BlendTime = 0.0, EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Linear, float32 BlendExp = 0.0, bool bLockOutgoing = false)",
			&FAngelscriptAPlayerControllerBinds::SetViewTargetWithBlend);
	});
AS_FORCE_LINK const FAngelscriptBind Bind_APawn(
	TEXT("APawn.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto APawn_ = Binds.ExistingClassForTarget("APawn");
		APawn_.Method("AController GetController() const", &FAngelscriptAPlayerControllerBinds::GetController);
		APawn_.Method("APlayerController GetPlayerController() const", &FAngelscriptAPlayerControllerBinds::GetPlayerController);
		APawn_.Method("bool IsLocallyControlled() const", &FAngelscriptAPlayerControllerBinds::IsLocallyControlled);
		APawn_.Method("bool IsPlayerControlled() const", &FAngelscriptAPlayerControllerBinds::IsPlayerControlled);
		APawn_.Method("bool IsBotControlled() const", &FAngelscriptAPlayerControllerBinds::IsBotControlled);
		APawn_.Method("APlayerState GetPlayerState() const", &FAngelscriptAPlayerControllerBinds::GetPawnPlayerState);
		APawn_.Method(
			"void AddMovementInput(FVector WorldDirection, float32 ScaleValue = 1.0, bool bForce = false)",
			&FAngelscriptAPlayerControllerBinds::AddMovementInput);
		APawn_.Method("void AddControllerYawInput(float32 Val)", &FAngelscriptAPlayerControllerBinds::AddControllerYawInput);
		APawn_.Method("void AddControllerPitchInput(float32 Val)", &FAngelscriptAPlayerControllerBinds::AddControllerPitchInput);
	});
