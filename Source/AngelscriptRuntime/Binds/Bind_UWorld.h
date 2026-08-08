#pragma once

#include "CoreMinimal.h"

class AActor;
class AGameStateBase;
class ALevelScriptActor;
class UGameInstance;
class ULevel;
class UObject;
class UWorld;

struct FAngelscriptUWorldBinds
{
	static UObject* GetWorldContext();
	static UWorld* GetCurrentWorld();
	static AGameStateBase* GetGameState(UWorld* World);
	static bool IsStartingUp(UWorld* World);
	static bool IsTearingDown(UWorld* World);
	static UGameInstance* GetGameInstance(UWorld* World);
	static ALevelScriptActor* GetWorldLevelScriptActor(UWorld* World);
	static ULevel* GetPersistentLevel(UWorld* World);
	static ALevelScriptActor* GetLevelScriptActor(ULevel* Level);
	static bool IsLevelVisible(ULevel* Level);
	static bool IsLevelBeingRemoved(ULevel* Level);
	static const TArray<AActor*>& GetLevelActors(ULevel* Level);
};
