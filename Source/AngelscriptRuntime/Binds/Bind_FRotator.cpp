#include "Misc/DefaultValueHelper.h"
#include "Kismet/KismetMathLibrary.h"

#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FRotator_Functions.h"

struct FRotatorType : TAngelscriptBaseStructType<FRotator>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FRotator");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FRotator(0.f);
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
			OutValue = TEXT("FRotator()");
			return true;
		}

		FRotator Value;
		if (FDefaultValueHelper::ParseRotator(InValue, Value))
		{
			OutValue = FString::Printf(TEXT("FRotator(%f,%f,%f)"), Value.Pitch, Value.Yaw, Value.Roll);
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(
		const FAngelscriptTypeUsage& Usage,
		const FString& CppForm,
		FString& OutForm) const override
	{
		if (FDefaultValueHelper::Is(CppForm, TEXT("FRotator :: ZeroRotator")))
		{
			return true;
		}

		FString Parameters;
		if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FRotator"), Parameters))
		{
			FRotator Rotator;
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
	void BindFRotatorType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FRotator>("FRotator", Flags);
	}

	void BindFRotatorInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FRotatorType>());
		FToStringHelper::Register(Binds, TEXT("FRotator"), &FAngelscriptFRotatorBinds::AppendToString);
	}

	void BindFRotatorFunctions(FAngelscriptBinds& Binds)
	{
		auto FRotator_ = Binds.ExistingClassForTarget("FRotator");
		FRotator_.Constructor(
			"void f(float64 Pitch, float64 Yaw, float64 Roll)",
			&FAngelscriptFRotatorBinds::ConstructComponents,
			"FRotator",
			true)
			.NoDiscard();
		FRotator_.Constructor("void f()", &FAngelscriptFRotatorBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FRotator", true, "0.f");
		FRotator_.Constructor("void f(float64 F)", &FAngelscriptFRotatorBinds::ConstructScalar, "FRotator", true).NoDiscard();
		FRotator_.Constructor(
			"void f(const FRotator& Other)",
			&FAngelscriptFRotatorBinds::ConstructCopy,
			"FRotator",
			true)
			.NoDiscard();
		FRotator_.Property("float64 Pitch", &FRotator::Pitch);
		FRotator_.Property("float64 Yaw", &FRotator::Yaw);
		FRotator_.Property("float64 Roll", &FRotator::Roll);
		FRotator_.Method("FRotator& opAssign(const FRotator& Other)", METHODPR_TRIVIAL(FRotator&, FRotator, operator=, (const FRotator&)));
		FRotator_.Method("FRotator opAdd(const FRotator& R) const", METHOD_TRIVIAL(FRotator, operator+));
		FRotator_.Method("FRotator opAddAssign(const FRotator& R)", METHOD_TRIVIAL(FRotator, operator+=));
		FRotator_.Method("FRotator opSub(const FRotator& R) const", METHOD_TRIVIAL(FRotator, operator-));
		FRotator_.Method("FRotator opSubAssign(const FRotator& R)", METHOD_TRIVIAL(FRotator, operator-=));
		FRotator_.Method("FRotator opMul(float64 Scale) const", METHODPR_TRIVIAL(FRotator, FRotator, operator*, (double) const));
		FRotator_.Method("FRotator opMulAssign(float64 Scale)", METHODPR_TRIVIAL(FRotator, FRotator, operator*=, (double)));
		FRotator_.Method("bool opEquals(const FRotator& R) const", METHOD_TRIVIAL(FRotator, operator==));
		FRotator_.Method("bool IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FRotator, IsNearlyZero));
		FRotator_.Method("bool IsZero() const", METHOD_TRIVIAL(FRotator, IsZero));
		FRotator_.Method("bool Equals(const FRotator& R, float64 Tolerance=KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FRotator, Equals));
		FRotator_.Method("FRotator GetInverse() const", METHOD_TRIVIAL(FRotator, GetInverse));
		FRotator_.Method("FRotator Clamp() const", METHOD_TRIVIAL(FRotator, Clamp));
		FRotator_.Method("FRotator GetNormalized() const", METHOD_TRIVIAL(FRotator, GetNormalized));
		FRotator_.Method("FRotator GetDenormalized() const", METHOD_TRIVIAL(FRotator, GetDenormalized));
		FRotator_.Method("void GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const", METHOD_TRIVIAL(FRotator, GetWindingAndRemainder));
		FRotator_.Method("float64 GetManhattanDistance(const FRotator& Rotator) const", METHOD_TRIVIAL(FRotator, GetManhattanDistance));
		FRotator_.Method("void Normalize()", METHOD_TRIVIAL(FRotator, Normalize));
		FRotator_.Method("bool ContainsNaN()", METHOD_TRIVIAL(FRotator, ContainsNaN));

		{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FRotator");
		Binds.BindGlobalVariableForTarget("const FRotator ZeroRotator", &FRotator::ZeroRotator);
		Binds.BindGlobalFunctionForTarget("float64 NormalizeAxis(float64 Angle) no_discard", FUNC_TRIVIAL(FRotator::NormalizeAxis));
		Binds.BindGlobalFunctionForTarget("float64 ClampAxis(float64 Angle) no_discard", FUNC_TRIVIAL(FRotator::ClampAxis));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromEuler(const FVector& Euler) no_discard", FUNCPR_TRIVIAL(FRotator, FRotator::MakeFromEuler, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromX(const FVector& XAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromX, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromY(const FVector& YAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromY, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromZ(const FVector& ZAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromZ, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromXY(const FVector& XAxis, const FVector& YAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromXY, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromXZ(const FVector& XAxis, const FVector& ZAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromXZ, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromYX(const FVector& YAxis, const FVector& XAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromYX, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromYZ(const FVector& YAxis, const FVector& ZAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromYZ, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromZX(const FVector& ZAxis, const FVector& XAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromZX, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromZY(const FVector& ZAxis, const FVector& YAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromZY, (const FVector&, const FVector&)));
		}

		FRotator_.Method("FVector Vector() const", METHOD_TRIVIAL(FRotator, Vector));
		FRotator_.Method("FQuat Quaternion() const", METHOD_TRIVIAL(FRotator, Quaternion));
		FRotator_.Method("FVector Euler() const", METHOD_TRIVIAL(FRotator, Euler));
		FRotator_.Method("FVector GetForwardVector() const", METHOD_TRIVIAL(FRotator, Vector));
		FRotator_.Method("FVector GetRightVector() const", &FAngelscriptFRotatorBinds::GetRightVector);
		FRotator_.Method("FVector GetUpVector() const", &FAngelscriptFRotatorBinds::GetUpVector);
		FRotator_.Method("FVector RotateVector(const FVector& V) const", METHOD_TRIVIAL(FRotator, RotateVector));
		FRotator_.Method("FVector UnrotateVector(const FVector& V) const", METHOD_TRIVIAL(FRotator, UnrotateVector));
		FRotator_.Constructor("void f(const FQuat& Quat)", &FAngelscriptFRotatorBinds::ConstructFromQuat, "FRotator", true);
		FRotator_.Constructor("void f(const FRotator3f& Rotator)", &FAngelscriptFRotatorBinds::ConstructFromRotator3f, "FRotator", true);
		FRotator_.Method("FString ToColorString() const", &FAngelscriptFRotatorBinds::ToColorString);
		FRotator_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FRotator, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FRotator_Type(TEXT("FRotator.Type"), EAngelscriptBindPhase::TypeDeclarations, &BindFRotatorType);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator_Infrastructure(TEXT("FRotator.Infrastructure"), EAngelscriptBindPhase::TypeInfrastructure, &BindFRotatorInfrastructure);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator(TEXT("FRotator.Functions"), EAngelscriptBindPhase::ManualBindings, &BindFRotatorFunctions);
