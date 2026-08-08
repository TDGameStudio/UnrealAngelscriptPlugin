#include "Bind_APlayerController.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

APawn* FAngelscriptAPlayerControllerBinds::GetPawn(const AController* Controller)
{
	return Controller != nullptr ? Controller->GetPawn() : nullptr;
}

ACharacter* FAngelscriptAPlayerControllerBinds::GetCharacter(const AController* Controller)
{
	return Controller != nullptr ? Controller->GetCharacter() : nullptr;
}

bool FAngelscriptAPlayerControllerBinds::IsLocalController(const AController* Controller)
{
	return Controller != nullptr && Controller->IsLocalController();
}

bool FAngelscriptAPlayerControllerBinds::IsPlayerController(const AController* Controller)
{
	return Controller != nullptr && Controller->IsPlayerController();
}

bool FAngelscriptAPlayerControllerBinds::IsLocalPlayerController(const AController* Controller)
{
	return Controller != nullptr && Controller->IsLocalPlayerController();
}

FRotator FAngelscriptAPlayerControllerBinds::GetControlRotation(const AController* Controller)
{
	return Controller != nullptr ? Controller->GetControlRotation() : FRotator::ZeroRotator;
}

void FAngelscriptAPlayerControllerBinds::SetControlRotation(
	AController* Controller,
	const FRotator& NewRotation)
{
	if (Controller != nullptr)
	{
		Controller->SetControlRotation(NewRotation);
	}
}

APlayerState* FAngelscriptAPlayerControllerBinds::GetPlayerState(const APlayerController* PlayerController)
{
	return PlayerController != nullptr ? PlayerController->PlayerState : nullptr;
}

APlayerCameraManager* FAngelscriptAPlayerControllerBinds::GetPlayerCameraManager(
	const APlayerController* PlayerController)
{
	return PlayerController != nullptr ? PlayerController->PlayerCameraManager : nullptr;
}

void FAngelscriptAPlayerControllerBinds::SetViewTargetWithBlend(
	APlayerController* PlayerController,
	AActor* NewViewTarget,
	const float BlendTime,
	const EViewTargetBlendFunction BlendFunc,
	const float BlendExp,
	const bool bLockOutgoing)
{
	if (PlayerController != nullptr)
	{
		PlayerController->SetViewTargetWithBlend(
			NewViewTarget,
			BlendTime,
			BlendFunc,
			BlendExp,
			bLockOutgoing);
	}
}

AController* FAngelscriptAPlayerControllerBinds::GetController(const APawn* Pawn)
{
	return Pawn != nullptr ? Pawn->GetController() : nullptr;
}

APlayerController* FAngelscriptAPlayerControllerBinds::GetPlayerController(const APawn* Pawn)
{
	return Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool FAngelscriptAPlayerControllerBinds::IsLocallyControlled(const APawn* Pawn)
{
	return Pawn != nullptr && Pawn->IsLocallyControlled();
}

bool FAngelscriptAPlayerControllerBinds::IsPlayerControlled(const APawn* Pawn)
{
	return Pawn != nullptr && Pawn->IsPlayerControlled();
}

bool FAngelscriptAPlayerControllerBinds::IsBotControlled(const APawn* Pawn)
{
	return Pawn != nullptr && Pawn->IsBotControlled();
}

APlayerState* FAngelscriptAPlayerControllerBinds::GetPawnPlayerState(const APawn* Pawn)
{
	return Pawn != nullptr ? Pawn->GetPlayerState() : nullptr;
}

void FAngelscriptAPlayerControllerBinds::AddMovementInput(
	APawn* Pawn,
	const FVector WorldDirection,
	const float ScaleValue,
	const bool bForce)
{
	if (Pawn != nullptr)
	{
		Pawn->AddMovementInput(WorldDirection, ScaleValue, bForce);
	}
}

void FAngelscriptAPlayerControllerBinds::AddControllerYawInput(APawn* Pawn, const float Value)
{
	if (Pawn != nullptr)
	{
		Pawn->AddControllerYawInput(Value);
	}
}

void FAngelscriptAPlayerControllerBinds::AddControllerPitchInput(APawn* Pawn, const float Value)
{
	if (Pawn != nullptr)
	{
		Pawn->AddControllerPitchInput(Value);
	}
}
