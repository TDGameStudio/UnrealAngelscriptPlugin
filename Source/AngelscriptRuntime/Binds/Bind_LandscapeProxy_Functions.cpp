#include "Bind_LandscapeProxy_Functions.h"

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
