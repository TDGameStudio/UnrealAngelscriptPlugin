#include "Bind_Logging.h"

#include "AngelscriptBinds.h"

/**
 * Global logging, exception, and on-screen diagnostic binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Log(const FString& Text);                                                             | Writes a standard log message to the default AngelScript category.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogInfo(const FString& Text);                                                         | Writes an informational log message to the default category.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogDisplay(const FString& Text);                                                      | Writes a display-level log message to the default category.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Error(const FString& Text);                                                           | Writes an error log message to the default category.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Warning(const FString& Text);                                                         | Writes a warning log message to the default category.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogIf(bool Condition, const FString& Text);                                           | Writes a standard message only when Condition is true.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogInfoIf(bool Condition, const FString& Text);                                       | Writes an informational message only when Condition is true.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogDisplayIf(bool Condition, const FString& Text);                                    | Writes a display-level message only when Condition is true.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void ErrorIf(bool Condition, const FString& Text);                                         | Writes an error message only when Condition is true.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void WarningIf(bool Condition, const FString& Text);                                       | Writes a warning message only when Condition is true.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Log(const FName& CategoryName, const FString& Text);                                  | Writes a standard message to the named log category.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogInfo(const FName& CategoryName, const FString& Text);                              | Writes an informational message to the named log category.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogDisplay(const FName& CategoryName, const FString& Text);                           | Writes a display-level message to the named log category.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Error(const FName& CategoryName, const FString& Text);                                | Writes an error message to the named log category.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Warning(const FName& CategoryName, const FString& Text);                              | Writes a warning message to the named log category.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogIf(bool Condition, const FName& CategoryName, const FString& Text);                | Writes a standard message to the named category only when Condition is true.                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogInfoIf(bool Condition, const FName& CategoryName, const FString& Text);            | Writes an informational message to the named category only when Condition is true.                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LogDisplayIf(bool Condition, const FName& CategoryName, const FString& Text);         | Writes a display-level message to the named category only when Condition is true.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void ErrorIf(bool Condition, const FName& CategoryName, const FString& Text);              | Writes an error message to the named category only when Condition is true.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void WarningIf(bool Condition, const FName& CategoryName, const FString& Text);            | Writes a warning message to the named category only when Condition is true.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Throw(const FString& Text);                                                           | Raises an AngelScript exception with Text.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void ThrowIf(bool Condition, const FString& Text);                                         | Raises an AngelScript exception with Text only when Condition is true.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void Print(const FString& Text, float32 Duration = 5.f,                                    | Prints to the current world on-screen/log diagnostic route.                                                          |
 * |     FLinearColor Color = FLinearColor::LucBlue);                                           | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void PrintFromObject(const UObject WorldContextObject, const FString& Text,                | Prints using WorldContextObject to resolve the target world.                                                         |
 * |     float32 Duration = 0.f, FLinearColor Color = FLinearColor::LucBlue);                   | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void PrintToScreen(const FString& Text, float32 Duration = 0.f,                            | Prints to the current world on-screen message route.                                                                 |
 * |     FLinearColor Color = FLinearColor::LucBlue);                                           | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void PrintDirectToScreen(const FString& Text, float32 Duration = 5.f,                      | Writes directly to the engine on-screen message system.                                                              |
 * |     FLinearColor Color = FLinearColor::LucBlue);                                           | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void DrawDebugStringFromObject(const UObject WorldContextObject,                           | Draws debug text in the world resolved from WorldContextObject.                                                      |
 * |     const FVector& TextLocation, const FString& Text, float32 Duration = 5.f,              | @param TextLocation World-space position in Unreal units.                                                            |
 * |     FLinearColor Color = FLinearColor::White);                                             | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void PrintWarning(const FString& Text, float32 Duration = 8.f,                             | Prints a warning diagnostic in the current world.                                                                    |
 * |     FLinearColor Color = FLinearColor::Yellow);                                            | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void PrintError(const FString& Text, float32 Duration = 8.f,                               | Prints an error diagnostic in the current world.                                                                     |
 * |     FLinearColor Color = FLinearColor::Red);                                               | @param Duration Display lifetime in seconds.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_Logging(
	TEXT("Logging.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
