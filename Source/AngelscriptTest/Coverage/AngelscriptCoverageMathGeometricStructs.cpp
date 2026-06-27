#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathGeometricStructs
// -----------------------------------------------------------------------------
// Coverage for geometric structures identified in Coverage_MathStructs.md:
// - FBox: AABB bounding box operations
// - FBox2D: 2D bounding box operations
// - FPlane: Plane operations
// - FTransform: Construction patterns not yet covered
//
// Test patterns: Pattern B (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

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
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
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
		TestRunner->TestEqual(Message, Result, Expected);
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

		// Default construction
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestDefaultConstruction()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform default construction"), Result.Equals(FTransform::Identity, 0.001));
		}

		// Identity
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestIdentity()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform::Identity"), Result.Equals(FTransform::Identity, 0.001));
		}

		// Location only
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestLocationOnly()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform location-only construction"), Result.GetLocation().Equals(FVector(100, 200, 300), 0.001));
		}

		// Full construction
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestFullConstruction()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform full construction location"), Result.GetLocation().Equals(FVector(100, 200, 300), 0.001));
			TestRunner->TestTrue(TEXT("FTransform full construction scale"), Result.GetScale3D().Equals(FVector(2, 2, 2), 0.001));
		}

		// GetLocation
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestGetLocation()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.Location accessor"), Result.Equals(FVector(100, 200, 300), 0.001));
		}

		// GetRotation
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat TestGetRotation()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.Rotation accessor"), !Result.Equals(FQuat::Identity, 0.001));
		}

		// GetScale
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestGetScale()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.Scale3D accessor"), Result.Equals(FVector(2, 3, 4), 0.001));
		}

		// SetLocation
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestSetLocation()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.Location setter"), Result.GetLocation().Equals(FVector(50, 100, 150), 0.001));
		}

		// SetScale
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestSetScale()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.Scale3D setter"), Result.GetScale3D().Equals(FVector(3, 3, 3), 0.001));
		}
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

		FBox TestConstructionFromPoints()
		{
			TArray<FVector> points;
			points.Add(FVector(0, 0, 0));
			points.Add(FVector(100, 100, 100));
			points.Add(FVector(50, 50, 50));
			return FBox(points);
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
			return box.GetSize();
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
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Construction
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox TestConstruction()"));
			FBox Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox construction"), Result.Min.Equals(FVector(0, 0, 0), 0.001) && Result.Max.Equals(FVector(100, 100, 100), 0.001));
		}

		// Construction from points
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox TestConstructionFromPoints()"));
			FBox Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox construction from points"), Result.IsValid != 0);
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInside()"), true, TEXT("FBox.IsInside() point inside"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInsideOutside()"), false, TEXT("FBox.IsInside() point outside"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestGetCenter()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox.GetCenter()"), Result.Equals(FVector(50, 50, 50), 0.001));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestGetExtent()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox.GetExtent()"), Result.Equals(FVector(50, 50, 50), 0.001));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestGetSize()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox.GetSize()"), Result.Equals(FVector(100, 100, 100), 0.001));
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestGetVolume()"), 1000.0f, TEXT("FBox.GetVolume()"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox TestExpandBy()"));
			FBox Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox.ExpandBy()"), Result.GetSize().Equals(FVector(120, 120, 120), 0.001));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox TestPlusOperator()"));
			FBox Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox + FVector operator"), Result.Max.Equals(FVector(200, 200, 200), 0.001));
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsValid()"), true, TEXT("FBox.IsValid"));
	}

	// -------------------------------------------------------------------------
	// FBox2D: 2D bounding box operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FBox2DOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FBox2D", ASTEST_AS(R"AS(
		FBox2D TestConstruction()
		{
			FVector2D min = FVector2D(0, 0);
			FVector2D max = FVector2D(100, 100);
			return FBox2D(min, max);
		}

		FBox2D TestConstructionFromPoints()
		{
			TArray<FVector2D> points;
			points.Add(FVector2D(0, 0));
			points.Add(FVector2D(100, 100));
			points.Add(FVector2D(50, 50));
			return FBox2D(points);
		}

		bool TestIsInside()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			FVector2D point = FVector2D(50, 50);
			return box.IsInside(point);
		}

		bool TestIsInsideOutside()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			FVector2D point = FVector2D(200, 200);
			return box.IsInside(point);
		}

		FVector2D TestGetCenter()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return box.GetCenter();
		}

		FVector2D TestGetExtent()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return box.GetExtent();
		}

		FVector2D TestGetSize()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return box.GetSize();
		}

		float TestGetArea()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(10, 10));
			return box.GetArea();
		}

		FBox2D TestExpandBy()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return box.ExpandBy(10.0);
		}

		bool TestIsValid()
		{
			FBox2D box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return box.bIsValid;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Construction
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox2D TestConstruction()"));
			FBox2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox2D construction"), Result.Min.Equals(FVector2D(0, 0), 0.001) && Result.Max.Equals(FVector2D(100, 100), 0.001));
		}

		// Construction from points
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox2D TestConstructionFromPoints()"));
			FBox2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox2D construction from points"), Result.bIsValid);
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInside()"), true, TEXT("FBox2D.IsInside() point inside"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInsideOutside()"), false, TEXT("FBox2D.IsInside() point outside"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D TestGetCenter()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox2D.GetCenter()"), Result.Equals(FVector2D(50, 50), 0.001));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D TestGetExtent()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox2D.GetExtent()"), Result.Equals(FVector2D(50, 50), 0.001));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D TestGetSize()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox2D.GetSize()"), Result.Equals(FVector2D(100, 100), 0.001));
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestGetArea()"), 100.0f, TEXT("FBox2D.GetArea()"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox2D TestExpandBy()"));
			FBox2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FBox2D.ExpandBy()"), Result.GetSize().Equals(FVector2D(120, 120), 0.001));
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsValid()"), true, TEXT("FBox2D.bIsValid"));
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
			float distance = 10.0;
			return FPlane(normal, distance);
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
			return FPlane(FVector(0, 0, 1), 0);
		}

		float TestPlaneDot()
		{
			FPlane plane = FPlane(FVector(0, 0, 1), 0);
			FVector point = FVector(0, 0, 10);
			return plane.PlaneDot(point);
		}

		FPlane TestNormalize()
		{
			FPlane plane = FPlane(FVector(0, 0, 2), 20);
			FPlane result = plane;
			result.Normalize();
			return result;
		}

		FPlane TestFlip()
		{
			FPlane plane = FPlane(FVector(0, 0, 1), 10);
			return plane.Flip();
		}

		bool TestEquals()
		{
			FPlane a = FPlane(FVector(0, 0, 1), 10);
			FPlane b = FPlane(FVector(0, 0, 1), 10);
			return a == b;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Construction
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FPlane TestConstruction()"));
			FPlane Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FPlane construction"), Result.Z > 0.9f && FMath::Abs(Result.W - 10.0f) < 0.1f);
		}

		// Construction from points
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FPlane TestConstructionFromPoints()"));
			FPlane Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FPlane construction from points"), Result.Z > 0.9f || Result.Z < -0.9f);
		}

		// Construction from vector
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FPlane TestConstructionFromVector()"));
			FPlane Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FPlane construction from vector"), Result.Z > 0.9f);
		}

		// PlaneDot
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestPlaneDot()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("FPlane.PlaneDot()"), FMath::Abs(Result - 10.0f) < 0.1f);
		}

		// Normalize
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FPlane TestNormalize()"));
			FPlane Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FPlane.Normalize()"), FMath::Abs(Result.Z - 1.0f) < 0.01f);
		}

		// Flip
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FPlane TestFlip()"));
			FPlane Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FPlane.Flip()"), Result.Z < -0.9f);
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FPlane equality"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
