#include "AngelscriptBinds.h"

#include "Helper_CppType.h"

#include "Bind_FNumberFormattingOptions_Functions.h"

namespace
{
	struct FNumberFormattingOptionsType : TAngelscriptCppType<FNumberFormattingOptions>
	{
		FString GetAngelscriptTypeName() const override
		{
			return TEXT("FNumberFormattingOptions");
		}

		bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
		{
			OutCppForm.CppType = GetAngelscriptTypeName();
			return true;
		}
	};

	void BindFNumberFormattingOptionsType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FNumberFormattingOptions>("FNumberFormattingOptions", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FNumberFormattingOptionsType>());
	}

	void BindFNumberFormattingOptionsManual(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FNumberFormattingOptions_Type(
	TEXT("FNumberFormattingOptions.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFNumberFormattingOptionsType);

AS_FORCE_LINK const FAngelscriptBind Bind_FNumberFormattingOptions(
	TEXT("FNumberFormattingOptions.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFNumberFormattingOptionsManual);
