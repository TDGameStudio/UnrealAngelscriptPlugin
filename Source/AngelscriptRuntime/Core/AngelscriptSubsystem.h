#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "AngelscriptEngine.h"
#include "Subsystems/EngineSubsystem.h"

#include "AngelscriptSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAngelscriptRuntimeReloadCompleted,
	const FAngelscriptRuntimeReloadResult&,
	Result);

UCLASS()
class ANGELSCRIPTRUNTIME_API UAngelscriptSubsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual ~UAngelscriptSubsystem() override;
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

	UFUNCTION(BlueprintCallable, Category="AngelScript|Runtime Reload")
	EAngelscriptRuntimeReloadRequestStatus RequestRuntimeReload();

	UPROPERTY(BlueprintAssignable, Category="AngelScript|Runtime Reload")
	FAngelscriptRuntimeReloadCompleted OnRuntimeReloadCompleted;

	void EnsurePrimaryEngineInitialized();

	static UAngelscriptSubsystem* Get();

private:
	void ReleasePrimaryEngine();

	UPROPERTY()
	FAngelscriptEngine OwnedEngine;
	FAngelscriptEngine* PrimaryEngine = nullptr;
	bool bOwnsPrimaryEngine = false;
	bool bInitializedPrimaryEngine = false;
};
