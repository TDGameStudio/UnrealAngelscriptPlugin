#pragma once

#include "Components/PrimitiveComponent.h"

struct FAngelscriptUPrimitiveComponentBinds
{
	static FVector GetBoundingBoxExtents(const UPrimitiveComponent* Component);
	static FVector GetBoundsOrigin(const UPrimitiveComponent* Component);
	static FVector GetBoundsExtent(const UPrimitiveComponent* Component);
	static double GetBoundsRadius(const UPrimitiveComponent* Component);
	static bool GetSelectable(const UPrimitiveComponent* Component);
	static void SetSelectable(UPrimitiveComponent* Component, bool bSelectable);
	static void SetLightmapType(UPrimitiveComponent* Component, ELightmapType Type);
};
