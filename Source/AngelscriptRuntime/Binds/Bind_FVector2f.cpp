#include "Bind_FVector2f.h"

#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

#include "Types/SlateVector2.h"

/**
 * FVector2f binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Value;                                                                                                             | Declares the two-component float vector type.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Value(float32 X, float32 Y);                                                                                       | Constructs a vector from explicit X and Y components.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Value();                                                                                                           | Constructs the zero vector.                                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Value(const FVector2f& Other);                                                                                     | Copies another FVector2f.                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Value(const FVector3f& Other);                                                                                     | Constructs from Other.X and Other.Y, discarding Z.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Value(const FVector2D& Other);                                                                                     | Converts FVector2D components to float32.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.X;                                                                                                         | Horizontal Cartesian component.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.Y;                                                                                                         | Vertical Cartesian component.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Vector = Other;                                                                                                    | Copies Other into Vector.                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector + Other;                                                                                           | Adds another vector component-wise.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector + Bias;                                                                                            | Adds Bias to both components.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector - Other;                                                                                           | Subtracts another vector component-wise.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector - Bias;                                                                                            | Subtracts Bias from both components.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector * Other;                                                                                           | Multiplies by Other component-wise.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector * Scale;                                                                                           | Scales both components.                                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector / Other;                                                                                           | Divides by Other component-wise.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = Vector / Scale;                                                                                           | Divides both components by Scale.                                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f Result = -Vector;                                                                                                  | Negates both components.                                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector *= Scale;                                                                                                             | Scales both components in place.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector *= Other;                                                                                                             | Multiplies by Other component-wise in place.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector /= Scale;                                                                                                             | Divides both components by Scale in place.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector /= Other;                                                                                                             | Divides by Other component-wise in place.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector += Other;                                                                                                             | Adds Other component-wise in place.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector -= Other;                                                                                                             | Subtracts Other component-wise in place.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector[Index] = Component;                                                                                                   | Writes a component by numeric index.                                                                                 |
 * |                                                                                                                              | @param Index 0 selects X; 1 selects Y.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 Component = Vector[Index];                                                                                           | Reads a component by numeric index.                                                                                  |
 * |                                                                                                                              | @param Index 0 selects X; 1 selects Y.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Vector == Other;                                                                                               | Reports exact component equality.                                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2f.Equals(const FVector2f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;                           | Compares both components within Tolerance.                                                                           |
 * |                                                                                                                              | @param Tolerance Maximum per-component difference.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.CrossProduct(const FVector2f& Other) const;                                                                | Returns the scalar 2D cross product with Other.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.DotProduct(const FVector2f& Other) const;                                                                  | Returns the dot product with Other.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.GetMax() const;                                                                                            | Returns the greater component.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.GetAbsMax() const;                                                                                         | Returns the greatest absolute component magnitude.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.GetMin() const;                                                                                            | Returns the lesser component.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f FVector2f.GetAbs() const;                                                                                          | Returns a copy with both components made non-negative.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.Size() const;                                                                                              | Returns the Euclidean vector length.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.SizeSquared() const;                                                                                       | Returns squared length without a square root.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2f.IsNearlyZero(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;                                             | Reports whether both component magnitudes are at most Tolerance.                                                     |
 * |                                                                                                                              | @param Tolerance Per-component zero threshold.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2f.IsZero() const;                                                                                               | Reports whether both components are exactly zero.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FVector2f.Normalize(float32 Tolerance = __SMALL_NUMBER_flt);                                                            | Normalizes this vector in place when its length is safe.                                                             |
 * |                                                                                                                              | @param Tolerance Squared-length threshold below which the vector becomes zero.                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f FVector2f.GetSafeNormal(float32 Tolerance = __SMALL_NUMBER_flt) const;                                             | Returns a unit-length copy, or zero when the length is too small.                                                    |
 * |                                                                                                                              | @param Tolerance Squared-length threshold for returning zero.                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FVector2f.ToDirectionAndLength(FVector2f& OutDir, float32& OutLength) const;                                            | Separates this vector into a unit direction and scalar length.                                                       |
 * |                                                                                                                              | @param OutDir Receives the unit direction, or zero for a zero-length vector.                                         |
 * |                                                                                                                              | @param OutLength Receives the original vector length.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2f.ContainsNaN() const;                                                                                          | Reports whether either component is NaN or non-finite.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f FVector2f.GetSignVector() const;                                                                                   | Returns a vector whose components are +1 or -1 from this vector's signs.                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.Distance(const FVector2f& Other) const;                                                                    | Returns Euclidean distance to Other.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector2f.DistSquared(const FVector2f& Other) const;                                                                 | Returns squared distance to Other without a square root.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2f FVector2f.GetClampedToMaxSize(float32 Max) const;                                                                  | Returns a copy whose length is limited to Max.                                                                       |
 * |                                                                                                                              | @param Max Maximum allowed vector length.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FVector2f.InitFromString(const FString& SourceString);                                                                  | Parses UE vector text into this value and reports success.                                                           |
 * |                                                                                                                              | @param SourceString UE-formatted two-component vector text.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FVector2f FVector2f::ZeroVector;                                                                                       | Zero vector constant (0,0).                                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FVector2f FVector2f::UnitVector;                                                                                       | Unit component vector constant (1,1).                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */


namespace
{
	void BindFVector2fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FVector2f>("FVector2f", Flags);
	}

	void BindFVector2fInfrastructure(FAngelscriptBinds& Binds)
	{
		auto Vector2fType = MakeShared<FVector2fType>();
		Binds.RegisterTypeForTarget(Vector2fType);
		Binds.RegisterTypeFinderForTarget([Vector2fType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp == nullptr)
				return false;

			if (StructProp->Struct == FDeprecateSlateVector2D::StaticStruct())
			{
				Usage.Type = Vector2fType;
				return true;
			}

			return false;
		});
		FToStringHelper::Register(Binds, TEXT("FVector2f"), &FAngelscriptFVector2fBinds::AppendToString);
	}

	void BindFVector2fFunctions(FAngelscriptBinds& Binds)
	{
		auto FVector2f_ = Binds.ExistingClassForTarget("FVector2f");
		FVector2f_.Constructor(
			"void f(float32 X, float32 Y)",
			&FAngelscriptFVector2fBinds::Construct,
			"FVector2f",
			true)
			.NoDiscard();
		FVector2f_.Constructor("void f()", &FAngelscriptFVector2fBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FVector2f", true, "0.f, 0.f");
		FVector2f_.Constructor(
			"void f(const FVector2f& Other)",
			&FAngelscriptFVector2fBinds::ConstructCopy,
			"FVector2f",
			true)
			.NoDiscard();
		FVector2f_.Constructor(
			"void f(const FVector3f& Other)",
			&FAngelscriptFVector2fBinds::ConstructFromVector3f,
			"FVector2f",
			true)
			.NoDiscard();
		FVector2f_.Constructor(
			"void f(const FVector2D& Other)",
			&FAngelscriptFVector2fBinds::ConstructFromVector2D,
			"FVector2f",
			true)
			.NoDiscard();

		FVector2f_.Property("float32 X", &FVector2f::X);
		FVector2f_.Property("float32 Y", &FVector2f::Y);
		FVector2f_.Method("FVector2f& opAssign(const FVector2f& Other)", METHODPR_TRIVIAL(FVector2f&, FVector2f, operator=, (const FVector2f&)));
		FVector2f_.Method("FVector2f opAdd(const FVector2f& Other) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator+, (const FVector2f&) const));
		FVector2f_.Method("FVector2f opSub(const FVector2f& Other) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator-, (const FVector2f&) const));
		FVector2f_.Method("FVector2f opMul(const FVector2f& Other) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator*, (const FVector2f&) const));
		FVector2f_.Method("FVector2f opDiv(const FVector2f& Other) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator/, (const FVector2f&) const));
		FVector2f_.Method("FVector2f opMul(float32 Scale) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator*, (float) const));
		FVector2f_.Method("FVector2f opDiv(float32 Scale) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator/, (float) const));
		FVector2f_.Method("FVector2f opAdd(float32 Bias) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator+, (float) const));
		FVector2f_.Method("FVector2f opSub(float32 Bias) const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator-, (float) const));
		FVector2f_.Method("FVector2f opNeg() const", METHODPR_TRIVIAL(FVector2f, FVector2f, operator-, () const));
		FVector2f_.Method("FVector2f opMulAssign(float32 Scale)", METHODPR_TRIVIAL(FVector2f, FVector2f, operator*=, (float)));
		FVector2f_.Method("FVector2f opDivAssign(float32 Scale)", METHODPR_TRIVIAL(FVector2f, FVector2f, operator/=, (float)));
		FVector2f_.Method("FVector2f opMulAssign(const FVector2f& Other)", METHODPR_TRIVIAL(FVector2f, FVector2f, operator*=, (const FVector2f&)));
		FVector2f_.Method("FVector2f opDivAssign(const FVector2f& Other)", METHODPR_TRIVIAL(FVector2f, FVector2f, operator/=, (const FVector2f&)));
		FVector2f_.Method("FVector2f opAddAssign(const FVector2f& Other)", METHODPR_TRIVIAL(FVector2f, FVector2f, operator+=, (const FVector2f&)));
		FVector2f_.Method("FVector2f opSubAssign(const FVector2f& Other)", METHODPR_TRIVIAL(FVector2f, FVector2f, operator-=, (const FVector2f&)));
		FVector2f_.Method("float32& opIndex(int32 Index)", METHODPR_TRIVIAL(float&, FVector2f, operator[], (int32)));
		FVector2f_.Method("float32 opIndex(int32 Index) const", METHODPR_TRIVIAL(float, FVector2f, operator[], (int32) const));
		FVector2f_.Method("bool opEquals(const FVector2f& Other) const", METHODPR_TRIVIAL(bool, FVector2f, operator==, (const FVector2f&) const));
		FVector2f_.Method("bool Equals(const FVector2f& Other, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector2f, Equals));
		FVector2f_.Method("float32 CrossProduct(const FVector2f& Other) const", FUNC_TRIVIAL(FVector2f::CrossProduct));
		FVector2f_.Method("float32 DotProduct(const FVector2f& Other) const", FUNC_TRIVIAL(FVector2f::DotProduct));
		FVector2f_.Method("float32 GetMax() const", METHOD_TRIVIAL(FVector2f, GetMax));
		FVector2f_.Method("float32 GetAbsMax() const", METHOD_TRIVIAL(FVector2f, GetAbsMax));
		FVector2f_.Method("float32 GetMin() const", METHOD_TRIVIAL(FVector2f, GetMin));
		FVector2f_.Method("FVector2f GetAbs() const", METHOD_TRIVIAL(FVector2f, GetAbs));
		FVector2f_.Method("float32 Size() const", METHOD_TRIVIAL(FVector2f, Size));
		FVector2f_.Method("float32 SizeSquared() const", METHOD_TRIVIAL(FVector2f, SizeSquared));
		FVector2f_.Method("bool IsNearlyZero(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector2f, IsNearlyZero));
		FVector2f_.Method("bool IsZero() const", METHOD_TRIVIAL(FVector2f, IsZero));
		FVector2f_.Method("void Normalize(float32 Tolerance = __SMALL_NUMBER_flt)", METHOD_TRIVIAL(FVector2f, Normalize));
		FVector2f_.Method("FVector2f GetSafeNormal(float32 Tolerance = __SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FVector2f, GetSafeNormal));
		FVector2f_.Method("void ToDirectionAndLength(FVector2f& OutDir, float32& OutLength) const", METHODPR_TRIVIAL(void, FVector2f, ToDirectionAndLength, (FVector2f&, float&) const));
		FVector2f_.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FVector2f, ContainsNaN));
		FVector2f_.Method("FVector2f GetSignVector() const", METHOD_TRIVIAL(FVector2f, GetSignVector));
		FVector2f_.Method("float32 Distance(const FVector2f& Other) const", FUNC_TRIVIAL(FVector2f::Distance));
		FVector2f_.Method("float32 DistSquared(const FVector2f& Other) const", FUNC_TRIVIAL(FVector2f::DistSquared));
		FVector2f_.Method("FVector2f GetClampedToMaxSize(float32 Max) const", &FAngelscriptFVector2fBinds::GetClampedToMaxSize);
		FVector2f_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FVector2f, InitFromString));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FVector2f");
		Binds.BindGlobalVariableForTarget("const FVector2f ZeroVector", &FVector2f::ZeroVector);
		Binds.BindGlobalVariableForTarget("const FVector2f UnitVector", &FVector2f::UnitVector);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2f_Type(
	TEXT("FVector2f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFVector2fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2f_Infrastructure(
	TEXT("FVector2f.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFVector2fInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FVector2f(
	TEXT("FVector2f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFVector2fFunctions);
