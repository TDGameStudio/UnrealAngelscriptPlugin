#include "Bind_FLinearColor.h"

#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Helper_ToString.h"


/**
 * FLinearColor construction, fields, operators, conversion, palette values, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color();                                                                                | Constructs the value from the supplied representation.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color(float32 R, float32 G, float32 B, float32 A = 1.f);                                | Constructs the value from the supplied representation.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color(const FLinearColor& Other);                                                       | Constructs the value from the supplied representation.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.R;                                                                                     | Exposes the r component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.G;                                                                                     | Exposes the g component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.B;                                                                                     | Exposes the b component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.A;                                                                                     | Exposes the a component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns the value.                                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Result = Color + ColorB;                                                                | Adds the operands.                                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Color += ColorB;                                                                                     | Adds to the value in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Result = Color - ColorB;                                                                | Subtracts the operands.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Color -= ColorB;                                                                                     | Subtracts from the value in place.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Result = Color * ColorB;                                                                | Multiplies the operands.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Color *= ColorB;                                                                                     | Multiplies the value in place.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Result = Color * Scalar;                                                                | Multiplies the operands.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Color *= Scalar;                                                                                     | Multiplies the value in place.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Result = Color / ColorB;                                                                | Divides the operands.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Color /= ColorB;                                                                                     | Divides the value in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Result = Color / Scalar;                                                                | Divides the operands.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Color /= Scalar;                                                                                     | Divides the value in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares values for exact equality.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color.GetClamped(float32 InMin = 0.f, float32 InMax = 1.f);                             | Returns clamped.                                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Color.Equals(const FLinearColor& ColorB, float32 Tolerance = KINDA_SMALL_NUMBER) const;         | Reports whether equals.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Color.IsAlmostBlack() const;                                                                    | Reports whether is almost black.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.GetMin() const;                                                                        | Returns min.                                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.GetMax() const;                                                                        | Returns max.                                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Color.GetLuminance() const;                                                                  | Returns luminance.                                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color.LinearRGBToHSV() const;                                                           | Performs linear r g b to h s v.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color.HSVToLinearRGB() const;                                                           | Performs h s v to linear r g b.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::MakeRandomColor();                                                        | Creates make random color.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::MakeFromColorTemperature(float32 Temp);                                   | Creates make from color temperature.                                                                             |
 * |                                                                                                      | @param Temp Color temperature in Kelvin.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::MakeFromHSV8(uint8 H, uint8 S, uint8 V);                                  | Creates make from h s v8.                                                                                        |
 * |                                                                                                      | @param H Hue encoded from 0 through 255.                                                                         |
 * |                                                                                                      | @param S Saturation encoded from 0 through 255.                                                                  |
 * |                                                                                                      | @param V Value encoded from 0 through 255.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::LerpUsingHSV(const FLinearColor& From,                                    | Performs lerp using h s v.                                                                                       |
 * |     const FLinearColor& To,                                                                          | @param Progress Interpolation factor, normally in the range 0 through 1.                                         |
 * |     const float32 Progress);                                                                         |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::MakeFromHex(uint32 HexColor, bool bSRGB = true);                          | Creates make from hex.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::White;                                                                    | Provides the white value.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Gray;                                                                     | Provides the gray value.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Black;                                                                    | Provides the black value.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Transparent;                                                              | Provides the transparent value.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Red;                                                                      | Provides the red value.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Green;                                                                    | Provides the green value.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Blue;                                                                     | Provides the blue value.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Yellow;                                                                   | Provides the yellow value.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::LucBlue;                                                                  | Provides the luc blue value.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::DPink;                                                                    | Provides the d pink value.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Teal;                                                                     | Provides the teal value.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FLinearColor::Purple;                                                                   | Provides the purple value.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color(const FVector& Other, float32 A = 1.f);                                           | Constructs the value from the supplied representation.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor Color(const FColor& Other);                                                             | Constructs the value from the supplied representation.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Color}";                                                                           | Formats the value through the shared string formatter contribution.                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	const FLinearColor LucBlue(0.f, 0.66f, 1.f);
	const FLinearColor DPink(0.92f, 0.33f, 0.47f);
	const FLinearColor Teal(0.f, 0.5019f, 0.5019f);
	const FLinearColor Purple(0.662f, 0.0274f, 0.89411f);



}

AS_FORCE_LINK const FAngelscriptBind Bind_FLinearColor_Type(
	TEXT("FLinearColor.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FLinearColor>("FLinearColor", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FLinearColor_Infrastructure(
	TEXT("FLinearColor.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FLinearColorType>());
		FToStringHelper::Register(Binds, TEXT("FLinearColor"), &FAngelscriptFLinearColorBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FLinearColor(
	TEXT("FLinearColor.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FLinearColor_ = Binds.ExistingClassForTarget("FLinearColor");

		FLinearColor_.Constructor("void f()", &FAngelscriptFLinearColorBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FLinearColor", true, "0.f, 0.f, 0.f, 1.f");

		FLinearColor_.Constructor(
			"void f(float32 R, float32 G, float32 B, float32 A = 1.f)",
			&FAngelscriptFLinearColorBinds::ConstructRGBA,
			"FLinearColor",
			true)
			.NoDiscard();

		FLinearColor_.Constructor(
			"void f(const FLinearColor& Other)",
			&FAngelscriptFLinearColorBinds::ConstructCopy,
			"FLinearColor",
			true)
			.NoDiscard();

		FLinearColor_.Property("float32 R", &FLinearColor::R);
		FLinearColor_.Property("float32 G", &FLinearColor::G);
		FLinearColor_.Property("float32 B", &FLinearColor::B);
		FLinearColor_.Property("float32 A", &FLinearColor::A);

		FLinearColor_.Method(
			"FLinearColor& opAssign(const FLinearColor& Other)",
			&FAngelscriptFLinearColorBinds::Assign)
			.NativeAssignment("FLinearColor", true);

		FLinearColor_.Method("FLinearColor opAdd(const FLinearColor& ColorB) const",
			METHODPR_TRIVIAL(FLinearColor, FLinearColor, operator+, (const FLinearColor&) const));
		FLinearColor_.Method("FLinearColor opAddAssign(const FLinearColor& ColorB)",
			METHODPR_TRIVIAL(FLinearColor&, FLinearColor, operator+=, (const FLinearColor&)));

		FLinearColor_.Method("FLinearColor opSub(const FLinearColor& ColorB) const",
			METHODPR_TRIVIAL(FLinearColor, FLinearColor, operator-, (const FLinearColor&) const));
		FLinearColor_.Method("FLinearColor opSubAssign(const FLinearColor& ColorB)",
			METHODPR_TRIVIAL(FLinearColor&, FLinearColor, operator-=, (const FLinearColor&)));

		FLinearColor_.Method("FLinearColor opMul(const FLinearColor& ColorB) const",
			METHODPR_TRIVIAL(FLinearColor, FLinearColor, operator*, (const FLinearColor&) const));
		FLinearColor_.Method("FLinearColor opMulAssign(const FLinearColor& ColorB)",
			METHODPR_TRIVIAL(FLinearColor&, FLinearColor, operator*=, (const FLinearColor&)));

		FLinearColor_.Method("FLinearColor opMul(float32 Scalar) const",
			METHODPR_TRIVIAL(FLinearColor, FLinearColor, operator*, (float) const));
		FLinearColor_.Method("FLinearColor opMulAssign(float32 Scalar)",
			METHODPR_TRIVIAL(FLinearColor&, FLinearColor, operator*=, (float)));

		FLinearColor_.Method("FLinearColor opDiv(const FLinearColor& ColorB) const",
			METHODPR_TRIVIAL(FLinearColor, FLinearColor, operator/, (const FLinearColor&) const));
		FLinearColor_.Method("FLinearColor opDivAssign(const FLinearColor& ColorB)",
			METHODPR_TRIVIAL(FLinearColor&, FLinearColor, operator/=, (const FLinearColor&)));

		FLinearColor_.Method("FLinearColor opDiv(float32 Scalar) const",
			METHODPR_TRIVIAL(FLinearColor, FLinearColor, operator/, (float) const));
		FLinearColor_.Method("FLinearColor opDivAssign(float32 Scalar)",
			METHODPR_TRIVIAL(FLinearColor&, FLinearColor, operator/=, (float)));

		FLinearColor_.Method("bool opEquals(const FLinearColor& ColorB) const",
			METHODPR_TRIVIAL(bool, FLinearColor, operator==, (const FLinearColor&) const));

		FLinearColor_.Method("FLinearColor GetClamped(float32 InMin = 0.f, float32 InMax = 1.f)", METHOD_TRIVIAL(FLinearColor, GetClamped));
		FLinearColor_.Method("bool Equals(const FLinearColor& ColorB, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FLinearColor, Equals));
		FLinearColor_.Method("bool IsAlmostBlack() const", METHOD_TRIVIAL(FLinearColor, IsAlmostBlack));
		FLinearColor_.Method("float32 GetMin() const", METHOD_TRIVIAL(FLinearColor, GetMin));
		FLinearColor_.Method("float32 GetMax() const", METHOD_TRIVIAL(FLinearColor, GetMax));
		FLinearColor_.Method("float32 GetLuminance() const", METHOD_TRIVIAL(FLinearColor, GetLuminance));

		FLinearColor_.Method("FLinearColor LinearRGBToHSV() const", METHOD_TRIVIAL(FLinearColor, LinearRGBToHSV));
		FLinearColor_.Method("FLinearColor HSVToLinearRGB() const", METHOD_TRIVIAL(FLinearColor, HSVToLinearRGB));

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FLinearColor");
			Binds.BindGlobalFunctionForTarget("FLinearColor MakeRandomColor() no_discard", &FLinearColor::MakeRandomColor);
			Binds.BindGlobalFunctionForTarget("FLinearColor MakeFromColorTemperature(float32 Temp) no_discard", &FLinearColor::MakeFromColorTemperature);
			Binds.BindGlobalFunctionForTarget("FLinearColor MakeFromHSV8(uint8 H, uint8 S, uint8 V) no_discard", &FLinearColor::MakeFromHSV8);
			Binds.BindGlobalFunctionForTarget("FLinearColor LerpUsingHSV(const FLinearColor& From, const FLinearColor& To, const float32 Progress) no_discard", &FLinearColor::LerpUsingHSV);

			Binds.BindGlobalFunctionForTarget(
				"FLinearColor MakeFromHex(uint32 HexColor, bool bSRGB = true) no_discard",
				&FAngelscriptFLinearColorBinds::MakeFromHex);

			Binds.BindGlobalVariableForTarget("FLinearColor White", &FLinearColor::White);
			Binds.BindGlobalVariableForTarget("FLinearColor Gray", &FLinearColor::Gray);
			Binds.BindGlobalVariableForTarget("FLinearColor Black", &FLinearColor::Black);
			Binds.BindGlobalVariableForTarget("FLinearColor Transparent", &FLinearColor::Transparent);
			Binds.BindGlobalVariableForTarget("FLinearColor Red", &FLinearColor::Red);
			Binds.BindGlobalVariableForTarget("FLinearColor Green", &FLinearColor::Green);
			Binds.BindGlobalVariableForTarget("FLinearColor Blue", &FLinearColor::Blue);
			Binds.BindGlobalVariableForTarget("FLinearColor Yellow", &FLinearColor::Yellow);
			Binds.BindGlobalVariableForTarget("FLinearColor LucBlue", &LucBlue);
			Binds.BindGlobalVariableForTarget("FLinearColor DPink", &DPink);
			Binds.BindGlobalVariableForTarget("FLinearColor Teal", &Teal);

			Binds.BindGlobalVariableForTarget("FLinearColor Purple", &Purple);
		}

		FLinearColor_.Constructor(
			"void f(const FVector& Other, float32 A = 1.f)",
			&FAngelscriptFLinearColorBinds::ConstructFromVector)
			.NoDiscard();

		FLinearColor_.Constructor(
			"void f(const FColor& Other)",
			&FAngelscriptFLinearColorBinds::ConstructFromColor)
			.NoDiscard();
	});
