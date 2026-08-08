#include "Bind_FPlatformMisc.h"

#include "HAL/PlatformMisc.h"

void FAngelscriptFPlatformMiscBinds::RequestExit(bool bForce)
{
	FPlatformMisc::RequestExit(bForce, nullptr);
}

void FAngelscriptFPlatformMiscBinds::RequestExitWithCallSite(bool bForce, const FString& CallSite)
{
	FPlatformMisc::RequestExit(bForce, *CallSite);
}

FString FAngelscriptFPlatformMiscBinds::GetEnvironmentVariable(const FString& VariableName)
{
	return FPlatformMisc::GetEnvironmentVariable(*VariableName);
}
