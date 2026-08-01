#pragma once

#include "CoreMinimal.h"

#include "AngelscriptTest.generated.h"

class AActor;
class UActorComponent;
class ULatentAutomationCommand;
class UObject;
class UWorld;
struct FAngelscriptTestCommandBuilder;

/**
 * Fieldless value facade for the currently executing reflected script-test
 * leaf. AngelScript exposes these functions in the FAngelscriptTest namespace.
 */
USTRUCT(meta = (ForceAngelscriptBind))
struct ANGELSCRIPTRUNTIME_API FAngelscriptTest
{
	GENERATED_BODY()

	static FAngelscriptTestCommandBuilder Commands();

	static void CreateTestWorld(bool bInitializeGameSubsystems = true);
	static void DestroyTestWorld();
	static UWorld* GetTestWorld();
	static UObject* SpawnObject(UClass* ObjectClass, UObject* Outer = nullptr);
	static AActor* SpawnActor(
		const TSubclassOf<AActor>& ActorClass,
		const FVector& Location = FVector::ZeroVector,
		const FRotator& Rotation = FRotator::ZeroRotator);
	static UActorComponent* SpawnComponent(
		const TSubclassOf<UActorComponent>& ComponentClass,
		AActor* Owner,
		bool bRegister = true);
	static void BeginPlay(AActor* Actor);
	static void BeginPlayAll();
	static void TickWorld(float DeltaSeconds, int32 NumTicks = 1);
	static void TickActor(
		AActor* Actor,
		float DeltaSeconds,
		int32 NumTicks = 1);
	static void TickComponent(
		UActorComponent* Component,
		float DeltaSeconds,
		int32 NumTicks = 1);
	static void AdvanceTime(float DeltaSeconds, int32 NumTicks = 1);
	static void DestroyActor(AActor* Actor, bool bDrain = true);
};

/**
 * Fieldless fluent command facade. Every call resolves the active test leaf;
 * copied values never retain a Suite, UObject, World, or execution context.
 */
USTRUCT(meta = (ForceAngelscriptBind))
struct ANGELSCRIPTRUNTIME_API FAngelscriptTestCommandBuilder
{
	GENERATED_BODY()

	FAngelscriptTestCommandBuilder Do(
		FName Action,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder Then(
		FName Action,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder StartWhen(
		FName Condition,
		float TimeoutSeconds = 5.0f,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder Until(
		FName Condition,
		float TimeoutSeconds = 5.0f,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder WaitDelay(
		float Seconds,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder OnTearDown(
		FName Action,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder OnCleanup(
		FName Action,
		const FString& Description = FString()) const;
	FAngelscriptTestCommandBuilder AddLatentCommand(
		ULatentAutomationCommand* Command,
		float TimeoutSeconds = 5.0f) const;
};
