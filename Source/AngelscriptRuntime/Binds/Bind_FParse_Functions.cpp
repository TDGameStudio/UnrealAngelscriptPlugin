#include "Bind_FParse_Functions.h"

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
