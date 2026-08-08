#pragma once

#include "Camera/PlayerCameraManager.h"
#include "CoreMinimal.h"

class AActor;
class ACharacter;
class AController;
class APawn;
class APlayerCameraManager;
class APlayerController;
class APlayerState;

struct FAngelscriptAPlayerControllerBinds
{
	static APawn* GetPawn(const AController* Controller);
	static ACharacter* GetCharacter(const AController* Controller);
	static bool IsLocalController(const AController* Controller);
	static bool IsPlayerController(const AController* Controller);
	static bool IsLocalPlayerController(const AController* Controller);
	static FRotator GetControlRotation(const AController* Controller);
	static void SetControlRotation(AController* Controller, const FRotator& NewRotation);

	static APlayerState* GetPlayerState(const APlayerController* PlayerController);
	static APlayerCameraManager* GetPlayerCameraManager(const APlayerController* PlayerController);
	static void SetViewTargetWithBlend(
		APlayerController* PlayerController,
		AActor* NewViewTarget,
		float BlendTime,
		EViewTargetBlendFunction BlendFunc,
		float BlendExp,
		bool bLockOutgoing);

	static AController* GetController(const APawn* Pawn);
	static APlayerController* GetPlayerController(const APawn* Pawn);
	static bool IsLocallyControlled(const APawn* Pawn);
	static bool IsPlayerControlled(const APawn* Pawn);
	static bool IsBotControlled(const APawn* Pawn);
	static APlayerState* GetPawnPlayerState(const APawn* Pawn);
	static void AddMovementInput(APawn* Pawn, FVector WorldDirection, float ScaleValue, bool bForce);
	static void AddControllerYawInput(APawn* Pawn, float Value);
	static void AddControllerPitchInput(APawn* Pawn, float Value);
};
