#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FBoxSphereBounds3f_Functions.h"

struct FGetBoxSphereBounds3f
{
	static UScriptStruct* Get();
};

UScriptStruct* FGetBoxSphereBounds3f::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.BoxSphereBounds3f"));
	return ScriptStruct;
}

struct FBoxSphereBounds3fType : TAngelscriptCoreStructType<FBoxSphereBounds3f, FGetBoxSphereBounds3f>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FBoxSphereBounds3f"); }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFBoxSphereBounds3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FBoxSphereBounds3f>("FBoxSphereBounds3f", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FBoxSphereBounds3fType>());
	}

	void BindFBoxSphereBounds3fToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FBoxSphereBounds3f"), &FAngelscriptFBoxSphereBounds3fBinds::AppendToString);
	}

	void BindFBoxSphereBounds3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FBoxSphereBounds3f_ = Binds.ExistingClassForTarget("FBoxSphereBounds3f");
		FBoxSphereBounds3f_.Constructor("void f()", &FAngelscriptFBoxSphereBounds3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FBoxSphereBounds3f", true, "ForceInit");
		FBoxSphereBounds3f_.Constructor(
			"void f(const FVector3f& InOrigin, const FVector3f& InBoxExtent, float32 InSphereRadius)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructOriginExtentRadius,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FBoxSphereBounds& Bounds)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromBounds,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FBox3f& Box, const FSphere3f& Sphere)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructBoxSphere,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FBox3f& Box)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromBox,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const FSphere3f& Sphere)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromSphere,
			"FBoxSphereBounds3f",
			true)
			.NoDiscard();
		FBoxSphereBounds3f_.Constructor(
			"void f(const TArray<FVector3f>& Points)",
			&FAngelscriptFBoxSphereBounds3fBinds::ConstructFromPoints)
			.NoDiscard();
		FBoxSphereBounds3f_.Property("FVector3f Origin", &FBoxSphereBounds3f::Origin);
		FBoxSphereBounds3f_.Property("FVector3f BoxExtent", &FBoxSphereBounds3f::BoxExtent);
		FBoxSphereBounds3f_.Property("float32 SphereRadius", &FBoxSphereBounds3f::SphereRadius);
		FBoxSphereBounds3f_.Method("FBoxSphereBounds3f opAdd(const FBoxSphereBounds3f& Other) const", METHODPR_TRIVIAL(FBoxSphereBounds3f, FBoxSphereBounds3f, operator+, (const FBoxSphereBounds3f&) const));
		FBoxSphereBounds3f_.Method("bool opEquals(const FBoxSphereBounds3f& Other) const", METHODPR_TRIVIAL(bool, FBoxSphereBounds3f, operator==, (const FBoxSphereBounds3f&) const));
		FBoxSphereBounds3f_.Method("float32 ComputeSquaredDistanceFromBoxToPoint( const FVector3f& Point ) const", METHODPR_TRIVIAL(float, FBoxSphereBounds3f, ComputeSquaredDistanceFromBoxToPoint, (const FVector3f&) const));
		FBoxSphereBounds3f_.Method("FBox3f GetBox() const", METHOD_TRIVIAL(FBoxSphereBounds3f, GetBox));
		FBoxSphereBounds3f_.Method("FVector3f GetBoxExtrema( uint32 Extrema ) const", METHODPR_TRIVIAL(FVector3f, FBoxSphereBounds3f, GetBoxExtrema, (uint32) const));
		FBoxSphereBounds3f_.Method("FSphere3f GetSphere() const", METHOD_TRIVIAL(FBoxSphereBounds3f, GetSphere));
		FBoxSphereBounds3f_.Method("FBoxSphereBounds3f ExpandBy( float32 ExpandAmount ) const", METHODPR_TRIVIAL(FBoxSphereBounds3f, FBoxSphereBounds3f, ExpandBy, (float) const));
		FBoxSphereBounds3f_.Method("FBoxSphereBounds3f TransformBy( const FTransform3f& M ) const", METHODPR_TRIVIAL(FBoxSphereBounds3f, FBoxSphereBounds3f, TransformBy, (const FTransform3f&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FBoxSphereBounds3f");
		Binds.BindGlobalFunctionForTarget("bool SpheresIntersect(const FBoxSphereBounds3f& A, const FBoxSphereBounds3f& B, float32 Tolerance = KINDA_SMALL_NUMBER) no_discard", FUNC_TRIVIAL(FBoxSphereBounds3f::SpheresIntersect));
		Binds.BindGlobalFunctionForTarget("bool BoxesIntersect(const FBoxSphereBounds3f& A, const FBoxSphereBounds3f& B) no_discard", FUNC_TRIVIAL(FBoxSphereBounds3f::BoxesIntersect));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds3f_Type(
	TEXT("FBoxSphereBounds3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFBoxSphereBounds3fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds3f_ToStringContribution(
	TEXT("FBoxSphereBounds3f.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFBoxSphereBounds3fToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FBoxSphereBounds3f(
	TEXT("FBoxSphereBounds3f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFBoxSphereBounds3fFunctions);
