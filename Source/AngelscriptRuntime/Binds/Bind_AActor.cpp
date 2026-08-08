#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "Bind_AActor_Functions.h"

#include "Components/InputComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

namespace
{
	void BindAActor(FAngelscriptBinds& Binds)
	{
		auto AActorType = Binds.ExistingClassForTarget("AActor");

		AActorType.Method("bool IsActorInitialized() const", METHOD_TRIVIAL(AActor, IsActorInitialized));
		AActorType.Method("bool HasActorBegunPlay() const", METHOD_TRIVIAL(AActor, HasActorBegunPlay));
		AActorType.Method("bool IsHidden() const", METHOD_TRIVIAL(AActor, IsHidden));
		AActorType.Method("FVector GetActorLocation() const", METHOD_TRIVIAL(AActor, GetActorLocation));
		AActorType.Method("FRotator GetActorRotation() const", METHOD_TRIVIAL(AActor, GetActorRotation));
		AActorType.Method("void SetActorScale3D(FVector NewScale3D)", METHOD_TRIVIAL(AActor, SetActorScale3D));
		AActorType.Method("void SetActorTickInterval(float32 TickInterval)", METHOD_TRIVIAL(AActor, SetActorTickInterval));
		AActorType.Method("FString GetActorNameOrLabel() const", METHOD_TRIVIAL(AActor, GetActorNameOrLabel));
		AActorType.Method(
			"UGameInstance GetGameInstance() const",
			METHODPR_TRIVIAL(UGameInstance*, AActor, GetGameInstance, () const));

		AActorType.Method(
			"void GetComponentsByClass(?& OutComponents) const",
			&FAngelscriptActorBinds::GetComponentsByClass);
		AActorType.Method(
			"void GetComponentsByClass(UClass ComponentClass, ?& OutComponents) const",
			&FAngelscriptActorBinds::GetComponentsByClassWithExplicitClass);
		AActorType.Method("APawn GetInstigator() const", &FAngelscriptActorBinds::GetInstigator);
		AActorType.Method(
			"AController GetInstigatorController() const",
			&FAngelscriptActorBinds::GetInstigatorController);
		AActorType.Method("UInputComponent GetInputComponent() const", &FAngelscriptActorBinds::GetInputComponent);
		AActorType.Method("void EnableInput(APlayerController PlayerController)", &FAngelscriptActorBinds::EnableInput);
		AActorType.Method("void DisableInput(APlayerController PlayerController)", &FAngelscriptActorBinds::DisableInput);
		AActorType.Method("void SetReplicates(bool bInReplicates)", &AActor::SetReplicates);

		Binds.BindGlobalFunctionForTarget(
			"void GetAllActorsOfClass(?& OutActors)",
			&FAngelscriptActorBinds::GetAllActorsOfClass);
		Binds.BindGlobalFunctionForTarget(
			"void GetAllActorsOfClass(UClass Class, ?& OutActors)",
			&FAngelscriptActorBinds::GetAllActorsOfClassWithExplicitClass);
		Binds.BindGlobalFunctionForTarget(
			"void __Actor_GetAllByClass(UClass Class, ?& OutActors)",
			&FAngelscriptActorBinds::GetAllActorsByClassUnchecked);
		Binds.BindGlobalFunctionForTarget(
			"void GetAllActorsOfClassWithTag(FName TagName, ?& OutActors)",
			&FAngelscriptActorBinds::GetAllActorsOfClassWithTag);

		Binds.BindGlobalFunctionForTarget(
			"AActor SpawnActor(const TSubclassOf<AActor>& Class, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator, const FName& Name = NAME_None, bool bDeferredSpawn = false, ULevel Level = nullptr)",
			FUNC(FAngelscriptActorBinds::SpawnActor))
			.DeterminesOutputType(0);
		Binds.BindGlobalFunctionForTarget(
			"void FinishSpawningActor(AActor Actor)",
			FUNC(FAngelscriptActorBinds::FinishSpawningActor));
		Binds.BindGlobalFunctionForTarget(
			"void FinishSpawningActor(AActor Actor, const FTransform& SpawnTransform)",
			FUNC(FAngelscriptActorBinds::FinishSpawningActor_Transform));
		Binds.BindGlobalFunctionForTarget(
			"AActor SpawnPersistentActor(const TSubclassOf<AActor>& Class, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator, const FName& Name = NAME_None, bool bDeferredSpawn = false)",
			FUNC(FAngelscriptActorBinds::SpawnPersistentActor))
			.DeterminesOutputType(0);
	}

	void BindActorPostReflectionAccessors(FAngelscriptBinds& Binds)
	{
		for (UClass* Class : TObjectRange<UClass>())
		{
			if (!Class->IsChildOf(AActor::StaticClass()))
				continue;

			const TSharedPtr<FAngelscriptType> Type = FAngelscriptType::GetByClass(
				Binds.GetTargetTypeDatabase(),
				Class);
			if (!Type.IsValid())
				continue;

			const FString ClassName = Type->GetAngelscriptTypeName();
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), ClassName);
			const FString FunctionDeclaration = FString::Printf(
				TEXT("%s Spawn(const FVector& Location = FVector::ZeroVector, ")
				TEXT("const FRotator& Rotation = FRotator::ZeroRotator, const FName& Name = NAME_None, ULevel Level = nullptr)"),
				*ClassName);
			Binds.BindGlobalFunctionForTarget(
				FunctionDeclaration,
				FUNC(FAngelscriptActorBinds::SpawnActorFromMeta),
				Class)
				.PassScriptFunctionAsFirstParam();
		}
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AActor(
	TEXT("AActor.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindAActor);

AS_FORCE_LINK const FAngelscriptBind Bind_Actors(
	TEXT("AActor.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	&BindActorPostReflectionAccessors);
