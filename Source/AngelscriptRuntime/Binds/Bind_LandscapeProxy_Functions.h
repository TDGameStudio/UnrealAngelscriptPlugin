#pragma once

#include "CoreMinimal.h"

class ALandscapeProxy;

struct FAngelscriptLandscapeProxyBinds
{
	static bool GetHeightAtLocation(const ALandscapeProxy* LandscapeProxy, FVector Location, float& OutHeight);
};
