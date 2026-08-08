#pragma once

#include "Engine/EngineTypes.h"

struct FAngelscriptUCollisionProfileBinds
{
	static ECollisionChannel ConvertToCollisionChannel(bool bTraceType, int32 Index);
	static EObjectTypeQuery ConvertToObjectType(ECollisionChannel CollisionChannel);
	static ETraceTypeQuery ConvertToTraceType(ECollisionChannel CollisionChannel);
};
