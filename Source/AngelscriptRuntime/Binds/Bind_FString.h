#pragma once

#include "CoreMinimal.h"
#include "Helper_CppType.h"

struct FStringType : TAngelscriptCppPropertyType<FStrProperty>
{
	FString GetAngelscriptTypeName() const override;
	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;
	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;
	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const override;
	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override;
	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool IsOrdered(const FAngelscriptTypeUsage& Usage) const override;
	int32 CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const override;
};

class asCScriptFunction;
class asIScriptGeneric;

struct FAngelscriptFStringBinds
{
	static void ConstructDefault(FString* Address);
	static void ConstructCopy(FString* Address, const FString& Other);
	static void Destruct(FString& String);
	static bool Equals(const FString& Left, const FString& Right);
	static int32 Compare(const FString& Left, const FString& Right);
	static FString Add(const FString& Left, const FString& Right);
	static TCHAR& Index(FString& String, int32 Index);
	static const TCHAR& IndexConst(const FString& String, int32 Index);
	static void RemoveAt(FString& String, int32 Index, int32 Count);
	static FString ConvertTabsToSpaces(const FString& String, int32 SpacesPerTab);
	static bool Split(
		const FString& String,
		const FString& Needle,
		FString& OutLeft,
		FString& OutRight,
		ESearchCase::Type SearchCase,
		ESearchDir::Type SearchDir);
	static FString Replace(
		const FString& String,
		const FString& From,
		const FString& To,
		ESearchCase::Type SearchCase);
	static int32 ReplaceInline(
		FString& String,
		const FString& SearchText,
		const FString& ReplacementText,
		ESearchCase::Type SearchCase);
	static FString ReplaceCharWithEscapedChar(const FString& String);
	static FString ReplaceEscapedCharWithChar(const FString& String);
	static FString ToUpper(const FString& String);
	static FString ToLower(const FString& String);
	static FString TrimQuotes(FString String, bool& OutQuotesRemoved);
	static FString TrimStartAndEnd(const FString& String);
	static FString TrimStart(const FString& String);
	static FString TrimEnd(const FString& String);
	static FString TrimChar(const FString& String, TCHAR CharacterToTrim);
	static FString ToDisplayName(FString& String, bool bIsBool);
	static uint32 GetHash(const FString& String);

	static FString AddConverted(FString& String, asCScriptFunction* ScriptFunction, void* Value);
	static FString& AddAssignConverted(FString& String, asCScriptFunction* ScriptFunction, void* Value);
	static FString& AppendConverted(FString& String, asCScriptFunction* ScriptFunction, void* Value);
	static FString TypeToString(void* Object, asCScriptFunction* ScriptFunction);
	static void ConstructConverted(FString* String, asCScriptFunction* ScriptFunction, void* Value);

	static FString Join(const TArray<FString>& StringArray, const FString& Separator);
	static FString FromInt(int32 Number);
	static FString SanitizeFloat(double Value, int32 MinFractionalDigits);
	static FString FormatAsNumber(int32 Number);
	static FString Chr(TCHAR Character);
	static FString ChrN(int32 NumCharacters, TCHAR Character);
	static void Format(asIScriptGeneric* Generic);

	static FString ApplyFormatInt32(int32 Value, const FString& Specifier);
	static FString ApplyFormatUInt32(uint32 Value, const FString& Specifier);
	static FString ApplyFormatInt64(int64 Value, const FString& Specifier);
	static FString ApplyFormatUInt64(uint64 Value, const FString& Specifier);
	static FString ApplyFormatInt16(int16 Value, const FString& Specifier);
	static FString ApplyFormatUInt16(uint16 Value, const FString& Specifier);
	static FString ApplyFormatInt8(int8 Value, const FString& Specifier);
	static FString ApplyFormatUInt8(uint8 Value, const FString& Specifier);
	static FString ApplyFormatBool(bool Value, const FString& Specifier);
	static FString ApplyFormatFloat(float Value, const FString& Specifier);
	static FString ApplyFormatDouble(double Value, const FString& Specifier);
	static FString ApplyFormatString(const FString& Value, const FString& Specifier);
	static FString ApplyFormatValue(void* ValuePtr, int TypeId, const FString& Specifier);

	static FString AddValue(const FString& String, void* ValuePtr, int TypeId);
	static FString& AddAssignValue(FString& String, void* ValuePtr, int TypeId);
	static FString& AppendValue(FString& String, void* ValuePtr, int TypeId);
	static int32 ParseIntoArray(
		const FString& String,
		TArray<FString>& OutArray,
		const FString& Delimiter,
		bool bCullEmpty);
	static int32 ParseIntoArrayMulti(
		const FString& String,
		TArray<FString>& OutArray,
		const TArray<FString>& Delimiters,
		bool bCullEmpty);
	static int32 ParseIntoArrayLines(const FString& String, TArray<FString>& OutArray, bool bCullEmpty);
	static int32 ParseIntoArrayWhitespace(const FString& String, TArray<FString>& OutArray, bool bCullEmpty);
};
