#pragma once

#include "CoreMinimal.h"

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
