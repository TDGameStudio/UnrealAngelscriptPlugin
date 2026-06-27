#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathStructsMissingTests
// -----------------------------------------------------------------------------
// Coverage for missing math struct methods identified in Coverage_MathStructs.md
// This file tests methods marked as ⬜ in the coverage matrix.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMathStructsMissingTest,
	"Angelscript.TestModule.Coverage.MathStructsMissing",
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

	// Helper for FVector with tolerance
	void ExpectVectorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message, double Tolerance = 0.001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FVector Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// Helper for FRotator with tolerance
	void ExpectRotatorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FRotator& Expected, const TCHAR* Message, double Tolerance = 0.001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FRotator Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// -------------------------------------------------------------------------
	// FVector methods: Size, Normalize, Distance, Lerp, ClampSize, ProjectOnTo, RotateAngleAxis
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorAdvancedMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathMissing_FVectorMethods", ASTEST_AS(R"AS(
		float TestSize()
		{
			FVector v = FVector(3, 4, 0);
			return v.Size();
		}

		float TestSizeSquared()
		{
			FVector v = FVector(3, 4, 0);
			return v.SizeSquared();
		}

		FVector TestGetSafeNormal()
		{
			FVector v = FVector(10, 0, 0);
			return v.GetSafeNormal();
		}

		bool TestNormalize()
		{
			FVector v = FVector(10, 0, 0);
			return v.Normalize();
		}

		bool TestIsNormalized()
		{
			FVector v = FVector(1, 0, 0);
			return v.IsNormalized();
		}

		float TestDistance()
		{
			FVector a = FVector(0, 0, 0);
			FVector b = FVector(3, 4, 0);
			return a.Distance(b);
		}

		float TestDistSquared()
		{
			FVector a = FVector(0, 0, 0);
			FVector b = FVector(3, 4, 0);
			return a.DistSquared(b);
		}

		FVector TestLerp()
		{
			FVector a = FVector(0, 0, 0);
			FVector b = FVector(10, 10, 10);
			return Math::Lerp(a, b, 0.5f);
		}

		FVector TestGetClampedToSize()
		{
			FVector v = FVector(10, 0, 0);
			return v.GetClampedToSize(2.0, 5.0);
		}

		FVector TestGetClampedToMaxSize()
		{
			FVector v = FVector(10, 0, 0);
			return v.GetClampedToMaxSize(5.0);
		}

		FVector TestProjectOnTo()
		{
			FVector v = FVector(3, 3, 0);
			FVector target = FVector(1, 0, 0);
			return v.ProjectOnTo(target);
		}

		FVector TestProjectOnToNormal()
		{
			FVector v = FVector(3, 3, 0);
			FVector normal = FVector(1, 0, 0);
			return v.ProjectOnToNormal(normal);
		}

		FVector TestRotateAngleAxis()
		{
			FVector v = FVector(1, 0, 0);
			return v.RotateAngleAxis(90.0, FVector(0, 0, 1));
		}

		bool TestIsNearlyZero()
		{
			FVector v = FVector(0.0001, 0.0001, 0.0001);
			return v.IsNearlyZero();
		}

		bool TestIsZero()
		{
			FVector v = FVector::ZeroVector;
			return v.IsZero();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSize()"), 5.0f, TEXT("FVector.Size()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSizeSquared()"), 25.0f, TEXT("FVector.SizeSquared()"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestGetSafeNormal()"), FVector(1, 0, 0), TEXT("FVector.GetSafeNormal()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestNormalize()"), true, TEXT("FVector.Normalize() returns true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNormalized()"), true, TEXT("FVector.IsNormalized()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDistance()"), 5.0f, TEXT("FVector.Distance()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDistSquared()"), 25.0f, TEXT("FVector.DistSquared()"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestLerp()"), FVector(5, 5, 5), TEXT("Math::Lerp for FVector"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestGetClampedToSize()"), FVector(5, 0, 0), TEXT("FVector.GetClampedToSize()"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestGetClampedToMaxSize()"), FVector(5, 0, 0), TEXT("FVector.GetClampedToMaxSize()"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestProjectOnTo()"), FVector(3, 0, 0), TEXT("FVector.ProjectOnTo()"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestProjectOnToNormal()"), FVector(3, 0, 0), TEXT("FVector.ProjectOnToNormal()"));

		// RotateAngleAxis: (1,0,0) rotated 90 degrees around Z axis = (0,1,0)
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestRotateAngleAxis()"), FVector(0, 1, 0), TEXT("FVector.RotateAngleAxis()"), 0.01);

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNearlyZero()"), true, TEXT("FVector.IsNearlyZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsZero()"), true, TEXT("FVector.IsZero()"));
	}

	// -------------------------------------------------------------------------
	// FRotator methods: Lerp, Clamp, Normalize
	// -------------------------------------------------------------------------
	TEST_METHOD(FRotatorAdvancedMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathMissing_FRotatorMethods", ASTEST_AS(R"AS(
		FRotator TestLerp()
		{
			FRotator a = FRotator(0, 0, 0);
			FRotator b = FRotator(90, 90, 90);
			return Math::Lerp(a, b, 0.5f);
		}

		FRotator TestClamp()
		{
			FRotator r = FRotator(100, 200, 100);
			return r.Clamp();
		}

		FRotator TestGetNormalized()
		{
			FRotator r = FRotator(400, 720, -400);
			return r.GetNormalized();
		}

		void TestNormalize(FRotator&out result)
		{
			FRotator r = FRotator(400, 720, -400);
			r.Normalize();
			result = r;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Lerp
		{
			FRotator a(0, 0, 0);
			FRotator b(90, 90, 90);
			FRotator Expected = FMath::Lerp(a, b, 0.5f);
			ExpectRotatorNearlyEqual(Engine, Module, TEXT("FRotator TestLerp()"), Expected, TEXT("Math::Lerp for FRotator"));
		}

		// Clamp
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator TestClamp()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FRotator.Clamp() pitch in range"), Result.Pitch >= -90.0 && Result.Pitch <= 90.0);
		}

		// GetNormalized
		{
			FRotator r(400, 720, -400);
			FRotator Expected = r.GetNormalized();
			ExpectRotatorNearlyEqual(Engine, Module, TEXT("FRotator TestGetNormalized()"), Expected, TEXT("FRotator.GetNormalized()"));
		}

		// Normalize (modifies in place)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void TestNormalize(FRotator&out)"));
			FRotator Result;
			Invoker.AddArgRef(Result);
			Invoker.Execute();
			FRotator r(400, 720, -400);
			FRotator Expected = r.GetNormalized();
			TestRunner->TestTrue(TEXT("FRotator.Normalize()"), Result.Equals(Expected, 0.001));
		}
	}

	// -------------------------------------------------------------------------
	// FTransform methods: Lerp, Blend, ToMatrix, TransformPosition
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformAdvancedMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathMissing_FTransformMethods", ASTEST_AS(R"AS(
		FTransform TestLerp()
		{
			FTransform a = FTransform(FVector(0, 0, 0));
			FTransform b = FTransform(FVector(100, 100, 100));
			return Math::Lerp(a, b, 0.5f);
		}

		FTransform TestBlend()
		{
			FTransform result = FTransform::Identity;
			FTransform a = FTransform(FVector(0, 0, 0));
			FTransform b = FTransform(FVector(100, 0, 0));
			result.Blend(a, b, 0.5f);
			return result;
		}

		FTransform TestBlendWith()
		{
			FTransform a = FTransform(FVector(0, 0, 0));
			FTransform b = FTransform(FVector(100, 0, 0));
			a.BlendWith(b, 0.5f);
			return a;
		}

		FVector TestTransformPosition()
		{
			FTransform t = FTransform(FVector(100, 0, 0));
			FVector point = FVector(10, 0, 0);
			return t.TransformPosition(point);
		}

		FVector TestTransformVector()
		{
			FTransform t = FTransform(FQuat::Identity, FVector(100, 0, 0), FVector(2, 2, 2));
			FVector vec = FVector(10, 0, 0);
			return t.TransformVector(vec);
		}

		FTransform TestInverse()
		{
			FTransform t = FTransform(FVector(100, 200, 300));
			return t.Inverse();
		}

		FVector TestInverseTransformPosition()
		{
			FTransform t = FTransform(FVector(100, 0, 0));
			FVector worldPoint = FVector(110, 0, 0);
			return t.InverseTransformPosition(worldPoint);
		}

		FVector TestInverseTransformVector()
		{
			FTransform t = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 2, 2));
			FVector worldVec = FVector(20, 0, 0);
			return t.InverseTransformVector(worldVec);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Lerp
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestLerp()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("Math::Lerp for FTransform"), Result.GetLocation().Equals(FVector(50, 50, 50), 0.01));
		}

		// Blend
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestBlend()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.Blend()"), Result.GetLocation().Equals(FVector(50, 0, 0), 0.01));
		}

		// BlendWith
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestBlendWith()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform.BlendWith()"), Result.GetLocation().Equals(FVector(50, 0, 0), 0.01));
		}

		// TransformPosition
		{
			FTransform t(FVector(100, 0, 0));
			FVector Expected = t.TransformPosition(FVector(10, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestTransformPosition()"), Expected, TEXT("FTransform.TransformPosition()"));
		}

		// TransformVector
		{
			FTransform t(FQuat::Identity, FVector(100, 0, 0), FVector(2, 2, 2));
			FVector Expected = t.TransformVector(FVector(10, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestTransformVector()"), Expected, TEXT("FTransform.TransformVector()"));
		}

		// Inverse
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform TestInverse()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FTransform t(FVector(100, 200, 300));
			FTransform Expected = t.Inverse();
			TestRunner->TestTrue(TEXT("FTransform.Inverse()"), Result.Equals(Expected, 0.01));
		}

		// InverseTransformPosition
		{
			FTransform t(FVector(100, 0, 0));
			FVector Expected = t.InverseTransformPosition(FVector(110, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestInverseTransformPosition()"), Expected, TEXT("FTransform.InverseTransformPosition()"));
		}

		// InverseTransformVector
		{
			FTransform t(FQuat::Identity, FVector::ZeroVector, FVector(2, 2, 2));
			FVector Expected = t.InverseTransformVector(FVector(20, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TestInverseTransformVector()"), Expected, TEXT("FTransform.InverseTransformVector()"));
		}
	}

	// -------------------------------------------------------------------------
	// FVector2D operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FVector2DOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathMissing_FVector2D", ASTEST_AS(R"AS(
		FVector2D TestConstruction()
		{
			return FVector2D(3.0, 4.0);
		}

		FVector2D TestAddition()
		{
			FVector2D a = FVector2D(1, 2);
			FVector2D b = FVector2D(3, 4);
			return a + b;
		}

		float TestSize()
		{
			FVector2D v = FVector2D(3, 4);
			return v.Size();
		}

		FVector2D TestNormalize()
		{
			FVector2D v = FVector2D(10, 0);
			return v.GetSafeNormal();
		}

		float TestDot()
		{
			FVector2D a = FVector2D(1, 0);
			FVector2D b = FVector2D(0, 1);
			return a | b;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D TestConstruction()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector2D construction"), Result, FVector2D(3, 4));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D TestAddition()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector2D addition"), Result, FVector2D(4, 6));
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSize()"), 5.0f, TEXT("FVector2D.Size()"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D TestNormalize()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FVector2D.GetSafeNormal()"), Result.Equals(FVector2D(1, 0), 0.001));
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDot()"), 0.0f, TEXT("FVector2D dot product"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
