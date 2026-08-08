#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Bind_FTransform_Functions.h"
#include "Helper_StructType.h"
#include "Helper_ToString.h"
#include "AngelscriptDocs.h"

struct FTransformType : TAngelscriptBaseStructType<FTransform>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FTransform");
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override { return false; }
	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFTransformType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FTransform>("FTransform", Flags);
	}

	void BindFTransformInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FTransformType>());
		FToStringHelper::Register(Binds, TEXT("FTransform"), &FAngelscriptFTransformBinds::AppendToString);
	}

	void BindFTransformFunctions(FAngelscriptBinds& Binds)
	{
		auto FTransform_ = Binds.ExistingClassForTarget("FTransform");

		FTransform_.Constructor(
			"void f()",
			&FAngelscriptFTransformBinds::ConstructDefault,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FTransform& Other)",
			&FAngelscriptFTransformBinds::ConstructCopy,
			"FTransform",
			true)
			.NoDiscard();

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FTransform");
			Binds.BindGlobalVariableForTarget("const FTransform Identity", &FTransform::Identity);
		}

		FTransform_.Constructor(
			"void f(const FVector& InTranslation)",
			&FAngelscriptFTransformBinds::ConstructFromTranslation,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FQuat& InRotation)",
			&FAngelscriptFTransformBinds::ConstructFromQuat,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FRotator& InRotation)",
			&FAngelscriptFTransformBinds::ConstructFromRotator,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector)",
			&FAngelscriptFTransformBinds::ConstructFromQuatTranslationScale,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector)",
			&FAngelscriptFTransformBinds::ConstructFromRotatorTranslationScale,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InTranslation)",
			&FAngelscriptFTransformBinds::ConstructFromAxes,
			"FTransform",
			true)
			.NoDiscard();

		FTransform_.Constructor(
			"void f(const FTransform3f& Transform)",
			&FAngelscriptFTransformBinds::ConstructFromTransform3f,
			"FTransform",
			true)
			.NoDiscard();

	FTransform_.Method("FTransform& opAssign(const FTransform& Other)", METHODPR_TRIVIAL(FTransform&, FTransform, operator=, (const FTransform&)));
	FTransform_.Method("FTransform Inverse() const", METHOD_TRIVIAL(FTransform, Inverse));

	FTransform_.Method("void Blend(const FTransform& Atom1, const FTransform& Atom2, float32 Alpha)", METHODPR_TRIVIAL(void, FTransform, Blend, (const FTransform&,const FTransform&,float)));
	FTransform_.Method("void BlendWith(const FTransform& OtherAtom, float32 Alpha)", METHODPR_TRIVIAL(void, FTransform, BlendWith, (const FTransform&, float)));

	FTransform_.Method("FTransform opMul(const FTransform& Other) const", METHODPR_TRIVIAL(FTransform, FTransform, operator*, (const FTransform&)const));
	FTransform_.Method("FTransform opMul(const FQuat& Other) const", METHODPR_TRIVIAL(FTransform, FTransform, operator*, (const FQuat&)const));

	FTransform_.Method("void opMulAssign(const FTransform& Other)", METHODPR_TRIVIAL(void, FTransform, operator*=, (const FTransform&)));
	FTransform_.Method("void opMulAssign(const FQuat& Other)", METHODPR_TRIVIAL(void, FTransform, operator*=, (const FQuat&)));

	FTransform_.Method("void ScaleTranslation(const FVector& InScale3D)", METHODPR_TRIVIAL(void, FTransform, ScaleTranslation, (const FVector&)));
	FTransform_.Method("void ScaleTranslation(const float64& Scale)", METHODPR_TRIVIAL(void, FTransform, ScaleTranslation, (const double&)));
	FTransform_.Method("void RemoveScaling(float64 Tolerance=SMALL_NUMBER)", METHODPR_TRIVIAL(void, FTransform, RemoveScaling, (double)));

	FTransform_.Method("void SetToRelativeTransform(const FTransform& Other)", METHOD_TRIVIAL(FTransform, SetToRelativeTransform));
	
	FTransform_.Method("void Accumulate(const FTransform& SourceAtom)", METHODPR_TRIVIAL(void, FTransform, Accumulate, (const FTransform&)))
		.Documentation(TEXT(
	"Accumulates another transform with this one\n"
    "Rotation is accumulated multiplicatively (Rotation = SourceAtom.Rotation * Rotation)\n"
    "Translation is accumulated additively (Translation += SourceAtom.Translation)\n"
    "Scale3D is accumulated multiplicatively (Scale3D *= SourceAtom.Scale3D)\n"
    "@param SourceAtom The other transform to accumulate into this one"
     ));

	FTransform_.Method("float32 GetMaximumAxisScale() const", METHODPR_TRIVIAL(float, FTransform, GetMaximumAxisScale, () const));
	FTransform_.Method("float32 GetMinimumAxisScale() const", METHODPR_TRIVIAL(float, FTransform, GetMinimumAxisScale, () const));

	FTransform_.Method("FTransform GetRelativeTransform(const FTransform& Other) const", METHOD_TRIVIAL(FTransform, GetRelativeTransform));
	FTransform_.Method("FTransform GetRelativeTransformReverse(const FTransform& Other) const", METHOD_TRIVIAL(FTransform, GetRelativeTransformReverse));

	FTransform_.Method("FVector TransformPosition(const FVector& V) const", METHOD_TRIVIAL(FTransform, TransformPosition));
	FTransform_.Method("FVector TransformPositionNoScale(const FVector& V) const", METHOD_TRIVIAL(FTransform, TransformPositionNoScale));

	FTransform_.Method("FVector InverseTransformPosition(const FVector& V) const", METHOD_TRIVIAL(FTransform, InverseTransformPosition));
	FTransform_.Method("FVector InverseTransformPositionNoScale(const FVector& V) const", METHOD_TRIVIAL(FTransform, InverseTransformPositionNoScale));

	FTransform_.Method("FVector TransformVector(const FVector& V) const", METHOD_TRIVIAL(FTransform, TransformVector));
	FTransform_.Method("FVector TransformVectorNoScale(const FVector& V) const", METHOD_TRIVIAL(FTransform, TransformVectorNoScale));

	FTransform_.Method("FVector InverseTransformVector(const FVector& V) const", METHOD_TRIVIAL(FTransform, InverseTransformVector));
	FTransform_.Method("FVector InverseTransformVectorNoScale(const FVector& V) const", METHOD_TRIVIAL(FTransform, InverseTransformVectorNoScale));

	FTransform_.Method("FQuat TransformRotation(const FQuat& Q) const", METHOD_TRIVIAL(FTransform, TransformRotation));
	FTransform_.Method("FQuat InverseTransformRotation(const FQuat& Q) const", METHOD_TRIVIAL(FTransform, InverseTransformRotation));

	FTransform_.Method("FVector SubtractTranslations(const FTransform& B) const", FUNC_TRIVIAL(FTransform::SubtractTranslations));

	FTransform_.Method("void NormalizeRotation()", METHOD_TRIVIAL(FTransform, NormalizeRotation));

	FTransform_.Method("bool IsRotationNormalized() const", METHOD_TRIVIAL(FTransform, IsRotationNormalized));

	FTransform_.Method("bool TranslationEquals(const FTransform& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FTransform, TranslationEquals, (const FTransform&, double) const));

	FTransform_.Method("bool EqualsNoScale(const FTransform& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FTransform, EqualsNoScale, (const FTransform&, double) const));

	FTransform_.Method("bool Equals(const FTransform& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FTransform, Equals, (const FTransform&, double) const));

	FTransform_.Method("FVector GetLocation() const", METHOD_TRIVIAL(FTransform, GetLocation));

	FTransform_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FTransform, ContainsNaN));
	FTransform_.Method("bool IsValid() const", METHOD_TRIVIAL(FTransform, IsValid));

	FTransform_.Method("float64 GetDeterminant() const", METHODPR_TRIVIAL(double, FTransform, GetDeterminant, () const));
	FTransform_.Method("FRotator Rotator() const", METHOD_TRIVIAL(FTransform, Rotator));

	FTransform_.Method("FVector GetTranslation() const", METHOD_TRIVIAL(FTransform, GetTranslation));
	FTransform_.Method("FVector GetScale3D() const", METHOD_TRIVIAL(FTransform, GetScale3D));
	FTransform_.Method("FQuat GetRotation() const", METHOD_TRIVIAL(FTransform, GetRotation));
	
	FTransform_.Method("FMatrix ToMatrixWithScale() const", METHOD_TRIVIAL(FTransform, ToMatrixWithScale));
	FTransform_.Method("FMatrix ToMatrixNoScale() const", METHOD_TRIVIAL(FTransform, ToMatrixNoScale));
	FTransform_.Method("FMatrix ToInverseMatrixWithScale() const", METHOD_TRIVIAL(FTransform, ToInverseMatrixWithScale));

	FTransform_.Method("void SetLocation(const FVector& Origin)", METHOD_TRIVIAL(FTransform, SetLocation));
	FTransform_.Method("void SetTranslation(const FVector& Origin)", METHOD_TRIVIAL(FTransform, SetTranslation));

	FTransform_.Method("void AddToTranslation(const FVector& Origin)", METHOD_TRIVIAL(FTransform, AddToTranslation));

	FTransform_.Method("void ConcatenateRotation(const FQuat& DeltaRotation)", METHOD_TRIVIAL(FTransform, ConcatenateRotation))
		.Documentation(TEXT(
	"Concatenates another rotation to this transformation\n"
    "@param DeltaRotation The rotation to concatenate in the following fashion: Rotation = Rotation * DeltaRotation"
    ));

	FTransform_.Method("void SetRotation(const FQuat& NewRotation)", METHOD_TRIVIAL(FTransform, SetRotation));
	
	FTransform_.Method("void SetScale3D(const FVector& NewScale3D)", METHOD_TRIVIAL(FTransform, SetScale3D));
	FTransform_.Method("void SetTranslationAndScale3D(const FVector& NewTranslation, const FVector& NewScale3D)", METHOD_TRIVIAL(FTransform, SetTranslationAndScale3D));
	FTransform_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FTransform, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform_Type(
	TEXT("FTransform.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFTransformType);

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform_Infrastructure(
	TEXT("FTransform.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFTransformInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform(
	TEXT("FTransform.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFTransformFunctions);
