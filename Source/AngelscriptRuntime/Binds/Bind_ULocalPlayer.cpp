#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Runtime/Engine/Classes/Engine/LocalPlayer.h"

/**
 * Local-player accessors.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | UGameInstance LocalPlayer.GetGameInstance() const;                                       | Returns the game instance that owns this local player.                                                             |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | int32 LocalPlayer.GetControllerId() const;                                               | Returns the platform/controller identifier assigned to this local player.                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_ULocalPlayer(
	TEXT("ULocalPlayer"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto ULocalPlayer_ = Binds.ExistingClassForTarget("ULocalPlayer");

		ULocalPlayer_.Method("UGameInstance GetGameInstance() const", METHOD_TRIVIAL(ULocalPlayer, GetGameInstance));
		ULocalPlayer_.Method("int32 GetControllerId() const", METHOD_TRIVIAL(ULocalPlayer, GetControllerId));
	});
