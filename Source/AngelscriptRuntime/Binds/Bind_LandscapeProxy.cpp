#include "CoreMinimal.h"

class ALandscapeProxy;

struct FAngelscriptLandscapeProxyBinds
{
	static bool GetHeightAtLocation(const ALandscapeProxy* LandscapeProxy, FVector Location, float& OutHeight);
};

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
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto ALandscapeProxy_ = Binds.ExistingClassForTarget("ALandscapeProxy");
		ALandscapeProxy_.Method(
			"bool GetHeightAtLocation(FVector Location, float32& OutHeight) const",
			&FAngelscriptLandscapeProxyBinds::GetHeightAtLocation);
	});

#include "Runtime/Landscape/Classes/LandscapeProxy.h"

bool FAngelscriptLandscapeProxyBinds::GetHeightAtLocation(
	const ALandscapeProxy* LandscapeProxy,
	FVector Location,
	float& OutHeight)
{
	const TOptional<float> Height = LandscapeProxy->GetHeightAtLocation(Location);
	if (!Height.IsSet())
	{
		return false;
	}

	OutHeight = Height.GetValue();
	return true;
}
