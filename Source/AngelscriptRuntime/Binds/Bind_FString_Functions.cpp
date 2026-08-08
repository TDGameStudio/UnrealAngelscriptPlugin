#include "Bind_FString.h"

#include "AngelscriptEngine.h"
#include "Helper_ToString.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

void FAngelscriptFStringBinds::ConstructDefault(FString* Address)
{
	new(Address) FString();
}

void FAngelscriptFStringBinds::ConstructCopy(FString* Address, const FString& Other)
{
	new(Address) FString(Other);
}

void FAngelscriptFStringBinds::Destruct(FString& String)
{
	String.~FString();
}

bool FAngelscriptFStringBinds::Equals(const FString& Left, const FString& Right)
{
	return Left == Right;
}

int32 FAngelscriptFStringBinds::Compare(const FString& Left, const FString& Right)
{
	return Left.Compare(Right);
}

FString FAngelscriptFStringBinds::Add(const FString& Left, const FString& Right)
{
	return Left + Right;
}

TCHAR& FAngelscriptFStringBinds::Index(FString& String, int32 Index)
{
	if (!String.IsValidIndex(Index))
	{
		FAngelscriptEngine::Throw("String index out of bounds.");
		static TCHAR InvalidChar;
		return InvalidChar;
	}

	return String[Index];
}

const TCHAR& FAngelscriptFStringBinds::IndexConst(const FString& String, int32 Index)
{
	if (!String.IsValidIndex(Index))
	{
		FAngelscriptEngine::Throw("String index out of bounds.");
		static TCHAR InvalidChar;
		return InvalidChar;
	}

	return String[Index];
}

void FAngelscriptFStringBinds::RemoveAt(FString& String, int32 Index, int32 Count)
{
	String.RemoveAt(Index, Count);
}

FString FAngelscriptFStringBinds::ConvertTabsToSpaces(const FString& String, int32 SpacesPerTab)
{
	return String.ConvertTabsToSpaces(SpacesPerTab);
}

bool FAngelscriptFStringBinds::Split(
	const FString& String,
	const FString& Needle,
	FString& OutLeft,
	FString& OutRight,
	ESearchCase::Type SearchCase,
	ESearchDir::Type SearchDir)
{
	return String.Split(Needle, &OutLeft, &OutRight, SearchCase, SearchDir);
}

FString FAngelscriptFStringBinds::Replace(
	const FString& String,
	const FString& From,
	const FString& To,
	ESearchCase::Type SearchCase)
{
	return String.Replace(*From, *To, SearchCase);
}

int32 FAngelscriptFStringBinds::ReplaceInline(
	FString& String,
	const FString& SearchText,
	const FString& ReplacementText,
	ESearchCase::Type SearchCase)
{
	return String.ReplaceInline(*SearchText, *ReplacementText, SearchCase);
}

FString FAngelscriptFStringBinds::ReplaceCharWithEscapedChar(const FString& String)
{
	return String.ReplaceCharWithEscapedChar();
}

FString FAngelscriptFStringBinds::ReplaceEscapedCharWithChar(const FString& String)
{
	return String.ReplaceEscapedCharWithChar();
}

FString FAngelscriptFStringBinds::ToUpper(const FString& String)
{
	return String.ToUpper();
}

FString FAngelscriptFStringBinds::ToLower(const FString& String)
{
	return String.ToLower();
}

FString FAngelscriptFStringBinds::TrimQuotes(FString String, bool& OutQuotesRemoved)
{
	return String.TrimQuotes(&OutQuotesRemoved);
}

FString FAngelscriptFStringBinds::TrimStartAndEnd(const FString& String)
{
	return String.TrimStartAndEnd();
}

FString FAngelscriptFStringBinds::TrimStart(const FString& String)
{
	return String.TrimStart();
}

FString FAngelscriptFStringBinds::TrimEnd(const FString& String)
{
	return String.TrimEnd();
}

FString FAngelscriptFStringBinds::TrimChar(const FString& String, TCHAR CharacterToTrim)
{
	return String.TrimChar(CharacterToTrim);
}

FString FAngelscriptFStringBinds::ToDisplayName(FString& String, bool bIsBool)
{
	return FName::NameToDisplayString(String, bIsBool);
}

uint32 FAngelscriptFStringBinds::GetHash(const FString& String)
{
	return GetTypeHash(String);
}

FString FAngelscriptFStringBinds::AddConverted(FString& String, asCScriptFunction* ScriptFunction, void* Value)
{
	FString OutValue = String;
	const auto ToString = reinterpret_cast<FToStringHelper::FToStringFunction>(ScriptFunction->userData);
	ToString(Value, OutValue);
	return OutValue;
}

FString& FAngelscriptFStringBinds::AddAssignConverted(FString& String, asCScriptFunction* ScriptFunction, void* Value)
{
	const auto ToString = reinterpret_cast<FToStringHelper::FToStringFunction>(ScriptFunction->userData);
	ToString(Value, String);
	return String;
}

FString& FAngelscriptFStringBinds::AppendConverted(FString& String, asCScriptFunction* ScriptFunction, void* Value)
{
	const auto ToString = reinterpret_cast<FToStringHelper::FToStringFunction>(ScriptFunction->userData);
	ToString(Value, String);
	return String;
}

FString FAngelscriptFStringBinds::TypeToString(void* Object, asCScriptFunction* ScriptFunction)
{
	const auto ToString = reinterpret_cast<FToStringHelper::FToStringFunction>(ScriptFunction->userData);
	FString String;
	ToString(Object, String);
	return String;
}

void FAngelscriptFStringBinds::ConstructConverted(FString* String, asCScriptFunction* ScriptFunction, void* Value)
{
	new(String) FString();
	const auto ToString = reinterpret_cast<FToStringHelper::FToStringFunction>(ScriptFunction->userData);
	ToString(Value, *String);
}

FString FAngelscriptFStringBinds::Join(const TArray<FString>& StringArray, const FString& Separator)
{
	return FString::Join(StringArray, *Separator);
}

FString FAngelscriptFStringBinds::FromInt(int32 Number)
{
	return FString::FromInt(Number);
}

FString FAngelscriptFStringBinds::SanitizeFloat(double Value, int32 MinFractionalDigits)
{
	return FString::SanitizeFloat(Value, MinFractionalDigits);
}

FString FAngelscriptFStringBinds::FormatAsNumber(int32 Number)
{
	return FString::FormatAsNumber(Number);
}

FString FAngelscriptFStringBinds::Chr(TCHAR Character)
{
	return FString::Chr(Character);
}

FString FAngelscriptFStringBinds::ChrN(int32 NumCharacters, TCHAR Character)
{
	return FString::ChrN(NumCharacters, Character);
}

FString FAngelscriptFStringBinds::AddValue(const FString& String, void* ValuePtr, int TypeId)
{
	FString NewString = String;
	FToStringHelper::Generic_AppendToString(NewString, ValuePtr, TypeId);
	return NewString;
}

FString& FAngelscriptFStringBinds::AddAssignValue(FString& String, void* ValuePtr, int TypeId)
{
	FToStringHelper::Generic_AppendToString(String, ValuePtr, TypeId);
	return String;
}

FString& FAngelscriptFStringBinds::AppendValue(FString& String, void* ValuePtr, int TypeId)
{
	FToStringHelper::Generic_AppendToString(String, ValuePtr, TypeId);
	return String;
}

int32 FAngelscriptFStringBinds::ParseIntoArray(
	const FString& String,
	TArray<FString>& OutArray,
	const FString& Delimiter,
	bool bCullEmpty)
{
	return String.ParseIntoArray(OutArray, *Delimiter, bCullEmpty);
}

int32 FAngelscriptFStringBinds::ParseIntoArrayMulti(
	const FString& String,
	TArray<FString>& OutArray,
	const TArray<FString>& Delimiters,
	bool bCullEmpty)
{
	if (Delimiters.Num() > 16)
	{
		FAngelscriptEngine::Throw("More than 16 delimiters is not supported by ParseIntoArray.");
		return 0;
	}

	const TCHAR* DelimiterList[16];
	for (int32 Index = 0; Index < Delimiters.Num(); ++Index)
	{
		DelimiterList[Index] = *Delimiters[Index];
	}

	return String.ParseIntoArray(OutArray, DelimiterList, Delimiters.Num(), bCullEmpty);
}

int32 FAngelscriptFStringBinds::ParseIntoArrayLines(const FString& String, TArray<FString>& OutArray, bool bCullEmpty)
{
	return String.ParseIntoArrayLines(OutArray, bCullEmpty);
}

int32 FAngelscriptFStringBinds::ParseIntoArrayWhitespace(const FString& String, TArray<FString>& OutArray, bool bCullEmpty)
{
	return String.ParseIntoArrayWS(OutArray, nullptr, bCullEmpty);
}
