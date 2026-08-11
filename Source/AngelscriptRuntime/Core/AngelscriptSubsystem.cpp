#include "AngelscriptSubsystem.h"

#include "Engine/Engine.h"

UAngelscriptSubsystem::~UAngelscriptSubsystem() = default;

bool UAngelscriptSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsUnreachable() || !Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return true;
}

void UAngelscriptSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsurePrimaryEngineInitialized();
}

void UAngelscriptSubsystem::Deinitialize()
{
	ReleasePrimaryEngine();
	Super::Deinitialize();
}

UWorld* UAngelscriptSubsystem::GetTickableGameObjectWorld() const
{
	return nullptr;
}

ETickableTickType UAngelscriptSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : FTickableGameObject::GetTickableTickType();
}

bool UAngelscriptSubsystem::IsAllowedToTick() const
{
	return !IsTemplate() && bInitializedPrimaryEngine && PrimaryEngine != nullptr;
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
	if (PrimaryEngine == nullptr)
	{
		return;
	}

	if (PrimaryEngine->ShouldTick())
	{
		PrimaryEngine->Tick(DeltaTime);
	}

	FAngelscriptRuntimeReloadResult ReloadResult;
	if (PrimaryEngine->ConsumePackagedRuntimeReloadResult(ReloadResult))
	{
		OnRuntimeReloadCompleted.Broadcast(ReloadResult);
	}
}

EAngelscriptRuntimeReloadRequestStatus
UAngelscriptSubsystem::RequestRuntimeReload()
{
	return PrimaryEngine != nullptr
		? PrimaryEngine->RequestPackagedRuntimeReload()
		: EAngelscriptRuntimeReloadRequestStatus::ShuttingDown;
}

TStatId UAngelscriptSubsystem::GetStatId() const
{
	return GetStatID();
}

void UAngelscriptSubsystem::EnsurePrimaryEngineInitialized()
{
	if (bInitializedPrimaryEngine && PrimaryEngine != nullptr)
	{
		return;
	}

	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		PrimaryEngine = CurrentEngine;
		bOwnsPrimaryEngine = false;
		bInitializedPrimaryEngine = true;
		if (PrimaryEngine->GetScriptEngine() == nullptr)
		{
			UE_LOG(Angelscript, Display, TEXT("[EngineSubsystemStartup] Initializing ambient primary engine=%p."), PrimaryEngine);
			PrimaryEngine->Initialize();
		}
		else
		{
			UE_LOG(Angelscript, Verbose, TEXT("[EngineSubsystemStartup] Adopted ambient primary engine=%p."), PrimaryEngine);
		}
		return;
	}

	PrimaryEngine = &OwnedEngine;
	bOwnsPrimaryEngine = true;
	bInitializedPrimaryEngine = true;
	UE_LOG(Angelscript, Display, TEXT("[EngineSubsystemStartup] Created owned primary engine=%p."), PrimaryEngine);
	PrimaryEngine->Initialize();
}

UAngelscriptSubsystem* UAngelscriptSubsystem::Get()
{
	return GEngine != nullptr ? GEngine->GetEngineSubsystem<UAngelscriptSubsystem>() : nullptr;
}

void UAngelscriptSubsystem::ReleasePrimaryEngine()
{
	if (PrimaryEngine != nullptr && bOwnsPrimaryEngine)
	{
		PrimaryEngine->Shutdown();
	}

	PrimaryEngine = nullptr;
	bOwnsPrimaryEngine = false;
	bInitializedPrimaryEngine = false;
}
