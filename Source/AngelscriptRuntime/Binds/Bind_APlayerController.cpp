#include "AngelscriptBinds.h"
#include "Bind_APlayerController_Functions.h"

#include "GameFramework/PlayerController.h"

namespace
{
	void BindController(FAngelscriptBinds& Binds)
	{
		auto AController_ = Binds.ExistingClassForTarget("AController");
		AController_.Method("APawn GetPawn() const", &FAngelscriptAPlayerControllerBinds::GetPawn);
		AController_.Method("ACharacter GetCharacter() const", &FAngelscriptAPlayerControllerBinds::GetCharacter);
		AController_.Method("bool IsLocalController() const", &FAngelscriptAPlayerControllerBinds::IsLocalController);
		AController_.Method("bool IsPlayerController() const", &FAngelscriptAPlayerControllerBinds::IsPlayerController);
		AController_.Method("bool IsLocalPlayerController() const", &FAngelscriptAPlayerControllerBinds::IsLocalPlayerController);
		AController_.Method("FRotator GetControlRotation() const", &FAngelscriptAPlayerControllerBinds::GetControlRotation);
		AController_.Method("void SetControlRotation(const FRotator& NewRotation)", &FAngelscriptAPlayerControllerBinds::SetControlRotation);
	}

	void BindPlayerController(FAngelscriptBinds& Binds)
	{
		auto APlayerController_ = Binds.ExistingClassForTarget("APlayerController");
		APlayerController_.Method("void SetPlayer(UPlayer InPlayer)", METHOD_TRIVIAL(APlayerController, SetPlayer));
		APlayerController_.Method("ULocalPlayer GetLocalPlayer() const", METHOD_TRIVIAL(APlayerController, GetLocalPlayer));
		APlayerController_.Method("APlayerState GetPlayerState() const", &FAngelscriptAPlayerControllerBinds::GetPlayerState);
		APlayerController_.Method("APlayerCameraManager GetPlayerCameraManager() const", &FAngelscriptAPlayerControllerBinds::GetPlayerCameraManager);
		APlayerController_.Method(
			"void SetViewTargetWithBlend(AActor NewViewTarget, float32 BlendTime = 0.0, EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Linear, float32 BlendExp = 0.0, bool bLockOutgoing = false)",
			&FAngelscriptAPlayerControllerBinds::SetViewTargetWithBlend);
	}

	void BindPawn(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AController(
	TEXT("AController.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindController);
AS_FORCE_LINK const FAngelscriptBind Bind_APlayerController(
	TEXT("APlayerController.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindPlayerController);
AS_FORCE_LINK const FAngelscriptBind Bind_APawn(
	TEXT("APawn.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindPawn);
