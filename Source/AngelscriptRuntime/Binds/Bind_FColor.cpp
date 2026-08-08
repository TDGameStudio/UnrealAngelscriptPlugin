#include "Bind_FColor.h"

#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Helper_ToString.h"

/**
 * FColor binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor Value(uint8 R, uint8 G, uint8 B, uint8 A = 255);                                                                      | Constructs a color from red, green, blue, and alpha bytes.                                                           |
 * |                                                                                                                              | @param A Alpha byte; defaults to fully opaque.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor Value(uint DWColor);                                                                                                  | Constructs a color from packed 32-bit channel storage.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint FColor.DWColor;                                                                                                         | Packed 32-bit storage for the color's four byte channels.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Color == ColorB;                                                                                               | Reports exact equality of all four byte channels.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Color += ColorB;                                                                                                             | Adds ColorB to Color channel by channel in place.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FColor.ToHex() const;                                                                                                | Returns the color as an RRGGBBAA hexadecimal string.                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FColor.InitFromString(const FString& SourceString);                                                                     | Parses UE color text into this value and reports success.                                                            |
 * |                                                                                                                              | @param SourceString UE-formatted byte-color text.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FColor.FromRGBE() const;                                                                                        | Decodes this packed RGBE value into linear floating-point color.                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FColor.ReinterpretAsLinear() const;                                                                             | Maps byte channels to linear 0..1 values without applying sRGB conversion.                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::FromHex(const FString& HexString);                                                                            | Parses an RGB or RGBA hexadecimal color string.                                                                      |
 * |                                                                                                                              | @param HexString Hex digits, with an optional leading number sign.                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::MakeRandomColor();                                                                                            | Returns a randomly generated opaque color.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::MakeRedToGreenColorFromScalar(float32 Scalar);                                                                | Maps a normalized scalar from red through yellow to green.                                                           |
 * |                                                                                                                              | @param Scalar Expected 0..1 interpolation input.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::MakeFromColorTemperature(float32 Temp);                                                                       | Approximates the visible color of a black-body temperature.                                                          |
 * |                                                                                                                              | @param Temp Color temperature in Kelvin.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::White;                                                                                                        | Opaque white color constant.                                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Black;                                                                                                        | Opaque black color constant.                                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Transparent;                                                                                                  | Fully transparent black color constant.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Red;                                                                                                          | Opaque red color constant.                                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Green;                                                                                                        | Opaque green color constant.                                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Blue;                                                                                                         | Opaque blue color constant.                                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Yellow;                                                                                                       | Opaque yellow color constant.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Cyan;                                                                                                         | Opaque cyan color constant.                                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Magenta;                                                                                                      | Opaque magenta color constant.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Orange;                                                                                                       | Opaque orange color constant.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Purple;                                                                                                       | Opaque purple color constant.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Turquoise;                                                                                                    | Opaque turquoise color constant.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Silver;                                                                                                       | Opaque silver color constant.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FColor::Emerald;                                                                                                      | Opaque emerald color constant.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FColor FLinearColor.ToFColor(bool bSRGB) const;                                                                              | Quantizes linear channels to bytes, optionally applying linear-to-sRGB conversion.                                   |
 * |                                                                                                                              | @param bSRGB Applies sRGB transfer conversion before quantization when true.                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

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
