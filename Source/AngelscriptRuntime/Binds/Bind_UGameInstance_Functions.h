#pragma once

class UGameInstance;
class ULocalPlayer;
struct FUniqueNetIdRepl;

struct FAngelscriptUGameInstanceBinds
{
	static ULocalPlayer* FindLocalPlayerFromUniqueNetId(const UGameInstance* GameInstance, const FUniqueNetIdRepl& UniqueNetId);
};
