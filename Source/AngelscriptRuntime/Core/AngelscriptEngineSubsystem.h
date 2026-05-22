#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "AngelscriptEngine.h"
#include "Subsystems/EngineSubsystem.h"

#include "AngelscriptEngineSubsystem.generated.h"

UCLASS()
class ANGELSCRIPTRUNTIME_API UAngelscriptEngineSubsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual ~UAngelscriptEngineSubsystem() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override final;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	FAngelscriptEngine* GetEngine() const
	{
		return PrimaryEngine;
	}

	void EnsurePrimaryEngineInitialized();

	static UAngelscriptEngineSubsystem* Get();

#if WITH_DEV_AUTOMATION_TESTS
	static void SetStartupEnvironmentOverrideForTesting(const TOptional<bool>& bIsEditorOverride, const TOptional<bool>& bIsRunningCommandletOverride);
	static void ClearStartupEnvironmentOverrideForTesting();
	static void SetInitializeOverrideForTesting(TFunction<FAngelscriptEngine*()> InOverride);
	static void ResetInitializeStateForTesting();
#endif

private:
	friend struct FAngelscriptEngineSubsystemTestAccess;

	void ReleasePrimaryEngine();
	bool ShouldBootstrapAngelscript() const;

	UPROPERTY()
	FAngelscriptEngine OwnedEngine;
	FAngelscriptEngine* PrimaryEngine = nullptr;
	bool bOwnsPrimaryEngine = false;
	bool bInitializedPrimaryEngine = false;
	bool bUsesOverridePrimaryEngine = false;
	bool bPushedPrimaryEngineContext = false;

#if WITH_DEV_AUTOMATION_TESTS
	static TOptional<bool> StartupIsEditorOverrideForTesting;
	static TOptional<bool> StartupIsRunningCommandletOverrideForTesting;
	static TFunction<FAngelscriptEngine*()> InitializeOverrideForTesting;
#endif
};
