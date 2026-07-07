#include "AngelscriptEngineSubsystem.h"

#include "AngelscriptSubsystem.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
TOptional<bool> UAngelscriptEngineSubsystem::StartupIsEditorOverrideForTesting;
TOptional<bool> UAngelscriptEngineSubsystem::StartupIsRunningCommandletOverrideForTesting;
TFunction<FAngelscriptEngine*()> UAngelscriptEngineSubsystem::InitializeOverrideForTesting;
#endif

UAngelscriptEngineSubsystem::~UAngelscriptEngineSubsystem() = default;

bool UAngelscriptEngineSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsUnreachable() || !Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return ShouldBootstrapAngelscript();
}

void UAngelscriptEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsurePrimaryEngineInitialized();
}

void UAngelscriptEngineSubsystem::Deinitialize()
{
	ReleasePrimaryEngine();
	Super::Deinitialize();
}

UWorld* UAngelscriptEngineSubsystem::GetTickableGameObjectWorld() const
{
	return nullptr;
}

ETickableTickType UAngelscriptEngineSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : FTickableGameObject::GetTickableTickType();
}

bool UAngelscriptEngineSubsystem::IsAllowedToTick() const
{
	return !IsTemplate() && bInitializedPrimaryEngine && PrimaryEngine != nullptr;
}

bool UAngelscriptEngineSubsystem::IsTickableInEditor() const
{
	return true;
}

bool UAngelscriptEngineSubsystem::IsTickableWhenPaused() const
{
	return true;
}

void UAngelscriptEngineSubsystem::Tick(float DeltaTime)
{
	if (PrimaryEngine == nullptr || UAngelscriptSubsystem::HasAnyTickOwner())
	{
		return;
	}

	if (PrimaryEngine->ShouldTick())
	{
		PrimaryEngine->Tick(DeltaTime);
	}
}

TStatId UAngelscriptEngineSubsystem::GetStatId() const
{
	return GetStatID();
}

void UAngelscriptEngineSubsystem::EnsurePrimaryEngineInitialized()
{
	if (bInitializedPrimaryEngine && PrimaryEngine != nullptr)
	{
		if (FAngelscriptEngine::TryGetCurrentEngine() == nullptr)
		{
			FAngelscriptEngineContextStack::Push(PrimaryEngine);
			bPushedPrimaryEngineContext = true;
			UE_LOG(Angelscript, Verbose, TEXT("[EngineSubsystemStartup] Restored primary engine context=%p."), PrimaryEngine);
		}
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
			FAngelscriptEngineContextStack::Push(PrimaryEngine);
			bPushedPrimaryEngineContext = true;
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
		bPushedPrimaryEngineContext = false;
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
	FAngelscriptEngineContextStack::Push(PrimaryEngine);
	bPushedPrimaryEngineContext = true;
	UE_LOG(Angelscript, Display, TEXT("[EngineSubsystemStartup] Created owned primary engine=%p."), PrimaryEngine);
	PrimaryEngine->Initialize();
}

UAngelscriptEngineSubsystem* UAngelscriptEngineSubsystem::Get()
{
	return GEngine != nullptr ? GEngine->GetEngineSubsystem<UAngelscriptEngineSubsystem>() : nullptr;
}

void UAngelscriptEngineSubsystem::ReleasePrimaryEngine()
{
	if (PrimaryEngine != nullptr && bPushedPrimaryEngineContext && FAngelscriptEngineContextStack::Peek() == PrimaryEngine)
	{
		FAngelscriptEngineContextStack::Pop(PrimaryEngine);
	}

	if (PrimaryEngine != nullptr && bOwnsPrimaryEngine)
	{
		PrimaryEngine->Shutdown();
	}

	PrimaryEngine = nullptr;
	bOwnsPrimaryEngine = false;
	bInitializedPrimaryEngine = false;
	bUsesOverridePrimaryEngine = false;
	bPushedPrimaryEngineContext = false;
}

bool UAngelscriptEngineSubsystem::ShouldBootstrapAngelscript() const
{
	bool bIsEditor = GIsEditor;
	bool bIsRunningCommandlet = IsRunningCommandlet();

#if WITH_DEV_AUTOMATION_TESTS
	if (StartupIsEditorOverrideForTesting.IsSet())
	{
		bIsEditor = StartupIsEditorOverrideForTesting.GetValue();
	}
	if (StartupIsRunningCommandletOverrideForTesting.IsSet())
	{
		bIsRunningCommandlet = StartupIsRunningCommandletOverrideForTesting.GetValue();
	}
#endif

	return bIsEditor || bIsRunningCommandlet;
}

#if WITH_DEV_AUTOMATION_TESTS
void UAngelscriptEngineSubsystem::SetStartupEnvironmentOverrideForTesting(const TOptional<bool>& bIsEditorOverride, const TOptional<bool>& bIsRunningCommandletOverride)
{
	StartupIsEditorOverrideForTesting = bIsEditorOverride;
	StartupIsRunningCommandletOverrideForTesting = bIsRunningCommandletOverride;
}

void UAngelscriptEngineSubsystem::ClearStartupEnvironmentOverrideForTesting()
{
	StartupIsEditorOverrideForTesting.Reset();
	StartupIsRunningCommandletOverrideForTesting.Reset();
}

void UAngelscriptEngineSubsystem::SetInitializeOverrideForTesting(TFunction<FAngelscriptEngine*()> InOverride)
{
	InitializeOverrideForTesting = MoveTemp(InOverride);
}

void UAngelscriptEngineSubsystem::ResetInitializeStateForTesting()
{
	ClearStartupEnvironmentOverrideForTesting();
	InitializeOverrideForTesting = nullptr;
}
#endif
