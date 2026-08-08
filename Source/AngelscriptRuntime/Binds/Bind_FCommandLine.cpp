#include "Bind_FCommandLine.h"

#include "AngelscriptBinds.h"

/**
 * Process command-line helpers.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | FString FCommandLine::Get();                                                             | Returns the process command line.                                                                                  |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void FCommandLine::Parse(const FString& CommandLine, TArray<FString>&out Tokens,         | Splits a command line into positional tokens and switches.                                                         |
 * | TArray<FString>&out Switches);                                                           | @param CommandLine Text to parse. @param Tokens Positional results. @param Switches Switch results.                |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindFCommandLine(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCommandLine");
		Binds.BindGlobalFunctionForTarget("FString Get()", &FAngelscriptFCommandLineBinds::Get);
		Binds.BindGlobalFunctionForTarget(
			"void Parse(const FString& CmdLine, TArray<FString>& OutTokens, TArray<FString>& OutSwitches)",
			&FAngelscriptFCommandLineBinds::Parse);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FCommandLine(
	TEXT("FCommandLine"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFCommandLine);
