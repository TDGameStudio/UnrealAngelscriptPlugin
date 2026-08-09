#include "CQTest.h"

#include "FunctionLibraries/AngelscriptMathLibrary.h"

#include <limits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptMathFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(WrapIndexSignedBoundaries)
	{
		ASSERT_THAT(AreEqual(2, UAngelscriptMathLibrary::WrapIndex(INT32_MIN, -2, 3)));
		ASSERT_THAT(AreEqual(2, UAngelscriptMathLibrary::WrapIndex(INT32_MAX, -2, 3)));
		ASSERT_THAT(AreEqual(2, UAngelscriptMathLibrary::WrapIndex(INT32_MIN, 3, -2)));
		ASSERT_THAT(AreEqual(7, UAngelscriptMathLibrary::WrapIndex(INT32_MAX, 7, 7)));
		ASSERT_THAT(AreEqual(INT32_MIN, UAngelscriptMathLibrary::WrapIndex(INT32_MAX, INT32_MIN, INT32_MAX)));
	}

	TEST_METHOD(WrapIndexUnsignedBoundaries)
	{
		ASSERT_THAT(AreEqual(uint32(5), UAngelscriptMathLibrary::WrapIndexUInt(0, 5, 10)));
		ASSERT_THAT(AreEqual(uint32(5), UAngelscriptMathLibrary::WrapIndexUInt(UINT32_MAX, 5, 10)));
		ASSERT_THAT(AreEqual(uint32(5), UAngelscriptMathLibrary::WrapIndexUInt(0, 10, 5)));
		ASSERT_THAT(AreEqual(uint32(7), UAngelscriptMathLibrary::WrapIndexUInt(UINT32_MAX, 7, 7)));
	}

	TEST_METHOD(VectorProjectedDistanceParity)
	{
		const FVector A(7.0, -2.0, 11.0);
		const FVector B(-3.0, 5.0, 4.0);
		const FVector UpX = FVector::XAxisVector;
		const FVector UpY = FVector::YAxisVector;
		const FVector UpDiagonal = FVector(1.0, 2.0, 3.0).GetSafeNormal();

		for (const FVector& Up : {UpX, UpY, UpDiagonal})
		{
			const FVector ProjectedA = FVector::VectorPlaneProject(A, Up);
			const FVector ProjectedB = FVector::VectorPlaneProject(B, Up);
			const double ExpectedSquared = FVector::DistSquared(ProjectedA, ProjectedB);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
				UAngelscriptFVectorMixinLibrary::DistSquared2D(A, B, Up), ExpectedSquared, 1.e-9)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
				UAngelscriptFVectorMixinLibrary::Dist2D(A, B, Up), FMath::Sqrt(ExpectedSquared), 1.e-9)));
		}
	}

	TEST_METHOD(Vector3fProjectedDistanceParity)
	{
		const FVector3f A(7.0f, -2.0f, 11.0f);
		const FVector3f B(-3.0f, 5.0f, 4.0f);
		const FVector3f UpX = FVector3f::XAxisVector;
		const FVector3f UpY = FVector3f::YAxisVector;
		const FVector3f UpDiagonal = FVector3f(1.0f, 2.0f, 3.0f).GetSafeNormal();

		for (const FVector3f& Up : {UpX, UpY, UpDiagonal})
		{
			const FVector3f ProjectedA = FVector3f::VectorPlaneProject(A, Up);
			const FVector3f ProjectedB = FVector3f::VectorPlaneProject(B, Up);
			const float ExpectedSquared = FVector3f::DistSquared(ProjectedA, ProjectedB);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
				UAngelscriptFVector3fMixinLibrary::DistSquared2D(A, B, Up), ExpectedSquared, 1.e-4f)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
				UAngelscriptFVector3fMixinLibrary::Dist2D(A, B, Up), FMath::Sqrt(ExpectedSquared), 1.e-4f)));
		}
	}

	TEST_METHOD(VectorAngularDistanceIsFinite)
	{
		const FVector ParallelA(2.0, 0.0, 0.0);
		const FVector ParallelB(4.0, 0.0, 0.0);
		const FVector Orthogonal(0.0, 3.0, 0.0);
		const FVector Opposite(-4.0, 0.0, 0.0);
		const FVector NearlyParallel(1.0, std::numeric_limits<double>::epsilon(), 0.0);

		ASSERT_THAT(IsTrue(FMath::IsNearlyZero(UAngelscriptFVectorMixinLibrary::AngularDistance(ParallelA, ParallelB), 1.e-9)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(UAngelscriptFVectorMixinLibrary::AngularDistance(ParallelA, Orthogonal), UE_DOUBLE_PI * 0.5, 1.e-9)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(UAngelscriptFVectorMixinLibrary::AngularDistance(ParallelA, Opposite), UE_DOUBLE_PI, 1.e-9)));
		ASSERT_THAT(IsTrue(FMath::IsFinite(UAngelscriptFVectorMixinLibrary::AngularDistance(ParallelA, NearlyParallel))));
		ASSERT_THAT(IsTrue(FMath::IsNearlyZero(UAngelscriptFVectorMixinLibrary::AngularDistance(FVector::ZeroVector, ParallelA))));
		ASSERT_THAT(IsTrue(FMath::IsFinite(UAngelscriptFVectorMixinLibrary::AngularDistanceForNormals(FVector(1.0 + UE_DOUBLE_SMALL_NUMBER, 0.0, 0.0), FVector::XAxisVector))));
	}

	TEST_METHOD(Vector3fAngularDistanceIsFinite)
	{
		const FVector3f ParallelA(2.0f, 0.0f, 0.0f);
		const FVector3f ParallelB(4.0f, 0.0f, 0.0f);
		const FVector3f Orthogonal(0.0f, 3.0f, 0.0f);
		const FVector3f Opposite(-4.0f, 0.0f, 0.0f);
		const FVector3f NearlyParallel(1.0f, std::numeric_limits<float>::epsilon(), 0.0f);

		ASSERT_THAT(IsTrue(FMath::IsNearlyZero(UAngelscriptFVector3fMixinLibrary::AngularDistance(ParallelA, ParallelB), 1.e-5f)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(UAngelscriptFVector3fMixinLibrary::AngularDistance(ParallelA, Orthogonal), UE_PI * 0.5f, 1.e-5f)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(UAngelscriptFVector3fMixinLibrary::AngularDistance(ParallelA, Opposite), UE_PI, 1.e-5f)));
		ASSERT_THAT(IsTrue(FMath::IsFinite(UAngelscriptFVector3fMixinLibrary::AngularDistance(ParallelA, NearlyParallel))));
		ASSERT_THAT(IsTrue(FMath::IsNearlyZero(UAngelscriptFVector3fMixinLibrary::AngularDistance(FVector3f::ZeroVector, ParallelA))));
		ASSERT_THAT(IsTrue(FMath::IsFinite(UAngelscriptFVector3fMixinLibrary::AngularDistanceForNormals(FVector3f(1.0f + UE_SMALL_NUMBER, 0.0f, 0.0f), FVector3f::XAxisVector))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
