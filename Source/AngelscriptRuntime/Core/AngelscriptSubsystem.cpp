#include "AngelscriptSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
TOptional<bool> UAngelscriptSubsystem::StartupIsEditorOverrideForTesting;
TOptional<bool> UAngelscriptSubsystem::StartupIsRunningCommandletOverrideForTesting;
TFunction<FAngelscriptEngine*()> UAngelscriptSubsystem::InitializeOverrideForTesting;
TWeakObjectPtr<UAngelscriptSubsystem> UAngelscriptSubsystem::SubsystemOverrideForTesting;
bool UAngelscriptSubsystem::bHasSubsystemOverrideForTesting = false;
#endif

UAngelscriptSubsystem::~UAngelscriptSubsystem() = default;

bool UAngelscriptSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsUnreachable() || !Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return ShouldBootstrapAngelscript();
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

#if WITH_DEV_AUTOMATION_TESTS
	if (InitializeOverrideForTesting)
	{
		if (FAngelscriptEngine* OverrideEngine = InitializeOverrideForTesting())
		{
			PrimaryEngine = OverrideEngine;
			bOwnsPrimaryEngine = false;
			bUsesOverridePrimaryEngine = true;
			bInitializedPrimaryEngine = true;
			UE_LOG(Angelscript, Verbose, TEXT("[EngineSubsystemStartup] Initialized with automation override engine=%p."), PrimaryEngine);
		}
		return;
	}
#endif

	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		PrimaryEngine = CurrentEngine;
		bOwnsPrimaryEngine = false;
		bUsesOverridePrimaryEngine = false;
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
	bUsesOverridePrimaryEngine = false;
	bInitializedPrimaryEngine = true;
	UE_LOG(Angelscript, Display, TEXT("[EngineSubsystemStartup] Created owned primary engine=%p."), PrimaryEngine);
	PrimaryEngine->Initialize();
}

UAngelscriptSubsystem* UAngelscriptSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bHasSubsystemOverrideForTesting)
	{
		return SubsystemOverrideForTesting.Get();
	}
#endif

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
	bUsesOverridePrimaryEngine = false;
}

bool UAngelscriptSubsystem::ShouldBootstrapAngelscript() const
{
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void UAngelscriptSubsystem::SetStartupEnvironmentOverrideForTesting(const TOptional<bool>& bIsEditorOverride, const TOptional<bool>& bIsRunningCommandletOverride)
{
	StartupIsEditorOverrideForTesting = bIsEditorOverride;
	StartupIsRunningCommandletOverrideForTesting = bIsRunningCommandletOverride;
}

void UAngelscriptSubsystem::ClearStartupEnvironmentOverrideForTesting()
{
	StartupIsEditorOverrideForTesting.Reset();
	StartupIsRunningCommandletOverrideForTesting.Reset();
}

void UAngelscriptSubsystem::SetInitializeOverrideForTesting(TFunction<FAngelscriptEngine*()> InOverride)
{
	InitializeOverrideForTesting = MoveTemp(InOverride);
}

void UAngelscriptSubsystem::SetSubsystemOverrideForTesting(UAngelscriptSubsystem* InSubsystem)
{
	SubsystemOverrideForTesting = InSubsystem;
	bHasSubsystemOverrideForTesting = true;
}

void UAngelscriptSubsystem::ResetInitializeStateForTesting()
{
	ClearStartupEnvironmentOverrideForTesting();
	InitializeOverrideForTesting = nullptr;
	SubsystemOverrideForTesting.Reset();
	bHasSubsystemOverrideForTesting = false;
}
#endif
