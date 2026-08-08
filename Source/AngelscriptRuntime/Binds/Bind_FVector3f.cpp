#include "Misc/DefaultValueHelper.h"

#include "Engine/NetSerialization.h"

#include "AngelscriptBinds.h"

#include "Bind_FVector3f_Functions.h"
#include "Helper_StructType.h"
#include "Helper_ToString.h"

struct FVector3fType : TAngelscriptVariantStructType<FVector3f>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FVector3f");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new(DestinationPtr) FVector3f(0.f, 0.f, 0.f);
	}

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr)
			return false;
		if (StructProp->Struct == GetStruct(Usage))
			return true;
		return false;
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue.IsEmpty())
		{
			OutValue = TEXT("FVector3f()");
			return true;
		}
		FVector3f Value;
		if (FDefaultValueHelper::ParseVector(InValue, Value))
		{
			OutValue = FString::Printf(TEXT("FVector3f(%f,%f,%f)"), Value.X, Value.Y, Value.Z);
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override
	{
		FString Parameters;
		if(FDefaultValueHelper::Is( CppForm, TEXT("FVector3f :: ZeroVector") ))
		{
			return true;
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: UpVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector3f::UpVector.X, FVector3f::UpVector.Y, FVector3f::UpVector.Z);
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: ForwardVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector3f::ForwardVector.X, FVector3f::ForwardVector.Y, FVector3f::ForwardVector.Z);
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: RightVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector3f::RightVector.X, FVector3f::RightVector.Y, FVector3f::RightVector.Z);
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: OneVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector3f::OneVector.X, FVector3f::OneVector.Y, FVector3f::OneVector.Z);
		}
		else if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector3f"), Parameters) )
		{
			FVector3f Vector;
			float Value;
			if (FDefaultValueHelper::ParseVector(Parameters, Vector))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					Vector.X, Vector.Y, Vector.Z);
			}
			else if (FDefaultValueHelper::ParseFloat(Parameters, Value))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					Value, Value, Value);
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

static void BindFVector3fTypeDeclarations(FAngelscriptBinds& Binds)
{
	FBindFlags Flags;
	Flags.bPOD = true;
	Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
	Binds.ValueClassForTarget<FVector3f>("FVector3f", Flags);
}

static void BindFVector3fInfrastructure(FAngelscriptBinds& Binds)
{
	auto VectorType = MakeShared<FVector3fType>();
	Binds.RegisterTypeForTarget(VectorType);
	FToStringHelper::Register(Binds, TEXT("FVector3f"), &FAngelscriptFVector3fBinds::AppendToString);
}

static void BindFVector3fManual(FAngelscriptBinds& Binds)
{
	auto FVector3f_ = Binds.ExistingClassForTarget("FVector3f");

	FVector3f_.Constructor("void f(float32 X, float32 Y, float32 Z)", &FAngelscriptFVector3fBinds::ConstructXYZ, "FVector3f", true)
		.NoDiscard();
	FVector3f_.Constructor("void f()", &FAngelscriptFVector3fBinds::ConstructZero)
		.NoDiscard()
		.NativeConstructor("FVector3f", true, "0.f");
	FVector3f_.Constructor("void f(float32 F)", &FAngelscriptFVector3fBinds::ConstructScalar, "FVector3f", true)
		.NoDiscard();
	FVector3f_.Constructor("void f(const FVector3f& Other)", &FAngelscriptFVector3fBinds::ConstructCopy, "FVector3f", true)
		.NoDiscard();

	FVector3f_.Property("float32 X", &FVector3f::X);
	FVector3f_.Property("float32 Y", &FVector3f::Y);
	FVector3f_.Property("float32 Z", &FVector3f::Z);

	FVector3f_.Method("FVector3f& opAssign(const FVector3f& Other)", METHODPR_TRIVIAL(FVector3f&, FVector3f, operator=, (const FVector3f&)));

	FVector3f_.Method("FVector3f opAdd(const FVector3f& Other) const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator+, (const FVector3f&) const));
	FVector3f_.Method("FVector3f opSub(const FVector3f& Other) const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator-, (const FVector3f&) const));
	FVector3f_.Method("FVector3f opMul(const FVector3f& Other) const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator*, (const FVector3f&) const));
	FVector3f_.Method("FVector3f opDiv(const FVector3f& Other) const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator/, (const FVector3f&) const));

	FVector3f_.Method("FVector3f opMul(float32 Scale) const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator*, (float) const));
	FVector3f_.Method("FVector3f opDiv(float32 Scale) const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator/, (float) const));

	FVector3f_.Method("FVector3f opNeg() const", METHODPR_TRIVIAL(FVector3f, FVector3f, operator-, () const));

	FVector3f_.Method("FVector3f opMulAssign(float32 Scale)", METHODPR_TRIVIAL(FVector3f, FVector3f, operator*=, (float)));
	FVector3f_.Method("FVector3f opDivAssign(float32 Scale)", METHODPR_TRIVIAL(FVector3f, FVector3f, operator/=, (float)));

	FVector3f_.Method("FVector3f opMulAssign(const FVector3f& Other)", METHODPR_TRIVIAL(FVector3f, FVector3f, operator*=, (const FVector3f&)));
	FVector3f_.Method("FVector3f opDivAssign(const FVector3f& Other)", METHODPR_TRIVIAL(FVector3f, FVector3f, operator/=, (const FVector3f&)));

	FVector3f_.Method("FVector3f opAddAssign(const FVector3f& Other)", METHODPR_TRIVIAL(FVector3f, FVector3f, operator+=, (const FVector3f&)));
	FVector3f_.Method("FVector3f opSubAssign(const FVector3f& Other)", METHODPR_TRIVIAL(FVector3f, FVector3f, operator-=, (const FVector3f&)));

	FVector3f_.Method("float32& opIndex(int32 Index)", METHODPR_TRIVIAL(float&, FVector3f, operator[], (int32)));
	FVector3f_.Method("float32 opIndex(int32 Index) const", METHODPR_TRIVIAL(float, FVector3f, operator[], (int32) const));

	FVector3f_.Method("bool opEquals(const FVector3f& Other) const", METHODPR_TRIVIAL(bool, FVector3f, operator==, (const FVector3f&) const));
	FVector3f_.Method("bool Equals(const FVector3f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector3f, Equals));


	FVector3f_.Method("FVector3f CrossProduct(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::CrossProduct));
	FVector3f_.Method("float32 DotProduct(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::DotProduct));

	FVector3f_.Method("bool AllComponentsEqual(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector3f, AllComponentsEqual));

	FVector3f_.Method(
		"bool Parallel(const FVector3f& Normal2, float32 ParallelCosineThreshold = __THRESH_NORMALS_ARE_PARALLEL_flt) const",
		FUNC_TRIVIAL(FVector3f::Parallel))
		.Documentation(UTF8_TO_TCHAR(
	 "* See if two normal vectors are nearly parallel, meaning the angle between them is close to 0 degrees. \n"
	 "* @param  Normal1 First normalized vector.\n"
	 "* @param  Normal1 Second normalized vector.\n"
	 "* @param  ParallelCosineThreshold Normals are parallel if absolute value of dot product (cosine of angle between them) is greater than or equal to this. For example: cos(1.0 degrees). \n"
	 "* @return true if vectors are nearly parallel, false otherwise. \n"
	));

	FVector3f_.Method(
		"bool Coincident(const FVector3f& Normal2, float32 ParallelCosineThreshold = __THRESH_NORMALS_ARE_PARALLEL_flt) const",
		FUNC_TRIVIAL(FVector3f::Coincident))
		.Documentation(UTF8_TO_TCHAR(
	 "* See if two normal vectors are coincident (nearly parallel and point in the same direction).\n"
	 "* @param  Normal1 First normalized vector.\n"
	 "* @param  Normal2 Second normalized vector.\n"
	 "* @param  ParallelCosineThreshold Normals are coincident if dot product (cosine of angle between them) is greater than or equal to this. For example: cos(1.0 degrees).\n"
	 "* @return true if vectors are coincident (nearly parallel and point in the same direction), false otherwise.\n"
	));

	FVector3f_.Method(
		"bool Orthogonal(const FVector3f& Normal2, float32 OrthogonalCosineThreshold = __THRESH_NORMALS_ARE_ORTHOGONAL_flt) const",
		FUNC_TRIVIAL(FVector3f::Orthogonal))
		.Documentation(UTF8_TO_TCHAR(
	 "* See if two normal vectors are nearly orthogonal (perpendicular), meaning the angle between them is close to 90 degrees.\n"
	 "* @param  Normal1 First normalized vector.\n"
	 "* @param  Normal2 Second normalized vector.\n"
	 "* @param  OrthogonalCosineThreshold Normals are orthogonal if absolute value of dot product (cosine of angle between them) is less than or equal to this. For example: cos(89.0 degrees).\n"
	 "* @return true if vectors are orthogonal (perpendicular), false otherwise.\n"
	));

	FVector3f_.Method("float32 GetMax() const", METHOD_TRIVIAL(FVector3f, GetMax));
	FVector3f_.Method("float32 GetAbsMax() const", METHOD_TRIVIAL(FVector3f, GetAbsMax));
	FVector3f_.Method("float32 GetMin() const", METHOD_TRIVIAL(FVector3f, GetMin));
	FVector3f_.Method("float32 GetAbsMin() const", METHOD_TRIVIAL(FVector3f, GetAbsMin));

	FVector3f_.Method("FVector3f ComponentMin(const FVector3f& Other) const", METHOD_TRIVIAL(FVector3f, ComponentMin));
	FVector3f_.Method("FVector3f ComponentMax(const FVector3f& Other) const", METHOD_TRIVIAL(FVector3f, ComponentMax));

	// This is only available as BoundToBox in C++, but I like the symmetry with ComponentMin/ComponentMax
	FVector3f_.Method("FVector3f ComponentClamp(const FVector3f& Min, const FVector3f& Max) const", METHOD_TRIVIAL(FVector3f, BoundToBox));

	FVector3f_.Method("FVector3f GetAbs() const", METHOD_TRIVIAL(FVector3f, GetAbs));
	FVector3f_.Method("float32 Size() const", METHOD_TRIVIAL(FVector3f, Size));
	FVector3f_.Method("float32 SizeSquared() const", METHOD_TRIVIAL(FVector3f, SizeSquared));
	FVector3f_.Method("float32 Size2D() const", METHOD_TRIVIAL(FVector3f, Size2D));
	FVector3f_.Method("float32 SizeSquared2D() const", METHOD_TRIVIAL(FVector3f, SizeSquared2D));

	FVector3f_.Method("bool IsNearlyZero(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector3f, IsNearlyZero));
	FVector3f_.Method("bool IsZero() const", METHOD_TRIVIAL(FVector3f, IsZero));

	FVector3f_.Method("bool Normalize(float32 Tolerance = __SMALL_NUMBER_flt)", METHOD_TRIVIAL(FVector3f, Normalize));
	FVector3f_.Method("bool IsNormalized() const", METHOD_TRIVIAL(FVector3f, IsNormalized));

	FVector3f_.Method("void ToDirectionAndLength(FVector3f& OutDir, float32& OutLength) const", METHODPR_TRIVIAL(void, FVector3f, ToDirectionAndLength, (FVector3f&, float&) const));

	FVector3f_.Method("FVector3f GetSignVector() const", METHOD_TRIVIAL(FVector3f, GetSignVector));
	FVector3f_.Method("FVector3f Projection() const", METHOD_TRIVIAL(FVector3f, Projection));
	FVector3f_.Method("FVector3f GetUnsafeNormal() const", METHOD_TRIVIAL(FVector3f, GetUnsafeNormal));
	FVector3f_.Method("FVector3f GridSnap(const float32& GridSize) const", METHOD_TRIVIAL(FVector3f, GridSnap));

	FVector3f_.Method("FVector3f BoundToCube(float32 Radius) const", METHOD_TRIVIAL(FVector3f, BoundToCube));
	FVector3f_.Method("FVector3f BoundToBox(const FVector3f& Min, const FVector3f& Max) const", METHOD_TRIVIAL(FVector3f, BoundToBox));
	FVector3f_.Method("FVector3f GetClampedToSize(float32 Min, float32 Max) const", METHOD_TRIVIAL(FVector3f, GetClampedToSize));
	FVector3f_.Method("FVector3f GetClampedToSize2D(float32 Min, float32 Max) const", METHOD_TRIVIAL(FVector3f, GetClampedToSize2D));
	FVector3f_.Method("FVector3f GetClampedToMaxSize(float32 Max) const", METHOD_TRIVIAL(FVector3f, GetClampedToMaxSize));
	FVector3f_.Method("FVector3f GetClampedToMaxSize2D(float32 Max) const", METHOD_TRIVIAL(FVector3f, GetClampedToMaxSize2D));

	FVector3f_.Method("void AddBounded(const FVector3f& V, float32 Radius=MAX_int16) const", METHOD_TRIVIAL(FVector3f, AddBounded));
	FVector3f_.Method("FVector3f Reciprocal() const", METHOD_TRIVIAL(FVector3f, Reciprocal));
	FVector3f_.Method("bool IsUniform(float32 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector3f, IsUniform));

	FVector3f_.Method("FVector3f MirrorByVector(const FVector3f& MirrorNormal) const", METHOD_TRIVIAL(FVector3f, MirrorByVector));
	FVector3f_.Method("FVector3f VectorPlaneProject(const FVector3f& PlaneNormal) const", FUNC_TRIVIAL(FVector3f::VectorPlaneProject));

	FVector3f_.Method("FVector3f RotateAngleAxis(float32 AngleDeg, const FVector3f& Axis) const", METHOD_TRIVIAL(FVector3f, RotateAngleAxis));

	FVector3f_.Method("FVector3f GetSafeNormal(float32 Tolerance = __SMALL_NUMBER_flt, const FVector3f& ResultIfZero = FVector3f::ZeroVector) const", METHOD_TRIVIAL(FVector3f, GetSafeNormal));
	FVector3f_.Method("FVector3f GetSafeNormal2D(float32 Tolerance = __SMALL_NUMBER_flt, const FVector3f& ResultIfZero = FVector3f::ZeroVector) const", METHOD_TRIVIAL(FVector3f, GetSafeNormal2D));


	FVector3f_.Method("float32 CosineAngle2D(FVector3f B) const", METHOD_TRIVIAL(FVector3f, CosineAngle2D));

	FVector3f_.Method(
		"FVector3f ProjectOnTo(const FVector3f& A) const",
		METHOD_TRIVIAL(FVector3f, ProjectOnTo))
		.Documentation(UTF8_TO_TCHAR(
	 "Gets a copy of this vector projected onto the input vector.\n\n"
	 "@param A	Vector to project onto, does not assume it is normalized.\n"
	 "@return Projected vector."
	));

	FVector3f_.Method(
		"FVector3f ProjectOnToNormal(const FVector3f& Normal) const",
		METHOD_TRIVIAL(FVector3f, ProjectOnToNormal))
		.Documentation(UTF8_TO_TCHAR(
	 "Gets a copy of this vector projected onto the input vector, which is assumed to be unit length.\n\n"
	 "@param A	Normal vector to project onto (assumed to be unit length).\n"
	 "@return Projected vector."
	));

	FVector3f_.Method("void FindBestAxisVectors(FVector3f& Axis1, FVector3f& Axis2) const", METHOD_TRIVIAL(FVector3f, FindBestAxisVectors));
	FVector3f_.Method("void UnwindEuler() const", METHOD_TRIVIAL(FVector3f, UnwindEuler));
	FVector3f_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FVector3f, ContainsNaN));

	FVector3f_.Method("bool IsUnit(float32 LengthSquaredTolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector3f, IsUnit));

	FVector3f_.Method("float32 HeadingAngle() const", METHOD_TRIVIAL(FVector3f, HeadingAngle));
	FVector3f_.Method("bool PointsAreSame(const FVector3f& P2) const", FUNC_TRIVIAL(FVector3f::PointsAreSame));
	FVector3f_.Method("bool PointsAreNear(const FVector3f& P2, float32 Dist) const", FUNC_TRIVIAL(FVector3f::PointsAreNear));

	FVector3f_.Method("float32 Distance(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::Distance));
	FVector3f_.Method("float32 DistSquared(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::DistSquared));
	FVector3f_.Method("float32 Dist2D(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::Dist2D));
	FVector3f_.Method("float32 DistXY(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::DistXY));
	FVector3f_.Method("float32 DistSquaredXY(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::DistSquaredXY));
	FVector3f_.Method("float32 DistSquared2D(const FVector3f& Other) const", FUNC_TRIVIAL(FVector3f::DistSquared2D));

	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FVector3f");
		Binds.BindGlobalVariableForTarget("const FVector3f ZeroVector", &FVector3f::ZeroVector);
		Binds.BindGlobalVariableForTarget("const FVector3f OneVector", &FVector3f::OneVector);
		Binds.BindGlobalVariableForTarget("const FVector3f UpVector", &FVector3f::UpVector);
		Binds.BindGlobalVariableForTarget("const FVector3f ForwardVector", &FVector3f::ForwardVector);
		Binds.BindGlobalVariableForTarget("const FVector3f RightVector", &FVector3f::RightVector);
	}

	FVector3f_.Method("FRotator3f ToOrientationRotator() const", METHOD_TRIVIAL(FVector3f, ToOrientationRotator));
	FVector3f_.Method("FQuat4f ToOrientationQuat() const", METHOD_TRIVIAL(FVector3f, ToOrientationQuat));
	FVector3f_.Method("FRotator3f Rotation() const", METHOD_TRIVIAL(FVector3f, Rotation));

	FVector3f_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FVector3f, InitFromString));

	FVector3f_.Constructor("void f(const FVector& Other)", &FAngelscriptFVector3fBinds::ConstructFromVector, "FVector3f", true)
		.NoDiscard();
}

AS_FORCE_LINK const FAngelscriptBind Bind_FVector3f_TypeDeclarations(
	TEXT("FVector3f.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFVector3fTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector3f_TypeInfrastructure(
	TEXT("FVector3f.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVector3fInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector3f(
	TEXT("FVector3f.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFVector3fManual);
