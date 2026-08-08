#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FVector4_Functions.h"

struct FVector4Type : TAngelscriptBaseStructType<FVector4>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FVector4");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FVector4(0.f, 0.f, 0.f, 0.f);
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
	void BindFVector4Type(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FVector4>("FVector4", Flags);
	}

	void BindFVector4Infrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FVector4Type>());
		FToStringHelper::Register(Binds, TEXT("FVector4"), &FAngelscriptFVector4Binds::AppendToString);
	}

	void BindFVector4Functions(FAngelscriptBinds& Binds)
	{
		auto FVector4_ = Binds.ExistingClassForTarget("FVector4");
		FVector4_.Constructor(
			"void f(float64 InX, float64 InY, float64 InZ, float64 InW)",
			&FAngelscriptFVector4Binds::Construct,
			"FVector4",
			true)
			.NoDiscard();
		FVector4_.Constructor("void f()", &FAngelscriptFVector4Binds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FVector4", true, "0, 0, 0, 0");
		FVector4_.Constructor(
			"void f(const FVector4& Other)",
			&FAngelscriptFVector4Binds::ConstructCopy,
			"FVector4",
			true)
			.NoDiscard();
		FVector4_.Constructor(
			"void f(FVector InVector, float64 InW)",
			&FAngelscriptFVector4Binds::ConstructFromVector,
			"FVector4",
			true)
			.NoDiscard();
		FVector4_.Constructor(
			"void f(const FVector4f& Other)",
			&FAngelscriptFVector4Binds::ConstructFromVector4f,
			"FVector4",
			true)
			.NoDiscard();

		FVector4_.Property("float64 X", &FVector4::X);
		FVector4_.Property("float64 Y", &FVector4::Y);
		FVector4_.Property("float64 Z", &FVector4::Z);
		FVector4_.Property("float64 W", &FVector4::W);
		FVector4_.Method("FVector4& opAssign(const FVector4& Other)", METHODPR_TRIVIAL(FVector4&, FVector4, operator=, (const FVector4&)));
		FVector4_.Method("FVector4 opAdd(const FVector4& Other) const", METHODPR_TRIVIAL(FVector4, FVector4, operator+, (const FVector4&) const));
		FVector4_.Method("FVector4 opSub(const FVector4& Other) const", METHODPR_TRIVIAL(FVector4, FVector4, operator-, (const FVector4&) const));
		FVector4_.Method("FVector4 opMul(float64 Scale) const", METHODPR_TRIVIAL(FVector4, FVector4, operator*, (double) const));
		FVector4_.Method("FVector4 opDiv(float64 Divisor) const", METHODPR_TRIVIAL(FVector4, FVector4, operator/, (double) const));
		FVector4_.Method("FVector4 opMulAssign(float64 S)", METHODPR_TRIVIAL(FVector4, FVector4, operator*=, (double)));
		FVector4_.Method("const float64& opIndex(int32 Index)", METHODPR_TRIVIAL(double&, FVector4, operator[], (int32)));
		FVector4_.Method("bool opEquals(const FVector4& Other) const", METHODPR_TRIVIAL(bool, FVector4, operator==, (const FVector4&) const));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4_Type(
	TEXT("FVector4.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFVector4Type);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4_Infrastructure(
	TEXT("FVector4.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVector4Infrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4(
	TEXT("FVector4.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFVector4Functions);
