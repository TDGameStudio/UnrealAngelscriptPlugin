#pragma once

#include "CoreMinimal.h"

// This hook can be used to forget what ensures we've seen already.
// Otherwise we will break on each ensure only once.
ANGELSCRIPTRUNTIME_API void AngelscriptForgetSeenEnsures();

// Makes it possible to turn off the actual debug break when you
// have a C++ debugger attached (errors are still logged on failing
// ensures however).
ANGELSCRIPTRUNTIME_API void AngelscriptDisableDebugBreaks();
ANGELSCRIPTRUNTIME_API void AngelscriptEnableDebugBreaks();
ANGELSCRIPTRUNTIME_API bool AreAngelscriptDebugBreaksEnabledForTesting();

struct FAngelscriptDebuggingBinds
{
	static void HandleEndPlayMap();
	static void DebugBreak();
	static bool Ensure(bool bCondition);
	static bool EnsureWithMessage(bool bCondition, const FString& Message);
	static bool EnsureAlways(bool bCondition);
	static bool EnsureAlwaysWithMessage(bool bCondition, const FString& Message);
	static void Check(bool bCondition);
	static void CheckWithMessage(bool bCondition, const FString& Message);
	static void Throw(const FString& Message);
	static TArray<FString> GetCallstack();
	static FString FormatCallstack();
};
