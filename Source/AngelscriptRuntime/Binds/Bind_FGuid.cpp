#include "CoreMinimal.h"

struct FAngelscriptFGuidBinds
{
	static void ConstructParts(FGuid* Address, uint32 A, uint32 B, uint32 C, uint32 D);
	static void ConstructString(FGuid* Address, const FString& GuidString);
	static bool Equals(const FGuid& Guid, const FGuid& Other);
	static int Compare(const FGuid& Guid, const FGuid& Other);
	static FString ToString(const FGuid& Guid);
	static uint32 Hash(const FGuid& Guid);
};

#include "AngelscriptBinds.h"

/**
 * FGuid formats, construction, operations, and namespace helpers.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum EGuidFormats { Digits, DigitsWithHyphens, DigitsWithHyphensInBraces,                            | Selects a textual GUID representation.                                                                           |
 * |     DigitsWithHyphensInParentheses, HexValuesInBraces, UniqueObjectGuid, Short, Base36Encoded };     |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FGuid Guid(uint32 InA, uint32 InB, uint32 InC, uint32 InD);                                          | Constructs a GUID from its four words.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FGuid Guid(const FString& InGuidStr);                                                                | Parses a GUID string during construction.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares GUID values for equality.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bOrdered = Left < Right;                                                                        | Uses GUID lexical ordering; the same opCmp surface supports <=, >, and >=.                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32& Word = Guid[Index];                                                                          | Returns a mutable GUID word by index.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const uint32& Word = ConstGuid[Index];                                                               | Returns a const GUID word by index.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Guid.Invalidate();                                                                              | Clears the GUID to its invalid value.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Guid.IsValid() const;                                                                           | Reports whether any GUID word is nonzero.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Guid.ToString() const;                                                                       | Formats the GUID with the default representation.                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Guid.ToString(EGuidFormats Format) const;                                                    | Formats the GUID using the requested representation.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Guid.GetTypeHash() const;                                                                     | Returns the GUID hash.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FGuid FGuid::NewGuid();                                                                              | Creates a new platform-generated GUID.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FGuid::Parse(const FString& GuidString, FGuid& OutGuid);                                        | Parses any recognized GUID representation.                                                                       |
 * |                                                                                                      | @param OutGuid Receives the parsed value when the function returns true.                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FGuid::ParseExact(const FString& GuidString, EGuidFormats Format, FGuid& OutGuid);              | Parses only the requested GUID representation.                                                                   |
 * |                                                                                                      | @param OutGuid Receives the parsed value when the function returns true.                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_EGuidFormats(
	TEXT("EGuidFormats"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		auto EGuidFormats_ = Binds.EnumForTarget("EGuidFormats");
		EGuidFormats_["Digits"] = EGuidFormats::Digits;
		EGuidFormats_["DigitsWithHyphens"] = EGuidFormats::DigitsWithHyphens;
		EGuidFormats_["DigitsWithHyphensInBraces"] = EGuidFormats::DigitsWithHyphensInBraces;
		EGuidFormats_["DigitsWithHyphensInParentheses"] = EGuidFormats::DigitsWithHyphensInParentheses;
		EGuidFormats_["HexValuesInBraces"] = EGuidFormats::HexValuesInBraces;
		EGuidFormats_["UniqueObjectGuid"] = EGuidFormats::UniqueObjectGuid;
		EGuidFormats_["Short"] = EGuidFormats::Short;
		EGuidFormats_["Base36Encoded"] = EGuidFormats::Base36Encoded;
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FGuid(
	TEXT("FGuid"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGuid_ = Binds.ExistingClassForTarget("FGuid");

		FGuid_.Constructor(
			"void f(uint32 InA, uint32 InB, uint32 InC, uint32 InD)",
			&FAngelscriptFGuidBinds::ConstructParts,
			"FGuid",
			true);
		FGuid_.Constructor(
			"void f(const FString& InGuidStr)",
			&FAngelscriptFGuidBinds::ConstructString,
			"FGuid",
			true);
		FGuid_.Method("bool opEquals(const FGuid& Other) const", &FAngelscriptFGuidBinds::Equals);
		FGuid_.Method("int opCmp(const FGuid& Other) const", &FAngelscriptFGuidBinds::Compare);
		FGuid_.Method("uint32& opIndex(int32 Index)", METHODPR_TRIVIAL(uint32&, FGuid, operator[], (int32)));
		FGuid_.Method("const uint32& opIndex(int32 Index) const", METHODPR_TRIVIAL(const uint32&, FGuid, operator[], (int32) const));
		FGuid_.Method("void Invalidate()", METHOD_TRIVIAL(FGuid, Invalidate));
		FGuid_.Method("bool IsValid() const", METHOD_TRIVIAL(FGuid, IsValid));
		FGuid_.Method("FString ToString() const", &FAngelscriptFGuidBinds::ToString);
		FGuid_.Method("FString ToString(EGuidFormats Format) const", METHODPR_TRIVIAL(FString, FGuid, ToString, (EGuidFormats Format) const));
		FGuid_.Method("uint32 GetTypeHash() const", &FAngelscriptFGuidBinds::Hash);

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FGuid");
		Binds.BindGlobalFunctionForTarget("FGuid NewGuid()", FUNC_TRIVIAL(FGuid::NewGuid));
		Binds.BindGlobalFunctionForTarget("bool Parse(const FString& GuidString, FGuid& OutGuid)", FUNCPR_TRIVIAL(bool, FGuid::Parse, (const FString&, FGuid&)));
		Binds.BindGlobalFunctionForTarget("bool ParseExact(const FString& GuidString, EGuidFormats Format, FGuid& OutGuid)", FUNCPR_TRIVIAL(bool, FGuid::ParseExact, (const FString&, EGuidFormats, FGuid&)));
	});

void FAngelscriptFGuidBinds::ConstructParts(FGuid* Address, uint32 A, uint32 B, uint32 C, uint32 D)
{
	new (Address) FGuid(A, B, C, D);
}

void FAngelscriptFGuidBinds::ConstructString(FGuid* Address, const FString& GuidString)
{
	new (Address) FGuid(GuidString);
}

bool FAngelscriptFGuidBinds::Equals(const FGuid& Guid, const FGuid& Other)
{
	return Guid == Other;
}

int FAngelscriptFGuidBinds::Compare(const FGuid& Guid, const FGuid& Other)
{
	if (Guid < Other)
	{
		return -1;
	}
	if (Other < Guid)
	{
		return 1;
	}
	return 0;
}

FString FAngelscriptFGuidBinds::ToString(const FGuid& Guid)
{
	return Guid.ToString();
}

uint32 FAngelscriptFGuidBinds::Hash(const FGuid& Guid)
{
	return GetTypeHash(Guid);
}
