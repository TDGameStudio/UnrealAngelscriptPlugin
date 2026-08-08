#include "AngelscriptBinds.h"

#include "Bind_Logging_Functions.h"

namespace
{
	void BindLoggingFunctions(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionForTarget(
			"void Log(const FString& Text)",
			&FAngelscriptLoggingBinds::Log)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogInfo(const FString& Text)",
			&FAngelscriptLoggingBinds::LogInfo)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogDisplay(const FString& Text)",
			&FAngelscriptLoggingBinds::LogDisplay)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void Error(const FString& Text)",
			&FAngelscriptLoggingBinds::Error)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void Warning(const FString& Text)",
			&FAngelscriptLoggingBinds::Warning)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogIf(bool Condition, const FString& Text)",
			&FAngelscriptLoggingBinds::LogIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogInfoIf(bool Condition, const FString& Text)",
			&FAngelscriptLoggingBinds::LogInfoIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogDisplayIf(bool Condition, const FString& Text)",
			&FAngelscriptLoggingBinds::LogDisplayIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void ErrorIf(bool Condition, const FString& Text)",
			&FAngelscriptLoggingBinds::ErrorIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void WarningIf(bool Condition, const FString& Text)",
			&FAngelscriptLoggingBinds::WarningIf)
			.CompileOutIfNoLog();

		Binds.BindGlobalFunctionForTarget(
			"void Log(const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::LogCategory)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogInfo(const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::LogInfoCategory)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogDisplay(const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::LogDisplayCategory)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void Error(const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::ErrorCategory)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void Warning(const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::WarningCategory)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogIf(bool Condition, const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::LogCategoryIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogInfoIf(bool Condition, const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::LogInfoCategoryIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void LogDisplayIf(bool Condition, const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::LogDisplayCategoryIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void ErrorIf(bool Condition, const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::ErrorCategoryIf)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void WarningIf(bool Condition, const FName& CategoryName, const FString& Text)",
			&FAngelscriptLoggingBinds::WarningCategoryIf)
			.CompileOutIfNoLog();

		Binds.BindGlobalFunctionForTarget("void Throw(const FString& Text)", &FAngelscriptLoggingBinds::Throw);
		Binds.BindGlobalFunctionForTarget("void ThrowIf(bool Condition, const FString& Text)", &FAngelscriptLoggingBinds::ThrowIf);

		Binds.BindGlobalFunctionForTarget(
			"void Print(const FString& Text, float32 Duration = 5.f, FLinearColor Color = FLinearColor::LucBlue)",
			&FAngelscriptLoggingBinds::Print)
			.CompileOutIfNoLog()
			.WorldContext();
		Binds.BindGlobalFunctionForTarget(
			"void PrintFromObject(const UObject WorldContextObject, const FString& Text, float32 Duration = 0.f, FLinearColor Color = FLinearColor::LucBlue)",
			&FAngelscriptLoggingBinds::PrintFromObject)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void PrintToScreen(const FString& Text, float32 Duration = 0.f, FLinearColor Color = FLinearColor::LucBlue)",
			&FAngelscriptLoggingBinds::PrintToScreen)
			.CompileOutIfNoLog()
			.WorldContext();
		Binds.BindGlobalFunctionForTarget(
			"void PrintDirectToScreen(const FString& Text, float32 Duration = 5.f, FLinearColor Color = FLinearColor::LucBlue)",
			&FAngelscriptLoggingBinds::PrintDirectToScreen)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void DrawDebugStringFromObject(const UObject WorldContextObject, const FVector& TextLocation, const FString& Text, float32 Duration = 5.f, FLinearColor Color = FLinearColor::White)",
			&FAngelscriptLoggingBinds::DrawDebugStringFromObject)
			.CompileOutIfNoLog();
		Binds.BindGlobalFunctionForTarget(
			"void PrintWarning(const FString& Text, float32 Duration = 8.f, FLinearColor Color = FLinearColor::Yellow)",
			&FAngelscriptLoggingBinds::PrintWarning)
			.CompileOutIfNoLog()
			.WorldContext();
		Binds.BindGlobalFunctionForTarget(
			"void PrintError(const FString& Text, float32 Duration = 8.f, FLinearColor Color = FLinearColor::Red)",
			&FAngelscriptLoggingBinds::PrintError)
			.CompileOutIfNoLog()
			.WorldContext();
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Logging(
	TEXT("Logging.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindLoggingFunctions);
