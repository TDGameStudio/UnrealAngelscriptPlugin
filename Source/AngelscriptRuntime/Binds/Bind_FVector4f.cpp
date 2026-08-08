#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FVector4f_Functions.h"

struct FVector4fType : TAngelscriptVariantStructType<FVector4f>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FVector4f");
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FVector4f(0.f, 0.f, 0.f, 0.f);
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
	void BindFVector4fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FVector4f>("FVector4f", Flags);
	}

	void BindFVector4fInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FVector4fType>());
		FToStringHelper::Register(Binds, TEXT("FVector4f"), &FAngelscriptFVector4fBinds::AppendToString);
	}

	void BindFVector4fFunctions(FAngelscriptBinds& Binds)
	{
		auto FVector4f_ = Binds.ExistingClassForTarget("FVector4f");
		FVector4f_.Constructor(
			"void f(float32 InX, float32 InY, float32 InZ, float32 InW)",
			&FAngelscriptFVector4fBinds::Construct,
			"FVector4f",
			true)
			.NoDiscard();
		FVector4f_.Constructor("void f()", &FAngelscriptFVector4fBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FVector4f", true, "0, 0, 0, 0");
		FVector4f_.Constructor(
			"void f(const FVector4f& Other)",
			&FAngelscriptFVector4fBinds::ConstructCopy,
			"FVector4f",
			true)
			.NoDiscard();
		FVector4f_.Constructor(
			"void f(FVector3f InVector, float32 InW)",
			&FAngelscriptFVector4fBinds::ConstructFromVector3f,
			"FVector4f",
			true)
			.NoDiscard();
		FVector4f_.Constructor(
			"void f(const FVector4& Other)",
			&FAngelscriptFVector4fBinds::ConstructFromVector4,
			"FVector4f",
			true)
			.NoDiscard();

		FVector4f_.Property("float32 X", &FVector4f::X);
		FVector4f_.Property("float32 Y", &FVector4f::Y);
		FVector4f_.Property("float32 Z", &FVector4f::Z);
		FVector4f_.Property("float32 W", &FVector4f::W);
		FVector4f_.Method("FVector4f& opAssign(const FVector4f& Other)", METHODPR_TRIVIAL(FVector4f&, FVector4f, operator=, (const FVector4f&)));
		FVector4f_.Method("FVector4f opAdd(const FVector4f& Other) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator+, (const FVector4f&) const));
		FVector4f_.Method("FVector4f opSub(const FVector4f& Other) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator-, (const FVector4f&) const));
		FVector4f_.Method("FVector4f opMul(float32 Scale) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator*, (float) const));
		FVector4f_.Method("FVector4f opDiv(float32 Divisor) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator/, (float) const));
		FVector4f_.Method("FVector4f opMulAssign(float32 S)", METHODPR_TRIVIAL(FVector4f, FVector4f, operator*=, (float)));
		FVector4f_.Method("const float32& opIndex(int32 Index)", METHODPR_TRIVIAL(float&, FVector4f, operator[], (int32)));
		FVector4f_.Method("bool opEquals(const FVector4f& Other) const", METHODPR_TRIVIAL(bool, FVector4f, operator==, (const FVector4f&) const));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4f_Type(
	TEXT("FVector4f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFVector4fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4f_Infrastructure(
	TEXT("FVector4f.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVector4fInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4f(
	TEXT("FVector4f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFVector4fFunctions);
