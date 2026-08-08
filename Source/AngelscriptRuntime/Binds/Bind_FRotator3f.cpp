#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FRotator3f_Functions.h"

struct FRotator3fType : TAngelscriptVariantStructType<FRotator3f>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FRotator3f");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FRotator3f(0.f);
	}

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool DefaultValue_UnrealToAngelscript(
		const FAngelscriptTypeUsage& Usage,
		const FString& InValue,
		FString& OutValue) const override
	{
		if (InValue.IsEmpty())
		{
			OutValue = TEXT("FRotator3f()");
			return true;
		}
		FRotator3f Value;
		if (FDefaultValueHelper::ParseRotator(InValue, Value))
		{
			OutValue = FString::Printf(TEXT("FRotator3f(%f,%f,%f)"), Value.Pitch, Value.Yaw, Value.Roll);
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(
		const FAngelscriptTypeUsage& Usage,
		const FString& CppForm,
		FString& OutForm) const override
	{
		if (FDefaultValueHelper::Is(CppForm, TEXT("FRotator3f :: ZeroRotator")))
		{
			return true;
		}
		FString Parameters;
		if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FRotator3f"), Parameters))
		{
			FRotator3f Rotator;
			if (FDefaultValueHelper::ParseRotator(Parameters, Rotator))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
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
	void BindFRotator3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FRotator3f>("FRotator3f", Flags);
	}

	void BindFRotator3fInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FRotator3fType>());
		FToStringHelper::Register(Binds, TEXT("FRotator3f"), &FAngelscriptFRotator3fBinds::AppendToString);
	}

	void BindFRotator3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FRotator3f_ = Binds.ExistingClassForTarget("FRotator3f");
		FRotator3f_.Constructor(
			"void f(float32 Pitch, float32 Yaw, float32 Roll)",
			&FAngelscriptFRotator3fBinds::ConstructComponents,
			"FRotator3f",
			true)
			.NoDiscard();
		FRotator3f_.Constructor("void f()", &FAngelscriptFRotator3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FRotator3f", true, "0.f");
		FRotator3f_.Constructor(
			"void f(float32 F)",
			&FAngelscriptFRotator3fBinds::ConstructScalar,
			"FRotator3f",
			true)
			.NoDiscard();
		FRotator3f_.Constructor(
			"void f(const FRotator3f& Other)",
			&FAngelscriptFRotator3fBinds::ConstructCopy,
			"FRotator3f",
			true)
			.NoDiscard();
		FRotator3f_.Property("float32 Pitch", &FRotator3f::Pitch);
		FRotator3f_.Property("float32 Yaw", &FRotator3f::Yaw);
		FRotator3f_.Property("float32 Roll", &FRotator3f::Roll);
		FRotator3f_.Method("FRotator3f& opAssign(const FRotator3f& Other)", METHODPR_TRIVIAL(FRotator3f&, FRotator3f, operator=, (const FRotator3f&)));
		FRotator3f_.Method("FRotator3f opAdd(const FRotator3f& R) const", METHOD_TRIVIAL(FRotator3f, operator+));
		FRotator3f_.Method("FRotator3f opAddAssign(const FRotator3f& R)", METHOD_TRIVIAL(FRotator3f, operator+=));
		FRotator3f_.Method("FRotator3f opSub(const FRotator3f& R) const", METHOD_TRIVIAL(FRotator3f, operator-));
		FRotator3f_.Method("FRotator3f opSubAssign(const FRotator3f& R)", METHOD_TRIVIAL(FRotator3f, operator-=));
		FRotator3f_.Method("FRotator3f opMul(float32 Scale) const", METHODPR_TRIVIAL(FRotator3f, FRotator3f, operator*, (float) const));
		FRotator3f_.Method("FRotator3f opMulAssign(float32 Scale)", METHODPR_TRIVIAL(FRotator3f, FRotator3f, operator*=, (float)));
		FRotator3f_.Method("bool opEquals(const FRotator3f& R) const", METHOD_TRIVIAL(FRotator3f, operator==));
		FRotator3f_.Method("bool IsNearlyZero(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FRotator3f, IsNearlyZero));
		FRotator3f_.Method("bool IsZero() const", METHOD_TRIVIAL(FRotator3f, IsZero));
		FRotator3f_.Method("bool Equals(const FRotator3f& R, float32 Tolerance=__KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FRotator3f, Equals));
		FRotator3f_.Method("FRotator3f GetInverse() const", METHOD_TRIVIAL(FRotator3f, GetInverse));
		FRotator3f_.Method("FRotator3f Clamp() const", METHOD_TRIVIAL(FRotator3f, Clamp));
		FRotator3f_.Method("FRotator3f GetNormalized() const", METHOD_TRIVIAL(FRotator3f, GetNormalized));
		FRotator3f_.Method("FRotator3f GetDenormalized() const", METHOD_TRIVIAL(FRotator3f, GetDenormalized));
		FRotator3f_.Method("void GetWindingAndRemainder(FRotator3f& Winding, FRotator3f& Remainder) const", METHOD_TRIVIAL(FRotator3f, GetWindingAndRemainder));
		FRotator3f_.Method("float32 GetManhattanDistance(const FRotator3f& Rotator) const", METHOD_TRIVIAL(FRotator3f, GetManhattanDistance));
		FRotator3f_.Method("void Normalize()", METHOD_TRIVIAL(FRotator3f, Normalize));
		FRotator3f_.Method("bool ContainsNaN()", METHOD_TRIVIAL(FRotator3f, ContainsNaN));

		{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FRotator3f");
		Binds.BindGlobalVariableForTarget("const FRotator3f ZeroRotator", &FRotator3f::ZeroRotator);
		Binds.BindGlobalFunctionForTarget("float32 NormalizeAxis(float32 Angle) no_discard", FUNC_TRIVIAL(FRotator3f::NormalizeAxis));
		Binds.BindGlobalFunctionForTarget("float32 ClampAxis(float32 Angle) no_discard", FUNC_TRIVIAL(FRotator3f::ClampAxis));
		Binds.BindGlobalFunctionForTarget("FRotator3f MakeFromEuler(const FVector3f& Euler) no_discard", FUNCPR_TRIVIAL(FRotator3f, FRotator3f::MakeFromEuler, (const FVector3f&)));
		}

		FRotator3f_.Method("FVector3f Vector() const", METHOD_TRIVIAL(FRotator3f, Vector));
		FRotator3f_.Method("FQuat4f Quaternion() const", METHOD_TRIVIAL(FRotator3f, Quaternion));
		FRotator3f_.Method("FVector3f Euler() const", METHOD_TRIVIAL(FRotator3f, Euler));
		FRotator3f_.Method("FVector3f RotateVector(const FVector3f& V) const", METHOD_TRIVIAL(FRotator3f, RotateVector));
		FRotator3f_.Method("FVector3f UnrotateVector(const FVector3f& V) const", METHOD_TRIVIAL(FRotator3f, UnrotateVector));
		FRotator3f_.Constructor("void f(const FQuat4f& Quat)", &FAngelscriptFRotator3fBinds::ConstructFromQuat4f, "FRotator3f", true);
		FRotator3f_.Constructor("void f(const FRotator& Rotator)", &FAngelscriptFRotator3fBinds::ConstructFromRotator, "FRotator3f", true);
		FRotator3f_.Method("FString ToColorString() const", &FAngelscriptFRotator3fBinds::ToColorString);
		FRotator3f_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FRotator3f, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FRotator3f_Type(TEXT("FRotator3f.Type"), EAngelscriptBindPhase::TypeDeclarations, &BindFRotator3fType);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator3f_Infrastructure(TEXT("FRotator3f.Infrastructure"), EAngelscriptBindPhase::TypeInfrastructure, &BindFRotator3fInfrastructure);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator3f(TEXT("FRotator3f.Functions"), EAngelscriptBindPhase::ManualBindings, &BindFRotator3fFunctions);
