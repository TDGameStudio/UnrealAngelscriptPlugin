#pragma once

#include "CoreMinimal.h"

class UObject;

struct FAngelscriptLoggingBinds
{
	static void Log(const FString& Text);
	static void LogInfo(const FString& Text);
	static void LogDisplay(const FString& Text);
	static void Error(const FString& Text);
	static void Warning(const FString& Text);
	static void LogIf(bool bCondition, const FString& Text);
	static void LogInfoIf(bool bCondition, const FString& Text);
	static void LogDisplayIf(bool bCondition, const FString& Text);
	static void ErrorIf(bool bCondition, const FString& Text);
	static void WarningIf(bool bCondition, const FString& Text);

	static void LogCategory(const FName& CategoryName, const FString& Text);
	static void LogInfoCategory(const FName& CategoryName, const FString& Text);
	static void LogDisplayCategory(const FName& CategoryName, const FString& Text);
	static void ErrorCategory(const FName& CategoryName, const FString& Text);
	static void WarningCategory(const FName& CategoryName, const FString& Text);
	static void LogCategoryIf(bool bCondition, const FName& CategoryName, const FString& Text);
	static void LogInfoCategoryIf(bool bCondition, const FName& CategoryName, const FString& Text);
	static void LogDisplayCategoryIf(bool bCondition, const FName& CategoryName, const FString& Text);
	static void ErrorCategoryIf(bool bCondition, const FName& CategoryName, const FString& Text);
	static void WarningCategoryIf(bool bCondition, const FName& CategoryName, const FString& Text);

	static void Throw(const FString& Text);
	static void ThrowIf(bool bCondition, const FString& Text);
	static void Print(const FString& Text, float Duration, FLinearColor Color);
	static void PrintFromObject(const UObject* WorldContextObject, const FString& Text, float Duration, FLinearColor Color);
	static void PrintToScreen(const FString& Text, float Duration, FLinearColor Color);
	static void PrintDirectToScreen(const FString& Text, float Duration, FLinearColor Color);
	static void DrawDebugStringFromObject(
		const UObject* WorldContextObject,
		const FVector& TextLocation,
		const FString& Text,
		float Duration,
		FLinearColor Color);
	static void PrintWarning(const FString& Text, float Duration, FLinearColor Color);
	static void PrintError(const FString& Text, float Duration, FLinearColor Color);
};
