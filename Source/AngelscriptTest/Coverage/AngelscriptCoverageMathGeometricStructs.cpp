#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/Function.h"

#include <type_traits>

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathGeometricStructs
// -----------------------------------------------------------------------------
// Coverage for geometric structures identified in Coverage_MathStructs.md:
// - FBox: AABB bounding box operations
// - FBox2D: unbound type boundary
// - FPlane: Plane operations
// - FTransform: Construction patterns not yet covered
//
// Test patterns: Pattern B (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMathGeometricStructsTest,
	"Angelscript.TestModule.Coverage.MathGeometricStructs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// Helper
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math geometric module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math geometric global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}
		T Result{};
		if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
			|| std::is_same_v<T, float>
			|| std::is_same_v<T, double>)
		{
			Result = Invoker.ExecuteAndGet<T>(T{});
		}
		else
		{
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		}
		if constexpr (std::is_floating_point_v<T>)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Expected, Result, static_cast<T>(0.001)), Message));
		}
		else
		{
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}
	}

	template <typename T>
	void ExpectGlobalStructSatisfies(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, TFunctionRef<bool(const T&)> Predicate, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math geometric module should compile before extracting struct")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math geometric struct function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		T Result{};
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		ASSERT_THAT(IsTrue(Predicate(Result), Message));
	}

	// -------------------------------------------------------------------------
	// FTransform construction patterns
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FTransformConstruct", ASTEST_AS(R"AS(
		FTransform TestDefaultConstruction()
		{
			return FTransform();
		}

		FTransform TestIdentity()
		{
			return FTransform::Identity;
		}

		FTransform TestLocationOnly()
		{
			return FTransform(FVector(100, 200, 300));
		}

		FTransform TestFullConstruction()
		{
			FQuat rot = FQuat(FRotator(0, 90, 0));
			FVector loc = FVector(100, 200, 300);
			FVector scale = FVector(2, 2, 2);
			return FTransform(rot, loc, scale);
		}

		FVector TestGetLocation()
		{
			FTransform t = FTransform(FVector(100, 200, 300));
			return t.Location;
		}

		FQuat TestGetRotation()
		{
			FQuat rot = FQuat(FRotator(0, 90, 0));
			FTransform t = FTransform(rot, FVector::ZeroVector, FVector(1,1,1));
			return t.Rotation;
		}

		FVector TestGetScale()
		{
			FTransform t = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 3, 4));
			return t.Scale3D;
		}

		FTransform TestSetLocation()
		{
			FTransform t = FTransform::Identity;
			t.Location = FVector(50, 100, 150);
			return t;
		}

		FTransform TestSetScale()
		{
			FTransform t = FTransform::Identity;
			t.Scale3D = FVector(3, 3, 3);
			return t;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestDefaultConstruction()"),
			[](const FTransform& Result) { return Result.Equals(FTransform::Identity, 0.001); },
			TEXT("FTransform default construction"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestIdentity()"),
			[](const FTransform& Result) { return Result.Equals(FTransform::Identity, 0.001); },
			TEXT("FTransform::Identity"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestLocationOnly()"),
			[](const FTransform& Result) { return Result.GetLocation().Equals(FVector(100, 200, 300), 0.001); },
			TEXT("FTransform location-only construction"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestFullConstruction()"),
			[](const FTransform& Result) { return Result.GetLocation().Equals(FVector(100, 200, 300), 0.001) && Result.GetScale3D().Equals(FVector(2, 2, 2), 0.001); },
			TEXT("FTransform full construction"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetLocation()"),
			[](const FVector& Result) { return Result.Equals(FVector(100, 200, 300), 0.001); },
			TEXT("FTransform.Location accessor"));
		ExpectGlobalStructSatisfies<FQuat>(
			Engine,
			Module,
			TEXT("FQuat TestGetRotation()"),
			[](const FQuat& Result) { return !Result.Equals(FQuat::Identity, 0.001); },
			TEXT("FTransform.Rotation accessor"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetScale()"),
			[](const FVector& Result) { return Result.Equals(FVector(2, 3, 4), 0.001); },
			TEXT("FTransform.Scale3D accessor"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestSetLocation()"),
			[](const FTransform& Result) { return Result.GetLocation().Equals(FVector(50, 100, 150), 0.001); },
			TEXT("FTransform.Location setter"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestSetScale()"),
			[](const FTransform& Result) { return Result.GetScale3D().Equals(FVector(3, 3, 3), 0.001); },
			TEXT("FTransform.Scale3D setter"));
	}

	TEST_METHOD(FTransformOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FTransformOperations", ASTEST_AS(R"AS(
		FVector TestMultiplyTransformLocation()
		{
			FTransform First = FTransform(FVector(10, 0, 0));
			FTransform Second = FTransform(FVector(0, 20, 0));
			FTransform Combined = First * Second;
			return Combined.GetLocation();
		}

		FVector TestTransformPosition()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			return Transform.TransformPosition(FVector(1, 2, 3));
		}

		FVector TestTransformVector()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 3, 4));
			return Transform.TransformVector(FVector(1, 2, 3));
		}

		FVector TestInverseTransformPosition()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			return Transform.InverseTransformPosition(FVector(12, 24, 36));
		}

		FVector TestScaleTranslationAndAdd()
		{
			FTransform Transform = FTransform(FVector(10, 20, 30));
			Transform.ScaleTranslation(2.0);
			Transform.AddToTranslation(FVector(1, 2, 3));
			return Transform.GetTranslation();
		}

		FVector TestBlendLocation()
		{
			FTransform A = FTransform(FVector(0, 0, 0));
			FTransform B = FTransform(FVector(10, 20, 30));
			FTransform Result;
			Result.Blend(A, B, 0.5f);
			return Result.GetTranslation();
		}

		bool TestEqualsNoScale()
		{
			FTransform A = FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(1, 1, 1));
			FTransform B = FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(4, 5, 6));
			return A.EqualsNoScale(B, 0.001);
		}

		bool TestValidityHelpers()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(2, 2, 2));
			return Transform.IsValid() && !Transform.ContainsNaN();
		}

		float TestAxisScale()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 5, 3));
			return Transform.GetMaximumAxisScale() + Transform.GetMinimumAxisScale();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform operations module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		auto ExpectVectorReturn = [this, &Engine, Module](const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message)
		{
			ExpectGlobalStructSatisfies<FVector>(
				Engine,
				Module,
				Declaration,
				[Expected](const FVector& Result) { return Result.Equals(Expected, 0.001); },
				Message);
		};

		ExpectVectorReturn(TEXT("FVector TestMultiplyTransformLocation()"), FVector(10, 20, 0), TEXT("FTransform multiplication should combine translations"));
		ExpectVectorReturn(TEXT("FVector TestTransformPosition()"), FVector(12, 24, 36), TEXT("FTransform.TransformPosition() should apply scale and translation"));
		ExpectVectorReturn(TEXT("FVector TestTransformVector()"), FVector(2, 6, 12), TEXT("FTransform.TransformVector() should apply scale without translation"));
		ExpectVectorReturn(TEXT("FVector TestInverseTransformPosition()"), FVector(1, 2, 3), TEXT("FTransform.InverseTransformPosition() should reverse TransformPosition"));
		ExpectVectorReturn(TEXT("FVector TestScaleTranslationAndAdd()"), FVector(21, 42, 63), TEXT("ScaleTranslation and AddToTranslation should mutate translation"));
		ExpectVectorReturn(TEXT("FVector TestBlendLocation()"), FVector(5, 10, 15), TEXT("FTransform.Blend() should interpolate translation"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEqualsNoScale()"), true, TEXT("FTransform.EqualsNoScale() should ignore scale"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestValidityHelpers()"), true, TEXT("FTransform validity helpers"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAxisScale()"), 7.0f, TEXT("FTransform axis scale helpers"));
	}

	TEST_METHOD(FMatrixUnsupportedBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString UnsupportedMethodSource = ASTEST_AS(R"AS(
		FMatrix TriggerUnsupportedMatrixReturn()
		{
			return FTransform::Identity.ToMatrixWithScale();
		}
		)AS");
		TArray<FString> UnsupportedMethodDiagnostics;
		UnsupportedMethodDiagnostics.Add(TEXT("FMatrix"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovMathGeom_FMatrixMethodUnsupported"),
			UnsupportedMethodSource,
			TEXT("FTransform matrix-return APIs should remain explicit compile-failure boundaries until FMatrix is registered"),
			MakeArrayView(UnsupportedMethodDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// FBox: AABB bounding box operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FBoxOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FBox", ASTEST_AS(R"AS(
		FBox TestConstruction()
		{
			FVector min = FVector(0, 0, 0);
			FVector max = FVector(100, 100, 100);
			return FBox(min, max);
		}

		FBox TestBuildAABB()
		{
			return FBox::BuildAABB(FVector(50, 50, 50), FVector(50, 50, 50));
		}

		bool TestIsInside()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			FVector point = FVector(50, 50, 50);
			return box.IsInside(point);
		}

		bool TestIsInsideOutside()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			FVector point = FVector(200, 200, 200);
			return box.IsInside(point);
		}

		FVector TestGetCenter()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.GetCenter();
		}

		FVector TestGetExtent()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.GetExtent();
		}

		FVector TestGetSize()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.Max - box.Min;
		}

		float TestGetVolume()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			return box.GetVolume();
		}

		FBox TestExpandBy()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.ExpandBy(10.0);
		}

		FBox TestPlusOperator()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			FVector point = FVector(200, 200, 200);
			return box + point;
		}

		bool TestIsValid()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.IsValid;
		}

		bool TestIntersectAndOverlap()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FBox other = FBox(FVector(5, 5, 5), FVector(20, 20, 20));
			FBox overlap = box.Overlap(other);
			return box.Intersect(other)
				&& box.IntersectXY(other)
				&& overlap.Min.Equals(FVector(5, 5, 5), 0.001)
				&& overlap.Max.Equals(FVector(10, 10, 10), 0.001);
		}

		bool TestInsideBoxAndBoundaryVariants()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FBox inner = FBox(FVector(2, 2, 2), FVector(8, 8, 8));
			FVector boundary = FVector(10, 5, 5);
			FVector outsideZ = FVector(5, 5, 20);
			return box.IsInside(inner)
				&& !box.IsInside(boundary)
				&& box.IsInsideOrOn(boundary)
				&& box.IsInsideXY(outsideZ)
				&& box.IsInsideOrOnXY(FVector(10, 5, 20));
		}

		bool TestGetCenterAndExtentsOutParams()
		{
			FBox box = FBox(FVector(-2, -4, -6), FVector(6, 8, 10));
			FVector center;
			FVector extents;
			box.GetCenterAndExtents(center, extents);
			return center.Equals(FVector(2, 2, 2), 0.001)
				&& extents.Equals(FVector(4, 6, 8), 0.001);
		}

		bool TestClosestShiftMoveAndVectorExpand()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FBox expanded = box.ExpandBy(FVector(1, 2, 3));
			FBox shifted = box.ShiftBy(FVector(5, 0, 0));
			FBox moved = box.MoveTo(FVector(100, 100, 100));
			FVector closest = box.GetClosestPointTo(FVector(20, 5, -5));
			return expanded.Min.Equals(FVector(-1, -2, -3), 0.001)
				&& expanded.Max.Equals(FVector(11, 12, 13), 0.001)
				&& shifted.Min.Equals(FVector(5, 0, 0), 0.001)
				&& shifted.Max.Equals(FVector(15, 10, 10), 0.001)
				&& moved.GetCenter().Equals(FVector(100, 100, 100), 0.001)
				&& closest.Equals(FVector(10, 5, 0), 0.001);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestConstruction()"),
			[](const FBox& Result) { return Result.Min.Equals(FVector(0, 0, 0), 0.001) && Result.Max.Equals(FVector(100, 100, 100), 0.001); },
			TEXT("FBox construction"));
		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestBuildAABB()"),
			[](const FBox& Result) { return Result.Min.Equals(FVector(0, 0, 0), 0.001) && Result.Max.Equals(FVector(100, 100, 100), 0.001); },
			TEXT("FBox BuildAABB"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInside()"), true, TEXT("FBox.IsInside() point inside"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInsideOutside()"), false, TEXT("FBox.IsInside() point outside"));

		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetCenter()"),
			[](const FVector& Result) { return Result.Equals(FVector(50, 50, 50), 0.001); },
			TEXT("FBox.GetCenter()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetExtent()"),
			[](const FVector& Result) { return Result.Equals(FVector(50, 50, 50), 0.001); },
			TEXT("FBox.GetExtent()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetSize()"),
			[](const FVector& Result) { return Result.Equals(FVector(100, 100, 100), 0.001); },
			TEXT("FBox.GetSize()"));

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestGetVolume()"), 1000.0f, TEXT("FBox.GetVolume()"));

		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestExpandBy()"),
			[](const FBox& Result) { return (Result.Max - Result.Min).Equals(FVector(120, 120, 120), 0.001); },
			TEXT("FBox.ExpandBy()"));
		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestPlusOperator()"),
			[](const FBox& Result) { return Result.Max.Equals(FVector(200, 200, 200), 0.001); },
			TEXT("FBox + FVector operator"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsValid()"), true, TEXT("FBox.IsValid"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIntersectAndOverlap()"), true, TEXT("FBox Intersect/IntersectXY/Overlap"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestInsideBoxAndBoundaryVariants()"), true, TEXT("FBox IsInside variants should distinguish boundary and XY checks"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestGetCenterAndExtentsOutParams()"), true, TEXT("FBox.GetCenterAndExtents() out params"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestClosestShiftMoveAndVectorExpand()"), true, TEXT("FBox closest/shift/move/vector ExpandBy helpers"));
	}

	TEST_METHOD(FBox2DUnsupportedBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString UnsupportedSource = ASTEST_AS(R"AS(
		bool TriggerUnsupportedFBox2D()
		{
			FBox2D Box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return Box.IsInside(FVector2D(50, 50));
		}
		)AS");
		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("FBox2D"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovMathGeom_FBox2DUnsupported"),
			UnsupportedSource,
			TEXT("FBox2D should remain an explicit compile-failure boundary until a runtime bind exists"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// FPlane: Plane operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FPlaneOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FPlane", ASTEST_AS(R"AS(
		FPlane TestConstruction()
		{
			FVector normal = FVector(0, 0, 1);
			FVector location = FVector(0, 0, 10);
			return FPlane(location, normal);
		}

		FPlane TestConstructionFromPoints()
		{
			FVector a = FVector(0, 0, 0);
			FVector b = FVector(1, 0, 0);
			FVector c = FVector(0, 1, 0);
			return FPlane(a, b, c);
		}

		FPlane TestConstructionFromVector()
		{
			return FPlane(FVector::ZeroVector, FVector(0, 0, 1));
		}

		float TestPlaneDot()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			FVector point = FVector(0, 0, 10);
			return plane.PlaneDot(point);
		}

		FVector TestGetNormal()
		{
			FPlane plane = FPlane(FVector(0, 0, 10), FVector(0, 0, 2));
			return plane.GetNormal();
		}

		FVector TestGetOrigin()
		{
			FPlane plane = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));
			return plane.GetOrigin();
		}

		bool TestEquals()
		{
			FPlane a = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));
			FPlane b = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));
			return a == b;
		}

		bool TestRayPlaneIntersection()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			FVector intersection = plane.RayPlaneIntersection(FVector(0, 0, -5), FVector(0, 0, 1));
			return intersection.Equals(FVector::ZeroVector, 0.001);
		}

		bool TestSegmentPlaneIntersection()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			FVector intersection;
			bool hit = plane.SegmentPlaneIntersection(FVector(0, 0, -5), FVector(0, 0, 5), intersection);
			return hit && intersection.Equals(FVector::ZeroVector, 0.001);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FPlane>(
			Engine,
			Module,
			TEXT("FPlane TestConstruction()"),
			[](const FPlane& Result) { return Result.Z > 0.9f && FMath::Abs(Result.W - 10.0f) < 0.1f; },
			TEXT("FPlane construction"));
		ExpectGlobalStructSatisfies<FPlane>(
			Engine,
			Module,
			TEXT("FPlane TestConstructionFromPoints()"),
			[](const FPlane& Result) { return Result.Z > 0.9f || Result.Z < -0.9f; },
			TEXT("FPlane construction from points"));
		ExpectGlobalStructSatisfies<FPlane>(
			Engine,
			Module,
			TEXT("FPlane TestConstructionFromVector()"),
			[](const FPlane& Result) { return Result.Z > 0.9f; },
			TEXT("FPlane construction from vector"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestPlaneDot()"), 10.0f, TEXT("FPlane.PlaneDot()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetNormal()"),
			[](const FVector& Result) { return Result.Equals(FVector(0, 0, 1), 0.001); },
			TEXT("FPlane.GetNormal()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetOrigin()"),
			[](const FVector& Result) { return Result.Equals(FVector(0, 0, 10), 0.001); },
			TEXT("FPlane.GetOrigin()"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FPlane equality"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestRayPlaneIntersection()"), true, TEXT("FPlane.RayPlaneIntersection()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestSegmentPlaneIntersection()"), true, TEXT("FPlane.SegmentPlaneIntersection() out param"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
