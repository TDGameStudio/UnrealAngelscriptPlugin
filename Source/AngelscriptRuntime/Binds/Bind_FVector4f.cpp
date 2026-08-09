#include "Bind_FVector4f.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FVector4f binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FVector4f;                                                                          | Declares the single-precision four-component vector value type.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector4f Vector(float32 InX, float32 InY, float32 InZ, float32 InW);                      | Constructs a vector from four components.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector4f Vector();                                                                        | Constructs the zero vector.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector4f Vector(const FVector4f& Other);                                                  | Copy-constructs a vector.                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector4f Vector(FVector3f InVector, float32 InW);                                         | Constructs a vector from FVector3f and a W component.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector4f Vector(const FVector4& Other);                                                   | Constructs a single-precision vector from FVector4.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector4f.X;                                                                       | Exposes the X component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector4f.Y;                                                                       | Exposes the Y component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector4f.Z;                                                                       | Exposes the Z component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FVector4f.W;                                                                       | Exposes the W component.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector = Other;                                                                            | Assigns another vector.                                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector + Other;                                                                            | Returns the component-wise sum.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector - Other;                                                                            | Returns the component-wise difference.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector * Scale;                                                                            | Returns the uniformly scaled vector.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector / Divisor;                                                                          | Returns the vector divided by a scalar.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector *= S;                                                                               | Scales the vector in place.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector[Index];                                                                             | Returns the indexed component by reference.                                                                          |
 * |                                                                                            | @param Index Component index from 0 through 3.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Vector == Other;                                                                           | Compares two vectors for exact equality.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + Vector;                                                                             | Appends FVector4f text to a string and returns the result.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += Vector;                                                                            | Appends FVector4f text to a string in place.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(Vector);                                                                       | Appends FVector4f text to a temporary or existing string.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FVector4f.ToString() const;                                                        | Returns the engine string representation.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4f_Type(
	TEXT("FVector4f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FVector4f>("FVector4f", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4f_Infrastructure(
	TEXT("FVector4f.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FVector4fType>());
		FToStringHelper::Register(Binds, TEXT("FVector4f"), &FAngelscriptFVector4fBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FVector4f(
	TEXT("FVector4f.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FVector4f_ = Binds.ExistingClassForTarget("FVector4f");
		FVector4f_.Constructor(
			"void f(float32 InX, float32 InY, float32 InZ, float32 InW)",
			&FAngelscriptFVector4fBinds::Construct,
			"FVector4f",
			true)
			.NoDiscard();
		FVector4f_.Constructor("void f()", &FAngelscriptFVector4fBinds::ConstructZero)
			.NoDiscard()
			.NativeConstructor("FVector4f", true, "0, 0, 0, 0");
		FVector4f_.Constructor(
			"void f(const FVector4f& Other)",
			&FAngelscriptFVector4fBinds::ConstructCopy,
			"FVector4f",
			true)
			.NoDiscard();
		FVector4f_.Constructor(
			"void f(FVector3f InVector, float32 InW)",
			&FAngelscriptFVector4fBinds::ConstructFromVector3f,
			"FVector4f",
			true)
			.NoDiscard();
		FVector4f_.Constructor(
			"void f(const FVector4& Other)",
			&FAngelscriptFVector4fBinds::ConstructFromVector4,
			"FVector4f",
			true)
			.NoDiscard();

		FVector4f_.Property("float32 X", &FVector4f::X);
		FVector4f_.Property("float32 Y", &FVector4f::Y);
		FVector4f_.Property("float32 Z", &FVector4f::Z);
		FVector4f_.Property("float32 W", &FVector4f::W);
		FVector4f_.Method("FVector4f& opAssign(const FVector4f& Other)", METHODPR_TRIVIAL(FVector4f&, FVector4f, operator=, (const FVector4f&)));
		FVector4f_.Method("FVector4f opAdd(const FVector4f& Other) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator+, (const FVector4f&) const));
		FVector4f_.Method("FVector4f opSub(const FVector4f& Other) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator-, (const FVector4f&) const));
		FVector4f_.Method("FVector4f opMul(float32 Scale) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator*, (float) const));
		FVector4f_.Method("FVector4f opDiv(float32 Divisor) const", METHODPR_TRIVIAL(FVector4f, FVector4f, operator/, (float) const));
		FVector4f_.Method("FVector4f opMulAssign(float32 S)", METHODPR_TRIVIAL(FVector4f, FVector4f, operator*=, (float)));
		FVector4f_.Method("const float32& opIndex(int32 Index)", METHODPR_TRIVIAL(float&, FVector4f, operator[], (int32)));
		FVector4f_.Method("bool opEquals(const FVector4f& Other) const", METHODPR_TRIVIAL(bool, FVector4f, operator==, (const FVector4f&) const));
	});

void FAngelscriptFVector4fBinds::Construct(FVector4f* Address, const float X, const float Y, const float Z, const float W)
{
	new (Address) FVector4f(X, Y, Z, W);
}

void FAngelscriptFVector4fBinds::ConstructZero(FVector4f* Address)
{
	new (Address) FVector4f(0.f, 0.f, 0.f, 0.f);
}

void FAngelscriptFVector4fBinds::ConstructCopy(FVector4f* Address, const FVector4f& Other)
{
	new (Address) FVector4f(Other);
}

void FAngelscriptFVector4fBinds::ConstructFromVector3f(FVector4f* Address, const FVector3f InVector, const float InW)
{
	new (Address) FVector4f(InVector, InW);
}

void FAngelscriptFVector4fBinds::ConstructFromVector4(FVector4f* Address, const FVector4& Other)
{
	new (Address) FVector4f(Other);
}

void FAngelscriptFVector4fBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FVector4f*>(Ptr)->ToString();
}

FString FVector4fType::GetAngelscriptTypeName() const
{
	return TEXT("FVector4f");
}

void FVector4fType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FVector4f(0.f, 0.f, 0.f, 0.f);
}

bool FVector4fType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector4fType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector4fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
