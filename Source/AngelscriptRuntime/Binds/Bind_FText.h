#pragma once

#include "CoreMinimal.h"
#include "Helper_CppType.h"
#include "UObject/TextProperty.h"

class asIScriptGeneric;

struct FTextType : TAngelscriptCppPropertyType<FTextProperty>
{
	FString GetAngelscriptTypeName() const override;
	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override;
	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;
};

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
