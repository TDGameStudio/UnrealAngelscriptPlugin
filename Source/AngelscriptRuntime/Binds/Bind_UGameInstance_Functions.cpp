#include "Bind_UGameInstance.h"

#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

ULocalPlayer* FAngelscriptUGameInstanceBinds::FindLocalPlayerFromUniqueNetId(
	const UGameInstance* GameInstance,
	const FUniqueNetIdRepl& UniqueNetId)
{
	return GameInstance->FindLocalPlayerFromUniqueNetId(*UniqueNetId);
}
