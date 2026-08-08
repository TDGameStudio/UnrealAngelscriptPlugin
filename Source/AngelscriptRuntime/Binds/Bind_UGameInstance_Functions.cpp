#include "Bind_UGameInstance_Functions.h"

#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

ULocalPlayer* FAngelscriptUGameInstanceBinds::FindLocalPlayerFromUniqueNetId(
	const UGameInstance* GameInstance,
	const FUniqueNetIdRepl& UniqueNetId)
{
	return GameInstance->FindLocalPlayerFromUniqueNetId(*UniqueNetId);
}
