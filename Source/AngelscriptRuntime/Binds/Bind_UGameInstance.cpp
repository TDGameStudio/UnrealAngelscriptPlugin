#include "AngelscriptBinds.h"

#include "Engine/GameInstance.h"
#include "GameFramework/OnlineReplStructs.h"

#include "Bind_UGameInstance_Functions.h"

namespace
{
	void BindUGameInstance(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UGameInstance(
	TEXT("UGameInstance"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUGameInstance);
