#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Bind_FText_Functions.h"
#include "Helper_CppType.h"
#include "Helper_GetTypeInfo.h"
#include "Helper_ToString.h"

#include "UObject/TextProperty.h"

struct FTextType : TAngelscriptCppPropertyType<FTextProperty>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FText");
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const override
	{
		FText& NativeValue = Usage.ResolvePrimitive<FText>(Address);

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.Value = TEXT("FText: \"") + NativeValue.ToString() + TEXT("\"");
		return true;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		return static_cast<FText*>(SourcePtr)->IdenticalTo(*static_cast<FText*>(DestinationPtr));
	}
};

namespace
{
	void BindFTextType(FAngelscriptBinds& Binds)
	{
		auto IdenticalMode = Binds.EnumForTarget("ETextIdenticalModeFlags");
		IdenticalMode["None"] = ETextIdenticalModeFlags::None;
		IdenticalMode["DeepCompare"] = ETextIdenticalModeFlags::DeepCompare;
		IdenticalMode["LexicalCompareInvariants"] = ETextIdenticalModeFlags::LexicalCompareInvariants;

		auto DateTimeStyle = Binds.EnumForTarget("EDateTimeStyle");
		DateTimeStyle["Default"] = EDateTimeStyle::Default;
		DateTimeStyle["Short"] = EDateTimeStyle::Short;
		DateTimeStyle["Medium"] = EDateTimeStyle::Medium;
		DateTimeStyle["Long"] = EDateTimeStyle::Long;
		DateTimeStyle["Full"] = EDateTimeStyle::Full;

		Binds.ValueClassForTarget<FText>("FText", FBindFlags());
	}

	void BindFTextInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FTextType>());
		auto Text = Binds.ExistingClassForTarget("FText");
		TGetStaticTypeInfo<FText>::SetForEngine(Binds.GetTargetEngine().GetScriptEngine(), Text.GetTypeInfo());
		FToStringHelper::Register(Binds, TEXT("FText"), &FAngelscriptFTextBinds::AppendToString);
	}

	void BindFTextFunctions(FAngelscriptBinds& Binds)
	{
		auto Text = Binds.ExistingClassForTarget("FText");
		Text.Constructor("void f()", &FAngelscriptFTextBinds::ConstructDefault, "FText", true).NoDiscard();
		Text.Constructor("void f(const FText& Other)", &FAngelscriptFTextBinds::ConstructCopy, "FText", true).NoDiscard();
		Text.Destructor("void f()", &FAngelscriptFTextBinds::Destroy).NativeDestructor("FText", true);

		Text.Method("bool IsEmpty() const", METHOD_TRIVIAL(FText, IsEmpty));
		Text.Method("bool IsEmptyOrWhitespace() const", METHOD_TRIVIAL(FText, IsEmptyOrWhitespace));
		Text.Method("bool IsTransient() const", METHOD_TRIVIAL(FText, IsTransient));
		Text.Method("bool IsCultureInvariant() const", METHOD_TRIVIAL(FText, IsCultureInvariant));
		Text.Method("bool IsInitializedFromString() const", METHOD_TRIVIAL(FText, IsInitializedFromString));
		Text.Method("bool IsFromStringTable() const", METHOD_TRIVIAL(FText, IsFromStringTable));
		Text.Method("bool IdenticalTo( const FText& Other, const ETextIdenticalModeFlags CompareModeFlags = ETextIdenticalModeFlags::None ) const", METHOD_TRIVIAL(FText, IdenticalTo));
		Text.Method("FText& opAssign(const FText& Other)", METHODPR_TRIVIAL(FText&, FText, operator=, (const FText&)));

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FText");
			Binds.BindGlobalFunctionForTarget("FText FromStringTable(const FName InTableId, const FString& InKey, const EStringTableLoadingPolicy InLoadingPolicy = EStringTableLoadingPolicy::FindOrLoad) no_discard", FUNC_TRIVIAL(FText::FromStringTable));
			Binds.BindGlobalFunctionForTarget("FText FromName(const FName& Val) no_discard", FUNC_TRIVIAL(FText::FromName));
			Binds.BindGlobalFunctionForTarget("FText FromString(const FString& Val) no_discard", FUNCPR_TRIVIAL(FText, FText::FromString, (const FString&)));
			Binds.BindGlobalFunctionForTarget("FText AsCultureInvariant(const FString& Val) no_discard", &FAngelscriptFTextBinds::AsCultureInvariant);
			Binds.BindGlobalFunctionForTarget("FText Join(const FText& Delimiter, const TArray<FFormatArgumentValue>& Args) no_discard", FUNCPR_TRIVIAL(FText, FText::Join, (const FText&, const TArray<FFormatArgumentValue>&)));
			Binds.BindGlobalFunctionForTarget("FText Join(const FText& Delimiter, const TArray<FText>& Args) no_discard", FUNCPR_TRIVIAL(FText, FText::Join, (const FText&, const TArray<FText>&)));

			Binds.BindGlobalFunctionForTarget("FText AsDate(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default) no_discard", &FAngelscriptFTextBinds::AsDate);
			Binds.BindGlobalFunctionForTarget("FText AsDateTime(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default, const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default) no_discard", &FAngelscriptFTextBinds::AsDateTime);
			Binds.BindGlobalFunctionForTarget("FText AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default) no_discard", &FAngelscriptFTextBinds::AsTime);
			Binds.BindGlobalFunctionForTarget("FText AsTimespan(const FTimespan& Timespan) no_discard", &FAngelscriptFTextBinds::AsTimespan);

			Binds.BindGlobalFunctionForTarget("FText AsNumber(float32 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<float>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(float64 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<double>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(int8 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<int8>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(int16 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<int16>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(int32 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<int32>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(int64 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<int64>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(uint8 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<uint8>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(uint16 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<uint16>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(uint32 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<uint32>);
			Binds.BindGlobalFunctionForTarget("FText AsNumber(uint64 Val, const FNumberFormattingOptions& Options) no_discard", &FAngelscriptFTextBinds::AsNumber<uint64>);
			Binds.BindGlobalFunctionForTarget("FText AsMemory(uint64 NumBytes) no_discard", &FAngelscriptFTextBinds::AsMemory);

			Binds.BindGlobalGenericFunctionForTarget("FText Format(const FText& Format, const ?& Arg0) no_discard", &FAngelscriptFTextBinds::GenericTextFormat);
			Binds.BindGlobalGenericFunctionForTarget("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1) no_discard", &FAngelscriptFTextBinds::GenericTextFormat);
			Binds.BindGlobalGenericFunctionForTarget("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2) no_discard", &FAngelscriptFTextBinds::GenericTextFormat);
			Binds.BindGlobalGenericFunctionForTarget("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3) no_discard", &FAngelscriptFTextBinds::GenericTextFormat);
			Binds.BindGlobalGenericFunctionForTarget("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3, const ?& Arg4) no_discard", &FAngelscriptFTextBinds::GenericTextFormat);
			Binds.BindGlobalFunctionForTarget("FText Format(const FText& Format, const TMap<FString, FFormatArgumentValue>& Arguments) no_discard", &FAngelscriptFTextBinds::NamedTextFormat);
			Binds.BindGlobalFunctionForTarget("FText Format(const FText& Format, const TArray<FFormatArgumentValue>& Arguments) no_discard", &FAngelscriptFTextBinds::OrderedTextFormat);
			Binds.BindGlobalFunctionForTarget("void GetFormatPatternParameters(const FText& Fmt, TArray<FString>&out ParameterNames)", &FAngelscriptFTextBinds::GetFormatPatternParameters);
		}

		Binds.BindGlobalFunctionForTarget(
			"FText NSLOCTEXT(const FString& Namespace, const FString& Key, const FString& Text) no_discard",
			&FAngelscriptFTextBinds::MakeLocalizableText)
			.Documentation(TEXT(
				"Function for using localization texts in Angelscript. Emulates NSLOCTEXT macro.\n"
				"Only string literals can be used as input arguments from Angelscript.\n"
				"Using variables (like FString) will run but will cause errors when strings are gathered for localization."));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FText_Type(
	TEXT("FText.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFTextType);

AS_FORCE_LINK const FAngelscriptBind Bind_FText_Infrastructure(
	TEXT("FText.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFTextInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FText(
	TEXT("FText.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFTextFunctions);
