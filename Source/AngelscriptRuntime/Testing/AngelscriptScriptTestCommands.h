#pragma once

#include "CoreMinimal.h"

struct FAngelscriptScriptTestCommand;

/**
 * Private policy shared by the reflected suite command bindings and execution
 * state machine. It intentionally is not a UObject and is not exported as an
 * AngelScript type.
 */
class FAngelscriptScriptTestCommands
{
public:
	static constexpr double DefaultTimeoutSeconds = 5.0;
	static constexpr double MaximumTimeoutSeconds = 15.0;

	static bool IsValidTimeout(double Seconds);
	static FString Describe(
		const FAngelscriptScriptTestCommand& Command,
		const TCHAR* Fallback);
};
