#pragma once

#include "Cache/AngelscriptCacheRestore.h"

struct FAngelscriptCacheRuntimeState
{
	uint64 NextFunctionRoutePublicationOrdinal = 1;
	TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
		ESPMode::ThreadSafe> FunctionRouteSnapshot;
};
