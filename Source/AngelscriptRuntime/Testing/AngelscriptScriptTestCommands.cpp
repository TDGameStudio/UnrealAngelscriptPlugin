#include "Testing/AngelscriptScriptTestCommands.h"

#include "Testing/AngelscriptScriptTestRunner.h"

bool FAngelscriptScriptTestCommands::IsValidTimeout(
	double Seconds)
{
	return FMath::IsFinite(Seconds)
		&& Seconds > 0.0
		&& Seconds <= MaximumTimeoutSeconds;
}

FString FAngelscriptScriptTestCommands::Describe(
	const FAngelscriptScriptTestCommand& Command,
	const TCHAR* Fallback)
{
	return Command.Description.IsEmpty()
		? FString(Fallback)
		: Command.Description;
}
