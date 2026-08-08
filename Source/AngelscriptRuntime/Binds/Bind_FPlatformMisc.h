#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFPlatformMiscBinds
{
	static void RequestExit(bool bForce);
	static void RequestExitWithCallSite(bool bForce, const FString& CallSite);
	static FString GetEnvironmentVariable(const FString& VariableName);
};
