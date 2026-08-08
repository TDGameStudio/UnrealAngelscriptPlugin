#include "Bind_FText.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Helper_GetTypeInfo.h"
#include "Helper_ToString.h"

/**
 * FText enums, lifecycle, comparison, localization, formatting, and conversion.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum ETextIdenticalModeFlags { None, DeepCompare, LexicalCompareInvariants };                        | Controls FText identity comparison.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum EDateTimeStyle { Default, Short, Medium, Long, Full };                                          | Selects localized date/time verbosity.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText Text();                                                                                        | Constructs empty text.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText Text(const FText& Other);                                                                      | Copy-constructs text.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IsEmpty() const;                                                                           | Reports whether the display string is empty.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IsEmptyOrWhitespace() const;                                                               | Reports whether the display string is empty or whitespace.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IsTransient() const;                                                                       | Reports whether the text is transient.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IsCultureInvariant() const;                                                                | Reports whether localization leaves the text unchanged.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IsInitializedFromString() const;                                                           | Reports whether the text originated from a string.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IsFromStringTable() const;                                                                 | Reports whether the text references a string table.                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Text.IdenticalTo(const FText& Other,                                                            | Compares text histories under the requested mode.                                                                |
 * |     const ETextIdenticalModeFlags CompareModeFlags = ETextIdenticalModeFlags::None) const;           |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns text.                                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::FromStringTable(const FName InTableId,                                                  | Loads text from a string table.                                                                                  |
 * |     const FString& InKey,                                                                            | @param InTableId String-table identifier.                                                                        |
 * |     const EStringTableLoadingPolicy InLoadingPolicy = EStringTableLoadingPolicy::FindOrLoad);        | @param InKey Entry key.                                                                                          |
 * |                                                                                                      | @param InLoadingPolicy Controls table loading.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::FromName(const FName& Val);                                                             | Creates text from a name.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::FromString(const FString& Val);                                                         | Creates text from a string.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsCultureInvariant(const FString& Val);                                                 | Creates culture-invariant text.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Join(const FText& Delimiter, const TArray<FFormatArgumentValue>& Args);                 | Joins format arguments with a delimiter.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Join(const FText& Delimiter, const TArray<FText>& Args);                                | Joins text values with a delimiter.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsDate(const FDateTime& DateTime,                                                       | Formats a localized date.                                                                                        |
 * |     const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default);                                 |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsDateTime(const FDateTime& DateTime,                                                   | Formats localized date and time.                                                                                 |
 * |     const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default,                                  |                                                                                                                  |
 * |     const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default);                                 |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsTime(const FDateTime& DateTime,                                                       | Formats a localized time.                                                                                        |
 * |     const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default);                                 |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsTimespan(const FTimespan& Timespan);                                                  | Formats a localized duration.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(float32 Val, const FNumberFormattingOptions& Options);                         | Formats a localized float32 value using the supplied options.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(float64 Val, const FNumberFormattingOptions& Options);                         | Formats a localized float64 value using the supplied options.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(int8 Val, const FNumberFormattingOptions& Options);                            | Formats a localized int8 value using the supplied options.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(int16 Val, const FNumberFormattingOptions& Options);                           | Formats a localized int16 value using the supplied options.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(int32 Val, const FNumberFormattingOptions& Options);                           | Formats a localized int32 value using the supplied options.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(int64 Val, const FNumberFormattingOptions& Options);                           | Formats a localized int64 value using the supplied options.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(uint8 Val, const FNumberFormattingOptions& Options);                           | Formats a localized uint8 value using the supplied options.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(uint16 Val, const FNumberFormattingOptions& Options);                          | Formats a localized uint16 value using the supplied options.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(uint32 Val, const FNumberFormattingOptions& Options);                          | Formats a localized uint32 value using the supplied options.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsNumber(uint64 Val, const FNumberFormattingOptions& Options);                          | Formats a localized uint64 value using the supplied options.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::AsMemory(uint64 NumBytes);                                                              | Formats a byte count using localized memory units.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format, const ?& Arg0);                                             | Formats positional wildcard arguments 0 through 0.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format, const ?& Arg0, const ?& Arg1);                              | Formats positional wildcard arguments 0 through 1.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2);               | Formats positional wildcard arguments 0 through 2.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format,                                                             | Formats positional wildcard arguments 0 through 3.                                                               |
 * |     const ?& Arg0,                                                                                   |                                                                                                                  |
 * |     const ?& Arg1,                                                                                   |                                                                                                                  |
 * |     const ?& Arg2,                                                                                   |                                                                                                                  |
 * |     const ?& Arg3);                                                                                  |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format,                                                             | Formats positional wildcard arguments 0 through 4.                                                               |
 * |     const ?& Arg0,                                                                                   |                                                                                                                  |
 * |     const ?& Arg1,                                                                                   |                                                                                                                  |
 * |     const ?& Arg2,                                                                                   |                                                                                                                  |
 * |     const ?& Arg3,                                                                                   |                                                                                                                  |
 * |     const ?& Arg4);                                                                                  |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format, const TMap<FString, FFormatArgumentValue>& Arguments);      | Formats named arguments.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText FText::Format(const FText& Format, const TArray<FFormatArgumentValue>& Arguments);             | Formats an ordered argument array.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void FText::GetFormatPatternParameters(const FText& Fmt, TArray<FString>&out ParameterNames);        | Extracts named placeholders.                                                                                     |
 * |                                                                                                      | @param ParameterNames Receives parameter names in the pattern.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FText NSLOCTEXT(const FString& Namespace, const FString& Key, const FString& Text);                  | Creates gatherable localizable text; all three arguments must be string literals.                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Value = f"{Text}";                                                                           | Formats FText through the shared string formatter contribution.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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
