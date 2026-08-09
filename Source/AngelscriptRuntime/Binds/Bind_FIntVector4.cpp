#include "Bind_FIntVector4.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FIntVector4 construction, fields, integer operators, indexing, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Vector(int32 X, int32 Y, int32 Z, int32 W);                                              | Constructs from four integer components.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Vector();                                                                                | Constructs the zero vector.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Vector(int32 F);                                                                         | Constructs with every component set to the same value.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Vector(const FIntVector4& Other);                                                        | Copy-constructs a vector.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Vector.X;                                                                                      | Exposes X.                                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Vector.Y;                                                                                      | Exposes Y.                                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Vector.Z;                                                                                      | Exposes Z.                                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Vector.W;                                                                                      | Exposes W.                                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns all components.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Sum = Left + Right;                                                                      | Adds components.                                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Difference = Left - Right;                                                               | Subtracts components.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Negated = -Vector;                                                                       | Negates every component.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Scaled = Vector * int32 Scale;                                                           | Multiplies every component by an integer scale.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FIntVector4 Quotient = Vector / int32 Divisor;                                                       | Divides every component by an integer.                                                                           |
 * |                                                                                                      | @param Divisor Nonzero integer divisor.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Vector *= int32 Scale;                                                                               | Scales the vector in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Vector /= int32 Divisor;                                                                             | Divides the vector in place.                                                                                     |
 * |                                                                                                      | @param Divisor Nonzero integer divisor.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Vector += Other;                                                                                     | Adds another vector in place.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Vector -= Other;                                                                                     | Subtracts another vector in place.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const int32& Component = Vector[int32 Index];                                                        | Returns a component reference.                                                                                   |
 * |                                                                                                      | @param Index Component index from 0 through 3.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares every component exactly.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Vector}";                                                                          | Formats the four components through the shared formatter.                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */


AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector4_Type(
	TEXT("FIntVector4.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FIntVector4>("FIntVector4", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FIntVector4Type>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector4_ToStringContribution(
	TEXT("FIntVector4.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FIntVector4"), &FAngelscriptFIntVector4Binds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FIntVector4(
	TEXT("FIntVector4.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
