#include "AngelscriptBinds.h"

#include "Math/Matrix.h"

struct FAngelscriptFMatrixBinds
{
	static FMatrix Identity()
	{
		return FMatrix::Identity;
	}

	static FMatrix Zero()
	{
		return FMatrix(ForceInit);
	}
};

/**
 * FMatrix binding surface.
 * +----------------------------------------------------------------------------------+--------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                      | Purpose / parameter notes                                                            |
 * +----------------------------------------------------------------------------------+--------------------------------------------------------------------------------------+
 * | FMatrix Matrix();                                                                | Uses UE's default construction; use Identity() when an identity matrix is required. |
 * +----------------------------------------------------------------------------------+--------------------------------------------------------------------------------------+
 * | FMatrix FMatrix::Identity();                                                     | Returns the engine-default identity matrix.                                          |
 * +----------------------------------------------------------------------------------+--------------------------------------------------------------------------------------+
 * | FVector4 FMatrix.TransformPosition(const FVector& Position) const;               | Transforms a position with translation, preserving homogeneous W.                    |
 * +----------------------------------------------------------------------------------+--------------------------------------------------------------------------------------+
 * | FVector4 FMatrix.TransformVector(const FVector& Vector) const;                  | Transforms a direction without translation, preserving homogeneous W.                |
 * +----------------------------------------------------------------------------------+--------------------------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_FMatrix(
	TEXT("FMatrix"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Matrix = Binds.ExistingClassForTarget("FMatrix");
		Matrix.Method("bool opEquals(const FMatrix& Other) const", METHODPR_TRIVIAL(bool, FMatrix, operator==, (const FMatrix&) const));
		Matrix.Method("FMatrix opMul(const FMatrix& Other) const", METHODPR_TRIVIAL(FMatrix, FMatrix, operator*, (const FMatrix&) const));
		Matrix.Method("FMatrix opMul(float64 Scale) const", METHODPR_TRIVIAL(FMatrix, FMatrix, operator*, (double) const));
		Matrix.Method("FMatrix opAdd(const FMatrix& Other) const", METHODPR_TRIVIAL(FMatrix, FMatrix, operator+, (const FMatrix&) const));
		Matrix.Method("FVector4 TransformPosition(const FVector& Position) const", METHOD_TRIVIAL(FMatrix, TransformPosition));
		Matrix.Method("FVector4 TransformVector(const FVector& Vector) const", METHOD_TRIVIAL(FMatrix, TransformVector));
		Matrix.Method("FVector InverseTransformPosition(const FVector& Position) const", METHOD_TRIVIAL(FMatrix, InverseTransformPosition));
		Matrix.Method("FVector InverseTransformVector(const FVector& Vector) const", METHOD_TRIVIAL(FMatrix, InverseTransformVector));
		Matrix.Method("void SetIdentity()", METHOD_TRIVIAL(FMatrix, SetIdentity));
		Matrix.Method("void SetOrigin(const FVector& Origin)", METHOD_TRIVIAL(FMatrix, SetOrigin));
		Matrix.Method("FVector GetOrigin() const", METHOD_TRIVIAL(FMatrix, GetOrigin));
		Matrix.Method("void GetScaledAxes(FVector&out X, FVector&out Y, FVector&out Z) const", METHOD_TRIVIAL(FMatrix, GetScaledAxes));
		Matrix.Method("void GetUnitAxes(FVector&out X, FVector&out Y, FVector&out Z) const", METHOD_TRIVIAL(FMatrix, GetUnitAxes));
		Matrix.Method("void SetAxis(int32 Index, const FVector& Axis)", METHOD_TRIVIAL(FMatrix, SetAxis));
		Matrix.Method("void SetColumn(int32 Index, const FVector& Column)", METHOD_TRIVIAL(FMatrix, SetColumn));
		Matrix.Method("float64 GetMaximumAxisScale() const", METHOD_TRIVIAL(FMatrix, GetMaximumAxisScale));
		Matrix.Method("float64 GetMinimumAxisScale() const", METHOD_TRIVIAL(FMatrix, GetMinimumAxisScale));
		Matrix.Method("float64 Determinant() const", METHOD_TRIVIAL(FMatrix, Determinant));
		Matrix.Method("float64 RotDeterminant() const", METHOD_TRIVIAL(FMatrix, RotDeterminant));
		Matrix.Method("FMatrix Inverse() const", METHOD_TRIVIAL(FMatrix, Inverse));
		Matrix.Method("FMatrix InverseFast() const", METHOD_TRIVIAL(FMatrix, InverseFast));
		Matrix.Method("FMatrix GetTransposed() const", METHOD_TRIVIAL(FMatrix, GetTransposed));
		Matrix.Method("bool ContainsNaN() const", METHOD_TRIVIAL(FMatrix, ContainsNaN));
		Matrix.Method("uint32 ComputeHash() const", METHOD_TRIVIAL(FMatrix, ComputeHash));
		Matrix.Method("bool GetFrustumNearPlane(FPlane&out OutPlane) const", METHOD_TRIVIAL(FMatrix, GetFrustumNearPlane));
		Matrix.Method("bool GetFrustumFarPlane(FPlane&out OutPlane) const", METHOD_TRIVIAL(FMatrix, GetFrustumFarPlane));
		Matrix.Method("bool GetFrustumLeftPlane(FPlane&out OutPlane) const", METHOD_TRIVIAL(FMatrix, GetFrustumLeftPlane));
		Matrix.Method("bool GetFrustumRightPlane(FPlane&out OutPlane) const", METHOD_TRIVIAL(FMatrix, GetFrustumRightPlane));
		Matrix.Method("bool GetFrustumTopPlane(FPlane&out OutPlane) const", METHOD_TRIVIAL(FMatrix, GetFrustumTopPlane));
		Matrix.Method("bool GetFrustumBottomPlane(FPlane&out OutPlane) const", METHOD_TRIVIAL(FMatrix, GetFrustumBottomPlane));
		Matrix.Method("FQuat ToQuat() const", METHOD_TRIVIAL(FMatrix, ToQuat));
		Matrix.Method("FRotator Rotator() const", METHOD_TRIVIAL(FMatrix, Rotator));
		Matrix.Method("FString ToString() const", METHOD_TRIVIAL(FMatrix, ToString));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FMatrix");
		Binds.BindGlobalFunctionForTarget("FMatrix Identity()", &FAngelscriptFMatrixBinds::Identity);
		Binds.BindGlobalFunctionForTarget("FMatrix Zero()", &FAngelscriptFMatrixBinds::Zero);
	});
