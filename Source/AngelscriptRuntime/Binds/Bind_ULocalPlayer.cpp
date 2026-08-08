#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Runtime/Engine/Classes/Engine/LocalPlayer.h"

namespace
{
	void BindULocalPlayer(FAngelscriptBinds& Binds)
	{
		auto ULocalPlayer_ = Binds.ExistingClassForTarget("ULocalPlayer");

		ULocalPlayer_.Method("UGameInstance GetGameInstance() const", METHOD_TRIVIAL(ULocalPlayer, GetGameInstance));
		ULocalPlayer_.Method("int32 GetControllerId() const", METHOD_TRIVIAL(ULocalPlayer, GetControllerId));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_ULocalPlayer(
	TEXT("ULocalPlayer"),
	EAngelscriptBindPhase::ManualBindings,
	&BindULocalPlayer);
