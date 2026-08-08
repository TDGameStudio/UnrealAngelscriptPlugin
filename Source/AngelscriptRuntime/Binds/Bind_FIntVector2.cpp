#include "Bind_FIntVector2.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FIntVector2 construction, fields, assignment, indexing, equality, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector2 Vector(int32 X, int32 Y);                                                                | Constructs from two integer components.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector2 Vector();                                                                                | Constructs the zero vector.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector2 Vector(int32 F);                                                                         | Constructs with both components set to the same value.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector2 Vector(const FIntVector2& Other);                                                        | Copy-constructs a vector.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Vector.X;                                                                                      | Exposes X.                                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Vector.Y;                                                                                      | Exposes Y.                                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns both components.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const int32& Component = Vector[int32 Index];                                                        | Returns a component reference.                                                                                   |
 * |                                                                                                      | @param Index Component index 0 or 1.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares both components exactly.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Vector}";                                                                          | Formats both components through the shared formatter.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */


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
