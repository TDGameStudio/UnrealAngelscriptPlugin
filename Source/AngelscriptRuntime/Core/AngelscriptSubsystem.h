#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "AngelscriptEngine.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "AngelscriptSubsystem.generated.h"

UCLASS()
class ANGELSCRIPTRUNTIME_API UAngelscriptSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual ~UAngelscriptSubsystem() override;
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

	static UAngelscriptSubsystem* GetCurrent();
	static bool HasAnyTickOwner();

private:
	UPROPERTY()
	FAngelscriptEngine OwnedEngine;
	FAngelscriptEngine* PrimaryEngine = nullptr;
	bool bOwnsPrimaryEngine = false;
	bool bInitialized = false;
	static int32 ActiveTickOwners;
};
