class UGameInstance;
class ULocalPlayer;
struct FUniqueNetIdRepl;

struct FAngelscriptUGameInstanceBinds
{
	static ULocalPlayer* FindLocalPlayerFromUniqueNetId(const UGameInstance* GameInstance, const FUniqueNetIdRepl& UniqueNetId);
};

#include "AngelscriptBinds.h"

#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

/**
 * UGameInstance manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.CreateInitialPlayer(FString& OutError);                         | Creates the initial local player.                                                                                    |
 * |                                                                                            | @param OutError Receives an error description when creation fails.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.CreateLocalPlayer(int32 ControllerId, FString& OutError,        | Creates a local player for a legacy controller identifier.                                                           |
 * |     bool bSpawnPlayerController);                                                          | @param OutError Receives an error description when creation fails.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.CreateLocalPlayer(FPlatformUserId UserId, FString& OutError,    | Creates a local player for a platform user.                                                                          |
 * |     bool bSpawnPlayerController);                                                          | @param OutError Receives an error description when creation fails.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 UGameInstance.AddLocalPlayer(ULocalPlayer NewPlayer, FPlatformUserId UserId);        | Adds an existing local player for the platform user and returns its index.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UGameInstance.RemoveLocalPlayer(ULocalPlayer ExistingPlayer);                         | Removes an existing local player.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 UGameInstance.GetNumLocalPlayers() const;                                            | Returns the number of registered local players.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.GetLocalPlayerByIndex(const int32 Index) const;                 | Returns the local player at the requested index.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | APlayerController UGameInstance.GetFirstLocalPlayerController(                             | Returns the first local player controller, optionally for the supplied world.                                        |
 * |     const UWorld World = nullptr) const;                                                   |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.FindLocalPlayerFromControllerId(                                | Returns the local player with the legacy controller identifier.                                                      |
 * |     const int32 ControllerId) const;                                                       |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.FindLocalPlayerFromUniqueNetId(                                 | Returns the local player associated with the replicated unique network identifier.                                   |
 * |     const FUniqueNetIdRepl& UniqueNetId) const;                                            |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULocalPlayer UGameInstance.GetFirstGamePlayer() const;                                     | Returns the first game local player.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UGameInstance(
	TEXT("UGameInstance"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UGameInstance_ = Binds.ExistingClassForTarget("UGameInstance");
		UGameInstance_.Method("ULocalPlayer CreateInitialPlayer(FString& OutError)", METHOD_TRIVIAL(UGameInstance, CreateInitialPlayer));
		UGameInstance_.Method("ULocalPlayer CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnPlayerController)", METHODPR_TRIVIAL(ULocalPlayer*, UGameInstance, CreateLocalPlayer, (int32, FString&, bool)));
		UGameInstance_.Method("ULocalPlayer CreateLocalPlayer(FPlatformUserId UserId, FString& OutError, bool bSpawnPlayerController)", METHODPR_TRIVIAL(ULocalPlayer*, UGameInstance, CreateLocalPlayer, (FPlatformUserId, FString&, bool)));
		UGameInstance_.Method("int32 AddLocalPlayer(ULocalPlayer NewPlayer, FPlatformUserId UserId)", METHODPR_TRIVIAL(int32, UGameInstance, AddLocalPlayer, (ULocalPlayer*, FPlatformUserId)));
		UGameInstance_.Method("bool RemoveLocalPlayer(ULocalPlayer ExistingPlayer)", METHOD_TRIVIAL(UGameInstance, RemoveLocalPlayer));
		UGameInstance_.Method("int32 GetNumLocalPlayers() const", METHOD_TRIVIAL(UGameInstance, GetNumLocalPlayers));
		UGameInstance_.Method("ULocalPlayer GetLocalPlayerByIndex(const int32 Index) const", METHOD_TRIVIAL(UGameInstance, GetLocalPlayerByIndex));
		UGameInstance_.Method("APlayerController GetFirstLocalPlayerController(const UWorld World = nullptr) const", METHOD_TRIVIAL(UGameInstance, GetFirstLocalPlayerController));
		UGameInstance_.Method("ULocalPlayer FindLocalPlayerFromControllerId(const int32 ControllerId) const", METHOD_TRIVIAL(UGameInstance, FindLocalPlayerFromControllerId));
		UGameInstance_.Method(
			"ULocalPlayer FindLocalPlayerFromUniqueNetId(const FUniqueNetIdRepl& UniqueNetId) const",
			&FAngelscriptUGameInstanceBinds::FindLocalPlayerFromUniqueNetId);
		UGameInstance_.Method("ULocalPlayer GetFirstGamePlayer() const", METHOD_TRIVIAL(UGameInstance, GetFirstGamePlayer));
	});

#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

ULocalPlayer* FAngelscriptUGameInstanceBinds::FindLocalPlayerFromUniqueNetId(
	const UGameInstance* GameInstance,
	const FUniqueNetIdRepl& UniqueNetId)
{
	return GameInstance->FindLocalPlayerFromUniqueNetId(*UniqueNetId);
}
