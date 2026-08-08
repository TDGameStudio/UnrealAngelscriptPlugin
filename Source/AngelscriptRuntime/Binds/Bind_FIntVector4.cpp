#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FIntVector4_Functions.h"

struct FIntVector4Type : TAngelscriptCoreStructType<FIntVector4, TBaseStructure<FIntVector4>, false>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FIntVector4"); }

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FIntVector4(0);
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
	void BindFIntVector4Type(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FIntVector4>("FIntVector4", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FIntVector4Type>());
	}

	void BindFIntVector4ToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FIntVector4"), &FAngelscriptFIntVector4Binds::AppendToString);
	}

	void BindFIntVector4Functions(FAngelscriptBinds& Binds)
	{
		auto FIntVector4_ = Binds.ExistingClassForTarget("FIntVector4");
		FIntVector4_.Constructor(
			"void f(int32 X, int32 Y, int32 Z, int32 W)",
			&FAngelscriptFIntVector4Binds::ConstructXYZW,
			"FIntVector4",
			true)
			.NoDiscard();
		FIntVector4_.Constructor("void f()", &FAngelscriptFIntVector4Binds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FIntVector4", true, "0");
		FIntVector4_.Constructor(
			"void f(int32 F)",
			&FAngelscriptFIntVector4Binds::ConstructScalar,
			"FIntVector4",
			true)
			.NoDiscard();
		FIntVector4_.Constructor(
			"void f(const FIntVector4& Other)",
			&FAngelscriptFIntVector4Binds::ConstructCopy,
			"FIntVector4",
			true)
			.NoDiscard();
		FIntVector4_.Property("int32 X", &FIntVector4::X);
		FIntVector4_.Property("int32 Y", &FIntVector4::Y);
		FIntVector4_.Property("int32 Z", &FIntVector4::Z);
		FIntVector4_.Property("int32 W", &FIntVector4::W);
		FIntVector4_.Method("FIntVector4& opAssign(const FIntVector4& Other)", METHODPR_TRIVIAL(FIntVector4&, FIntVector4, operator=, (const FIntVector4&)));
		FIntVector4_.Method("FIntVector4 opAdd(const FIntVector4& Other) const", METHODPR_TRIVIAL(FIntVector4, FIntVector4, operator+, (const FIntVector4&) const));
		FIntVector4_.Method("FIntVector4 opSub(const FIntVector4& Other) const", METHODPR_TRIVIAL(FIntVector4, FIntVector4, operator-, (const FIntVector4&) const));
		FIntVector4_.Method("FIntVector4 opNeg() const", &FAngelscriptFIntVector4Binds::Negate);
		FIntVector4_.Method("FIntVector4 opMul(int32 Scale) const", METHODPR_TRIVIAL(FIntVector4, FIntVector4, operator*, (int32) const));
		FIntVector4_.Method("FIntVector4 opDiv(int32 Divisor) const", METHODPR_TRIVIAL(FIntVector4, FIntVector4, operator/, (int32) const));
		FIntVector4_.Method("FIntVector4& opMulAssign(int32 Scale)", METHODPR_TRIVIAL(FIntVector4&, FIntVector4, operator*=, (int32)));
		FIntVector4_.Method("FIntVector4& opDivAssign(int32 Scale)", METHODPR_TRIVIAL(FIntVector4&, FIntVector4, operator/=, (int32)));
		FIntVector4_.Method("FIntVector4 opAddAssign(const FIntVector4& Other)", METHODPR_TRIVIAL(FIntVector4&, FIntVector4, operator+=, (const FIntVector4&)));
		FIntVector4_.Method("FIntVector4 opSubAssign(const FIntVector4& Other)", METHODPR_TRIVIAL(FIntVector4&, FIntVector4, operator-=, (const FIntVector4&)));
		FIntVector4_.Method("const int32& opIndex(int32 Index)", METHODPR_TRIVIAL(int32&, FIntVector4, operator[], (const int32)));
		FIntVector4_.Method("bool opEquals(const FIntVector4& Other) const", METHODPR_TRIVIAL(bool, FIntVector4, operator==, (const FIntVector4&) const));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector4_Type(
	TEXT("FIntVector4.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFIntVector4Type);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector4_ToStringContribution(
	TEXT("FIntVector4.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFIntVector4ToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector4(
	TEXT("FIntVector4.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFIntVector4Functions);
