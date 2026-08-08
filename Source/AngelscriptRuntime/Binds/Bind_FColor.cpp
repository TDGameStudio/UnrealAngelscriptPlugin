#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Bind_FColor_Functions.h"
#include "Helper_StructType.h"
#include "Helper_ToString.h"

namespace
{
	void BindFColorManual(FAngelscriptBinds& Binds)
	{
		auto FColor_ = Binds.ExistingClassForTarget("FColor");

		FColor_.Constructor(
			"void f(uint8 R, uint8 G, uint8 B, uint8 A = 255)",
			&FAngelscriptFColorBinds::ConstructRGBA,
			"FColor",
			true)
			.NoDiscard();

		FColor_.Constructor(
			"void f(uint DWColor)",
			&FAngelscriptFColorBinds::ConstructPacked,
			"FColor",
			true)
			.NoDiscard();

		FColor_.Property("uint DWColor", 0);

		FColor_.Method("bool opEquals(const FColor& ColorB) const",
			METHODPR_TRIVIAL(bool, FColor, operator==, (const FColor&) const));

		FColor_.Method("void opAddAssign(const FColor& ColorB)",
			METHODPR_TRIVIAL(void, FColor, operator+=, (const FColor&)));

		FColor_.Method("FString ToHex() const", METHOD_TRIVIAL(FColor, ToHex));
		FColor_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FColor, InitFromString));

		FColor_.Method("FLinearColor FromRGBE() const", METHOD_TRIVIAL(FColor, FromRGBE));
		FColor_.Method("FLinearColor ReinterpretAsLinear() const", METHOD_TRIVIAL(FColor, ReinterpretAsLinear));

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FColor");
			Binds.BindGlobalFunctionForTarget("FColor FromHex(const FString& HexString) no_discard", &FColor::FromHex);
			Binds.BindGlobalFunctionForTarget("FColor MakeRandomColor() no_discard", &FColor::MakeRandomColor);
			Binds.BindGlobalFunctionForTarget("FColor MakeRedToGreenColorFromScalar(float32 Scalar) no_discard", &FColor::MakeRedToGreenColorFromScalar);
			Binds.BindGlobalFunctionForTarget("FColor MakeFromColorTemperature(float32 Temp) no_discard", &FColor::MakeFromColorTemperature);

			Binds.BindGlobalVariableForTarget("FColor White", &FColor::White);
			Binds.BindGlobalVariableForTarget("FColor Black", &FColor::Black);
			Binds.BindGlobalVariableForTarget("FColor Transparent", &FColor::Transparent);
			Binds.BindGlobalVariableForTarget("FColor Red", &FColor::Red);
			Binds.BindGlobalVariableForTarget("FColor Green", &FColor::Green);
			Binds.BindGlobalVariableForTarget("FColor Blue", &FColor::Blue);
			Binds.BindGlobalVariableForTarget("FColor Yellow", &FColor::Yellow);
			Binds.BindGlobalVariableForTarget("FColor Cyan", &FColor::Cyan);
			Binds.BindGlobalVariableForTarget("FColor Magenta", &FColor::Magenta);
			Binds.BindGlobalVariableForTarget("FColor Orange", &FColor::Orange);
			Binds.BindGlobalVariableForTarget("FColor Purple", &FColor::Purple);
			Binds.BindGlobalVariableForTarget("FColor Turquoise", &FColor::Turquoise);
			Binds.BindGlobalVariableForTarget("FColor Silver", &FColor::Silver);
			Binds.BindGlobalVariableForTarget("FColor Emerald", &FColor::Emerald);
		}

		auto FLinearColor_ = Binds.ExistingClassForTarget("FLinearColor");
		FLinearColor_.Method("FColor ToFColor(bool bSRGB) const", METHOD_TRIVIAL(FLinearColor, ToFColor));
	}

	void BindFColorToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FColor"), &FAngelscriptFColorBinds::AppendToString);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FColor(
	TEXT("FColor"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFColorManual);

AS_FORCE_LINK const FAngelscriptBind Bind_FColor_ToStringContribution(
	TEXT("FColor.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFColorToStringContribution);
