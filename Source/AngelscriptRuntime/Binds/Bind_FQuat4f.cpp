#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FQuat4f_Functions.h"

struct FQuat4fType : TAngelscriptVariantStructType<FQuat4f>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FQuat4f");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FQuat4f(0.f, 0.f, 0.f, 1.f);
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFQuat4fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FQuat4f>("FQuat4f", Flags);
	}

	void BindFQuat4fInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FQuat4fType>());
		FToStringHelper::Register(Binds, TEXT("FQuat4f"), &FAngelscriptFQuat4fBinds::AppendToString);
	}

	void BindFQuat4fFunctions(FAngelscriptBinds& Binds)
	{
		auto FQuat4f_ = Binds.ExistingClassForTarget("FQuat4f");
		FQuat4f_.Constructor("void f()", &FAngelscriptFQuat4fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FQuat4f", true, "0.f, 0.f, 0.f, 1.f");
		FQuat4f_.Constructor(
			"void f(const FQuat4f& Q)",
			&FAngelscriptFQuat4fBinds::ConstructCopy,
			"FQuat4f",
			true)
			.NoDiscard();
		FQuat4f_.Constructor(
			"void f(float32 X, float32 Y, float32 Z, float32 W)",
			&FAngelscriptFQuat4fBinds::ConstructComponents,
			"FQuat4f",
			true)
			.NoDiscard();
		FQuat4f_.Method(
			"FQuat4f& opAssign(const FQuat4f& Other)",
			METHODPR_TRIVIAL(FQuat4f&, FQuat4f, operator=, (const FQuat4f&)))
			.NoDiscard();

		FQuat4f_.Method("FQuat4f opAdd(const FQuat4f& Other) const", METHOD_TRIVIAL(FQuat4f, operator+));
		FQuat4f_.Method("FQuat4f opSub(const FQuat4f& Other) const", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator-, (const FQuat4f&) const));
		FQuat4f_.Method("FQuat4f opAddAssign(const FQuat4f& Other)", METHOD_TRIVIAL(FQuat4f, operator+=));
		FQuat4f_.Method("FQuat4f opSubAssign(const FQuat4f& Other)", METHOD_TRIVIAL(FQuat4f, operator-=));
		FQuat4f_.Method("bool opEquals(const FQuat4f& Other) const", METHOD_TRIVIAL(FQuat4f, operator==));
		FQuat4f_.Method("bool Equals(const FQuat4f& Other, float32 Tolerance=KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FQuat4f, Equals));
		FQuat4f_.Method("bool IsIdentity(float32 Tolerance=SMALL_NUMBER) const", METHOD_TRIVIAL(FQuat4f, IsIdentity));
		FQuat4f_.Method("FQuat4f opMul(const FQuat4f& Other) const", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator*, (const FQuat4f&) const));
		FQuat4f_.Method("FQuat4f opMulAssign(const FQuat4f& Other)", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator*=, (const FQuat4f&)));
		FQuat4f_.Method("FQuat4f opMul(float32 Scale) const", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator*, (const float) const));
		FQuat4f_.Method("FQuat4f opMulAssign(float32 Scale)", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator*=, (const float)));
		FQuat4f_.Method("FQuat4f opDiv(float32 Scale) const", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator/, (const float) const));
		FQuat4f_.Method("FQuat4f opDivAssign(float32 Scale)", METHODPR_TRIVIAL(FQuat4f, FQuat4f, operator/=, (const float)));
		FQuat4f_.Method("void Normalize(float32 Tolerance = SMALL_NUMBER)", METHOD_TRIVIAL(FQuat4f, Normalize));
		FQuat4f_.Method("FQuat4f GetNormalized(float32 Tolerance = SMALL_NUMBER) const", METHOD_TRIVIAL(FQuat4f, GetNormalized));
		FQuat4f_.Method("bool IsNormalized() const", METHOD_TRIVIAL(FQuat4f, IsNormalized));
		FQuat4f_.Method("float32 Size() const", METHOD_TRIVIAL(FQuat4f, Size));
		FQuat4f_.Method("float32 SizeSquared() const", METHOD_TRIVIAL(FQuat4f, SizeSquared));
		FQuat4f_.Method("float32 GetAngle() const", METHOD_TRIVIAL(FQuat4f, GetAngle));
		FQuat4f_.Method("FQuat4f Log() const", METHOD_TRIVIAL(FQuat4f, Log));
		FQuat4f_.Method("FQuat4f Exp() const", METHOD_TRIVIAL(FQuat4f, Exp));
		FQuat4f_.Method("FQuat4f Inverse() const", METHOD_TRIVIAL(FQuat4f, Inverse));
		FQuat4f_.Method("float32 AngularDistance(const FQuat4f& Q) const", METHOD_TRIVIAL(FQuat4f, AngularDistance));
		FQuat4f_.Property("float32 X", &FQuat4f::X);
		FQuat4f_.Property("float32 Y", &FQuat4f::Y);
		FQuat4f_.Property("float32 Z", &FQuat4f::Z);
		FQuat4f_.Property("float32 W", &FQuat4f::W);
		FQuat4f_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FQuat4f, ContainsNaN));
		FQuat4f_.Method("void EnforceShortestArcWith(const FQuat4f& Other)", METHOD_TRIVIAL(FQuat4f, EnforceShortestArcWith));

		FQuat4f_.Constructor(
			"void f(const FRotator3f& R)",
			&FAngelscriptFQuat4fBinds::ConstructFromRotator3f,
			"FQuat4f",
			true)
			.NoDiscard();
		FQuat4f_.Constructor(
			"void f(FVector3f Axis, float32 AngleRad)",
			&FAngelscriptFQuat4fBinds::ConstructAxisAngle,
			"FQuat4f",
			true)
			.NoDiscard();
		FQuat4f_.Constructor(
			"void f(const FQuat& Quat)",
			&FAngelscriptFQuat4fBinds::ConstructFromQuat,
			"FQuat4f",
			true)
			.NoDiscard();
		FQuat4f_.Method("void ToAxisAndAngle(FVector3f& Axis, float32& Angle) const", METHODPR_TRIVIAL(void, FQuat4f, ToAxisAndAngle, (FVector3f&, float&) const));
		FQuat4f_.Method("FVector3f opMul(const FVector3f& Other) const", METHODPR_TRIVIAL(FVector3f, FQuat4f, operator*, (const FVector3f&) const));
		FQuat4f_.Method("FVector3f Euler() const", METHOD_TRIVIAL(FQuat4f, Euler));
		FQuat4f_.Method("FVector3f RotateVector(FVector3f V) const", METHOD_TRIVIAL(FQuat4f, RotateVector));
		FQuat4f_.Method("FVector3f UnrotateVector(FVector3f V) const", METHOD_TRIVIAL(FQuat4f, UnrotateVector));

		{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FQuat4f");
		Binds.BindGlobalVariableForTarget("const FQuat4f Identity", &FQuat4f::Identity);
		Binds.BindGlobalFunctionForTarget("FQuat4f MakeFromEuler(const FVector3f& Euler) no_discard", FUNC_TRIVIAL(FQuat4f::MakeFromEuler));
		Binds.BindGlobalFunctionForTarget("FQuat4f FindBetween(const FVector3f& Vector1, const FVector3f& Vector2) no_discard", FUNC_TRIVIAL(FQuat4f::FindBetween));
		Binds.BindGlobalFunctionForTarget("FQuat4f FindBetweenVectors(const FVector3f& Vector1, const FVector3f& Vector2) no_discard", FUNC_TRIVIAL(FQuat4f::FindBetweenVectors));
		Binds.BindGlobalFunctionForTarget("FQuat4f FindBetweenNormals(const FVector3f& Normal1, const FVector3f& Normal2) no_discard", FUNC_TRIVIAL(FQuat4f::FindBetweenNormals));
		Binds.BindGlobalFunctionForTarget("FQuat4f FastLerp(const FQuat4f& A, const FQuat4f& B, const float32 Alpha) no_discard", FUNC_TRIVIAL(FQuat4f::FastLerp));
		Binds.BindGlobalFunctionForTarget("FQuat4f FastBilerp(const FQuat4f& P00, const FQuat4f& P10, const FQuat4f& P01, const FQuat4f& P11, float32 FracX, float32 FracY) no_discard", FUNC_TRIVIAL(FQuat4f::FastBilerp));
		Binds.BindGlobalFunctionForTarget("FQuat4f Slerp_NotNormalized(const FQuat4f& Quat1, const FQuat4f& Quat2, float32 Slerp) no_discard", FUNC_TRIVIAL(FQuat4f::Slerp_NotNormalized));
		Binds.BindGlobalFunctionForTarget("FQuat4f Slerp(const FQuat4f& Quat1, const FQuat4f& Quat2, float32 Slerp) no_discard", FUNC_TRIVIAL(FQuat4f::Slerp));
		Binds.BindGlobalFunctionForTarget("float32 Error(const FQuat4f& Q1, const FQuat4f& Q2) no_discard", FUNC_TRIVIAL(FQuat4f::Error));
		Binds.BindGlobalFunctionForTarget("float32 ErrorAutoNormalize(const FQuat4f& A, const FQuat4f& B) no_discard", FUNC_TRIVIAL(FQuat4f::ErrorAutoNormalize));
		Binds.BindGlobalFunctionForTarget("FQuat4f SlerpFullPath_NotNormalized(const FQuat4f& Quat1, const FQuat4f& Quat2, float32 Slerp) no_discard", FUNC_TRIVIAL(FQuat4f::SlerpFullPath_NotNormalized));
		Binds.BindGlobalFunctionForTarget("FQuat4f SlerpFullPath(const FQuat4f& Quat1, const FQuat4f& Quat2, float32 Slerp) no_discard", FUNC_TRIVIAL(FQuat4f::SlerpFullPath));
		Binds.BindGlobalFunctionForTarget("FQuat4f Squad(const FQuat4f& Quat1, const FQuat4f& Tang1, const FQuat4f& Quat2, const FQuat4f& Tang2, float32 Alpha) no_discard", FUNC_TRIVIAL(FQuat4f::Squad));
		Binds.BindGlobalFunctionForTarget("FQuat4f SquadFullPath(const FQuat4f& Quat1, const FQuat4f& Tang1, const FQuat4f& Quat2, const FQuat4f& Tang2, float32 Alpha) no_discard", FUNC_TRIVIAL(FQuat4f::SquadFullPath));
		Binds.BindGlobalFunctionForTarget("void CalcTangents(const FQuat4f& PrevP, const FQuat4f& P, const FQuat4f& NextP, float32 Tension, FQuat4f& OutTan) no_discard", FUNC_TRIVIAL(FQuat4f::CalcTangents));
		}

		FQuat4f_.Method("FVector3f GetAxisX() const", METHOD_TRIVIAL(FQuat4f, GetAxisX));
		FQuat4f_.Method("FVector3f GetAxisY() const", METHOD_TRIVIAL(FQuat4f, GetAxisY));
		FQuat4f_.Method("FVector3f GetAxisZ() const", METHOD_TRIVIAL(FQuat4f, GetAxisZ));
		FQuat4f_.Method("FVector3f GetForwardVector() const", METHOD_TRIVIAL(FQuat4f, GetForwardVector));
		FQuat4f_.Method("FVector3f GetRightVector() const", METHOD_TRIVIAL(FQuat4f, GetRightVector));
		FQuat4f_.Method("FVector3f GetUpVector() const", METHOD_TRIVIAL(FQuat4f, GetUpVector));
		FQuat4f_.Method("FVector3f Vector() const", METHOD_TRIVIAL(FQuat4f, Vector));
		FQuat4f_.Method("FVector3f GetRotationAxis() const", METHOD_TRIVIAL(FQuat4f, GetRotationAxis));
		FQuat4f_.Method("FRotator3f Rotator() const", METHOD_TRIVIAL(FQuat4f, Rotator));
		FQuat4f_.Method("void ToSwingTwist(const FVector3f& InTwistAxis, FQuat4f& OutSwing, FQuat4f& OutTwist) const", METHOD_TRIVIAL(FQuat4f, ToSwingTwist));
		FQuat4f_.Method("float32 GetTwistAngle(const FVector3f& InTwistAxis) const", METHOD_TRIVIAL(FQuat4f, GetTwistAngle));
		FQuat4f_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FQuat4f, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FQuat4f_Type(TEXT("FQuat4f.Type"), EAngelscriptBindPhase::TypeDeclarations, &BindFQuat4fType);
AS_FORCE_LINK const FAngelscriptBind Bind_FQuat4f_Infrastructure(TEXT("FQuat4f.Infrastructure"), EAngelscriptBindPhase::TypeInfrastructure, &BindFQuat4fInfrastructure);
AS_FORCE_LINK const FAngelscriptBind Bind_FQuat4f(TEXT("FQuat4f.Functions"), EAngelscriptBindPhase::ManualBindings, &BindFQuat4fFunctions);
