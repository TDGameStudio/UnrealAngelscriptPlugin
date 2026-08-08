#include "AngelscriptBinds.h"

#include "Bind_FFormatArgumentValue_Functions.h"
#include "Helper_CppType.h"

struct FFormatArgumentValueType : TAngelscriptCppType<FFormatArgumentValue>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FFormatArgumentValue");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFFormatArgumentValueType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FFormatArgumentValue>("FFormatArgumentValue", Flags);
	}

	void BindFFormatArgumentValueInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FFormatArgumentValueType>());
	}

	void BindFFormatArgumentValueFunctions(FAngelscriptBinds& Binds)
	{
		auto Value = Binds.ExistingClassForTarget("FFormatArgumentValue");

		Value.Constructor(
			"void f()",
			&FAngelscriptFFormatArgumentValueBinds::ConstructDefault,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const int32 Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructInt32,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const uint32 Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructUInt32,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const int64 Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructInt64,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const uint64 Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructUInt64,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const float32 Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructFloat,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const float64 Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructDouble,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(const FText& Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructText,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
		Value.Constructor(
			"void f(ETextGender Value)",
			&FAngelscriptFFormatArgumentValueBinds::ConstructGender,
			"FFormatArgumentValue",
			true)
			.NoDiscard();
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FFormatArgumentValue_Type(
	TEXT("FFormatArgumentValue.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFFormatArgumentValueType);

AS_FORCE_LINK const FAngelscriptBind Bind_FFormatArgumentValue_Infrastructure(
	TEXT("FFormatArgumentValue.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFFormatArgumentValueInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FFormatArgumentValue(
	TEXT("FFormatArgumentValue.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFFormatArgumentValueFunctions);
