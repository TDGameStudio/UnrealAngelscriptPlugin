#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

class AController;
class APawn;
class APlayerController;
class UInputComponent;
class ULevel;
class asCScriptFunction;

struct ANGELSCRIPTRUNTIME_API FAngelscriptActorBinds
{
	static void GetComponentsByClass(const AActor* Actor, TArray<UActorComponent*>& OutComponents, int TypeId);
	static void GetComponentsByClassWithExplicitClass(
		const AActor* Actor,
		UClass* ComponentClass,
		TArray<UActorComponent*>& OutComponents,
		int TypeId);
	static APawn* GetInstigator(const AActor* Actor);
	static AController* GetInstigatorController(const AActor* Actor);
	static UInputComponent* GetInputComponent(const AActor* Actor);
	static void EnableInput(AActor* Actor, APlayerController* PlayerController);
	static void DisableInput(AActor* Actor, APlayerController* PlayerController);

	static UActorComponent* CreateComponent(AActor* Actor, const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName);
	static UActorComponent* GetComponent(AActor* Actor, const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName);
	static UActorComponent* GetOrCreateComponent(AActor* Actor, const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName);
	static void GetAllComponents(AActor* Actor, UClass* ComponentClass, TArray<UActorComponent*>& OutComponents);

	static UActorComponent* GetComponentFromMeta(asCScriptFunction* Meta, AActor* Actor, const FName& WithName);
	static UActorComponent* GetOrCreateComponentFromMeta(asCScriptFunction* Meta, AActor* Actor, const FName& WithName);
	static UActorComponent* CreateComponentFromMeta(asCScriptFunction* Meta, AActor* Actor, const FName& WithName);

	static void GetComponentGeneric(AActor* Actor, const TSubclassOf<UObject>& ComponentClass, void* OutComponent, int TypeId, const FName& WithName)
	{
		*(UActorComponent**)OutComponent = GetComponent(Actor, ComponentClass.Get(), WithName);
	}

	static void GetOrCreateComponentGeneric(AActor* Actor, const TSubclassOf<UObject>& ComponentClass, void* OutComponent, int TypeId, const FName& WithName)
	{
		*(UActorComponent**)OutComponent = GetOrCreateComponent(Actor, ComponentClass.Get(), WithName);
	}

	static void CreateComponentGeneric(AActor* Actor, const TSubclassOf<UObject>& ComponentClass, void* OutComponent, int TypeId, const FName& WithName)
	{
		*(UActorComponent**)OutComponent = CreateComponent(Actor, ComponentClass.Get(), WithName);
	}

	static void GetAllComponentsGeneric(AActor* Actor, const TSubclassOf<UObject>& ComponentClass, void* OutComponentsArray, int TypeId, const FName& WithName)
	{
		GetAllComponents(Actor, ComponentClass.Get(), *(TArray<UActorComponent*>*)OutComponentsArray);
	}

	static AActor* SpawnActorFromMeta(asCScriptFunction* Meta, const FVector& Location, const FRotator& Rotation, const FName& Name, ULevel* Level);
	static AActor* SpawnActor(const TSubclassOf<AActor>& Class, const FVector& Location, const FRotator& Rotation, const FName& Name, bool bDeferredSpawn, ULevel* Level);
	static AActor* SpawnPersistentActor(const TSubclassOf<AActor>& Class, const FVector& Location, const FRotator& Rotation, const FName& Name, bool bDeferredSpawn);
	static void FinishSpawningActor(AActor* Actor);
	static void FinishSpawningActor_Transform(AActor* Actor, const FTransform& Transform);

	static void GetAllActorsOfClass(TArray<AActor*>& OutActors, int TypeId);
	static void GetAllActorsOfClassWithExplicitClass(UClass* ActorClass, TArray<AActor*>& OutActors, int TypeId);
	static void GetAllActorsOfClassWithTag(FName Tag, TArray<AActor*>& OutActors, int TypeId);
};
