#include "Bind_FIntPoint.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FIntPoint binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Value;                                                                                                             | Declares the two-component integer point type.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Value(int32 X, int32 Y);                                                                                           | Constructs a point from explicit X and Y coordinates.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Value();                                                                                                           | Constructs the zero point.                                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Value(int32 F);                                                                                                    | Constructs a point with both coordinates set to F.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Value(const FIntPoint& Other);                                                                                     | Copies another FIntPoint.                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntPoint.X;                                                                                                           | Horizontal integer coordinate.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntPoint.Y;                                                                                                           | Vertical integer coordinate.                                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Point = Other;                                                                                                     | Copies Other into Point.                                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Result = Point + Other;                                                                                            | Adds coordinates component-wise.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Result = Point - Other;                                                                                            | Subtracts coordinates component-wise.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Result = -Point;                                                                                                   | Negates both coordinates.                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Result = Point * Scale;                                                                                            | Multiplies both coordinates by Scale.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntPoint Result = Point / Divisor;                                                                                          | Divides both coordinates by Divisor using integer division.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Point *= Scale;                                                                                                              | Multiplies both coordinates by Scale in place.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Point /= Scale;                                                                                                              | Divides both coordinates by Scale in place using integer division.                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Point += Other;                                                                                                              | Adds Other component-wise in place.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Point -= Other;                                                                                                              | Subtracts Other component-wise in place.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 Component = Point[Index];                                                                                              | Reads a coordinate by numeric index.                                                                                 |
 * |                                                                                                                              | @param Index 0 selects X; 1 selects Y.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Point == Other;                                                                                                | Reports exact coordinate equality.                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntPoint.GetMax() const;                                                                                              | Returns the greater coordinate.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntPoint.GetMin() const;                                                                                              | Returns the lesser coordinate.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntPoint.Size() const;                                                                                                | Returns the integer-truncated Euclidean magnitude.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */


AS_FORCE_LINK const FAngelscriptBind Bind_FIntPoint_Type(
	TEXT("FIntPoint.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FIntPoint>("FIntPoint", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FIntPointType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FIntPoint(
	TEXT("FIntPoint.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FIntPoint_ = Binds.ExistingClassForTarget("FIntPoint");
		FIntPoint_.Constructor(
			"void f(int32 X, int32 Y)",
			&FAngelscriptFIntPointBinds::ConstructXY,
			"FIntPoint",
			true)
			.NoDiscard();
		FIntPoint_.Constructor("void f()", &FAngelscriptFIntPointBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FIntPoint", true, "0");
		FIntPoint_.Constructor(
			"void f(int32 F)",
			&FAngelscriptFIntPointBinds::ConstructScalar,
			"FIntPoint",
			true)
			.NoDiscard();
		FIntPoint_.Constructor(
			"void f(const FIntPoint& Other)",
			&FAngelscriptFIntPointBinds::ConstructCopy,
			"FIntPoint",
			true)
			.NoDiscard();
		FIntPoint_.Property("int32 X", &FIntPoint::X);
		FIntPoint_.Property("int32 Y", &FIntPoint::Y);
		FIntPoint_.Method("FIntPoint& opAssign(const FIntPoint& Other)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator=, (const FIntPoint&)));
		FIntPoint_.Method("FIntPoint opAdd(const FIntPoint& Other) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator+, (const FIntPoint&) const));
		FIntPoint_.Method("FIntPoint opSub(const FIntPoint& Other) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator-, (const FIntPoint&) const));
		FIntPoint_.Method("FIntPoint opNeg() const", &FAngelscriptFIntPointBinds::Negate);
		FIntPoint_.Method("FIntPoint opMul(int32 Scale) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator*, (int32) const));
		FIntPoint_.Method("FIntPoint opDiv(int32 Divisor) const", METHODPR_TRIVIAL(FIntPoint, FIntPoint, operator/, (int32) const));
		FIntPoint_.Method("FIntPoint& opMulAssign(int32 Scale)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator*=, (int32)));
		FIntPoint_.Method("FIntPoint& opDivAssign(int32 Scale)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator/=, (int32)));
		FIntPoint_.Method("FIntPoint opAddAssign(const FIntPoint& Other)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator+=, (const FIntPoint&)));
		FIntPoint_.Method("FIntPoint opSubAssign(const FIntPoint& Other)", METHODPR_TRIVIAL(FIntPoint&, FIntPoint, operator-=, (const FIntPoint&)));
		FIntPoint_.Method("const int32& opIndex(int32 Index) no_discard", METHODPR_TRIVIAL(int32&, FIntPoint, operator[], (const int32)));
		FIntPoint_.Method("bool opEquals(const FIntPoint& Other) const", METHODPR_TRIVIAL(bool, FIntPoint, operator==, (const FIntPoint&) const));
		FIntPoint_.Method("int32 GetMax() const", METHOD_TRIVIAL(FIntPoint, GetMax));
		FIntPoint_.Method("int32 GetMin() const", METHOD_TRIVIAL(FIntPoint, GetMin));
		FIntPoint_.Method("int32 Size() const", METHOD_TRIVIAL(FIntPoint, Size));
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FIntPoint_ToStringContribution(
	TEXT("FIntPoint.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FIntPoint"), &FAngelscriptFIntPointBinds::AppendToString);
	});

void FAngelscriptFIntPointBinds::ConstructXY(FIntPoint* Address, int32 X, int32 Y)
{
	new (Address) FIntPoint(X, Y);
}

void FAngelscriptFIntPointBinds::ConstructZero(FIntPoint* Address)
{
	new (Address) FIntPoint(0);
}

void FAngelscriptFIntPointBinds::ConstructScalar(FIntPoint* Address, int32 Scalar)
{
	new (Address) FIntPoint(Scalar);
}

void FAngelscriptFIntPointBinds::ConstructCopy(FIntPoint* Address, const FIntPoint& Other)
{
	new (Address) FIntPoint(Other);
}

FIntPoint FAngelscriptFIntPointBinds::Negate(FIntPoint* Point)
{
	return FIntPoint(-Point->X, -Point->Y);
}

void FAngelscriptFIntPointBinds::AppendToString(void* Address, FString& String)
{
	String += static_cast<FIntPoint*>(Address)->ToString();
}
