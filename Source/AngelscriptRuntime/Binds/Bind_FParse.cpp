#include "AngelscriptBinds.h"

#include "Bind_FParse_Functions.h"

namespace
{
	void BindFParse(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FParse");
		Binds.BindGlobalFunctionForTarget(
			"bool Value(const FString& Stream, const FString& Match, FString& Value)",
			&FAngelscriptFParseBinds::ValueString);
		Binds.BindGlobalFunctionForTarget(
			"bool Value(const FString& Stream, const FString& Match, float32& Value)",
			&FAngelscriptFParseBinds::ValueFloat);
		Binds.BindGlobalFunctionForTarget(
			"bool Value(const FString& Stream, const FString& Match, int& Value)",
			&FAngelscriptFParseBinds::ValueInt);
		Binds.BindGlobalFunctionForTarget(
			"bool Bool(const FString& Stream, const FString& Match, bool& OnOff)",
			&FAngelscriptFParseBinds::Bool);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FParse(
	TEXT("FParse"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFParse);
