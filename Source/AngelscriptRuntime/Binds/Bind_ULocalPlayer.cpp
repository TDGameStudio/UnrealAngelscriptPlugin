#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Runtime/Engine/Classes/Engine/LocalPlayer.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_ULocalPlayer(FAngelscriptBinds::EOrder::Late, [] {
	auto ULocalPlayer_ = FAngelscriptBinds::ExistingClass("ULocalPlayer");

	ULocalPlayer_.Method("UGameInstance GetGameInstance() const", METHOD_TRIVIAL(ULocalPlayer, GetGameInstance));
	ULocalPlayer_.Method("int32 GetControllerId() const", METHOD_TRIVIAL(ULocalPlayer, GetControllerId));
});
