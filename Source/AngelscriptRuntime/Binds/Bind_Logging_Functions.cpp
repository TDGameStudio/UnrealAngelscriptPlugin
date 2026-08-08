#include "Bind_Logging.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"

#include "AngelscriptEngine.h"

void FAngelscriptLoggingBinds::Log(const FString& Text)
{
	UE_LOG(Angelscript, Log, TEXT("%s"), *Text);
}

void FAngelscriptLoggingBinds::LogInfo(const FString& Text)
{
	UE_LOG(Angelscript, Log, TEXT("[Information] %s"), *Text);
}

void FAngelscriptLoggingBinds::LogDisplay(const FString& Text)
{
	UE_LOG(Angelscript, Display, TEXT("[Display] %s"), *Text);
}

void FAngelscriptLoggingBinds::Error(const FString& Text)
{
	UE_LOG(Angelscript, Error, TEXT("%s"), *Text);
}

void FAngelscriptLoggingBinds::Warning(const FString& Text)
{
	UE_LOG(Angelscript, Warning, TEXT("%s"), *Text);
}

void FAngelscriptLoggingBinds::LogIf(const bool bCondition, const FString& Text)
{
	if (bCondition)
	{
		Log(Text);
	}
}

void FAngelscriptLoggingBinds::LogInfoIf(const bool bCondition, const FString& Text)
{
	if (bCondition)
	{
		LogInfo(Text);
	}
}

void FAngelscriptLoggingBinds::LogDisplayIf(const bool bCondition, const FString& Text)
{
	if (bCondition)
	{
		LogDisplay(Text);
	}
}

void FAngelscriptLoggingBinds::ErrorIf(const bool bCondition, const FString& Text)
{
	if (bCondition)
	{
		Error(Text);
	}
}

void FAngelscriptLoggingBinds::WarningIf(const bool bCondition, const FString& Text)
{
	if (bCondition)
	{
		Warning(Text);
	}
}

void FAngelscriptLoggingBinds::LogCategory(const FName& CategoryName, const FString& Text)
{
	FMsg::Logf(nullptr, 0, CategoryName, ELogVerbosity::Log, TEXT("%s"), *Text);
}

void FAngelscriptLoggingBinds::LogInfoCategory(const FName& CategoryName, const FString& Text)
{
	FMsg::Logf(nullptr, 0, CategoryName, ELogVerbosity::Log, TEXT("[Information] %s"), *Text);
}

void FAngelscriptLoggingBinds::LogDisplayCategory(const FName& CategoryName, const FString& Text)
{
	FMsg::Logf(nullptr, 0, CategoryName, ELogVerbosity::Display, TEXT("[Display] %s"), *Text);
}

void FAngelscriptLoggingBinds::ErrorCategory(const FName& CategoryName, const FString& Text)
{
	FMsg::Logf(nullptr, 0, CategoryName, ELogVerbosity::Error, TEXT("%s"), *Text);
}

void FAngelscriptLoggingBinds::WarningCategory(const FName& CategoryName, const FString& Text)
{
	FMsg::Logf(nullptr, 0, CategoryName, ELogVerbosity::Warning, TEXT("%s"), *Text);
}

void FAngelscriptLoggingBinds::LogCategoryIf(
	const bool bCondition,
	const FName& CategoryName,
	const FString& Text)
{
	if (bCondition)
	{
		LogCategory(CategoryName, Text);
	}
}

void FAngelscriptLoggingBinds::LogInfoCategoryIf(
	const bool bCondition,
	const FName& CategoryName,
	const FString& Text)
{
	if (bCondition)
	{
		LogInfoCategory(CategoryName, Text);
	}
}

void FAngelscriptLoggingBinds::LogDisplayCategoryIf(
	const bool bCondition,
	const FName& CategoryName,
	const FString& Text)
{
	if (bCondition)
	{
		LogDisplayCategory(CategoryName, Text);
	}
}

void FAngelscriptLoggingBinds::ErrorCategoryIf(
	const bool bCondition,
	const FName& CategoryName,
	const FString& Text)
{
	if (bCondition)
	{
		ErrorCategory(CategoryName, Text);
	}
}

void FAngelscriptLoggingBinds::WarningCategoryIf(
	const bool bCondition,
	const FName& CategoryName,
	const FString& Text)
{
	if (bCondition)
	{
		WarningCategory(CategoryName, Text);
	}
}

void FAngelscriptLoggingBinds::Throw(const FString& Text)
{
	FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Text));
}

void FAngelscriptLoggingBinds::ThrowIf(const bool bCondition, const FString& Text)
{
	if (bCondition)
	{
		Throw(Text);
	}
}

void FAngelscriptLoggingBinds::Print(
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	UKismetSystemLibrary::PrintString(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		Text,
		true,
		Duration > 0.f,
		Color,
		Duration);
}

void FAngelscriptLoggingBinds::PrintFromObject(
	const UObject* WorldContextObject,
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	UKismetSystemLibrary::PrintString(WorldContextObject, Text, true, true, Color, Duration);
}

void FAngelscriptLoggingBinds::PrintToScreen(
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	UKismetSystemLibrary::PrintString(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		Text,
		true,
		false,
		Color,
		Duration);
}

void FAngelscriptLoggingBinds::PrintDirectToScreen(
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color.ToFColor(true), Text);
		UE_LOG(
			Angelscript,
			Display,
			TEXT("[Display] PrintDirectToScreen queued: ScreenMessages=%s, EngineScreen=%s, EngineDisplay=%s, Text=%s"),
			GAreScreenMessagesEnabled ? TEXT("true") : TEXT("false"),
			GEngine->bEnableOnScreenDebugMessages ? TEXT("true") : TEXT("false"),
			GEngine->bEnableOnScreenDebugMessagesDisplay ? TEXT("true") : TEXT("false"),
			*Text);
	}
}

void FAngelscriptLoggingBinds::DrawDebugStringFromObject(
	const UObject* WorldContextObject,
	const FVector& TextLocation,
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	UKismetSystemLibrary::DrawDebugString(
		WorldContextObject,
		TextLocation,
		Text,
		nullptr,
		Color,
		Duration);
}

void FAngelscriptLoggingBinds::PrintWarning(
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	const FString WarningText = FString::Printf(TEXT("[Warning] ")) + Text;
	UKismetSystemLibrary::PrintString(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		WarningText,
		true,
		true,
		Color,
		Duration);
}

void FAngelscriptLoggingBinds::PrintError(
	const FString& Text,
	const float Duration,
	const FLinearColor Color)
{
	const FString ErrorText = FString::Printf(TEXT("[Error] ")) + Text;
	UKismetSystemLibrary::PrintString(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		ErrorText,
		true,
		true,
		Color,
		Duration);
}
