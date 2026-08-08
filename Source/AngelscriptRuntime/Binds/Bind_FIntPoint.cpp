#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FIntPoint_Functions.h"

struct FIntPointType : TAngelscriptBaseStructType<FIntPoint>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FIntPoint");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FIntPoint(0);
	}

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFIntPointType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FIntPoint>("FIntPoint", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FIntPointType>());
	}

	void BindFIntPointFunctions(FAngelscriptBinds& Binds)
	{
		auto FIntPoint_ = Binds.ExistingClassForTarget("FIntPoint");
		FIntPoint_.Constructor(
			"void f(int32 X, int32 Y)",
			&FAngelscriptFIntPointBinds::ConstructXY,
			"FIntPoint",
			true)
			.NoDiscard();
		FIntPoint_.Constructor("void f()", &FAngelscriptFIntPointBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FIntPoint", true, "0");
		FIntPoint_.Constructor(
			"void f(int32 F)",
			&FAngelscriptFIntPointBinds::ConstructScalar,
			"FIntPoint",
			true)
			.NoDiscard();
		FIntPoint_.Constructor(
			"void f(const FIntPoint& Other)",
			&FAngelscriptFIntPointBinds::ConstructCopy,
			"FIntPoint",
			true)
			.NoDiscard();
		FIntPoint_.Property("int32 X", &FIntPoint::X);
		FIntPoint_.Property("int32 Y", &FIntPoint::Y);
		FIntPoint_.Method("FIntPoint& opAssign(const FIntPoint& Other)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator=, (const FIntPoint&)));
		FIntPoint_.Method("FIntPoint opAdd(const FIntPoint& Other) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator+, (const FIntPoint&) const));
		FIntPoint_.Method("FIntPoint opSub(const FIntPoint& Other) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator-, (const FIntPoint&) const));
		FIntPoint_.Method("FIntPoint opNeg() const", &FAngelscriptFIntPointBinds::Negate);
		FIntPoint_.Method("FIntPoint opMul(int32 Scale) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator*, (int32) const));
		FIntPoint_.Method("FIntPoint opDiv(int32 Divisor) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator/, (int32) const));
		FIntPoint_.Method("FIntPoint& opMulAssign(int32 Scale)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator*=, (int32)));
		FIntPoint_.Method("FIntPoint& opDivAssign(int32 Scale)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator/=, (int32)));
		FIntPoint_.Method("FIntPoint opAddAssign(const FIntPoint& Other)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator+=, (const FIntPoint&)));
		FIntPoint_.Method("FIntPoint opSubAssign(const FIntPoint& Other)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator-=, (const FIntPoint&)));
		FIntPoint_.Method("const int32& opIndex(int32 Index) no_discard", METHODPR_TRIVIAL(int32&, FIntPoint, operator[], (const int32)));
		FIntPoint_.Method("bool opEquals(const FIntPoint& Other) const", METHODPR_TRIVIAL(bool, FIntPoint, operator==, (const FIntPoint&) const));
		FIntPoint_.Method("int32 GetMax() const", METHOD_TRIVIAL(FIntPoint, GetMax));
		FIntPoint_.Method("int32 GetMin() const", METHOD_TRIVIAL(FIntPoint, GetMin));
		FIntPoint_.Method("int32 Size() const", METHOD_TRIVIAL(FIntPoint, Size));
	}

	void BindFIntPointToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FIntPoint"), &FAngelscriptFIntPointBinds::AppendToString);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FIntPoint_Type(
	TEXT("FIntPoint.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFIntPointType);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntPoint(
	TEXT("FIntPoint.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFIntPointFunctions);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntPoint_ToStringContribution(
	TEXT("FIntPoint.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFIntPointToStringContribution);
