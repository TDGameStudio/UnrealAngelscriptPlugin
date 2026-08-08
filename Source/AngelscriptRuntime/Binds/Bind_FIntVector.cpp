#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FIntVector_Functions.h"

struct FIntVectorType : TAngelscriptBaseStructType<FIntVector>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FIntVector"); }

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FIntVector(0);
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
	void BindFIntVectorType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FIntVector>("FIntVector", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FIntVectorType>());
	}

	void BindFIntVectorToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FIntVector"), &FAngelscriptFIntVectorBinds::AppendToString);
	}

	void BindFIntVectorFunctions(FAngelscriptBinds& Binds)
	{
		auto FIntVector_ = Binds.ExistingClassForTarget("FIntVector");
		FIntVector_.Constructor(
			"void f(int32 X, int32 Y, int32 Z)",
			&FAngelscriptFIntVectorBinds::ConstructXYZ,
			"FIntVector",
			true)
			.NoDiscard();
		FIntVector_.Constructor("void f()", &FAngelscriptFIntVectorBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FIntVector", true, "0");
		FIntVector_.Constructor(
			"void f(int32 F)",
			&FAngelscriptFIntVectorBinds::ConstructScalar,
			"FIntVector",
			true)
			.NoDiscard();
		FIntVector_.Constructor(
			"void f(const FIntVector& Other)",
			&FAngelscriptFIntVectorBinds::ConstructCopy,
			"FIntVector",
			true)
			.NoDiscard();
		FIntVector_.Property("int32 X", &FIntVector::X);
		FIntVector_.Property("int32 Y", &FIntVector::Y);
		FIntVector_.Property("int32 Z", &FIntVector::Z);
		FIntVector_.Method("FIntVector& opAssign(const FIntVector& Other)", METHODPR_TRIVIAL(FIntVector&, FIntVector, operator=, (const FIntVector&)));
		FIntVector_.Method("FIntVector opAdd(const FIntVector& Other) const", METHODPR_TRIVIAL(FIntVector, FIntVector, operator+, (const FIntVector&) const));
		FIntVector_.Method("FIntVector opSub(const FIntVector& Other) const", METHODPR_TRIVIAL(FIntVector, FIntVector, operator-, (const FIntVector&) const));
		FIntVector_.Method("FIntVector opNeg() const", &FAngelscriptFIntVectorBinds::Negate);
		FIntVector_.Method("FIntVector opMul(int32 Scale) const", METHODPR_TRIVIAL(FIntVector, FIntVector, operator*, (int32) const));
		FIntVector_.Method("FIntVector opDiv(int32 Divisor) const", METHODPR_TRIVIAL(FIntVector, FIntVector, operator/, (int32) const));
		FIntVector_.Method("FIntVector& opMulAssign(int32 Scale)", METHODPR_TRIVIAL(FIntVector&, FIntVector, operator*=, (int32)));
		FIntVector_.Method("FIntVector& opDivAssign(int32 Scale)", METHODPR_TRIVIAL(FIntVector&, FIntVector, operator/=, (int32)));
		FIntVector_.Method("FIntVector opAddAssign(const FIntVector& Other)", METHODPR_TRIVIAL(FIntVector&, FIntVector, operator+=, (const FIntVector&)));
		FIntVector_.Method("FIntVector opSubAssign(const FIntVector& Other)", METHODPR_TRIVIAL(FIntVector&, FIntVector, operator-=, (const FIntVector&)));
		FIntVector_.Method("const int32& opIndex(int32 Index)", METHODPR_TRIVIAL(int32&, FIntVector, operator[], (const int32)));
		FIntVector_.Method("bool opEquals(const FIntVector& Other) const", METHODPR_TRIVIAL(bool, FIntVector, operator==, (const FIntVector&) const));
		FIntVector_.Method("int32 GetMax() const", METHOD_TRIVIAL(FIntVector, GetMax));
		FIntVector_.Method("int32 GetMin() const", METHOD_TRIVIAL(FIntVector, GetMin));
		FIntVector_.Method("int32 Size() const", METHOD_TRIVIAL(FIntVector, Size));
		FIntVector_.Method("bool IsZero() const", METHOD_TRIVIAL(FIntVector, IsZero));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector_Type(
	TEXT("FIntVector.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFIntVectorType);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector_ToStringContribution(
	TEXT("FIntVector.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFIntVectorToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector(
	TEXT("FIntVector.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFIntVectorFunctions);
