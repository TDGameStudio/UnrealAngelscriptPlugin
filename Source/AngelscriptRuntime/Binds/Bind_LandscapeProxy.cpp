#include "Bind_LandscapeProxy.h"

#include "AngelscriptBinds.h"

/**
 * Landscape height query.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool Landscape.GetHeightAtLocation(FVector Location, float32&out Height) const;          | Samples the landscape height at a world-space location.                                                            |
 * |                                                                                          | @param Location World position to sample. @param Height Receives the sampled Z value on success.                   |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_ALandscapeProxy(
	TEXT("ALandscapeProxy.GetHeightAtLocation"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto ALandscapeProxy_ = Binds.ExistingClassForTarget("ALandscapeProxy");
		ALandscapeProxy_.Method(
			"bool GetHeightAtLocation(FVector Location, float32& OutHeight) const",
			&FAngelscriptLandscapeProxyBinds::GetHeightAtLocation);
	});
