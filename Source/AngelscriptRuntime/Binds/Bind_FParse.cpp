#include "CoreMinimal.h"

struct FAngelscriptFParseBinds
{
	static bool ValueString(const FString& Stream, const FString& Match, FString& Value);
	static bool ValueFloat(const FString& Stream, const FString& Match, float& Value);
	static bool ValueInt(const FString& Stream, const FString& Match, int& Value);
	static bool Bool(const FString& Stream, const FString& Match, bool& bValue);
};

#include "AngelscriptBinds.h"

/**
 * FParse namespace binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FParse::Value(const FString& Stream, const FString& Match, FString& Value);           | Parses the string value following Match.                                                                             |
 * |                                                                                            | @param Value Receives the parsed text on success.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FParse::Value(const FString& Stream, const FString& Match, float32& Value);           | Parses the floating-point value following Match.                                                                     |
 * |                                                                                            | @param Value Receives the parsed number on success.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FParse::Value(const FString& Stream, const FString& Match, int& Value);               | Parses the integer value following Match.                                                                            |
 * |                                                                                            | @param Value Receives the parsed number on success.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FParse::Bool(const FString& Stream, const FString& Match, bool& OnOff);               | Parses the Boolean value following Match.                                                                            |
 * |                                                                                            | @param OnOff Receives the parsed state on success.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FParse(
	TEXT("FParse"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

#include "Misc/Parse.h"

bool FAngelscriptFParseBinds::ValueString(const FString& Stream, const FString& Match, FString& Value)
{
	return FParse::Value(*Stream, *Match, Value);
}

bool FAngelscriptFParseBinds::ValueFloat(const FString& Stream, const FString& Match, float& Value)
{
	return FParse::Value(*Stream, *Match, Value);
}

bool FAngelscriptFParseBinds::ValueInt(const FString& Stream, const FString& Match, int& Value)
{
	return FParse::Value(*Stream, *Match, Value);
}

bool FAngelscriptFParseBinds::Bool(const FString& Stream, const FString& Match, bool& bValue)
{
	return FParse::Bool(*Stream, *Match, bValue);
}
