#include "Bind_FRotator.h"

#include "Misc/DefaultValueHelper.h"
#include "Kismet/KismetMathLibrary.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FRotator binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FRotator;                                                                           | Declares the double-precision Euler rotator value type.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator Rotator(float64 Pitch, float64 Yaw, float64 Roll);                                | Constructs a rotator from degree components.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator Rotator();                                                                        | Constructs the zero rotator.                                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator Rotator(float64 F);                                                               | Constructs a rotator with every degree component set to F.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator Rotator(const FRotator& Other);                                                   | Copy-constructs a rotator.                                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator Rotator(const FQuat& Quat);                                                       | Constructs a rotator from a quaternion.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator Rotator(const FRotator3f& Rotator);                                               | Constructs a double-precision rotator from FRotator3f.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRotator.Pitch;                                                                    | Exposes pitch in degrees.                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRotator.Yaw;                                                                      | Exposes yaw in degrees.                                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRotator.Roll;                                                                     | Exposes roll in degrees.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator = Other;                                                                           | Assigns another rotator.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator + Other;                                                                           | Returns the component-wise degree sum.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator += Other;                                                                          | Adds degree components in place.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator - Other;                                                                           | Returns the component-wise degree difference.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator -= Other;                                                                          | Subtracts degree components in place.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator * Scale;                                                                           | Returns the degree components scaled uniformly.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator *= Scale;                                                                          | Scales the degree components in place.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Rotator == Other;                                                                          | Compares degree components for exact equality.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FRotator.IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const;                  | Returns whether normalized degree components are within Tolerance of zero.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FRotator.IsZero() const;                                                              | Returns whether every degree component is exactly zero.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FRotator.Equals(const FRotator& R,                                                    | Returns whether normalized component differences are within Tolerance degrees.                                       |
 * |     float64 Tolerance = KINDA_SMALL_NUMBER) const;                                         |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator.GetInverse() const;                                                      | Returns the inverse rotation.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator.Clamp() const;                                                           | Returns components clamped to the [0, 360) degree range.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator.GetNormalized() const;                                                   | Returns components normalized to the (-180, 180] degree range.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator.GetDenormalized() const;                                                 | Returns components clamped to the [0, 360) degree range.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRotator.GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const;        | Splits full-turn winding from normalized remainder.                                                                  |
 * |                                                                                            | @param Winding Receives whole 360-degree turns. @param Remainder Receives normalized angles.                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRotator.GetManhattanDistance(const FRotator& Rotator) const;                      | Returns the sum of absolute component differences in degrees.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRotator.Normalize();                                                                 | Normalizes components in place to the (-180, 180] degree range.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FRotator.ContainsNaN();                                                               | Returns whether any component is non-finite.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FRotator FRotator::ZeroRotator;                                                      | Exposes the zero-rotation constant.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRotator::NormalizeAxis(float64 Angle);                                            | Normalizes Angle in degrees to the (-180, 180] range.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRotator::ClampAxis(float64 Angle);                                                | Clamps Angle in degrees to the [0, 360) range.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromEuler(const FVector& Euler);                                    | Constructs a rotator from Euler degree components.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromX(const FVector& XAxis);                                        | Builds a rotation whose X axis follows XAxis.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromY(const FVector& YAxis);                                        | Builds a rotation whose Y axis follows YAxis.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromZ(const FVector& ZAxis);                                        | Builds a rotation whose Z axis follows ZAxis.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromXY(const FVector& XAxis, const FVector& YAxis);                 | Builds an orthonormal rotation prioritizing XAxis then YAxis.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromXZ(const FVector& XAxis, const FVector& ZAxis);                 | Builds an orthonormal rotation prioritizing XAxis then ZAxis.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromYX(const FVector& YAxis, const FVector& XAxis);                 | Builds an orthonormal rotation prioritizing YAxis then XAxis.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromYZ(const FVector& YAxis, const FVector& ZAxis);                 | Builds an orthonormal rotation prioritizing YAxis then ZAxis.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromZX(const FVector& ZAxis, const FVector& XAxis);                 | Builds an orthonormal rotation prioritizing ZAxis then XAxis.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRotator FRotator::MakeFromZY(const FVector& ZAxis, const FVector& YAxis);                 | Builds an orthonormal rotation prioritizing ZAxis then YAxis.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.Vector() const;                                                           | Returns the forward unit vector.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FQuat FRotator.Quaternion() const;                                                         | Returns the equivalent quaternion.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.Euler() const;                                                            | Returns Euler degree components.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.GetForwardVector() const;                                                 | Returns the forward unit vector.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.GetRightVector() const;                                                   | Returns the right unit vector.                                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.GetUpVector() const;                                                      | Returns the up unit vector.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.RotateVector(const FVector& V) const;                                     | Rotates V by this rotation.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRotator.UnrotateVector(const FVector& V) const;                                   | Rotates V by the inverse of this rotation.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FRotator.ToColorString() const;                                                    | Returns the editor color-coded rotator string.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FRotator.InitFromString(const FString& SourceString);                                 | Parses degree components from SourceString and reports success.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + Rotator;                                                                            | Appends FRotator text to a string and returns the result.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += Rotator;                                                                           | Appends FRotator text to a string in place.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(Rotator);                                                                      | Appends FRotator text to a temporary or existing string.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FRotator.ToString() const;                                                         | Returns the engine string representation.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindFRotatorType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FRotator>("FRotator", Flags);
	}

	void BindFRotatorInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FRotatorType>());
		FToStringHelper::Register(Binds, TEXT("FRotator"), &FAngelscriptFRotatorBinds::AppendToString);
	}

	void BindFRotatorFunctions(FAngelscriptBinds& Binds)
	{
		auto FRotator_ = Binds.ExistingClassForTarget("FRotator");
		FRotator_.Constructor(
			"void f(float64 Pitch, float64 Yaw, float64 Roll)",
			&FAngelscriptFRotatorBinds::ConstructComponents,
			"FRotator",
			true)
			.NoDiscard();
		FRotator_.Constructor("void f()", &FAngelscriptFRotatorBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FRotator", true, "0.f");
		FRotator_.Constructor("void f(float64 F)", &FAngelscriptFRotatorBinds::ConstructScalar, "FRotator", true).NoDiscard();
		FRotator_.Constructor(
			"void f(const FRotator& Other)",
			&FAngelscriptFRotatorBinds::ConstructCopy,
			"FRotator",
			true)
			.NoDiscard();
		FRotator_.Property("float64 Pitch", &FRotator::Pitch);
		FRotator_.Property("float64 Yaw", &FRotator::Yaw);
		FRotator_.Property("float64 Roll", &FRotator::Roll);
		FRotator_.Method("FRotator& opAssign(const FRotator& Other)", METHODPR_TRIVIAL(FRotator&, FRotator, operator=, (const FRotator&)));
		FRotator_.Method("FRotator opAdd(const FRotator& R) const", METHOD_TRIVIAL(FRotator, operator+));
		FRotator_.Method("FRotator opAddAssign(const FRotator& R)", METHOD_TRIVIAL(FRotator, operator+=));
		FRotator_.Method("FRotator opSub(const FRotator& R) const", METHOD_TRIVIAL(FRotator, operator-));
		FRotator_.Method("FRotator opSubAssign(const FRotator& R)", METHOD_TRIVIAL(FRotator, operator-=));
		FRotator_.Method("FRotator opMul(float64 Scale) const", METHODPR_TRIVIAL(FRotator, FRotator, operator*, (double) const));
		FRotator_.Method("FRotator opMulAssign(float64 Scale)", METHODPR_TRIVIAL(FRotator, FRotator, operator*=, (double)));
		FRotator_.Method("bool opEquals(const FRotator& R) const", METHOD_TRIVIAL(FRotator, operator==));
		FRotator_.Method("bool IsNearlyZero(float64 Tolerance = KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FRotator, IsNearlyZero));
		FRotator_.Method("bool IsZero() const", METHOD_TRIVIAL(FRotator, IsZero));
		FRotator_.Method("bool Equals(const FRotator& R, float64 Tolerance=KINDA_SMALL_NUMBER) const", METHOD_TRIVIAL(FRotator, Equals));
		FRotator_.Method("FRotator GetInverse() const", METHOD_TRIVIAL(FRotator, GetInverse));
		FRotator_.Method("FRotator Clamp() const", METHOD_TRIVIAL(FRotator, Clamp));
		FRotator_.Method("FRotator GetNormalized() const", METHOD_TRIVIAL(FRotator, GetNormalized));
		FRotator_.Method("FRotator GetDenormalized() const", METHOD_TRIVIAL(FRotator, GetDenormalized));
		FRotator_.Method("void GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const", METHOD_TRIVIAL(FRotator, GetWindingAndRemainder));
		FRotator_.Method("float64 GetManhattanDistance(const FRotator& Rotator) const", METHOD_TRIVIAL(FRotator, GetManhattanDistance));
		FRotator_.Method("void Normalize()", METHOD_TRIVIAL(FRotator, Normalize));
		FRotator_.Method("bool ContainsNaN()", METHOD_TRIVIAL(FRotator, ContainsNaN));

		{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FRotator");
		Binds.BindGlobalVariableForTarget("const FRotator ZeroRotator", &FRotator::ZeroRotator);
		Binds.BindGlobalFunctionForTarget("float64 NormalizeAxis(float64 Angle) no_discard", FUNC_TRIVIAL(FRotator::NormalizeAxis));
		Binds.BindGlobalFunctionForTarget("float64 ClampAxis(float64 Angle) no_discard", FUNC_TRIVIAL(FRotator::ClampAxis));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromEuler(const FVector& Euler) no_discard", FUNCPR_TRIVIAL(FRotator, FRotator::MakeFromEuler, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromX(const FVector& XAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromX, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromY(const FVector& YAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromY, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromZ(const FVector& ZAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromZ, (const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromXY(const FVector& XAxis, const FVector& YAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromXY, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromXZ(const FVector& XAxis, const FVector& ZAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromXZ, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromYX(const FVector& YAxis, const FVector& XAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromYX, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromYZ(const FVector& YAxis, const FVector& ZAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromYZ, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromZX(const FVector& ZAxis, const FVector& XAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromZX, (const FVector&, const FVector&)));
		Binds.BindGlobalFunctionForTarget("FRotator MakeFromZY(const FVector& ZAxis, const FVector& YAxis) no_discard", FUNCPR_TRIVIAL(FRotator, UKismetMathLibrary::MakeRotFromZY, (const FVector&, const FVector&)));
		}

		FRotator_.Method("FVector Vector() const", METHOD_TRIVIAL(FRotator, Vector));
		FRotator_.Method("FQuat Quaternion() const", METHOD_TRIVIAL(FRotator, Quaternion));
		FRotator_.Method("FVector Euler() const", METHOD_TRIVIAL(FRotator, Euler));
		FRotator_.Method("FVector GetForwardVector() const", METHOD_TRIVIAL(FRotator, Vector));
		FRotator_.Method("FVector GetRightVector() const", &FAngelscriptFRotatorBinds::GetRightVector);
		FRotator_.Method("FVector GetUpVector() const", &FAngelscriptFRotatorBinds::GetUpVector);
		FRotator_.Method("FVector RotateVector(const FVector& V) const", METHOD_TRIVIAL(FRotator, RotateVector));
		FRotator_.Method("FVector UnrotateVector(const FVector& V) const", METHOD_TRIVIAL(FRotator, UnrotateVector));
		FRotator_.Constructor("void f(const FQuat& Quat)", &FAngelscriptFRotatorBinds::ConstructFromQuat, "FRotator", true);
		FRotator_.Constructor("void f(const FRotator3f& Rotator)", &FAngelscriptFRotatorBinds::ConstructFromRotator3f, "FRotator", true);
		FRotator_.Method("FString ToColorString() const", &FAngelscriptFRotatorBinds::ToColorString);
		FRotator_.Method("bool InitFromString(const FString& SourceString)", METHOD_TRIVIAL(FRotator, InitFromString));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FRotator_Type(TEXT("FRotator.Type"), EAngelscriptBindPhase::TypeDeclarations, &BindFRotatorType);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator_Infrastructure(TEXT("FRotator.Infrastructure"), EAngelscriptBindPhase::TypeInfrastructure, &BindFRotatorInfrastructure);
AS_FORCE_LINK const FAngelscriptBind Bind_FRotator(TEXT("FRotator.Functions"), EAngelscriptBindPhase::ManualBindings, &BindFRotatorFunctions);
