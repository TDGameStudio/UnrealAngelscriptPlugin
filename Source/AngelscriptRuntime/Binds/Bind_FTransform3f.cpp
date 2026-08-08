#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Bind_FTransform3f_Functions.h"
#include "Helper_StructType.h"
#include "Helper_ToString.h"
#include "AngelscriptDocs.h"

struct FTransform3fType : TAngelscriptVariantStructType<FTransform3f>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FTransform3f");
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
	void BindFTransform3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FTransform3f>("FTransform3f", Flags);
	}

	void BindFTransform3fInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FTransform3fType>());
		FToStringHelper::Register(Binds, TEXT("FTransform3f"), &FAngelscriptFTransform3fBinds::AppendToString);
	}

	void BindFTransform3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FTransform3f_ = Binds.ExistingClassForTarget("FTransform3f");

		FTransform3f_.Constructor(
			"void f()",
			&FAngelscriptFTransform3fBinds::ConstructDefault,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FTransform3f& Other)",
			&FAngelscriptFTransform3fBinds::ConstructCopy,
			"FTransform3f",
			true)
			.NoDiscard();

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FTransform3f");
			Binds.BindGlobalVariableForTarget("const FTransform3f Identity", &FTransform3f::Identity);
		}

		FTransform3f_.Constructor(
			"void f(const FVector3f& InTranslation)",
			&FAngelscriptFTransform3fBinds::ConstructFromTranslation,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FQuat4f& InRotation)",
			&FAngelscriptFTransform3fBinds::ConstructFromQuat,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FRotator3f& InRotation)",
			&FAngelscriptFTransform3fBinds::ConstructFromRotator,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FQuat4f& InRotation, const FVector3f& InTranslation, const FVector3f& InScale3D = FVector3f::OneVector)",
			&FAngelscriptFTransform3fBinds::ConstructFromQuatTranslationScale,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FRotator3f& InRotation, const FVector3f& InTranslation, const FVector3f& InScale3D = FVector3f::OneVector)",
			&FAngelscriptFTransform3fBinds::ConstructFromRotatorTranslationScale,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FVector3f& InX, const FVector3f& InY, const FVector3f& InZ, const FVector3f& InTranslation)",
			&FAngelscriptFTransform3fBinds::ConstructFromAxes,
			"FTransform3f",
			true)
			.NoDiscard();

		FTransform3f_.Constructor(
			"void f(const FTransform& Transform)",
			&FAngelscriptFTransform3fBinds::ConstructFromTransform,
			"FTransform3f",
			true)
			.NoDiscard();

	FTransform3f_.Method("FTransform3f& opAssign(const FTransform3f& Other)", METHODPR_TRIVIAL(FTransform3f&, FTransform3f, operator=, (const FTransform3f&)));
	FTransform3f_.Method("FTransform3f Inverse() const", METHOD_TRIVIAL(FTransform3f, Inverse));
	FTransform3f_.Method("void Blend(const FTransform3f& Atom1, const FTransform3f& Atom2, float32 Alpha)", METHODPR_TRIVIAL(void, FTransform3f, Blend, (const FTransform3f&,const FTransform3f&,float)));
	FTransform3f_.Method("void BlendWith(const FTransform3f& OtherAtom, float32 Alpha)", METHODPR_TRIVIAL(void, FTransform3f, BlendWith, (const FTransform3f&, float)));

	FTransform3f_.Method("FTransform3f opMul(const FTransform3f& Other) const", METHODPR_TRIVIAL(FTransform3f, FTransform3f, operator*, (const FTransform3f&)const));
	FTransform3f_.Method("FTransform3f opMul(const FQuat4f& Other) const", METHODPR_TRIVIAL(FTransform3f, FTransform3f, operator*, (const FQuat4f&)const));

	FTransform3f_.Method("void opMulAssign(const FTransform3f& Other)", METHODPR_TRIVIAL(void, FTransform3f, operator*=, (const FTransform3f&)));
	FTransform3f_.Method("void opMulAssign(const FQuat4f& Other)", METHODPR_TRIVIAL(void, FTransform3f, operator*=, (const FQuat4f&)));

	FTransform3f_.Method("void ScaleTranslation(const FVector3f& InScale3D)", METHODPR_TRIVIAL(void, FTransform3f, ScaleTranslation, (const FVector3f&)));
	FTransform3f_.Method("void ScaleTranslation(const float32& Scale)", METHODPR_TRIVIAL(void, FTransform3f, ScaleTranslation, (const float&)));
	FTransform3f_.Method("void RemoveScaling(float32 Tolerance=__SMALL_NUMBER_flt)", METHODPR_TRIVIAL(void, FTransform3f, RemoveScaling, (float)));

	FTransform3f_.Method("void SetToRelativeTransform(const FTransform3f& Other)", METHOD_TRIVIAL(FTransform3f, SetToRelativeTransform));
	
	FTransform3f_.Method("void Accumulate(const FTransform3f& SourceAtom)", METHODPR_TRIVIAL(void, FTransform3f, Accumulate, (const FTransform3f&)))
		.Documentation(TEXT(
	"Accumulates another transform with this one\n"
    "Rotation is accumulated multiplicatively (Rotation = SourceAtom.Rotation * Rotation)\n"
    "Translation is accumulated additively (Translation += SourceAtom.Translation)\n"
    "Scale3D is accumulated multiplicatively (Scale3D *= SourceAtom.Scale3D)\n"
    "@param SourceAtom The other transform to accumulate into this one"
     ));

	FTransform3f_.Method("float32 GetMaximumAxisScale() const", METHODPR_TRIVIAL(float, FTransform3f, GetMaximumAxisScale, () const));
	FTransform3f_.Method("float32 GetMinimumAxisScale() const", METHODPR_TRIVIAL(float, FTransform3f, GetMinimumAxisScale, () const));

	FTransform3f_.Method("FTransform3f GetRelativeTransform(const FTransform3f& Other) const", METHOD_TRIVIAL(FTransform3f, GetRelativeTransform));
	FTransform3f_.Method("FTransform3f GetRelativeTransformReverse(const FTransform3f& Other) const", METHOD_TRIVIAL(FTransform3f, GetRelativeTransformReverse));

	FTransform3f_.Method("FVector3f TransformPosition(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, TransformPosition));
	FTransform3f_.Method("FVector3f TransformPositionNoScale(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, TransformPositionNoScale));

	FTransform3f_.Method("FVector3f InverseTransformPosition(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, InverseTransformPosition));
	FTransform3f_.Method("FVector3f InverseTransformPositionNoScale(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, InverseTransformPositionNoScale));

	FTransform3f_.Method("FVector3f TransformVector(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, TransformVector));
	FTransform3f_.Method("FVector3f TransformVectorNoScale(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, TransformVectorNoScale));

	FTransform3f_.Method("FVector3f InverseTransformVector(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, InverseTransformVector));
	FTransform3f_.Method("FVector3f InverseTransformVectorNoScale(const FVector3f& V) const", METHOD_TRIVIAL(FTransform3f, InverseTransformVectorNoScale));

	FTransform3f_.Method("FQuat4f TransformRotation(const FQuat4f& Q) const", METHOD_TRIVIAL(FTransform3f, TransformRotation));
	FTransform3f_.Method("FQuat4f InverseTransformRotation(const FQuat4f& Q) const", METHOD_TRIVIAL(FTransform3f, InverseTransformRotation));

	FTransform3f_.Method("FVector3f SubtractTranslations(const FTransform3f& B) const", FUNC_TRIVIAL(FTransform3f::SubtractTranslations));

	FTransform3f_.Method("void NormalizeRotation()", METHOD_TRIVIAL(FTransform3f, NormalizeRotation));

	FTransform3f_.Method("bool IsRotationNormalized() const", METHOD_TRIVIAL(FTransform3f, IsRotationNormalized));

	FTransform3f_.Method("bool TranslationEquals(const FTransform3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHODPR_TRIVIAL(bool, FTransform3f, TranslationEquals, (const FTransform3f&, float) const));

	FTransform3f_.Method("bool EqualsNoScale(const FTransform3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHODPR_TRIVIAL(bool, FTransform3f, EqualsNoScale, (const FTransform3f&, float) const));

	FTransform3f_.Method("bool Equals(const FTransform3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHODPR_TRIVIAL(bool, FTransform3f, Equals, (const FTransform3f&, float) const));

	FTransform3f_.Method("FVector3f GetLocation() const", METHOD_TRIVIAL(FTransform3f, GetLocation));

	FTransform3f_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FTransform3f, ContainsNaN));
	FTransform3f_.Method("bool IsValid() const", METHOD_TRIVIAL(FTransform3f, IsValid));

	FTransform3f_.Method("float32 GetDeterminant() const", METHODPR_TRIVIAL(float, FTransform3f, GetDeterminant, () const));
	FTransform3f_.Method("FRotator3f Rotator() const", METHOD_TRIVIAL(FTransform3f, Rotator));

	FTransform3f_.Method("FVector3f GetTranslation() const", METHOD_TRIVIAL(FTransform3f, GetTranslation));
	FTransform3f_.Method("FVector3f GetScale3D() const", METHOD_TRIVIAL(FTransform3f, GetScale3D));
	FTransform3f_.Method("FQuat4f GetRotation() const", METHOD_TRIVIAL(FTransform3f, GetRotation));

	FTransform3f_.Method("void SetLocation(const FVector3f& Origin)", METHOD_TRIVIAL(FTransform3f, SetLocation));
	FTransform3f_.Method("void SetTranslation(const FVector3f& Origin)", METHOD_TRIVIAL(FTransform3f, SetTranslation));

	FTransform3f_.Method("void AddToTranslation(const FVector3f& Origin)", METHOD_TRIVIAL(FTransform3f, AddToTranslation));

	FTransform3f_.Method("void ConcatenateRotation(const FQuat4f& DeltaRotation)", METHOD_TRIVIAL(FTransform3f, ConcatenateRotation))
		.Documentation(TEXT(
	"Concatenates another rotation to this transformation\n"
    "@param DeltaRotation The rotation to concatenate in the following fashion: Rotation = Rotation * DeltaRotation"
    ));

	FTransform3f_.Method("void SetRotation(const FQuat4f& NewRotation)", METHOD_TRIVIAL(FTransform3f, SetRotation));
	FTransform3f_.Method("void SetScale3D(const FVector3f& NewScale3D)", METHOD_TRIVIAL(FTransform3f, SetScale3D));
	FTransform3f_.Method("void SetTranslationAndScale3D(const FVector3f& NewTranslation, const FVector3f& NewScale3D)", METHOD_TRIVIAL(FTransform3f, SetTranslationAndScale3D));

	FTransform3f_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FTransform3f, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform3f_Type(
	TEXT("FTransform3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFTransform3fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform3f_Infrastructure(
	TEXT("FTransform3f.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFTransform3fInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform3f(
	TEXT("FTransform3f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFTransform3fFunctions);
