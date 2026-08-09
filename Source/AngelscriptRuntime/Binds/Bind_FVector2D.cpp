#include "Bind_FVector2D.h"

#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FVector2D binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FVector2D;                                                                          | Declares the double-precision two-component vector value type.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D Vector(float64 X, float64 Y);                                                    | Constructs a vector from two components.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D Vector();                                                                        | Constructs the zero vector.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D Vector(const FVector2D& Other);                                                  | Copy-constructs a vector.                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D Vector(const FVector2f& Other);                                                  | Constructs a double-precision vector from FVector2f.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.X;                                                                       | Exposes the X component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.Y;                                                                       | Exposes the Y component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector = Other;                                                                            | Assigns another vector.                                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector + Other;                                                                            | Returns the component-wise sum.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector - Other;                                                                            | Returns the component-wise difference.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector * Other;                                                                            | Returns the component-wise product.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector / Other;                                                                            | Returns the component-wise quotient.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector * Scale;                                                                            | Returns the vector multiplied by a scalar.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector / Scale;                                                                            | Returns the vector divided by a scalar.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector + Bias;                                                                             | Returns Bias added to each component.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector - Bias;                                                                             | Returns Bias subtracted from each component.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | -Vector;                                                                                   | Returns the component-wise negation.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector *= Scale;                                                                           | Multiplies each component by Scale in place.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector /= Scale;                                                                           | Divides each component by Scale in place.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector *= Other;                                                                           | Multiplies by another vector component-wise in place.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector /= Other;                                                                           | Divides by another vector component-wise in place.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector += Other;                                                                           | Adds another vector in place.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector -= Other;                                                                           | Subtracts another vector in place.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector[Index];                                                                             | Returns a mutable component reference for a non-const vector.                                                        |
 * |                                                                                            | @param Index Component index 0 for X or 1 for Y.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ConstVector[Index];                                                                        | Returns a component value for a const vector.                                                                        |
 * |                                                                                            | @param Index Component index 0 for X or 1 for Y.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector == Other;                                                                           | Compares two vectors for exact equality.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2D.Equals(const FVector2D& Other,                                              | Returns whether both component differences are within Tolerance.                                                     |
 * |     float64 Tolerance = KINDA_SMALL_NUMBER) const;                                         |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.CrossProduct(const FVector2D& Other) const;                              | Returns the scalar 2D cross product.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.DotProduct(const FVector2D& Other) const;                                | Returns the dot product.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.GetMax() const;                                                          | Returns the greatest component.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.GetAbsMax() const;                                                       | Returns the greatest absolute component.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.GetMin() const;                                                          | Returns the smallest component.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FVector2D.GetAbs() const;                                                        | Returns component-wise absolute values.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.Size() const;                                                            | Returns the Euclidean magnitude.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.SizeSquared() const;                                                     | Returns the squared Euclidean magnitude.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2D.IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const;                 | Returns whether both absolute components are within Tolerance.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2D.IsZero() const;                                                             | Returns whether both components are exactly zero.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FVector2D.Normalize(float64 Tolerance = SMALL_NUMBER);                                | Normalizes in place, producing zero when squared length is below Tolerance.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FVector2D.GetSafeNormal(float64 Tolerance = SMALL_NUMBER) const;                 | Returns a normalized copy, or zero when squared length is below Tolerance.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2D.ContainsNaN() const;                                                        | Returns whether either component is non-finite.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FVector2D.GetSignVector() const;                                                 | Returns a vector containing the sign of each component.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.Distance(const FVector2D& Other) const;                                  | Returns Euclidean distance to Other.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FVector2D.DistSquared(const FVector2D& Other) const;                               | Returns squared Euclidean distance to Other.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FVector2D.GetClampedToMaxSize(float64 Max) const;                                | Returns a copy whose magnitude does not exceed Max.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2D.InitFromString(const FString& SourceString);                                | Parses components from SourceString and reports success.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FVector2D FVector2D::ZeroVector;                                                     | Exposes the zero-vector constant.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FVector2D FVector2D::UnitVector;                                                     | Exposes the (1, 1) vector constant.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + Vector;                                                                             | Appends FVector2D text to a string and returns the result.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += Vector;                                                                            | Appends FVector2D text to a string in place.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(Vector);                                                                       | Appends FVector2D text to a temporary or existing string.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FVector2D.ToString() const;                                                        | Returns the engine string representation.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2D_Type(
	TEXT("FVector2D.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FVector2D>("FVector2D", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2D_Infrastructure(
	TEXT("FVector2D.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FVector2DType>());
		FToStringHelper::Register(Binds, TEXT("FVector2D"), &FAngelscriptFVector2DBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2D(
	TEXT("FVector2D.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FVector2D_ = Binds.ExistingClassForTarget("FVector2D");
		FVector2D_.Constructor(
			"void f(float64 X, float64 Y)",
			&FAngelscriptFVector2DBinds::Construct,
			"FVector2D",
			true)
			.NoDiscard();
		FVector2D_.Constructor("void f()", &FAngelscriptFVector2DBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FVector2D", true, "0.f, 0.f");
		FVector2D_.Constructor(
			"void f(const FVector2D& Other)",
			&FAngelscriptFVector2DBinds::ConstructCopy,
			"FVector2D",
			true)
			.NoDiscard();
		FVector2D_.Constructor(
			"void f(const FVector2f& Other)",
			&FAngelscriptFVector2DBinds::ConstructFromVector2f,
			"FVector2D",
			true)
			.NoDiscard();

		FVector2D_.Property("float64 X", &FVector2D::X);
		FVector2D_.Property("float64 Y", &FVector2D::Y);
		FVector2D_.Method("FVector2D& opAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D&, FVector2D, operator=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opAdd(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator+, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opSub(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opMul(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opDiv(const FVector2D& Other) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/, (const FVector2D&) const));
		FVector2D_.Method("FVector2D opMul(float64 Scale) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*, (double) const));
		FVector2D_.Method("FVector2D opDiv(float64 Scale) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/, (double) const));
		FVector2D_.Method("FVector2D opAdd(float64 Bias) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator+, (double) const));
		FVector2D_.Method("FVector2D opSub(float64 Bias) const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-, (double) const));
		FVector2D_.Method("FVector2D opNeg() const", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-, () const));
		FVector2D_.Method("FVector2D opMulAssign(float64 Scale)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*=, (double)));
		FVector2D_.Method("FVector2D opDivAssign(float64 Scale)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/=, (double)));
		FVector2D_.Method("FVector2D opMulAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator*=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opDivAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator/=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opAddAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator+=, (const FVector2D&)));
		FVector2D_.Method("FVector2D opSubAssign(const FVector2D& Other)", METHODPR_TRIVIAL(FVector2D, FVector2D, operator-=, (const FVector2D&)));
		FVector2D_.Method("float64& opIndex(int32 Index)", METHODPR_TRIVIAL(double&, FVector2D, operator[], (int32)));
		FVector2D_.Method("float64 opIndex(int32 Index) const", METHODPR_TRIVIAL(double, FVector2D, operator[], (int32) const));
		FVector2D_.Method("bool opEquals(const FVector2D& Other) const", METHODPR_TRIVIAL(bool, FVector2D, operator==, (const FVector2D&) const));
		FVector2D_.Method("bool Equals(const FVector2D& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector2D, Equals));
		FVector2D_.Method("float64 CrossProduct(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::CrossProduct));
		FVector2D_.Method("float64 DotProduct(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::DotProduct));
		FVector2D_.Method("float64 GetMax() const", METHOD_TRIVIAL(FVector2D, GetMax));
		FVector2D_.Method("float64 GetAbsMax() const", METHOD_TRIVIAL(FVector2D, GetAbsMax));
		FVector2D_.Method("float64 GetMin() const", METHOD_TRIVIAL(FVector2D, GetMin));
		FVector2D_.Method("FVector2D GetAbs() const", METHOD_TRIVIAL(FVector2D, GetAbs));
		FVector2D_.Method("float64 Size() const", METHOD_TRIVIAL(FVector2D, Size));
		FVector2D_.Method("float64 SizeSquared() const", METHOD_TRIVIAL(FVector2D, SizeSquared));
		FVector2D_.Method("bool IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FVector2D, IsNearlyZero));
		FVector2D_.Method("bool IsZero() const", METHOD_TRIVIAL(FVector2D, IsZero));
		FVector2D_.Method("void Normalize(float64 Tolerance = SMALL_NUMBER)", METHOD_TRIVIAL(FVector2D, Normalize));
		FVector2D_.Method("FVector2D GetSafeNormal(float64 Tolerance = SMALL_NUMBER) const", METHOD_TRIVIAL(FVector2D, GetSafeNormal));
		FVector2D_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FVector2D, ContainsNaN));
		FVector2D_.Method("FVector2D GetSignVector() const", METHOD_TRIVIAL(FVector2D, GetSignVector));
		FVector2D_.Method("float64 Distance(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::Distance));
		FVector2D_.Method("float64 DistSquared(const FVector2D& Other) const", FUNC_TRIVIAL(FVector2D::DistSquared));
		FVector2D_.Method("FVector2D GetClampedToMaxSize(float64 Max) const", &FAngelscriptFVector2DBinds::GetClampedToMaxSize);
		FVector2D_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FVector2D, InitFromString));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FVector2D");
		Binds.BindGlobalVariableForTarget("const FVector2D ZeroVector", &FVector2D::ZeroVector);
		Binds.BindGlobalVariableForTarget("const FVector2D UnitVector", &FVector2D::UnitVector);
	});
