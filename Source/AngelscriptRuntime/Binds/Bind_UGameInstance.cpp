#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UGameInstance(FAngelscriptBinds::EOrder::Late, [] {
	auto UGameInstance_ = FAngelscriptBinds::ExistingClass("UGameInstance");

	UGameInstance_.Method("ULocalPlayer CreateInitialPlayer(FString& OutError)", METHOD_TRIVIAL(UGameInstance, CreateInitialPlayer));

	UGameInstance_.Method("ULocalPlayer CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnPlayerController)", METHODPR_TRIVIAL(ULocalPlayer*, UGameInstance, CreateLocalPlayer, (int32, FString&, bool)));
	UGameInstance_.Method("ULocalPlayer CreateLocalPlayer(FPlatformUserId UserId, FString& OutError, bool bSpawnPlayerController)", METHODPR_TRIVIAL(ULocalPlayer*, UGameInstance, CreateLocalPlayer, (FPlatformUserId, FString&, bool)));

	UGameInstance_.Method("int32 AddLocalPlayer(ULocalPlayer NewPlayer, FPlatformUserId UserId)", METHODPR_TRIVIAL(int32, UGameInstance, AddLocalPlayer, (ULocalPlayer*, FPlatformUserId)));

	UGameInstance_.Method("bool RemoveLocalPlayer(ULocalPlayer ExistingPlayer)", METHOD_TRIVIAL(UGameInstance, RemoveLocalPlayer));

	UGameInstance_.Method("int32 GetNumLocalPlayers() const", METHOD_TRIVIAL(UGameInstance, GetNumLocalPlayers));
	UGameInstance_.Method("ULocalPlayer GetLocalPlayerByIndex(const int32 Index) const", METHOD_TRIVIAL(UGameInstance, GetLocalPlayerByIndex));
	UGameInstance_.Method("APlayerController GetFirstLocalPlayerController(const UWorld World = nullptr) const", METHOD_TRIVIAL(UGameInstance, GetFirstLocalPlayerController));
	UGameInstance_.Method("ULocalPlayer FindLocalPlayerFromControllerId(const int32 ControllerId) const", METHOD_TRIVIAL(UGameInstance, FindLocalPlayerFromControllerId));
	UGameInstance_.Method("ULocalPlayer FindLocalPlayerFromUniqueNetId(const FUniqueNetIdRepl& UniqueNetId) const",
		[](const UGameInstance* GameInstance, const FUniqueNetIdRepl& UniqueNetId) -> ULocalPlayer* { return GameInstance->FindLocalPlayerFromUniqueNetId(*UniqueNetId); });
	UGameInstance_.Method("ULocalPlayer GetFirstGamePlayer() const", METHOD_TRIVIAL(UGameInstance, GetFirstGamePlayer));
});
