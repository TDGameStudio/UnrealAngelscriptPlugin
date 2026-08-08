#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FVector2D_Functions.h"

struct FVector2DType : TAngelscriptBaseStructType<FVector2D>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FVector2D");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FVector2D(0.f, 0.f);
	}

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue.IsEmpty())
		{
			OutValue = TEXT("FVector2D()");
			return true;
		}
		FVector2D Value;
		if (FDefaultValueHelper::ParseVector2D(InValue, Value))
		{
			OutValue = FString::Printf(TEXT("FVector2D(%f,%f)"), Value.X, Value.Y);
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override
	{
		FString Parameters;
		if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2D :: ZeroVector")))
		{
			return true;
		}
		if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2D :: UnitVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f"), FVector2D::UnitVector.X, FVector2D::UnitVector.Y);
		}
		else if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector2D"), Parameters))
		{
			FVector2D Vector;
			double Value;
			if (FDefaultValueHelper::ParseVector2D(Parameters, Vector))
			{
				OutForm = FString::Printf(TEXT("%f,%f"), Vector.X, Vector.Y);
			}
			else if (FDefaultValueHelper::ParseDouble(Parameters, Value))
			{
				OutForm = FString::Printf(TEXT("%f,%f"), Value, Value);
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
	void BindFVector2DType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FVector2D>("FVector2D", Flags);
	}

	void BindFVector2DInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FVector2DType>());
		FToStringHelper::Register(Binds, TEXT("FVector2D"), &FAngelscriptFVector2DBinds::AppendToString);
	}

	void BindFVector2DFunctions(FAngelscriptBinds& Binds)
	{
		auto FVector2D_ = Binds.ExistingClassForTarget("FVector2D");
		FVector2D_.Constructor(
			"void f(float64 X, float64 Y)",
			&FAngelscriptFVector2DBinds::Construct,
			"FVector2D",
			true)
			.NoDiscard();
		FVector2D_.Constructor("void f()", &FAngelscriptFVector2DBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FVector2D", true, "0.f, 0.f");
		FVector2D_.Constructor(
			"void f(const FVector2D& Other)",
			&FAngelscriptFVector2DBinds::ConstructCopy,
			"FVector2D",
			true)
			.NoDiscard();
		FVector2D_.Constructor(
			"void f(const FVector2f& Other)",
			&FAngelscriptFVector2DBinds::ConstructFromVector2f,
			"FVector2D",
			true)
			.NoDiscard();

		FVector2D_.Property("float64 X", &FVector2D::X);
		FVector2D_.Property("float64 Y", &FVector2D::Y);
		FVector2D_.Method("FVector2D& opAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D&, FVector2D, operator=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opAdd(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator+, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opSub(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opMul(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opDiv(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opMul(float64 Scale) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*, (double) const));
		FVector2D_.Method("FVector2D opDiv(float64 Scale) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/, (double) const));
		FVector2D_.Method("FVector2D opAdd(float64 Bias) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator+, (double) const));
		FVector2D_.Method("FVector2D opSub(float64 Bias) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-, (double) const));
		FVector2D_.Method("FVector2D opNeg() const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-, () const));
		FVector2D_.Method("FVector2D opMulAssign(float64 Scale)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*=, (double)));
		FVector2D_.Method("FVector2D opDivAssign(float64 Scale)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/=, (double)));
		FVector2D_.Method("FVector2D opMulAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opDivAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opAddAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator+=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opSubAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-=, (const FVector2D&)));
		FVector2D_.Method("float64& opIndex(int32 Index)", METHODPR_TRIVIAL(double&, FVector2D, operator[], (int32)));
		FVector2D_.Method("float64 opIndex(int32 Index) const", METHODPR_TRIVIAL(double, FVector2D, operator[], (int32) const));
		FVector2D_.Method("bool opEquals(const FVector2D& Other) const", METHODPR_TRIVIAL(bool, FVector2D, operator==, (const FVector2D&) const));
		FVector2D_.Method("bool Equals(const FVector2D& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector2D, Equals));
		FVector2D_.Method("float64 CrossProduct(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::CrossProduct));
		FVector2D_.Method("float64 DotProduct(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::DotProduct));
		FVector2D_.Method("float64 GetMax() const", METHOD_TRIVIAL(FVector2D, GetMax));
		FVector2D_.Method("float64 GetAbsMax() const", METHOD_TRIVIAL(FVector2D, GetAbsMax));
		FVector2D_.Method("float64 GetMin() const", METHOD_TRIVIAL(FVector2D, GetMin));
		FVector2D_.Method("FVector2D GetAbs() const", METHOD_TRIVIAL(FVector2D, GetAbs));
		FVector2D_.Method("float64 Size() const", METHOD_TRIVIAL(FVector2D, Size));
		FVector2D_.Method("float64 SizeSquared() const", METHOD_TRIVIAL(FVector2D, SizeSquared));
		FVector2D_.Method("bool IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector2D, IsNearlyZero));
		FVector2D_.Method("bool IsZero() const", METHOD_TRIVIAL(FVector2D, IsZero));
		FVector2D_.Method("void Normalize(float64 Tolerance = SMALL_NUMBER)", METHOD_TRIVIAL(FVector2D, Normalize));
		FVector2D_.Method("FVector2D GetSafeNormal(float64 Tolerance = SMALL_NUMBER) const", METHOD_TRIVIAL(FVector2D, GetSafeNormal));
		FVector2D_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FVector2D, ContainsNaN));
		FVector2D_.Method("FVector2D GetSignVector() const", METHOD_TRIVIAL(FVector2D, GetSignVector));
		FVector2D_.Method("float64 Distance(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::Distance));
		FVector2D_.Method("float64 DistSquared(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::DistSquared));
		FVector2D_.Method("FVector2D GetClampedToMaxSize(float64 Max) const", &FAngelscriptFVector2DBinds::GetClampedToMaxSize);
		FVector2D_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FVector2D, InitFromString));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FVector2D");
		Binds.BindGlobalVariableForTarget("const FVector2D ZeroVector", &FVector2D::ZeroVector);
		Binds.BindGlobalVariableForTarget("const FVector2D UnitVector", &FVector2D::UnitVector);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2D_Type(
	TEXT("FVector2D.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFVector2DType);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2D_Infrastructure(
	TEXT("FVector2D.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVector2DInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2D(
	TEXT("FVector2D.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFVector2DFunctions);
