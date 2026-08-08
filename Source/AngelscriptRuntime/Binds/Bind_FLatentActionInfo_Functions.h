#pragma once

#include "Engine/LatentActionManager.h"

struct FAngelscriptFLatentActionInfoBinds
{
	static void Construct(
		FLatentActionInfo* Address,
		int32 Linkage,
		int32 Uuid,
		FName FunctionName,
		UObject* CallbackTarget);
};
