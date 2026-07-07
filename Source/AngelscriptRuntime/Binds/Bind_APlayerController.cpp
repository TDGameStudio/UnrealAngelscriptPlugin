#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Runtime/Engine/Classes/GameFramework/PlayerController.h"
#include "Runtime/Engine/Classes/GameFramework/Controller.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/GameFramework/PlayerState.h"
#include "Runtime/Engine/Classes/Camera/PlayerCameraManager.h"
#include "Runtime/Engine/Classes/Components/InputComponent.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AController(FAngelscriptBinds::EOrder::Late, [] {
	auto AController_ = FAngelscriptBinds::ExistingClass("AController");

	AController_.Method("APawn GetPawn() const",
	[](const AController* Controller) -> APawn*
	{
		if (Controller == nullptr)
			return nullptr;
		return Controller->GetPawn();
	});

	AController_.Method("ACharacter GetCharacter() const",
	[](const AController* Controller) -> ACharacter*
	{
		if (Controller == nullptr)
			return nullptr;
		return Controller->GetCharacter();
	});

	AController_.Method("bool IsLocalController() const",
	[](const AController* Controller) -> bool
	{
		if (Controller == nullptr)
			return false;
		return Controller->IsLocalController();
	});

	AController_.Method("bool IsPlayerController() const",
	[](const AController* Controller) -> bool
	{
		if (Controller == nullptr)
			return false;
		return Controller->IsPlayerController();
	});

	AController_.Method("bool IsLocalPlayerController() const",
	[](const AController* Controller) -> bool
	{
		if (Controller == nullptr)
			return false;
		return Controller->IsLocalPlayerController();
	});

	AController_.Method("FRotator GetControlRotation() const",
	[](const AController* Controller) -> FRotator
	{
		if (Controller == nullptr)
			return FRotator::ZeroRotator;
		return Controller->GetControlRotation();
	});

	AController_.Method("void SetControlRotation(const FRotator& NewRotation)",
	[](AController* Controller, const FRotator& NewRotation)
	{
		if (Controller != nullptr)
			Controller->SetControlRotation(NewRotation);
	});
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_APlayerController(FAngelscriptBinds::EOrder::Late, [] {
	auto APlayerController_ = FAngelscriptBinds::ExistingClass("APlayerController");

	APlayerController_.Method("void SetPlayer(UPlayer InPlayer)", METHOD_TRIVIAL(APlayerController, SetPlayer));
	APlayerController_.Method("ULocalPlayer GetLocalPlayer() const", METHOD_TRIVIAL(APlayerController, GetLocalPlayer));

	APlayerController_.Method("APlayerState GetPlayerState() const",
	[](const APlayerController* PC) -> APlayerState*
	{
		if (PC == nullptr)
			return nullptr;
		return PC->PlayerState;
	});

	APlayerController_.Method("APlayerCameraManager GetPlayerCameraManager() const",
	[](const APlayerController* PC) -> APlayerCameraManager*
	{
		if (PC == nullptr)
			return nullptr;
		return PC->PlayerCameraManager;
	});

	APlayerController_.Method("void SetViewTargetWithBlend(AActor NewViewTarget, float32 BlendTime = 0.0, EViewTargetBlendFunction BlendFunc = EViewTargetBlendFunction::VTBlend_Linear, float32 BlendExp = 0.0, bool bLockOutgoing = false)",
	[](APlayerController* PC, AActor* NewViewTarget, float BlendTime, EViewTargetBlendFunction BlendFunc, float BlendExp, bool bLockOutgoing)
	{
		if (PC != nullptr)
			PC->SetViewTargetWithBlend(NewViewTarget, BlendTime, BlendFunc, BlendExp, bLockOutgoing);
	});
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_APawn(FAngelscriptBinds::EOrder::Late, [] {
	auto APawn_ = FAngelscriptBinds::ExistingClass("APawn");

	APawn_.Method("AController GetController() const",
	[](const APawn* Pawn) -> AController*
	{
		if (Pawn == nullptr)
			return nullptr;
		return Pawn->GetController();
	});

	APawn_.Method("APlayerController GetPlayerController() const",
	[](const APawn* Pawn) -> APlayerController*
	{
		if (Pawn == nullptr)
			return nullptr;
		return Cast<APlayerController>(Pawn->GetController());
	});

	APawn_.Method("bool IsLocallyControlled() const",
	[](const APawn* Pawn) -> bool
	{
		if (Pawn == nullptr)
			return false;
		return Pawn->IsLocallyControlled();
	});

	APawn_.Method("bool IsPlayerControlled() const",
	[](const APawn* Pawn) -> bool
	{
		if (Pawn == nullptr)
			return false;
		return Pawn->IsPlayerControlled();
	});

	APawn_.Method("bool IsBotControlled() const",
	[](const APawn* Pawn) -> bool
	{
		if (Pawn == nullptr)
			return false;
		return Pawn->IsBotControlled();
	});

	APawn_.Method("APlayerState GetPlayerState() const",
	[](const APawn* Pawn) -> APlayerState*
	{
		if (Pawn == nullptr)
			return nullptr;
		return Pawn->GetPlayerState();
	});

	APawn_.Method("void AddMovementInput(FVector WorldDirection, float32 ScaleValue = 1.0, bool bForce = false)",
	[](APawn* Pawn, FVector WorldDirection, float ScaleValue, bool bForce)
	{
		if (Pawn != nullptr)
			Pawn->AddMovementInput(WorldDirection, ScaleValue, bForce);
	});

	APawn_.Method("void AddControllerYawInput(float32 Val)",
	[](APawn* Pawn, float Val)
	{
		if (Pawn != nullptr)
			Pawn->AddControllerYawInput(Val);
	});

	APawn_.Method("void AddControllerPitchInput(float32 Val)",
	[](APawn* Pawn, float Val)
	{
		if (Pawn != nullptr)
			Pawn->AddControllerPitchInput(Val);
	});
});
