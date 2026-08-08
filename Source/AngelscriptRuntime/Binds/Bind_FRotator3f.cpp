#include "Bind_FRotator3f.h"

#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"


/**
 * FRotator3f construction, operators, rotation helpers, constants, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator(float32 Pitch, float32 Yaw, float32 Roll);                                        | Constructs from degree angles.                                                                                   |
 * |                                                                                                      | @param Pitch Rotation around Y in degrees.                                                                       |
 * |                                                                                                      | @param Yaw Rotation around Z in degrees.                                                                         |
 * |                                                                                                      | @param Roll Rotation around X in degrees.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator();                                                                                | Constructs the zero rotator.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator(float32 F);                                                                       | Initializes every angle from one scalar.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator(const FRotator3f& Other);                                                         | Copy-constructs a rotator.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Rotator.Pitch;                                                                               | Exposes pitch in degrees.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Rotator.Yaw;                                                                                 | Exposes yaw in degrees.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Rotator.Roll;                                                                                | Exposes roll in degrees.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns a rotator.                                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Sum = Left + Right;                                                                       | Adds angles component-wise.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left += Right;                                                                                       | Adds angles in place.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Difference = Left - Right;                                                                | Subtracts angles component-wise.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left -= Right;                                                                                       | Subtracts angles in place.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Scaled = Rotator * Scale;                                                                 | Scales every angle.                                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Rotator *= Scale;                                                                                    | Scales every angle in place.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares angles exactly.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Rotator.IsNearlyZero(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;                       | Reports whether all normalized angles are nearly zero.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Rotator.IsZero() const;                                                                         | Reports whether all raw angles are exactly zero.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Rotator.Equals(const FRotator3f& R, float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const;        | Compares normalized angles within a tolerance.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator.GetInverse() const;                                                               | Returns the inverse rotation.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator.Clamp() const;                                                                    | Clamps each angle to [0, 360).                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator.GetNormalized() const;                                                            | Returns normalized angles in the conventional signed range.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator.GetDenormalized() const;                                                          | Returns angles in [0, 360).                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Rotator.GetWindingAndRemainder(FRotator3f& Winding, FRotator3f& Remainder) const;               | Splits winding turns from normalized remainder.                                                                  |
 * |                                                                                                      | @param Winding Receives whole-turn components.                                                                   |
 * |                                                                                                      | @param Remainder Receives normalized residual angles.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Rotator.GetManhattanDistance(const FRotator3f& Other) const;                                 | Returns the sum of absolute component differences.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Rotator.Normalize();                                                                            | Normalizes angles in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Rotator.ContainsNaN();                                                                          | Reports whether any angle is non-finite.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const FRotator3f FRotator3f::ZeroRotator;                                                            | Provides the zero-rotation constant.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 FRotator3f::NormalizeAxis(float32 Angle);                                                    | Normalizes an angle to the signed axis range.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 FRotator3f::ClampAxis(float32 Angle);                                                        | Clamps an angle to [0, 360).                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f FRotator3f::MakeFromEuler(const FVector3f& Euler);                                        | Constructs from Euler angles.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Rotator.Vector() const;                                                                    | Returns the forward direction.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FQuat4f Rotator.Quaternion() const;                                                                  | Converts to a quaternion.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Rotator.Euler() const;                                                                     | Returns Euler angles.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Rotator.RotateVector(const FVector3f& V) const;                                            | Rotates a vector.                                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector3f Rotator.UnrotateVector(const FVector3f& V) const;                                          | Applies the inverse rotation to a vector.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator(const FQuat4f& Quat);                                                             | Constructs from a quaternion.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FRotator3f Rotator(const FRotator& Rotator);                                                         | Converts a double-precision rotator.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Rotator.ToColorString() const;                                                               | Formats components as a color-style string.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Rotator.InitFromString(const FString& SourceString);                                            | Parses component values from text.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Rotator}";                                                                         | Formats the rotator through the shared string formatter contribution.                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindFRotator3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FRotator3f>("FRotator3f", Flags);
	}

	void BindFRotator3fInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FRotator3fType>());
		FToStringHelper::Register(Binds, TEXT("FRotator3f"), &FAngelscriptFRotator3fBinds::AppendToString);
	}

	void BindFRotator3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FRotator3f_ = Binds.ExistingClassForTarget("FRotator3f");
		FRotator3f_.Constructor(
			"void f(float32 Pitch, float32 Yaw, float32 Roll)",
			&FAngelscriptFRotator3fBinds::ConstructComponents,
			"FRotator3f",
			true)
			.NoDiscard();
		FRotator3f_.Constructor("void f()", &FAngelscriptFRotator3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FRotator3f", true, "0.f");
		FRotator3f_.Constructor(
			"void f(float32 F)",
			&FAngelscriptFRotator3fBinds::ConstructScalar,
			"FRotator3f",
			true)
			.NoDiscard();
		FRotator3f_.Constructor(
			"void f(const FRotator3f& Other)",
			&FAngelscriptFRotator3fBinds::ConstructCopy,
			"FRotator3f",
			true)
			.NoDiscard();
		FRotator3f_.Property("float32 Pitch", &FRotator3f::Pitch);
		FRotator3f_.Property("float32 Yaw", &FRotator3f::Yaw);
		FRotator3f_.Property("float32 Roll", &FRotator3f::Roll);
		FRotator3f_.Method("FRotator3f& opAssign(const FRotator3f& Other)", METHODPR_TRIVIAL(FRotator3f&, FRotator3f, operator=, (const FRotator3f&)));
		FRotator3f_.Method("FRotator3f opAdd(const FRotator3f& R) const", METHOD_TRIVIAL(FRotator3f, operator+));
		FRotator3f_.Method("FRotator3f opAddAssign(const FRotator3f& R)", METHOD_TRIVIAL(FRotator3f, operator+=));
		FRotator3f_.Method("FRotator3f opSub(const FRotator3f& R) const", METHOD_TRIVIAL(FRotator3f, operator-));
		FRotator3f_.Method("FRotator3f opSubAssign(const FRotator3f& R)", METHOD_TRIVIAL(FRotator3f, operator-=));
		FRotator3f_.Method("FRotator3f opMul(float32 Scale) const", METHODPR_TRIVIAL(FRotator3f, FRotator3f, operator*, (float) const));
		FRotator3f_.Method("FRotator3f opMulAssign(float32 Scale)", METHODPR_TRIVIAL(FRotator3f, FRotator3f, operator*=, (float)));
		FRotator3f_.Method("bool opEquals(const FRotator3f& R) const", METHOD_TRIVIAL(FRotator3f, operator==));
		FRotator3f_.Method("bool IsNearlyZero(float32 Tolerance = __KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FRotator3f, IsNearlyZero));
		FRotator3f_.Method("bool IsZero() const", METHOD_TRIVIAL(FRotator3f, IsZero));
		FRotator3f_.Method("bool Equals(const FRotator3f& R, float32 Tolerance=__KINDA_SMALL_NUMBER_flt) const", METHOD_TRIVIAL(FRotator3f, Equals));
		FRotator3f_.Method("FRotator3f GetInverse() const", METHOD_TRIVIAL(FRotator3f, GetInverse));
		FRotator3f_.Method("FRotator3f Clamp() const", METHOD_TRIVIAL(FRotator3f, Clamp));
		FRotator3f_.Method("FRotator3f GetNormalized() const", METHOD_TRIVIAL(FRotator3f, GetNormalized));
		FRotator3f_.Method("FRotator3f GetDenormalized() const", METHOD_TRIVIAL(FRotator3f, GetDenormalized));
		FRotator3f_.Method("void GetWindingAndRemainder(FRotator3f& Winding, FRotator3f& Remainder) const", METHOD_TRIVIAL(FRotator3f, GetWindingAndRemainder));
		FRotator3f_.Method("float32 GetManhattanDistance(const FRotator3f& Rotator) const", METHOD_TRIVIAL(FRotator3f, GetManhattanDistance));
		FRotator3f_.Method("void Normalize()", METHOD_TRIVIAL(FRotator3f, Normalize));
		FRotator3f_.Method("bool ContainsNaN()", METHOD_TRIVIAL(FRotator3f, ContainsNaN));

		{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FRotator3f");
		Binds.BindGlobalVariableForTarget("const FRotator3f ZeroRotator", &FRotator3f::ZeroRotator);
		Binds.BindGlobalFunctionForTarget("float32 NormalizeAxis(float32 Angle) no_discard", FUNC_TRIVIAL(FRotator3f::NormalizeAxis));
		Binds.BindGlobalFunctionForTarget("float32 ClampAxis(float32 Angle) no_discard", FUNC_TRIVIAL(FRotator3f::ClampAxis));
		Binds.BindGlobalFunctionForTarget("FRotator3f MakeFromEuler(const FVector3f& Euler) no_discard", FUNCPR_TRIVIAL(FRotator3f, FRotator3f::MakeFromEuler, (const FVector3f&)));
		}

		FRotator3f_.Method("FVector3f Vector() const", METHOD_TRIVIAL(FRotator3f, Vector));
		FRotator3f_.Method("FQuat4f Quaternion() const", METHOD_TRIVIAL(FRotator3f, Quaternion));
		FRotator3f_.Method("FVector3f Euler() const", METHOD_TRIVIAL(FRotator3f, Euler));
		FRotator3f_.Method("FVector3f RotateVector(const FVector3f& V) const", METHOD_TRIVIAL(FRotator3f, RotateVector));
		FRotator3f_.Method("FVector3f UnrotateVector(const FVector3f& V) const", METHOD_TRIVIAL(FRotator3f, UnrotateVector));
		FRotator3f_.Constructor("void f(const FQuat4f& Quat)", &FAngelscriptFRotator3fBinds::ConstructFromQuat4f, "FRotator3f", true);
		FRotator3f_.Constructor("void f(const FRotator& Rotator)", &FAngelscriptFRotator3fBinds::ConstructFromRotator, "FRotator3f", true);
		FRotator3f_.Method("FString ToColorString() const", &FAngelscriptFRotator3fBinds::ToColorString);
		FRotator3f_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FRotator3f, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FRotator3f_Type(TEXT("FRotator3f.Type"), EAngelscriptBindPhase::TypeDeclarations, &BindFRotator3fType);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator3f_Infrastructure(TEXT("FRotator3f.Infrastructure"), EAngelscriptBindPhase::TypeInfrastructure, &BindFRotator3fInfrastructure);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator3f(TEXT("FRotator3f.Functions"), EAngelscriptBindPhase::ManualBindings, &BindFRotator3fFunctions);
