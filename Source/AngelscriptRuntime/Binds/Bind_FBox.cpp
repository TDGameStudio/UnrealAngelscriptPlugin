#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FBox_Functions.h"

struct FBoxType : TAngelscriptCoreStructType<FBox, FGetBox>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FBox"); }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFBoxType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FBox>("FBox", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FBoxType>());
	}

	void BindFBoxToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FBox"), &FAngelscriptFBoxBinds::AppendToString);
	}

	void BindFBoxFunctions(FAngelscriptBinds& Binds)
	{
		auto FBox_ = Binds.ExistingClassForTarget("FBox");
		FBox_.Constructor("void f()", &FAngelscriptFBoxBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FBox", true, "ForceInit");
		FBox_.Constructor(
			"void f(const FVector& InMin, const FVector& InMax)",
			&FAngelscriptFBoxBinds::ConstructMinMax,
			"FBox",
			true)
			.NoDiscard();
		FBox_.Constructor("void f(const FBox3f& Box)", &FAngelscriptFBoxBinds::ConstructFromBox3f, "FBox", true).NoDiscard();
		FBox_.Property("FVector Min", &FBox::Min);
		FBox_.Property("FVector Max", &FBox::Max);
		FBox_.Method("FBox opAdd(const FBox& Other) const", METHODPR_TRIVIAL(FBox, FBox, operator+, (const FBox&) const));
		FBox_.Method("FBox& opAddAssign(const FBox& Other)", METHODPR_TRIVIAL(FBox&, FBox, operator+=, (const FBox&)));
		FBox_.Method("bool opEquals(const FBox& Other) const", METHODPR_TRIVIAL(bool, FBox, operator==, (const FBox&) const));
		FBox_.Method("FBox opAdd(const FVector& Other) const", METHODPR_TRIVIAL(FBox, FBox, operator+, (const FVector&) const));
		FBox_.Method("FBox& opAddAssign(const FVector& Other)", METHODPR_TRIVIAL(FBox&, FBox, operator+=, (const FVector&)));
		FBox_.Method("FVector& opIndex(int32 Index)", METHODPR_TRIVIAL(FVector&, FBox, operator[], (int32)));
		FBox_.Method("FVector GetCenter() const", METHOD_TRIVIAL(FBox, GetCenter));
		FBox_.Method("FVector GetExtent() const", METHOD_TRIVIAL(FBox, GetExtent));
		FBox_.Method("float64 GetVolume() const", METHOD_TRIVIAL(FBox, GetVolume));
		FBox_.Method("void GetCenterAndExtents(FVector& Center, FVector& Extents) const", METHOD_TRIVIAL(FBox, GetCenterAndExtents));
		FBox_.Method("FVector GetClosestPointTo( const FVector& In ) const", METHOD_TRIVIAL(FBox, GetClosestPointTo));
		FBox_.Method("FBox InverseTransformBy( const FTransform& M ) const", METHOD_TRIVIAL(FBox, InverseTransformBy));
		FBox_.Method("FBox TransformBy( const FTransform& M ) const", METHODPR_TRIVIAL(FBox, FBox, TransformBy, (const FTransform&) const));
		FBox_.Method("bool Equals(const FBox& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FBox, Equals));
		FBox_.Method("bool Intersect(const FBox& Other) const", METHOD_TRIVIAL(FBox, Intersect));
		FBox_.Method("bool IntersectXY(const FBox& Other) const", METHOD_TRIVIAL(FBox, IntersectXY));
		FBox_.Method("FBox Overlap(const FBox& Other) const", METHOD_TRIVIAL(FBox, Overlap));
		FBox_.Method("FBox ExpandBy(float64 W) const", METHODPR_TRIVIAL(FBox, FBox, ExpandBy, (double) const));
		FBox_.Method("FBox ExpandBy(const FVector& V) const", METHODPR_TRIVIAL(FBox, FBox, ExpandBy, (const FVector&) const));
		FBox_.Method("FBox ShiftBy(const FVector& Offset) const", METHODPR_TRIVIAL(FBox, FBox, ShiftBy, (const FVector&) const));
		FBox_.Method("FBox MoveTo(const FVector& Destination) const", METHODPR_TRIVIAL(FBox, FBox, MoveTo, (const FVector&) const));
		FBox_.Method("bool IsInside( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInside, (const FVector&) const));
		FBox_.Method("bool IsInsideOrOn( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideOrOn, (const FVector&) const));
		FBox_.Method("bool IsInside( const FBox& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInside, (const FBox&) const));
		FBox_.Method("bool IsInsideXY( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideXY, (const FVector&) const));
		FBox_.Method("bool IsInsideOrOnXY( const FVector& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideOrOnXY, (const FVector&) const));
		FBox_.Method("bool IsInsideXY( const FBox& In ) const", METHODPR_TRIVIAL(bool, FBox, IsInsideXY, (const FBox&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FBox");
		Binds.BindGlobalFunctionForTarget("FBox BuildAABB( const FVector& Origin, const FVector& Extent) no_discard", FUNC_TRIVIAL(FBox::BuildAABB));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FBox_Type(
	TEXT("FBox.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFBoxType);

AS_FORCE_LINK const FAngelscriptBind Bind_FBox_ToStringContribution(
	TEXT("FBox.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFBoxToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FBox(
	TEXT("FBox.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFBoxFunctions);
