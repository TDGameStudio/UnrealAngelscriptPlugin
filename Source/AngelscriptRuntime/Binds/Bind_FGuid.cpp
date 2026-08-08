#include "AngelscriptBinds.h"

#include "Bind_FGuid_Functions.h"

namespace
{
	void BindEGuidFormats(FAngelscriptBinds& Binds)
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
	}

	void BindFGuid(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_EGuidFormats(
	TEXT("EGuidFormats"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindEGuidFormats);

AS_FORCE_LINK const FAngelscriptBind Bind_FGuid(
	TEXT("FGuid"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFGuid);
