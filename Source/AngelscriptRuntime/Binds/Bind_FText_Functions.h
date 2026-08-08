#pragma once

#include "CoreMinimal.h"

class asIScriptGeneric;

struct FAngelscriptFTextBinds
{
	static void ConstructDefault(FText* Address);
	static void ConstructCopy(FText* Address, const FText& Other);
	static void Destroy(FText& Text);
	static void AppendToString(void* Address, FString& OutString);

	static FText AsCultureInvariant(const FString& Value);
	static FText AsDate(const FDateTime& DateTime, EDateTimeStyle::Type DateStyle);
	static FText AsDateTime(
		const FDateTime& DateTime,
		EDateTimeStyle::Type DateStyle,
		EDateTimeStyle::Type TimeStyle);
	static FText AsTime(const FDateTime& DateTime, EDateTimeStyle::Type TimeStyle);
	static FText AsTimespan(const FTimespan& Timespan);

	template <typename ValueType>
	static FText AsNumber(ValueType Value, const FNumberFormattingOptions& Options)
	{
		return FText::AsNumber(Value, &Options);
	}

	static FText AsMemory(uint64 NumBytes);
	static void GenericTextFormat(asIScriptGeneric* Generic);
	static FText NamedTextFormat(
		const FText& Format,
		const TMap<FString, FFormatArgumentValue>& Arguments);
	static FText OrderedTextFormat(
		const FText& Format,
		const TArray<FFormatArgumentValue>& Arguments);
	static void GetFormatPatternParameters(const FText& Format, TArray<FString>& ParameterNames);
	static FText MakeLocalizableText(
		const FString& Namespace,
		const FString& Key,
		const FString& Text);
};
