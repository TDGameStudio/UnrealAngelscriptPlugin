#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FBoxSphereBounds_Functions.h"

struct FBoxSphereBoundsType : TAngelscriptCoreStructType<FBoxSphereBounds, FGetBoxSphereBounds>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FBoxSphereBounds"); }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFBoxSphereBoundsType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FBoxSphereBounds>("FBoxSphereBounds", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FBoxSphereBoundsType>());
	}

	void BindFBoxSphereBoundsToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FBoxSphereBounds"), &FAngelscriptFBoxSphereBoundsBinds::AppendToString);
	}

	void BindFBoxSphereBoundsFunctions(FAngelscriptBinds& Binds)
	{
		auto FBoxSphereBounds_ = Binds.ExistingClassForTarget("FBoxSphereBounds");
		FBoxSphereBounds_.Constructor("void f()", &FAngelscriptFBoxSphereBoundsBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FBoxSphereBounds", true, "ForceInit");
		FBoxSphereBounds_.Constructor(
			"void f(const FVector& InOrigin, const FVector& InBoxExtent, float64 InSphereRadius)",
			&FAngelscriptFBoxSphereBoundsBinds::ConstructOriginExtentRadius,
			"FBoxSphereBounds",
			true)
			.NoDiscard();
		FBoxSphereBounds_.Constructor(
			"void f(const FBox& Box, const FSphere& Sphere)",
			&FAngelscriptFBoxSphereBoundsBinds::ConstructBoxSphere,
			"FBoxSphereBounds",
			true)
			.NoDiscard();
		FBoxSphereBounds_.Constructor(
			"void f(const FBoxSphereBounds3f& Bounds)",
			&FAngelscriptFBoxSphereBoundsBinds::ConstructFromBounds3f,
			"FBoxSphereBounds",
			true)
			.NoDiscard();
		FBoxSphereBounds_.Constructor(
			"void f(const FBox& Box)",
			&FAngelscriptFBoxSphereBoundsBinds::ConstructFromBox,
			"FBoxSphereBounds",
			true)
			.NoDiscard();
		FBoxSphereBounds_.Constructor(
			"void f(const FSphere& Sphere)",
			&FAngelscriptFBoxSphereBoundsBinds::ConstructFromSphere,
			"FBoxSphereBounds",
			true)
			.NoDiscard();
		FBoxSphereBounds_.Constructor(
			"void f(const TArray<FVector>& Points)",
			&FAngelscriptFBoxSphereBoundsBinds::ConstructFromPoints)
			.NoDiscard();
		FBoxSphereBounds_.Property("FVector Origin", &FBoxSphereBounds::Origin);
		FBoxSphereBounds_.Property("FVector BoxExtent", &FBoxSphereBounds::BoxExtent);
		FBoxSphereBounds_.Property("float64 SphereRadius", &FBoxSphereBounds::SphereRadius);
		FBoxSphereBounds_.Method("FBoxSphereBounds opAdd(const FBoxSphereBounds& Other) const", METHODPR_TRIVIAL(FBoxSphereBounds, FBoxSphereBounds, operator+, (const FBoxSphereBounds&) const));
		FBoxSphereBounds_.Method("bool opEquals(const FBoxSphereBounds& Other) const", METHODPR_TRIVIAL(bool, FBoxSphereBounds, operator==, (const FBoxSphereBounds&) const));
		FBoxSphereBounds_.Method("float64 ComputeSquaredDistanceFromBoxToPoint( const FVector& Point ) const", METHODPR_TRIVIAL(double, FBoxSphereBounds, ComputeSquaredDistanceFromBoxToPoint, (const FVector&) const));
		FBoxSphereBounds_.Method("FBox GetBox() const", METHOD_TRIVIAL(FBoxSphereBounds, GetBox));
		FBoxSphereBounds_.Method("FVector GetBoxExtrema( uint32 Extrema ) const", METHODPR_TRIVIAL(FVector, FBoxSphereBounds, GetBoxExtrema, (uint32) const));
		FBoxSphereBounds_.Method("FSphere GetSphere() const", METHOD_TRIVIAL(FBoxSphereBounds, GetSphere));
		FBoxSphereBounds_.Method("FBoxSphereBounds ExpandBy( float64 ExpandAmount ) const", METHODPR_TRIVIAL(FBoxSphereBounds, FBoxSphereBounds, ExpandBy, (double) const));
		FBoxSphereBounds_.Method("FBoxSphereBounds TransformBy( const FTransform& M ) const", METHODPR_TRIVIAL(FBoxSphereBounds, FBoxSphereBounds, TransformBy, (const FTransform&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FBoxSphereBounds");
		Binds.BindGlobalFunctionForTarget("bool SpheresIntersect(const FBoxSphereBounds& A, const FBoxSphereBounds& B, float64 Tolerance = KINDA_SMALL_NUMBER) no_discard", FUNC_TRIVIAL(FBoxSphereBounds::SpheresIntersect));
		Binds.BindGlobalFunctionForTarget("bool BoxesIntersect(const FBoxSphereBounds& A, const FBoxSphereBounds& B) no_discard", FUNC_TRIVIAL(FBoxSphereBounds::BoxesIntersect));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds_Type(
	TEXT("FBoxSphereBounds.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFBoxSphereBoundsType);

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds_ToStringContribution(
	TEXT("FBoxSphereBounds.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFBoxSphereBoundsToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds(
	TEXT("FBoxSphereBounds.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFBoxSphereBoundsFunctions);
