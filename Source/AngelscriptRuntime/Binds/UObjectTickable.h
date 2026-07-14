#pragma once

#include "CoreMinimal.h"
#include "UObjectTickable.generated.h"

UCLASS(Blueprintable)
class ANGELSCRIPTRUNTIME_API UObjectTickable : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	//UPROPERTY(ScriptReadWrite)
	UPROPERTY(BlueprintReadWrite)
	UWorld* World;

	UPROPERTY(EditDefaultsOnly, Category = "Tickable Object")
	bool bTickWhilePaused = false;

	virtual UWorld* GetWorld() const override;

	//UFUNCTION(ScriptCallable)
	UFUNCTION(BlueprintCallable)
	void SetWorld(UWorld* InWorld);

	//UFUNCTION(ScriptCallable)
	UFUNCTION(BlueprintCallable)
	void SetWorldContext(UObject* WorldContext);

	//UFUNCTION(ScriptCallable)
	void DestroyObject()
	{
		SetTickableTickType(ETickableTickType::Never);
		MarkAsGarbage();
	}

	UFUNCTION(BlueprintImplementableEvent)
	void BP_Tick(float DeltaTime);

	bool IsTickableWhenPaused() const override { return bTickWhilePaused; }
	UWorld* GetTickableGameObjectWorld() const { return GetWorld(); }
	bool IsTickable() const override { return !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed); }

	void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;
};
