#pragma once

#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// -----------------------------------------------------------------------------
// FAngelscriptTestWorld
// -----------------------------------------------------------------------------
// Composition harness on top of FActorTestSpawner. Owns an FAngelscriptEngine
// reference and an FAngelscriptEngineScope for the duration of the harness,
// plus convenience helpers for spawning AS-script actors, dispatching
// BeginPlay / Tick / Destroy events, and driving precise per-actor or
// per-component ticks.
//
// Three tick driving paths are exposed and distinct:
//   - Tick(dt, n)           : World.Tick + manual TActorIterator dispatch
//                             (matches AngelscriptFunctionalTestUtils::TickWorld).
//   - TickViaManager(dt, n) : World.Tick only; relies on the test world
//                             scheduler. ReceiveTick may only fire on the
//                             first frame for a newly-registered actor.
//   - DispatchActorTick / DispatchComponentTick : direct loop over
//                             Actor->Tick / Component->TickComponent,
//                             bypassing the world scheduler. TickCount is
//                             always exactly NumTicks.
//
// Module lifecycle is intentionally NOT owned by this harness — callers are
// expected to keep the existing pattern:
//   ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };
// -----------------------------------------------------------------------------
struct FAngelscriptTestWorld
{
	FAngelscriptTestWorld(FAutomationTestBase& InTest, FAngelscriptEngine& InEngine)
		: Test(InTest)
		, Engine(InEngine)
		, EngineScope(InEngine)
	{
		Spawner.InitializeGameSubsystems();
		World = &Spawner.GetWorld();
		bIsValid = Test.TestNotNull(TEXT("FAngelscriptTestWorld should create a valid test world"), World);
	}

	FAngelscriptTestWorld(const FAngelscriptTestWorld&) = delete;
	FAngelscriptTestWorld(FAngelscriptTestWorld&&) = delete;
	FAngelscriptTestWorld& operator=(const FAngelscriptTestWorld&) = delete;
	FAngelscriptTestWorld& operator=(FAngelscriptTestWorld&&) = delete;

	bool                IsValid() const   { return bIsValid && World != nullptr; }
	UWorld&             GetWorld() const  { return *World; }
	FAngelscriptEngine& GetEngine() const { return Engine; }
	FActorTestSpawner&  GetSpawner()      { return Spawner; }

	template <typename ActorType = AActor>
	ActorType* SpawnActorOfClass(
		UClass* ActorClass,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(),
		const FVector& Location = FVector::ZeroVector,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		return AngelscriptFunctionalTestUtils::SpawnScriptActor<ActorType>(
			Test, Spawner, ActorClass, SpawnParameters, Location, Rotation);
	}

	void BeginPlay(AActor& Actor) const
	{
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, Actor);
	}

	void BeginPlayAll(TArrayView<AActor* const> Actors) const
	{
		for (AActor* Actor : Actors)
		{
			if (Actor != nullptr)
			{
				AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);
			}
		}
	}

	void Tick(float DeltaTime, int32 NumTicks) const
	{
		if (IsValid())
		{
			AngelscriptFunctionalTestUtils::TickWorld(Engine, *World, DeltaTime, NumTicks);
		}
	}

	void TickViaManager(float DeltaTime, int32 NumTicks) const
	{
		if (!IsValid())
		{
			return;
		}

		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope WorldScope(Engine);
			World->Tick(ELevelTick::LEVELTICK_All, DeltaTime);
		}
	}

	static void DispatchActorTick(FAngelscriptEngine& InEngine, AActor& Actor, float DeltaTime, int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope ActorScope(InEngine, &Actor);
			Actor.Tick(DeltaTime);
		}
	}

	static void DispatchComponentTick(FAngelscriptEngine& InEngine, UActorComponent& Component, float DeltaTime, int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope ComponentScope(InEngine, &Component);
			Component.TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, &Component.PrimaryComponentTick);
		}
	}

	void DispatchActorTick(AActor& Actor, float DeltaTime, int32 NumTicks) const
	{
		DispatchActorTick(Engine, Actor, DeltaTime, NumTicks);
	}

	void DispatchComponentTick(UActorComponent& Component, float DeltaTime, int32 NumTicks) const
	{
		DispatchComponentTick(Engine, Component, DeltaTime, NumTicks);
	}

	void DestroyAndDrain(AActor& Actor) const
	{
		Actor.Destroy();
		Tick(0.0f, 1);
	}

private:
	FAutomationTestBase&    Test;
	FAngelscriptEngine&     Engine;
	FActorTestSpawner       Spawner;
	FAngelscriptEngineScope EngineScope;
	UWorld*                 World = nullptr;
	bool                    bIsValid = false;
};


#endif // WITH_DEV_AUTOMATION_TESTS
