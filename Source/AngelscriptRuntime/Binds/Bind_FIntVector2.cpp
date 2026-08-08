#include "AngelscriptBinds.h"

#include "Helper_StructType.h"
#include "Helper_ToString.h"

#include "Bind_FIntVector2_Functions.h"

struct FGetIntVector2
{
	static UScriptStruct* Get();
};

UScriptStruct* FGetIntVector2::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.IntVector2"));
	return ScriptStruct;
}

struct FIntVector2Type : TAngelscriptCoreStructType<FIntVector2, FGetIntVector2, false>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FIntVector2"); }

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new (DestinationPtr) FIntVector2(0);
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
	void BindFIntVector2Type(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FIntVector2>("FIntVector2", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FIntVector2Type>());
	}

	void BindFIntVector2ToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FIntVector2"), &FAngelscriptFIntVector2Binds::AppendToString);
	}

	void BindFIntVector2Functions(FAngelscriptBinds& Binds)
	{
		auto FIntVector2_ = Binds.ExistingClassForTarget("FIntVector2");
		FIntVector2_.Constructor(
			"void f(int32 X, int32 Y)",
			&FAngelscriptFIntVector2Binds::ConstructXY,
			"FIntVector2",
			true)
			.NoDiscard();
		FIntVector2_.Constructor("void f()", &FAngelscriptFIntVector2Binds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FIntVector2", true, "0");
		FIntVector2_.Constructor(
			"void f(int32 F)",
			&FAngelscriptFIntVector2Binds::ConstructScalar,
			"FIntVector2",
			true)
			.NoDiscard();
		FIntVector2_.Constructor(
			"void f(const FIntVector2& Other)",
			&FAngelscriptFIntVector2Binds::ConstructCopy,
			"FIntVector2",
			true)
			.NoDiscard();
		FIntVector2_.Property("int32 X", &FIntVector2::X);
		FIntVector2_.Property("int32 Y", &FIntVector2::Y);
		FIntVector2_.Method("FIntVector2& opAssign(const FIntVector2& Other)", METHODPR_TRIVIAL(FIntVector2&, FIntVector2, operator=, (const FIntVector2&)));
		FIntVector2_.Method("const int32& opIndex(int32 Index)", METHODPR_TRIVIAL(int32&, FIntVector2, operator[], (const int32)));
		FIntVector2_.Method("bool opEquals(const FIntVector2& Other) const", METHODPR_TRIVIAL(bool, FIntVector2, operator==, (const FIntVector2&) const));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector2_Type(
	TEXT("FIntVector2.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFIntVector2Type);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector2_ToStringContribution(
	TEXT("FIntVector2.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFIntVector2ToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector2(
	TEXT("FIntVector2.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFIntVector2Functions);
