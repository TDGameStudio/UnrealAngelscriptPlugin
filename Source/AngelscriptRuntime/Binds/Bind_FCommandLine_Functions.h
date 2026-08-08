#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFCommandLineBinds
{
	static FString Get();
	static void Parse(const FString& CommandLine, TArray<FString>& OutTokens, TArray<FString>& OutSwitches);
};
