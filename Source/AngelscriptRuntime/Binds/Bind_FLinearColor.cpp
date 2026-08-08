#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Bind_FLinearColor_Functions.h"
#include "Helper_StructType.h"
#include "Helper_ToString.h"

struct FLinearColorType : TAngelscriptBaseStructType<FLinearColor>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FLinearColor");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new(DestinationPtr) FLinearColor(0.f, 0.f, 0.f, 1.f);
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue.IsEmpty())
		{
			OutValue = TEXT("FLinearColor()");
			return true;
		}
		FLinearColor Value;
		if (Value.InitFromString(InValue))
		{
			OutValue = FString::Printf(TEXT("FLinearColor(%f,%f,%f,%f)"), Value.R, Value.G, Value.B, Value.A);
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override
	{
		if( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: White") ) )
		{
			OutForm = FLinearColor::White.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Gray") ) )
		{
			OutForm = FLinearColor::Gray.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Black") ) )
		{
			OutForm = FLinearColor::Black.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Transparent") ) )
		{
			OutForm = FLinearColor::Transparent.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Red") ) )
		{
			OutForm = FLinearColor::Red.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Green") ) )
		{
			OutForm = FLinearColor::Green.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Blue") ) )
		{
			OutForm = FLinearColor::Blue.ToString();
		}
		else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor :: Yellow") ) )
		{
			OutForm = FLinearColor::Yellow.ToString();
		}
		else
		{
			FString Parameters;
			if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FLinearColor"), Parameters) )
			{
				FLinearColor Color;
				if( FDefaultValueHelper::ParseLinearColor(Parameters, Color) )
				{
					OutForm = Color.ToString();
				}
			}
		}

		return !OutForm.IsEmpty();
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	const FLinearColor LucBlue(0.f, 0.66f, 1.f);
	const FLinearColor DPink(0.92f, 0.33f, 0.47f);
	const FLinearColor Teal(0.f, 0.5019f, 0.5019f);
	const FLinearColor Purple(0.662f, 0.0274f, 0.89411f);

	void BindFLinearColorType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FLinearColor>("FLinearColor", Flags);
	}

	void BindFLinearColorInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FLinearColorType>());
		FToStringHelper::Register(Binds, TEXT("FLinearColor"), &FAngelscriptFLinearColorBinds::AppendToString);
	}

	void BindFLinearColorFunctions(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FLinearColor_Type(
	TEXT("FLinearColor.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFLinearColorType);

AS_FORCE_LINK const FAngelscriptBind Bind_FLinearColor_Infrastructure(
	TEXT("FLinearColor.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFLinearColorInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FLinearColor(
	TEXT("FLinearColor.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFLinearColorFunctions);
