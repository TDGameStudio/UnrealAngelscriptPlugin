#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FRandomStream_Functions.h"

struct FRandomStreamType : TAngelscriptBaseStructType<FRandomStream>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FRandomStream");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFRandomStreamType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FRandomStream>("FRandomStream", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FRandomStreamType>());
	}

	void BindFRandomStreamFunctions(FAngelscriptBinds& Binds)
	{
		auto FRandomStream_ = Binds.ExistingClassForTarget("FRandomStream");
		FRandomStream_.Constructor("void f()", &FAngelscriptFRandomStreamBinds::ConstructDefault);
		FRandomStream_.Constructor("void f(int32 InSeed)", &FAngelscriptFRandomStreamBinds::ConstructIntSeed);
		FRandomStream_.Constructor("void f(uint32 InSeed)", &FAngelscriptFRandomStreamBinds::ConstructUIntSeed);
		FRandomStream_.Method("void Initialize(int32 InSeed)", METHODPR_TRIVIAL(void, FRandomStream, Initialize, (int32)));
		FRandomStream_.Method("void Initialize(uint32 InSeed)", METHODPR_TRIVIAL(void, FRandomStream, Initialize, (int32)));
		FRandomStream_.Method("void Reset() const", METHOD_TRIVIAL(FRandomStream, Reset));
		FRandomStream_.Method("int GetInitialSeed() const", METHOD_TRIVIAL(FRandomStream, GetInitialSeed));
		FRandomStream_.Method("void GenerateNewSeed()", METHOD_TRIVIAL(FRandomStream, GenerateNewSeed));
		FRandomStream_.Method("float32 GetFraction() const", METHOD_TRIVIAL(FRandomStream, GetFraction));
		FRandomStream_.Method("uint32 GetUnsignedInt() const", METHOD_TRIVIAL(FRandomStream, GetUnsignedInt));
		FRandomStream_.Method("int32 GetCurrentSeed() const", METHOD_TRIVIAL(FRandomStream, GetCurrentSeed));
		FRandomStream_.Method("int32 RandRange(int32 Min, int32 Max) const", METHODPR_TRIVIAL(int32, FRandomStream, RandRange, (int32, int32) const));
		FRandomStream_.Method("float64 RandRange(float64 Min, float64 Max) const", METHODPR_TRIVIAL(double, FRandomStream, FRandRange, (double, double) const));
		FRandomStream_.Method("void Initialize(FName InName)", METHODPR_TRIVIAL(void, FRandomStream, Initialize, (FName)));
		FRandomStream_.Method("FVector GetUnitVector() const", METHOD_TRIVIAL(FRandomStream, GetUnitVector));
		FRandomStream_.Method("FVector VRand() const", METHOD_TRIVIAL(FRandomStream, VRand));
		FRandomStream_.Method("FVector VRandCone(const FVector& Dir, float32 ConeHalfAngleRad) const", METHODPR_TRIVIAL(FVector, FRandomStream, VRandCone, (FVector const&, float) const));
		FRandomStream_.Method("FVector VRandCone(const FVector& Dir, float32 HorizontalConeHalfAngleRad, float32 VerticalConeHalfAngleRad) const", METHODPR_TRIVIAL(FVector, FRandomStream, VRandCone, (FVector const&, float, float) const));
	}

	void BindFRandomStreamToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FRandomStream"), &FAngelscriptFRandomStreamBinds::AppendToString);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FRandomStream_Type(
	TEXT("FRandomStream.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFRandomStreamType);

AS_FORCE_LINK const FAngelscriptBind Bind_FRandomStream(
	TEXT("FRandomStream.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFRandomStreamFunctions);

AS_FORCE_LINK const FAngelscriptBind Bind_FRandomStream_ToStringContribution(
	TEXT("FRandomStream.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFRandomStreamToStringContribution);
