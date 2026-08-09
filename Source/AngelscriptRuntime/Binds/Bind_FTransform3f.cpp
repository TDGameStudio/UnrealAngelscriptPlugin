#include "Bind_FTransform3f.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Helper_ToString.h"
#include "AngelscriptDocs.h"

/**
 * FTransform3f binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value;                                                                                                          | Declares the FTransform3f value type.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value();                                                                                                        | Constructs the identity transform.                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(const FTransform3f& Other);                                                                               | Copies another FTransform3f.                                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(const FVector3f& InTranslation);                                                                          | Constructs an identity rotation and scale with InTranslation.                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(const FQuat4f& InRotation);                                                                               | Constructs InRotation with zero translation and unit scale.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(const FRotator3f& InRotation);                                                                            | Constructs a quaternion from InRotation with zero translation and unit scale.                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(                                                                                                          | Constructs a transform from quaternion rotation, translation, and scale.                                             |
 * |     const FQuat4f& InRotation,                                                                                               |                                                                                                                      |
 * |     const FVector3f& InTranslation,                                                                                          |                                                                                                                      |
 * |     const FVector3f& InScale3D = FVector3f::OneVector);                                                                      |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(                                                                                                          | Constructs a transform from rotator rotation, translation, and scale.                                                |
 * |     const FRotator3f& InRotation,                                                                                            |                                                                                                                      |
 * |     const FVector3f& InTranslation,                                                                                          |                                                                                                                      |
 * |     const FVector3f& InScale3D = FVector3f::OneVector);                                                                      |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(const FVector3f& InX, const FVector3f& InY, const FVector3f& InZ, const FVector3f& InTranslation);        | Constructs from basis axes and translation.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Value(const FTransform& Transform);                                                                             | Converts a double-precision FTransform to float32 components.                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Transform = Other;                                                                                              | Copies Other's rotation, translation, and scale into Transform.                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f FTransform3f.Inverse() const;                                                                                   | Returns the inverse transform.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.Blend(const FTransform3f& Atom1, const FTransform3f& Atom2, float32 Alpha);                                | Replaces this value with a blend from Atom1 to Atom2.                                                                |
 * |                                                                                                                              | @param Alpha Blend weight; 0 uses Atom1 and 1 uses Atom2.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.BlendWith(const FTransform3f& OtherAtom, float32 Alpha);                                                   | Blends this value toward OtherAtom.                                                                                  |
 * |                                                                                                                              | @param Alpha Blend weight; 0 keeps this value and 1 uses OtherAtom.                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Result = Transform * OtherTransform;                                                                            | Composes Transform with OtherTransform.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f Result = Transform * OtherRotation;                                                                             | Appends quaternion rotation OtherRotation to Transform.                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Transform *= OtherTransform;                                                                                                 | Composes OtherTransform into Transform in place.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Transform *= OtherRotation;                                                                                                  | Appends quaternion rotation OtherRotation in place.                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.ScaleTranslation(const FVector3f& InScale3D);                                                              | Scales translation independently along each axis.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.ScaleTranslation(const float32& Scale);                                                                    | Scales all translation components uniformly.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.RemoveScaling(float32 Tolerance=__SMALL_NUMBER_flt);                                                       | Removes scale while preserving translation and a normalized rotation.                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.SetToRelativeTransform(const FTransform3f& Other);                                                         | Rewrites this transform relative to Other.                                                                           |
 * |                                                                                                                              | @param Other Parent or reference-space transform.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.Accumulate(const FTransform3f& SourceAtom);                                                                | Prepends SourceAtom rotation, adds translation, and multiplies scale into this value.                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FTransform3f.GetMaximumAxisScale() const;                                                                            | Returns the largest absolute scale component.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FTransform3f.GetMinimumAxisScale() const;                                                                            | Returns the smallest absolute scale component.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f FTransform3f.GetRelativeTransform(const FTransform3f& Other) const;                                             | Returns this transform expressed relative to Other.                                                                  |
 * |                                                                                                                              | @param Other Parent or reference-space transform.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform3f FTransform3f.GetRelativeTransformReverse(const FTransform3f& Other) const;                                      | Returns Other expressed relative to this transform (reverse operand order).                                          |
 * |                                                                                                                              | @param Other Transform converted relative to this value.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.TransformPosition(const FVector3f& V) const;                                                          | Transforms a local position to parent space, including scale and translation.                                        |
 * |                                                                                                                              | @param V Position in this transform's local space.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.TransformPositionNoScale(const FVector3f& V) const;                                                   | Transforms a local position to parent space without applying scale.                                                  |
 * |                                                                                                                              | @param V Position in this transform's local space.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.InverseTransformPosition(const FVector3f& V) const;                                                   | Transforms a parent-space position to local space, including inverse scale.                                          |
 * |                                                                                                                              | @param V Position in the parent space.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.InverseTransformPositionNoScale(const FVector3f& V) const;                                            | Transforms a parent-space position to local space without inverse scale.                                             |
 * |                                                                                                                              | @param V Position in the parent space.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.TransformVector(const FVector3f& V) const;                                                            | Transforms a local vector to parent space with rotation and scale, but no translation.                               |
 * |                                                                                                                              | @param V Direction or displacement in local space.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.TransformVectorNoScale(const FVector3f& V) const;                                                     | Rotates a local vector to parent space without scale or translation.                                                 |
 * |                                                                                                                              | @param V Direction or displacement in local space.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.InverseTransformVector(const FVector3f& V) const;                                                     | Transforms a parent-space vector to local space with inverse rotation and scale.                                     |
 * |                                                                                                                              | @param V Direction or displacement in parent space.                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.InverseTransformVectorNoScale(const FVector3f& V) const;                                              | Inverse-rotates a parent-space vector without scale or translation.                                                  |
 * |                                                                                                                              | @param V Direction or displacement in parent space.                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FQuat4f FTransform3f.TransformRotation(const FQuat4f& Q) const;                                                              | Converts a local-space rotation to parent space.                                                                     |
 * |                                                                                                                              | @param Q Rotation in this transform's local space.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FQuat4f FTransform3f.InverseTransformRotation(const FQuat4f& Q) const;                                                       | Converts a parent-space rotation to this transform's local space.                                                    |
 * |                                                                                                                              | @param Q Rotation in the parent space.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.SubtractTranslations(const FTransform3f& B) const;                                                    | Returns this translation minus B's translation.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.NormalizeRotation();                                                                                       | Normalizes the rotation quaternion in place.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.IsRotationNormalized() const;                                                                              | Reports whether the rotation quaternion has unit length.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.TranslationEquals(const FTransform3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;          | Compares translation components within Tolerance.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.EqualsNoScale(const FTransform3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;              | Compares rotation and translation within Tolerance, ignoring scale.                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.Equals(const FTransform3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;                     | Compares rotation, translation, and scale within Tolerance.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.GetLocation() const;                                                                                  | Returns the translation component.                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.ContainsNaN() const;                                                                                       | Reports whether rotation, translation, or scale contains a non-finite value.                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.IsValid() const;                                                                                           | Reports whether all components are finite and the rotation is normalized.                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FTransform3f.GetDeterminant() const;                                                                                 | Returns the determinant represented by the scale component.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f FTransform3f.Rotator() const;                                                                                     | Returns the rotation component as a rotator.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.GetTranslation() const;                                                                               | Returns the translation component.                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector3f FTransform3f.GetScale3D() const;                                                                                   | Returns the per-axis scale component.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FQuat4f FTransform3f.GetRotation() const;                                                                                    | Returns the rotation quaternion.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.SetLocation(const FVector3f& Origin);                                                                      | Replaces the translation component with Origin.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.SetTranslation(const FVector3f& Origin);                                                                   | Replaces the translation component with Origin.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.AddToTranslation(const FVector3f& Origin);                                                                 | Adds Origin to the translation component.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.ConcatenateRotation(const FQuat4f& DeltaRotation);                                                         | Post-multiplies the rotation by DeltaRotation.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.SetRotation(const FQuat4f& NewRotation);                                                                   | Replaces the rotation component.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.SetScale3D(const FVector3f& NewScale3D);                                                                   | Replaces the per-axis scale component.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FTransform3f.SetTranslationAndScale3D(const FVector3f& NewTranslation, const FVector3f& NewScale3D);                    | Replaces translation and scale together, leaving rotation unchanged.                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTransform3f.InitFromString(const FString& SourceString);                                                               | Parses UE transform text into this value and reports whether parsing succeeded.                                      |
 * |                                                                                                                              | @param SourceString UE-formatted transform text.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FTransform3f FTransform3f::Identity;                                                                                   | Identity transform constant: zero translation, identity rotation, and unit scale.                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */


AS_FORCE_LINK const FAngelscriptBind Bind_FTransform3f_Type(
	TEXT("FTransform3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FTransform3f>("FTransform3f", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform3f_Infrastructure(
	TEXT("FTransform3f.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FTransform3fType>());
		FToStringHelper::Register(Binds, TEXT("FTransform3f"), &FAngelscriptFTransform3fBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FTransform3f(
	TEXT("FTransform3f.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});
