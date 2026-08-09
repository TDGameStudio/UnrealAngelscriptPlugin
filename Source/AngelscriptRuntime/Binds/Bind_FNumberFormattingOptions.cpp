#include "Bind_FNumberFormattingOptions.h"

#include "AngelscriptBinds.h"

/**
 * FNumberFormattingOptions binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FNumberFormattingOptions;                                                           | Declares the value type used to configure localized number formatting.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions Options();                                                        | Constructs options initialized to the engine defaults.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetAlwaysSign(bool InValue);            | Sets whether positive values include an explicit sign.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetUseGrouping(bool InValue);           | Sets whether integral digits use grouping separators.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetRoundingMode(ERoundingMode InValue); | Sets the rounding mode.                                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetMinimumIntegralDigits(               | Sets the minimum number of integral digits.                                                                          |
 * |     int32 InValue);                                                                        |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetMaximumIntegralDigits(               | Sets the maximum number of integral digits.                                                                          |
 * |     int32 InValue);                                                                        |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetMinimumFractionalDigits(             | Sets the minimum number of fractional digits.                                                                        |
 * |     int32 InValue);                                                                        |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FNumberFormattingOptions& FNumberFormattingOptions.SetMaximumFractionalDigits(             | Sets the maximum number of fractional digits.                                                                        |
 * |     int32 InValue);                                                                        |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint32 FNumberFormattingOptions.GetTypeHash() const;                                       | Returns a hash for these options.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FNumberFormattingOptions.IsIdentical(                                                 | Returns whether every formatting option matches Other.                                                               |
 * |     const FNumberFormattingOptions& Other) const;                                          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FNumberFormattingOptions& FNumberFormattingOptions::DefaultWithGrouping();           | Returns the shared default options with digit grouping enabled.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FNumberFormattingOptions& FNumberFormattingOptions::DefaultNoGrouping();             | Returns the shared default options with digit grouping disabled.                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FNumberFormattingOptions_Type(
	TEXT("FNumberFormattingOptions.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FNumberFormattingOptions>("FNumberFormattingOptions", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FNumberFormattingOptionsType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FNumberFormattingOptions(
	TEXT("FNumberFormattingOptions.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Options_ = Binds.ExistingClassForTarget("FNumberFormattingOptions");
		Options_.Constructor(
			"void f()",
			&FAngelscriptFNumberFormattingOptionsBinds::Construct,
			"FNumberFormattingOptions",
			true)
			.NoDiscard();
		Options_.Method("FNumberFormattingOptions& SetAlwaysSign(bool InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetAlwaysSign));
		Options_.Method("FNumberFormattingOptions& SetUseGrouping(bool InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetUseGrouping));
		Options_.Method("FNumberFormattingOptions& SetRoundingMode(ERoundingMode InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetRoundingMode));
		Options_.Method("FNumberFormattingOptions& SetMinimumIntegralDigits(int32 InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetMinimumIntegralDigits));
		Options_.Method("FNumberFormattingOptions& SetMaximumIntegralDigits(int32 InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetMaximumIntegralDigits));
		Options_.Method("FNumberFormattingOptions& SetMinimumFractionalDigits(int32 InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetMinimumFractionalDigits));
		Options_.Method("FNumberFormattingOptions& SetMaximumFractionalDigits(int32 InValue)", METHOD_TRIVIAL(FNumberFormattingOptions, SetMaximumFractionalDigits));
		Options_.Method("uint32 GetTypeHash() const", &FAngelscriptFNumberFormattingOptionsBinds::Hash);
		Options_.Method("bool IsIdentical( const FNumberFormattingOptions& Other ) const", METHOD_TRIVIAL(FNumberFormattingOptions, IsIdentical));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FNumberFormattingOptions");
		Binds.BindGlobalFunctionForTarget("const FNumberFormattingOptions& DefaultWithGrouping() no_discard", &FNumberFormattingOptions::DefaultWithGrouping)
			.NativeFunction("FNumberFormattingOptions::DefaultWithGrouping", true);
		Binds.BindGlobalFunctionForTarget("const FNumberFormattingOptions& DefaultNoGrouping() no_discard", &FNumberFormattingOptions::DefaultNoGrouping)
			.NativeFunction("FNumberFormattingOptions::DefaultNoGrouping", true);
	});
