#include "Misc/DefaultValueHelper.h"

#include "Engine/NetSerialization.h"

#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"

#include "Bind_FVector_Functions.h"
#include "Helper_StructType.h"
#include "Helper_ToString.h"

struct FVectorType : TAngelscriptBaseStructType<FVector>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FVector");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new(DestinationPtr) FVector(0.f, 0.f, 0.f);
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
		if (StructProp->Struct == FVector_NetQuantize::StaticStruct())
			return true;
		if (StructProp->Struct == FVector_NetQuantize10::StaticStruct())
			return true;
		if (StructProp->Struct == FVector_NetQuantize100::StaticStruct())
			return true;
		if (StructProp->Struct == FVector_NetQuantizeNormal::StaticStruct())
			return true;
		return false;
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue.IsEmpty())
		{
			OutValue = TEXT("FVector()");
			return true;
		}
		FVector Value;
		if (FDefaultValueHelper::ParseVector(InValue, Value))
		{
			OutValue = FString::Printf(TEXT("FVector(%f,%f,%f)"), Value.X, Value.Y, Value.Z);
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const override
	{
		FString Parameters;
		if(FDefaultValueHelper::Is( CppForm, TEXT("FVector :: ZeroVector") ))
		{
			return true;
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: UpVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector::UpVector.X, FVector::UpVector.Y, FVector::UpVector.Z);
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: ForwardVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector::ForwardVector.X, FVector::ForwardVector.Y, FVector::ForwardVector.Z);
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: RightVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector::RightVector.X, FVector::RightVector.Y, FVector::RightVector.Z);
		}
		else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: OneVector")))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				FVector::OneVector.X, FVector::OneVector.Y, FVector::OneVector.Z);
		}
		else if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector"), Parameters) )
		{
			FVector Vector;
			double Value;
			if (FDefaultValueHelper::ParseVector(Parameters, Vector))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					Vector.X, Vector.Y, Vector.Z);
			}
			else if (FDefaultValueHelper::ParseDouble(Parameters, Value))
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

static void BindFVectorTypeDeclarations(FAngelscriptBinds& Binds)
{
	FBindFlags Flags;
	Flags.bPOD = true;
	Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
	Binds.ValueClassForTarget<FVector>("FVector", Flags);
}

static void BindFVectorInfrastructure(FAngelscriptBinds& Binds)
{
	auto VectorType = MakeShared<FVectorType>();
	Binds.RegisterTypeForTarget(VectorType);
	Binds.RegisterTypeFinderForTarget([VectorType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr)
			return false;

		if (StructProp->Struct == FVector_NetQuantize::StaticStruct()
			|| StructProp->Struct == FVector_NetQuantize10::StaticStruct()
			|| StructProp->Struct == FVector_NetQuantize100::StaticStruct()
			|| StructProp->Struct == FVector_NetQuantizeNormal::StaticStruct())
		{
			Usage.Type = VectorType;
			return true;
		}

		return false;
	});
}

static void BindFVectorManual(FAngelscriptBinds& Binds)
{
	auto FVector_ = Binds.ExistingClassForTarget("FVector");

	FVector_.Constructor("void f(float64 X, float64 Y, float64 Z)", &FAngelscriptFVectorBinds::ConstructXYZ, "FVector", true)
		.NoDiscard();

	FVector_.Constructor("void f()", &FAngelscriptFVectorBinds::ConstructZero)
		.NoDiscard()
		.NativeConstructor("FVector", true, "0.f");

	FVector_.Constructor("void f(float64 F)", &FAngelscriptFVectorBinds::ConstructScalar, "FVector", true).NoDiscard();

	FVector_.Constructor("void f(const FVector& Other)", &FAngelscriptFVectorBinds::ConstructCopy, "FVector", true)
		.NoDiscard();

	FVector_.Property("float64 X", &FVector::X);
	FVector_.Property("float64 Y", &FVector::Y);
	FVector_.Property("float64 Z", &FVector::Z);

	FVector_.Method("FVector& opAssign(const FVector& Other)", METHODPR_TRIVIAL(FVector&, FVector, operator=, (const FVector&)));

	FVector_.Method("FVector opAdd(const FVector& Other) const", METHODPR_TRIVIAL(FVector, FVector, operator+, (const FVector&) const));
	FVector_.Method("FVector opSub(const FVector& Other) const", METHODPR_TRIVIAL(FVector, FVector, operator-, (const FVector&) const));
	FVector_.Method("FVector opMul(const FVector& Other) const", METHODPR_TRIVIAL(FVector, FVector, operator*, (const FVector&) const));
	FVector_.Method("FVector opDiv(const FVector& Other) const", METHODPR_TRIVIAL(FVector, FVector, operator/, (const FVector&) const));

	FVector_.Method("FVector opMul(float64 Scale) const", METHODPR_TRIVIAL(FVector, FVector, operator*, (double) const));
	FVector_.Method("FVector opDiv(float64 Scale) const", METHODPR_TRIVIAL(FVector, FVector, operator/, (double) const));

	FVector_.Method("FVector opNeg() const", METHODPR_TRIVIAL(FVector, FVector, operator-, () const));

	FVector_.Method("FVector opMulAssign(float64 Scale)", METHODPR_TRIVIAL(FVector, FVector, operator*=, (double)));
	FVector_.Method("FVector opDivAssign(float64 Scale)", METHODPR_TRIVIAL(FVector, FVector, operator/=, (double)));

	FVector_.Method("FVector opMulAssign(const FVector& Other)", METHODPR_TRIVIAL(FVector, FVector, operator*=, (const FVector&)));
	FVector_.Method("FVector opDivAssign(const FVector& Other)", METHODPR_TRIVIAL(FVector, FVector, operator/=, (const FVector&)));

	FVector_.Method("FVector opAddAssign(const FVector& Other)", METHODPR_TRIVIAL(FVector, FVector, operator+=, (const FVector&)));
	FVector_.Method("FVector opSubAssign(const FVector& Other)", METHODPR_TRIVIAL(FVector, FVector, operator-=, (const FVector&)));

	FVector_.Method("float64& opIndex(int32 Index)", METHODPR_TRIVIAL(double&, FVector, operator[], (int32)));
	FVector_.Method("float64 opIndex(int32 Index) const", METHODPR_TRIVIAL(double, FVector, operator[], (int32) const));

	FVector_.Method("bool opEquals(const FVector& Other) const", METHODPR_TRIVIAL(bool, FVector, operator==, (const FVector&) const));
	FVector_.Method("bool Equals(const FVector& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector, Equals));


	FVector_.Method("FVector CrossProduct(const FVector& Other) const", FUNC_TRIVIAL(FVector::CrossProduct));
	FVector_.Method("float64 DotProduct(const FVector& Other) const", FUNC_TRIVIAL(FVector::DotProduct));

	FVector_.Method("bool AllComponentsEqual(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector, AllComponentsEqual));

	FVector_.Method(
		"bool Parallel(const FVector& Normal2, float64 ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL) const",
		FUNC_TRIVIAL(FVector::Parallel))
		.Documentation(UTF8_TO_TCHAR(
	 "* See if two normal vectors are nearly parallel, meaning the angle between them is close to 0 degrees. \n"
	 "* @param  Normal1 First normalized vector.\n"
	 "* @param  Normal1 Second normalized vector.\n"
	 "* @param  ParallelCosineThreshold Normals are parallel if absolute value of dot product (cosine of angle between them) is greater than or equal to this. For example: cos(1.0 degrees). \n"
	 "* @return true if vectors are nearly parallel, false otherwise. \n"
	));

	FVector_.Method(
		"bool Coincident(const FVector& Normal2, float64 ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL) const",
		FUNC_TRIVIAL(FVector::Coincident))
		.Documentation(UTF8_TO_TCHAR(
	 "* See if two normal vectors are coincident (nearly parallel and point in the same direction).\n"
	 "* @param  Normal1 First normalized vector.\n"
	 "* @param  Normal2 Second normalized vector.\n"
	 "* @param  ParallelCosineThreshold Normals are coincident if dot product (cosine of angle between them) is greater than or equal to this. For example: cos(1.0 degrees).\n"
	 "* @return true if vectors are coincident (nearly parallel and point in the same direction), false otherwise.\n"
	));

	FVector_.Method(
		"bool Orthogonal(const FVector& Normal2, float64 OrthogonalCosineThreshold = THRESH_NORMALS_ARE_ORTHOGONAL) const",
		FUNC_TRIVIAL(FVector::Orthogonal))
		.Documentation(UTF8_TO_TCHAR(
	 "* See if two normal vectors are nearly orthogonal (perpendicular), meaning the angle between them is close to 90 degrees.\n"
	 "* @param  Normal1 First normalized vector.\n"
	 "* @param  Normal2 Second normalized vector.\n"
	 "* @param  OrthogonalCosineThreshold Normals are orthogonal if absolute value of dot product (cosine of angle between them) is less than or equal to this. For example: cos(89.0 degrees).\n"
	 "* @return true if vectors are orthogonal (perpendicular), false otherwise.\n"
	));

	FVector_.Method("float64 GetMax() const", METHOD_TRIVIAL(FVector, GetMax));
	FVector_.Method("float64 GetAbsMax() const", METHOD_TRIVIAL(FVector, GetAbsMax));
	FVector_.Method("float64 GetMin() const", METHOD_TRIVIAL(FVector, GetMin));
	FVector_.Method("float64 GetAbsMin() const", METHOD_TRIVIAL(FVector, GetAbsMin));

	FVector_.Method("FVector ComponentMin(const FVector& Other) const", METHOD_TRIVIAL(FVector, ComponentMin));
	FVector_.Method("FVector ComponentMax(const FVector& Other) const", METHOD_TRIVIAL(FVector, ComponentMax));

	// This is only available as BoundToBox in C++, but I like the symmetry with ComponentMin/ComponentMax
	FVector_.Method("FVector ComponentClamp(const FVector& Min, const FVector& Max) const", METHOD_TRIVIAL(FVector, BoundToBox));

	FVector_.Method("FVector GetAbs() const", METHOD_TRIVIAL(FVector, GetAbs));
	FVector_.Method("float64 Size() const", METHOD_TRIVIAL(FVector, Size));
	FVector_.Method("float64 SizeSquared() const", METHOD_TRIVIAL(FVector, SizeSquared));
	FVector_.Method("float64 Size2D() const", METHOD_TRIVIAL(FVector, Size2D));
	FVector_.Method("float64 SizeSquared2D() const", METHOD_TRIVIAL(FVector, SizeSquared2D));

	FVector_.Method("bool IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector, IsNearlyZero));
	FVector_.Method("bool IsZero() const", METHOD_TRIVIAL(FVector, IsZero));

	FVector_.Method("bool Normalize(float64 Tolerance = SMALL_NUMBER)", METHOD_TRIVIAL(FVector, Normalize));
	FVector_.Method("bool IsNormalized() const", METHOD_TRIVIAL(FVector, IsNormalized));

	FVector_.Method("void ToDirectionAndLength(FVector& OutDir, float64& OutLength) const", METHODPR_TRIVIAL(void, FVector, ToDirectionAndLength, (FVector&, double&) const));
	FVector_.Method("void ToDirectionAndLength(FVector& OutDir, float32& OutLength) const", METHODPR_TRIVIAL(void, FVector, ToDirectionAndLength, (FVector&, float&) const));

	FVector_.Method("FVector GetSignVector() const", METHOD_TRIVIAL(FVector, GetSignVector));
	FVector_.Method("FVector Projection() const", METHOD_TRIVIAL(FVector, Projection));
	FVector_.Method("FVector GetUnsafeNormal() const", METHOD_TRIVIAL(FVector, GetUnsafeNormal));
	FVector_.Method("FVector GridSnap(const float64& GridSize) const", METHOD_TRIVIAL(FVector, GridSnap));

	FVector_.Method("FVector BoundToCube(float64 Radius) const", METHOD_TRIVIAL(FVector, BoundToCube));
	FVector_.Method("FVector BoundToBox(const FVector& Min, const FVector& Max) const", METHOD_TRIVIAL(FVector, BoundToBox));
	FVector_.Method("FVector GetClampedToSize(float64 Min, float64 Max) const", METHOD_TRIVIAL(FVector, GetClampedToSize));
	FVector_.Method("FVector GetClampedToSize2D(float64 Min, float64 Max) const", METHOD_TRIVIAL(FVector, GetClampedToSize2D));
	FVector_.Method("FVector GetClampedToMaxSize(float64 Max) const", METHOD_TRIVIAL(FVector, GetClampedToMaxSize));
	FVector_.Method("FVector GetClampedToMaxSize2D(float64 Max) const", METHOD_TRIVIAL(FVector, GetClampedToMaxSize2D));

	FVector_.Method("void AddBounded(const FVector& V, float64 Radius=MAX_int16)", METHOD_TRIVIAL(FVector, AddBounded));
	FVector_.Method("FVector Reciprocal() const", METHOD_TRIVIAL(FVector, Reciprocal));
	FVector_.Method("bool IsUniform(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector, IsUniform));

	FVector_.Method("FVector MirrorByVector(const FVector& MirrorNormal) const", METHOD_TRIVIAL(FVector, MirrorByVector));
	FVector_.Method("FVector VectorPlaneProject(const FVector& PlaneNormal) const", FUNC_TRIVIAL(FVector::VectorPlaneProject));

	FVector_.Method("FVector RotateAngleAxis(float64 AngleDeg, const FVector& Axis) const", METHOD_TRIVIAL(FVector, RotateAngleAxis));

	FVector_.Method("FVector GetSafeNormal(float64 Tolerance = SMALL_NUMBER, const FVector& ResultIfZero = FVector::ZeroVector) const", METHOD_TRIVIAL(FVector, GetSafeNormal));
	FVector_.Method("FVector GetSafeNormal2D(float64 Tolerance = SMALL_NUMBER, const FVector& ResultIfZero = FVector::ZeroVector) const", METHOD_TRIVIAL(FVector, GetSafeNormal2D));


	FVector_.Method("float64 CosineAngle2D(FVector B) const", METHOD_TRIVIAL(FVector, CosineAngle2D));

	FVector_.Method(
		"FVector ProjectOnTo(const FVector& A) const",
		METHOD_TRIVIAL(FVector, ProjectOnTo))
		.Documentation(UTF8_TO_TCHAR(
	 "Gets a copy of this vector projected onto the input vector.\n\n"
	 "@param A	Vector to project onto, does not assume it is normalized.\n"
	 "@return Projected vector."
	));

	FVector_.Method(
		"FVector ProjectOnToNormal(const FVector& Normal) const",
		METHOD_TRIVIAL(FVector, ProjectOnToNormal))
		.Documentation(UTF8_TO_TCHAR(
	 "Gets a copy of this vector projected onto the input vector, which is assumed to be unit length.\n\n"
	 "@param A	Normal vector to project onto (assumed to be unit length).\n"
	 "@return Projected vector."
	));

	FVector_.Method("void FindBestAxisVectors(FVector& Axis1, FVector& Axis2) const", METHOD_TRIVIAL(FVector, FindBestAxisVectors));
	FVector_.Method("void UnwindEuler() const", METHOD_TRIVIAL(FVector, UnwindEuler));
	FVector_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FVector, ContainsNaN));

	FVector_.Method("bool IsUnit(float64 LengthSquaredTolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector, IsUnit));

	FVector_.Method("float64 HeadingAngle() const", METHOD_TRIVIAL(FVector, HeadingAngle));
	FVector_.Method("bool PointsAreSame(const FVector& P2) const", FUNC_TRIVIAL(FVector::PointsAreSame));
	FVector_.Method("bool PointsAreNear(const FVector& P2, float64 Dist) const", FUNC_TRIVIAL(FVector::PointsAreNear));

	FVector_.Method("float64 Distance(const FVector& Other) const", FUNC_TRIVIAL(FVector::Distance));
	FVector_.Method("float64 DistSquared(const FVector& Other) const", FUNC_TRIVIAL(FVector::DistSquared));
	FVector_.Method("float64 Dist2D(const FVector& Other) const", FUNC_TRIVIAL(FVector::Dist2D));
	FVector_.Method("float64 DistXY(const FVector& Other) const", FUNC_TRIVIAL(FVector::DistXY));
	FVector_.Method("float64 DistSquaredXY(const FVector& Other) const", FUNC_TRIVIAL(FVector::DistSquaredXY));
	FVector_.Method("float64 DistSquared2D(const FVector& Other) const", FUNC_TRIVIAL(FVector::DistSquared2D));

	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FVector");
		Binds.BindGlobalVariableForTarget("const FVector ZeroVector", &FVector::ZeroVector);
		Binds.BindGlobalVariableForTarget("const FVector OneVector", &FVector::OneVector);
		
		Binds.BindGlobalVariableForTarget("const FVector UpVector", &FVector::UpVector);
		Binds.BindGlobalVariableForTarget("const FVector DownVector", &FVector::DownVector);
		
		Binds.BindGlobalVariableForTarget("const FVector ForwardVector", &FVector::ForwardVector);
		Binds.BindGlobalVariableForTarget("const FVector BackwardVector", &FVector::BackwardVector);
		
		Binds.BindGlobalVariableForTarget("const FVector RightVector", &FVector::RightVector);
		Binds.BindGlobalVariableForTarget("const FVector LeftVector", &FVector::LeftVector);
	}

	FVector_.Method("FRotator ToOrientationRotator() const", METHOD_TRIVIAL(FVector, ToOrientationRotator));
	FVector_.Method("FQuat ToOrientationQuat() const", METHOD_TRIVIAL(FVector, ToOrientationQuat));
	FVector_.Method("FRotator Rotation() const", METHOD_TRIVIAL(FVector, Rotation));

	FVector_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FVector, InitFromString));

	FVector_.Constructor("void f(const FVector3f& Other)", &FAngelscriptFVectorBinds::ConstructFromVector3f, "FVector", true)
		.NoDiscard();
}

static void BindFVectorToStringContribution(FAngelscriptBinds& Binds)
{
	FToStringHelper::Register(Binds, TEXT("FVector"), &FAngelscriptFVectorBinds::AppendToString);
}

AS_FORCE_LINK const FAngelscriptBind Bind_FVector_TypeDeclarations(
	TEXT("FVector.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFVectorTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector_TypeInfrastructure(
	TEXT("FVector.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVectorInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector(
	TEXT("FVector"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFVectorManual);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector_ToStringContribution(
	TEXT("FVector.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVectorToStringContribution);
