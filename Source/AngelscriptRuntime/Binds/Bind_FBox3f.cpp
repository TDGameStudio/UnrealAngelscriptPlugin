#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FBox3f_Functions.h"

struct FGetBox3f
{
	static UScriptStruct* Get();
};

UScriptStruct* FGetBox3f::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Box3f"));
	return ScriptStruct;
}

struct FBox3fType : TAngelscriptCoreStructType<FBox3f, FGetBox3f>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FBox3f"); }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFBox3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FBox3f>("FBox3f", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FBox3fType>());
	}

	void BindFBox3fToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FBox3f"), &FAngelscriptFBox3fBinds::AppendToString);
	}

	void BindFBox3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FBox3f_ = Binds.ExistingClassForTarget("FBox3f");
		FBox3f_.Constructor("void f()", &FAngelscriptFBox3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FBox3f", true, "ForceInit");
		FBox3f_.Constructor(
			"void f(const FVector3f& InMin, const FVector3f& InMax)",
			&FAngelscriptFBox3fBinds::ConstructMinMax,
			"FBox3f",
			true)
			.NoDiscard();
		FBox3f_.Constructor(
			"void f(const FBox& Box)",
			&FAngelscriptFBox3fBinds::ConstructFromBox,
			"FBox3f",
			true)
			.NoDiscard();
		FBox3f_.Property("FVector3f Min", &FBox3f::Min);
		FBox3f_.Property("FVector3f Max", &FBox3f::Max);
		FBox3f_.Method("FBox3f opAdd(const FBox3f& Other) const", METHODPR_TRIVIAL(FBox3f, FBox3f, operator+, (const FBox3f&) const));
		FBox3f_.Method("FBox3f& opAddAssign(const FBox3f& Other)", METHODPR_TRIVIAL(FBox3f&, FBox3f, operator+=, (const FBox3f&)));
		FBox3f_.Method("bool opEquals(const FBox3f& Other) const", METHODPR_TRIVIAL(bool, FBox3f, operator==, (const FBox3f&) const));
		FBox3f_.Method("bool Intersect(const FBox3f& other) const", METHOD_TRIVIAL(FBox3f, Intersect));
		FBox3f_.Method("FBox3f opAdd(const FVector3f& Other) const", METHODPR_TRIVIAL(FBox3f, FBox3f, operator+, (const FVector3f&) const));
		FBox3f_.Method("FBox3f& opAddAssign(const FVector3f& Other)", METHODPR_TRIVIAL(FBox3f&, FBox3f, operator+=, (const FVector3f&)));
		FBox3f_.Method("FVector3f& opIndex(int32 Index)", METHODPR_TRIVIAL(FVector3f&, FBox3f, operator[], (int32)));
		FBox3f_.Method("FVector3f GetCenter() const", METHOD_TRIVIAL(FBox3f, GetCenter));
		FBox3f_.Method("FVector3f GetExtent() const", METHOD_TRIVIAL(FBox3f, GetExtent));
		FBox3f_.Method("void GetCenterAndExtents(FVector3f& Center, FVector3f& Extents) const", METHOD_TRIVIAL(FBox3f, GetCenterAndExtents));
		FBox3f_.Method("FVector3f GetClosestPointTo( const FVector3f& In ) const", METHOD_TRIVIAL(FBox3f, GetClosestPointTo));
		FBox3f_.Method("FBox3f InverseTransformBy( const FTransform& M ) const", METHOD_TRIVIAL(FBox3f, InverseTransformBy));
		FBox3f_.Method("FBox3f TransformBy( const FTransform3f& M ) const", METHODPR_TRIVIAL(FBox3f, FBox3f, TransformBy, (const FTransform3f&) const));
		FBox3f_.Method("bool IsInside( const FVector3f& In ) const", METHODPR_TRIVIAL(bool, FBox3f, IsInside, (const FVector3f&) const));
		FBox3f_.Method("bool IsInsideOrOn( const FVector3f& In ) const", METHODPR_TRIVIAL(bool, FBox3f, IsInsideOrOn, (const FVector3f&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FBox3f");
		Binds.BindGlobalFunctionForTarget("FBox3f BuildAABB( const FVector3f& Origin, const FVector3f& Extent) no_discard", FUNC_TRIVIAL(FBox3f::BuildAABB));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FBox3f_Type(
	TEXT("FBox3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFBox3fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FBox3f_ToStringContribution(
	TEXT("FBox3f.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFBox3fToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FBox3f(
	TEXT("FBox3f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFBox3fFunctions);
