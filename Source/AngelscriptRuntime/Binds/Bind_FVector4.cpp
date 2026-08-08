#include "Bind_FVector4.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"


/**
 * FVector4 construction, fields, operators, and string formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Vector(float64 InX, float64 InY, float64 InZ, float64 InW);                                 | Constructs a vector from four components.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Vector();                                                                                   | Constructs the zero vector.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Vector(const FVector4& Other);                                                              | Copy-constructs a vector.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Vector(FVector InVector, float64 InW);                                                      | Constructs from a three-dimensional vector and W component.                                                      |
 * |                                                                                                      | @param InVector Supplies X, Y, and Z.                                                                            |
 * |                                                                                                      | @param InW Supplies W.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Vector(const FVector4f& Other);                                                             | Converts a single-precision four-dimensional vector.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Vector.X;                                                                                    | Exposes the X component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Vector.Y;                                                                                    | Exposes the Y component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Vector.Z;                                                                                    | Exposes the Z component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Vector.W;                                                                                    | Exposes the W component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns a vector.                                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Sum = Left + Right;                                                                         | Adds vectors component-wise.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Difference = Left - Right;                                                                  | Subtracts vectors component-wise.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Scaled = Vector * Scale;                                                                    | Scales every component.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector4 Quotient = Vector / Divisor;                                                                | Divides every component by a scalar.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Vector *= Scale;                                                                                     | Scales the vector in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const float64& Component = Vector[Index];                                                            | Returns a component by index.                                                                                    |
 * |                                                                                                      | @param Index Component index in the range 0 through 3.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares all components for exact equality.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Vector}";                                                                          | Formats the vector through the shared string formatter contribution.                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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
