#include "AngelscriptSubsystem.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

int32 UAngelscriptSubsystem::ActiveTickOwners = 0;

UAngelscriptSubsystem::~UAngelscriptSubsystem() = default;

void UAngelscriptSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bInitialized = true;
	PrimaryEngine = FAngelscriptEngine::TryGetCurrentEngine();
	if (PrimaryEngine == nullptr)
	{
		PrimaryEngine = &OwnedEngine;
		FAngelscriptEngineContextStack::Push(PrimaryEngine);
		OwnedEngine.Initialize();
		bOwnsPrimaryEngine = true;
	}

	if (PrimaryEngine != nullptr)
	{
		++ActiveTickOwners;
	}
}

void UAngelscriptSubsystem::Deinitialize()
{
	if (PrimaryEngine != nullptr)
	{
		ActiveTickOwners = FMath::Max(0, ActiveTickOwners - 1);
	}

	if (bOwnsPrimaryEngine)
	{
		FAngelscriptEngineContextStack::Pop(PrimaryEngine);
		if (PrimaryEngine != nullptr)
		{
			PrimaryEngine->Shutdown();
		}
		bOwnsPrimaryEngine = false;
	}

	PrimaryEngine = nullptr;
	bInitialized = false;

	Super::Deinitialize();
}

UWorld* UAngelscriptSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

ETickableTickType UAngelscriptSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : FTickableGameObject::GetTickableTickType();
}

bool UAngelscriptSubsystem::IsAllowedToTick() const
{
	return !IsTemplate() && bInitialized && PrimaryEngine != nullptr;
}

bool UAngelscriptSubsystem::IsTickableInEditor() const
{
	return true;
}

bool UAngelscriptSubsystem::IsTickableWhenPaused() const
{
	return true;
}

void UAngelscriptSubsystem::Tick(float DeltaTime)
{
	if (PrimaryEngine != nullptr && PrimaryEngine->ShouldTick())
	{
		PrimaryEngine->Tick(DeltaTime);
	}
}

TStatId UAngelscriptSubsystem::GetStatId() const
{
	return GetStatID();
}

UAngelscriptSubsystem* UAngelscriptSubsystem::GetCurrent()
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::GetAmbientWorldContext(), EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UAngelscriptSubsystem>();
}

bool UAngelscriptSubsystem::HasAnyTickOwner()
{
	return ActiveTickOwners > 0;
}
