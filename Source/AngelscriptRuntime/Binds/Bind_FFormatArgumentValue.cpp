#include "Bind_FFormatArgumentValue.h"

#include "AngelscriptBinds.h"

/**
 * FFormatArgumentValue binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FFormatArgumentValue;                                                               | Declares the tagged value used as a localized FText format argument.                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value();                                                              | Constructs the default empty format argument.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const int32 Value);                                             | Constructs a signed 32-bit numeric format argument.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const uint32 Value);                                            | Constructs an unsigned 32-bit numeric format argument.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const int64 Value);                                             | Constructs a signed 64-bit numeric format argument.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const uint64 Value);                                            | Constructs an unsigned 64-bit numeric format argument.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const float32 Value);                                           | Constructs a single-precision numeric format argument.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const float64 Value);                                           | Constructs a double-precision numeric format argument.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(const FText& Value);                                            | Constructs a localized text format argument.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FFormatArgumentValue Value(ETextGender Value);                                             | Constructs a grammatical-gender format argument.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FFormatArgumentValue_Type(
	TEXT("FFormatArgumentValue.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FFormatArgumentValue>("FFormatArgumentValue", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FFormatArgumentValue_Infrastructure(
	TEXT("FFormatArgumentValue.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FFormatArgumentValueType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FFormatArgumentValue(
	TEXT("FFormatArgumentValue.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

void FAngelscriptFFormatArgumentValueBinds::ConstructDefault(FFormatArgumentValue* Address)
{
	new (Address) FFormatArgumentValue();
}

void FAngelscriptFFormatArgumentValueBinds::ConstructInt32(FFormatArgumentValue* Address, const int32 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructUInt32(FFormatArgumentValue* Address, const uint32 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructInt64(FFormatArgumentValue* Address, const int64 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructUInt64(FFormatArgumentValue* Address, const uint64 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructFloat(FFormatArgumentValue* Address, const float Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructDouble(FFormatArgumentValue* Address, const double Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructText(FFormatArgumentValue* Address, const FText& Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructGender(FFormatArgumentValue* Address, const ETextGender Value)
{
	new (Address) FFormatArgumentValue(Value);
}
