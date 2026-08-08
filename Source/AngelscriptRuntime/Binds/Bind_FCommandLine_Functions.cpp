#include "Bind_FCommandLine_Functions.h"

#include "Misc/CommandLine.h"

FString FAngelscriptFCommandLineBinds::Get()
{
	return FString(FCommandLine::Get());
}

void FAngelscriptFCommandLineBinds::Parse(
	const FString& CommandLine,
	TArray<FString>& OutTokens,
	TArray<FString>& OutSwitches)
{
	FCommandLine::Parse(*CommandLine, OutTokens, OutSwitches);
}
