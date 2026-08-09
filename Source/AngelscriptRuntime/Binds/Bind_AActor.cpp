#include "Bind_AActor.h"

#include "AngelscriptBinds.h"
#include "AngelscriptType.h"

#include "Components/InputComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

/**
 * AActor binding surface (manual bindings and post-reflection actor factories).
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                      | Purpose / parameter notes                                                                    |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | bool Actor.IsActorInitialized() const;                                                           | Reports whether Unreal has completed actor initialization.                                   |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | bool Actor.HasActorBegunPlay() const;                                                            | Reports whether BeginPlay has already run.                                                   |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | bool Actor.IsHidden() const;                                                                     | Returns the actor's hidden state.                                                            |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | FVector Actor.GetActorLocation() const;                                                          | Returns the actor's world-space location.                                                    |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | FRotator Actor.GetActorRotation() const;                                                         | Returns the actor's world-space rotation.                                                    |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.SetActorScale3D(FVector NewScale3D);                                                  | Sets the actor's world-space scale.                                                          |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.SetActorTickInterval(float32 TickInterval);                                           | Sets the interval between actor ticks.                                                       |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | FString Actor.GetActorNameOrLabel() const;                                                       | Returns the editor label or runtime object name.                                             |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | UGameInstance Actor.GetGameInstance() const;                                                     | Returns the owning world's game instance.                                                    |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.GetComponentsByClass(                                                                 | Appends matching components without clearing the output array.                               |
 * |     ?& OutComponents) const;                                                                     | @param OutComponents Its element type selects the component class; existing entries are      |
 * |                                                                                                  | preserved.                                                                                   |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.GetComponentsByClass(                                                                 | Appends matching components without clearing the output array.                               |
 * |     UClass ComponentClass,                                                                       | @param ComponentClass Matches this class or subclasses; it must derive from the              |
 * |     ?& OutComponents) const;                                                                     | OutComponents element type.                                                                  |
 * |                                                                                                  | @param OutComponents Receives matches while preserving existing entries.                     |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | APawn Actor.GetInstigator() const;                                                               | Returns the pawn responsible for the actor.                                                  |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AController Actor.GetInstigatorController() const;                                               | Returns the instigator's controller.                                                         |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | UInputComponent Actor.GetInputComponent() const;                                                 | Returns the actor's input component when available.                                          |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.EnableInput(APlayerController PlayerController);                                      | Enables input through the supplied player controller.                                        |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.DisableInput(APlayerController PlayerController);                                     | Disables input through the supplied player controller.                                       |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor.SetReplicates(bool bInReplicates);                                                    | Enables or disables network replication.                                                     |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor::GetAllActorsOfClass(                                                                 | Appends actors without clearing the output array.                                            |
 * |     ?& OutActors);                                                                               | @param OutActors Its element type selects the actor class; existing entries are preserved.   |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor::GetAllActorsOfClass(                                                                 | Appends actors without clearing the output array.                                            |
 * |     UClass Class,                                                                                | @param Class Matches this actor class or subclasses; it must derive from the OutActors       |
 * |     ?& OutActors);                                                                               | element type.                                                                                |
 * |                                                                                                  | @param OutActors Receives matches while preserving existing entries.                         |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor::GetAllActorsOfClassWithTag(                                                          | Appends tagged actors whose class is inferred from the output array.                         |
 * |     FName TagName,                                                                               | @param TagName Entry in AActor::Tags; this is an Actor Tag, not a GameplayTag.               |
 * |     ?& OutActors);                                                                               | @param OutActors Its element type selects the actor class; existing entries are preserved.   |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AActor Actor::SpawnActor(                                                                        | Spawns with an explicit transform and complete spawn parameters.                            |
 * |     const TSubclassOf<AActor>& Class,                                                            | @param SpawnParameters controls owner, instigator, template, collision, naming, and deferred |
 * |     const FTransform& SpawnTransform,                                                            | construction. A null OverrideLevel uses normal dynamic/caller level resolution.              |
 * |     const FActorSpawnParameters& SpawnParameters);                                               |                                                                                              |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AActor Actor::SpawnPersistentActor(                                                              | Spawns with explicit parameters without inheriting the caller's streaming level.             |
 * |     const TSubclassOf<AActor>& Class,                                                            | @param SpawnParameters OverrideLevel is passed through unchanged.                            |
 * |     const FTransform& SpawnTransform,                                                            |                                                                                              |
 * |     const FActorSpawnParameters& SpawnParameters);                                               |                                                                                              |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AActor UWorld.SpawnActor(                                                                        | Spawns directly in this World using the supplied complete parameter object.                  |
 * |     const TSubclassOf<AActor>& Class,                                                            | @param SpawnParameters is passed to Unreal unchanged, including OverrideLevel.               |
 * |     const FTransform& SpawnTransform,                                                            |                                                                                              |
 * |     const FActorSpawnParameters& SpawnParameters);                                               |                                                                                              |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AActor Actor::SpawnActor(                                                                        | Spawns an actor class in the current world.                                                  |
 * |     const TSubclassOf<AActor>& Class,                                                            | @param Class Actor subclass to spawn; a null class raises a script exception.                |
 * |     const FVector& Location = FVector::ZeroVector,                                               | @param Name Requested UObject name; NAME_None lets Unreal select the name.                   |
 * |     const FRotator& Rotation = FRotator::ZeroRotator,                                            | @param bDeferredSpawn Defers construction so properties can be assigned before               |
 * |     const FName& Name = NAME_None,                                                               | Actor::FinishSpawningActor is called.                                                        |
 * |     bool bDeferredSpawn = false,                                                                 | @param Level Explicit destination level; nullptr tries the dynamic spawn level, then the     |
 * |     ULevel Level = nullptr);                                                                     | calling actor/component level, otherwise the world's default level.                          |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor::FinishSpawningActor(                                                                 | Completes a deferred spawn using the actor's current transform.                              |
 * |     AActor Actor);                                                                               | @param Actor Deferred-spawn result to finish; nullptr is ignored, while an actor that has    |
 * |                                                                                                  | begun play raises a script exception.                                                        |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | void Actor::FinishSpawningActor(                                                                 | Completes a deferred spawn using an explicit final transform.                                |
 * |     AActor Actor,                                                                                | @param Actor Deferred-spawn result to finish; nullptr is ignored, while an actor that has    |
 * |     const FTransform& SpawnTransform);                                                           | begun play raises a script exception.                                                        |
 * |                                                                                                  | @param SpawnTransform Passed to FinishSpawning instead of the actor's current transform.     |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | AActor Actor::SpawnPersistentActor(                                                              | Spawns without inheriting the caller's streaming level.                                      |
 * |     const TSubclassOf<AActor>& Class,                                                            | @param Class Actor subclass to spawn; a null class raises a script exception.                |
 * |     const FVector& Location = FVector::ZeroVector,                                               | @param Name Requested UObject name; NAME_None lets Unreal select the name.                   |
 * |     const FRotator& Rotation = FRotator::ZeroRotator,                                            | @param bDeferredSpawn Defers construction; the caller must finish the spawn.                 |
 * |     const FName& Name = NAME_None,                                                               |                                                                                              |
 * |     bool bDeferredSpawn = false);                                                                |                                                                                              |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | <ActorType> <ActorType>::Spawn(                                                                  | Spawns the reflected actor type with a strongly typed return value.                          |
 * |     const FVector& Location = FVector::ZeroVector,                                               | @param Name Requested UObject name; NAME_None lets Unreal select the name.                   |
 * |     const FRotator& Rotation = FRotator::ZeroRotator,                                            | @param Level Uses the same explicit/dynamic/caller/default resolution as SpawnActor.         |
 * |     const FName& Name = NAME_None,                                                               |                                                                                              |
 * |     ULevel Level = nullptr);                                                                     |                                                                                              |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 * | <ActorType> <ActorType>::Spawn(                                                                  | Spawns the reflected Actor type with explicit transform and parameters.        |
 * |     const FTransform& SpawnTransform,                                                            |                                                                                              |
 * |     const FActorSpawnParameters& SpawnParameters);                                               |                                                                                              |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_AActor(
	TEXT("AActor.Manual"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
			"void GetAllActorsOfClassWithTag(FName TagName, ?& OutActors)",
			&FAngelscriptActorBinds::GetAllActorsOfClassWithTag);

		Binds.BindGlobalFunctionForTarget(
			"AActor SpawnActor(const TSubclassOf<AActor>& Class, const FTransform& SpawnTransform, const FActorSpawnParameters& SpawnParameters)",
			FUNC(FAngelscriptActorBinds::SpawnActorWithParameters))
			.DeterminesOutputType(0);
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
			"AActor SpawnPersistentActor(const TSubclassOf<AActor>& Class, const FTransform& SpawnTransform, const FActorSpawnParameters& SpawnParameters)",
			FUNC(FAngelscriptActorBinds::SpawnPersistentActorWithParameters))
			.DeterminesOutputType(0);
		Binds.BindGlobalFunctionForTarget(
			"AActor SpawnPersistentActor(const TSubclassOf<AActor>& Class, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator, const FName& Name = NAME_None, bool bDeferredSpawn = false)",
			FUNC(FAngelscriptActorBinds::SpawnPersistentActor))
			.DeterminesOutputType(0);

		auto WorldType = Binds.ExistingClassForTarget("UWorld");
		WorldType.Method(
			"AActor SpawnActor(const TSubclassOf<AActor>& Class, const FTransform& SpawnTransform, const FActorSpawnParameters& SpawnParameters)",
			&FAngelscriptActorBinds::SpawnActorInWorld)
			.DeterminesOutputType(0);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_Actors(
	TEXT("AActor.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	[](FAngelscriptBinds& Binds)
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

			const FString SpawnWithParametersDeclaration = FString::Printf(
				TEXT("%s Spawn(const FTransform& SpawnTransform, const FActorSpawnParameters& SpawnParameters)"),
				*ClassName);
			Binds.BindGlobalFunctionForTarget(
				SpawnWithParametersDeclaration,
				FUNC(FAngelscriptActorBinds::SpawnActorFromMetaWithParameters),
				Class)
				.PassScriptFunctionAsFirstParam();
		}
	});
