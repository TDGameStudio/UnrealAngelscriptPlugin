#pragma once

#include "CoreMinimal.h"

struct FAngelscriptOfflineBundleFixtureReadResult
{
	bool bSuccess = false;
	FString Error;
	int64 SymbolCount = 0;
	int64 AssetCount = 0;
};

/**
 * Strict producer-side fixture reader. This intentionally lives in the test
 * module; the no-UE standalone consumer owns its own streaming loader.
 */
class FAngelscriptOfflineBundleFixtureReader
{
public:
	static FAngelscriptOfflineBundleFixtureReadResult Read(
		const FString& BundleDirectory);
};
