#include "Bind_UWorld_Functions.h"

#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"

#include "AngelscriptEngine.h"

UObject* FAngelscriptUWorldBinds::GetWorldContext()
{
	return FAngelscriptEngine::TryGetCurrentWorldContextObject();
}

UWorld* FAngelscriptUWorldBinds::GetCurrentWorld()
{
	return GEngine->GetWorldFromContextObject(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		EGetWorldErrorMode::ReturnNull);
}

AGameStateBase* FAngelscriptUWorldBinds::GetGameState(UWorld* World)
{
	return World->GetGameState();
}

bool FAngelscriptUWorldBinds::IsStartingUp(UWorld* World)
{
	return World->bStartup;
}

bool FAngelscriptUWorldBinds::IsTearingDown(UWorld* World)
{
	return World->bIsTearingDown;
}

UGameInstance* FAngelscriptUWorldBinds::GetGameInstance(UWorld* World)
{
	return World->GetGameInstance();
}

ALevelScriptActor* FAngelscriptUWorldBinds::GetWorldLevelScriptActor(UWorld* World)
{
	return World->GetLevelScriptActor();
}

ULevel* FAngelscriptUWorldBinds::GetPersistentLevel(UWorld* World)
{
	return World->PersistentLevel;
}

ALevelScriptActor* FAngelscriptUWorldBinds::GetLevelScriptActor(ULevel* Level)
{
	return Level->LevelScriptActor;
}

bool FAngelscriptUWorldBinds::IsLevelVisible(ULevel* Level)
{
	return Level->bIsVisible;
}

bool FAngelscriptUWorldBinds::IsLevelBeingRemoved(ULevel* Level)
{
	return Level->bIsBeingRemoved;
}

const TArray<AActor*>& FAngelscriptUWorldBinds::GetLevelActors(ULevel* Level)
{
	return Level->Actors;
}
