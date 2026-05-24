#pragma once

#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace AngelscriptFunctionalTestUtils
{
	inline FAngelscriptEngine& RequireCurrentEngine()
	{
		FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
		checkf(CurrentEngine != nullptr, TEXT("Functional test helpers require an active FAngelscriptEngineScope or subsystem-owned engine."));
		return *CurrentEngine;
	}

	inline UClass* CompileScriptModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const FString& Filename,
		const FString& InScriptSource,
		FName ExpectedGeneratedClassName)
	{
		FAngelscriptEngineScope EngineScope(Engine);

		if (!Test.TestTrue(
			*FString::Printf(TEXT("Test module '%s' should compile"), *ModuleName.ToString()),
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, InScriptSource)))
		{
			return nullptr;
		}

		UClass* ScriptClass = FindGeneratedClass(&Engine, ExpectedGeneratedClassName);
		Test.TestNotNull(
			*FString::Printf(TEXT("Test class '%s' should be generated"), *ExpectedGeneratedClassName.ToString()),
			ScriptClass);
		return ScriptClass;
	}

	inline void TickWorld(FAngelscriptEngine& Engine, UWorld& World, float DeltaTime, int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope WorldScope(Engine);
			World.Tick(ELevelTick::LEVELTICK_All, DeltaTime);

			for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
			{
				if (AActor* Actor = *ActorIt)
				{
					FAngelscriptEngineScope ActorScope(Engine, Actor);
					Actor->Tick(DeltaTime);

					TArray<UActorComponent*> Components;
					Actor->GetComponents(Components);
					for (UActorComponent* Component : Components)
					{
						if (Component != nullptr && Component->IsRegistered() && Component->IsComponentTickEnabled())
						{
							FAngelscriptEngineScope ComponentScope(Engine, Component);
							Component->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, &Component->PrimaryComponentTick);
						}
					}
				}
			}
		}
	}

	inline void TickWorld(UWorld& World, float DeltaTime, int32 NumTicks)
	{
		TickWorld(RequireCurrentEngine(), World, DeltaTime, NumTicks);
	}

	inline void BeginPlayActor(FAngelscriptEngine& Engine, AActor& Actor)
	{
		FAngelscriptEngineScope ActorScope(Engine, &Actor);

		if (!Actor.HasActorBegunPlay())
		{
			Actor.DispatchBeginPlay();
		}
	}

	inline void BeginPlayActor(AActor& Actor)
	{
		BeginPlayActor(RequireCurrentEngine(), Actor);
	}

	template <typename ActorType = AActor>
	inline ActorType* SpawnScriptActor(
		FAutomationTestBase& Test,
		FActorTestSpawner& Spawner,
		UClass* ScriptClass,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(),
		const FVector& Location = FVector::ZeroVector,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		if (!Test.TestNotNull(TEXT("Test actor class should be valid for spawning"), ScriptClass))
		{
			return nullptr;
		}

		return &Spawner.SpawnActorAt<ActorType>(Location, Rotation, SpawnParameters, ScriptClass);
	}

	template <typename PropertyType, typename ValueType>
	inline bool ReadPropertyValue(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		ValueType& OutValue)
	{
		if (!Test.TestNotNull(TEXT("Test object should be valid for reflected property reads"), Object))
		{
			return false;
		}

		PropertyType* Property = FindFProperty<PropertyType>(Object->GetClass(), PropertyName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Test property '%s' should exist"), *PropertyName.ToString()),
			Property))
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	inline bool ReadIntPropertyChecked(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		int32& OutValue)
	{
		return ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue);
	}

	inline bool ReadStringPropertyChecked(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		FString& OutValue)
	{
		return ReadPropertyValue<FStrProperty>(Test, Object, PropertyName, OutValue);
	}
}
