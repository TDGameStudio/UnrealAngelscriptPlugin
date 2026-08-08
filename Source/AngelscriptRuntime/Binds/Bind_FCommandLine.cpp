#include "AngelscriptBinds.h"

#include "Bind_FCommandLine_Functions.h"

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
