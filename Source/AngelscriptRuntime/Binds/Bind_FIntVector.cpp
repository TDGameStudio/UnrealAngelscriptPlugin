#include "Bind_FIntVector.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FIntVector binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FIntVector;                                                                         | Declares the three-component integer vector value type.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntVector Vector(int32 X, int32 Y, int32 Z);                                              | Constructs a vector from three integer components.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntVector Vector();                                                                       | Constructs the zero vector.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntVector Vector(int32 F);                                                                | Constructs a vector with all components set to F.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FIntVector Vector(const FIntVector& Other);                                                | Copy-constructs a vector.                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntVector.X;                                                                        | Exposes the X component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntVector.Y;                                                                        | Exposes the Y component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntVector.Z;                                                                        | Exposes the Z component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector = Other;                                                                            | Assigns another vector.                                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector + Other;                                                                            | Returns the component-wise sum.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector - Other;                                                                            | Returns the component-wise difference.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | -Vector;                                                                                   | Returns the component-wise negation.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector * Scale;                                                                            | Returns the vector multiplied by an integer scalar.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector / Divisor;                                                                          | Returns the vector divided by an integer scalar.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector *= Scale;                                                                           | Multiplies this vector by an integer scalar in place.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector /= Scale;                                                                           | Divides this vector by an integer scalar in place.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector += Other;                                                                           | Adds another vector in place.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector -= Other;                                                                           | Subtracts another vector in place.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector[Index];                                                                             | Returns an indexed component by reference.                                                                           |
 * |                                                                                            | @param Index Component index from 0 through 2.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector == Other;                                                                           | Compares two vectors for exact equality.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntVector.GetMax() const;                                                           | Returns the greatest component.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntVector.GetMin() const;                                                           | Returns the smallest component.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FIntVector.Size() const;                                                             | Returns the integer Euclidean magnitude.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FIntVector.IsZero() const;                                                            | Returns whether every component is zero.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + Vector;                                                                             | Appends FIntVector text to a string and returns the result.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += Vector;                                                                            | Appends FIntVector text to a string in place.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(Vector);                                                                       | Appends FIntVector text to a temporary or existing string.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FIntVector.ToString() const;                                                       | Returns the engine string representation.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

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
