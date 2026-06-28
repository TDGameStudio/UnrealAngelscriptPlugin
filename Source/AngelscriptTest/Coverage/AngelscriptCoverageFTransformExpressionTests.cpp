#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

#include <type_traits>

// -----------------------------------------------------------------------------
// AngelscriptCoverageFTransformExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FTransform *expression usage* -- operators, construction,
// methods, and transformation operations.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFTransformExpressionTest,
	"Angelscript.TestModule.Coverage.FTransformExpression",
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform expression module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("FTransform expression global function should resolve and prepare")));
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
		ASSERT_THAT(AreEqual(Expected, Result, Message));
	}

	// Helper for FVector with tolerance
	void ExpectVectorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform expression module should compile before executing vector function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("FTransform vector function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}
		FVector Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		ASSERT_THAT(IsTrue(Result.Equals(Expected, Tolerance), Message));
	}

	// Helper for FTransform with tolerance
	void ExpectTransformNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FTransform& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform expression module should compile before executing transform function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("FTransform function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}
		FTransform Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		ASSERT_THAT(IsTrue(Result.Equals(Expected, Tolerance), Message));
	}

	// -------------------------------------------------------------------------
	// FTransform construction: default, identity, location-only, full.
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_Construct", ASTEST_AS(R"AS(
		FTransform ConstructDefault()
		{
			return FTransform();
		}

		FTransform ConstructIdentity()
		{
			return FTransform::Identity;
		}

		FTransform ConstructLocation()
		{
			return FTransform(FVector(100, 200, 300));
		}

		FTransform ConstructFull()
		{
			return FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
		}

		FTransform ConstructRotationAndLocation()
		{
			FRotator Rot = FRotator(0, 90, 0);  // Yaw 90 degrees
			return FTransform(Rot, FVector(50, 100, 150));
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ConstructDefault()"), FTransform::Identity, TEXT("FTransform() default is identity"));
		ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ConstructIdentity()"), FTransform::Identity, TEXT("FTransform::Identity"));

		{
			FTransform Expected(FVector(100, 200, 300));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ConstructLocation()"), Expected, TEXT("FTransform(Location)"));
		}

		{
			FTransform Expected(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ConstructFull()"), Expected, TEXT("FTransform(Rotation, Location, Scale)"));
		}

		{
			FRotator Rot(0, 90, 0);
			FTransform Expected(Rot, FVector(50, 100, 150));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ConstructRotationAndLocation()"), Expected, TEXT("FTransform(Rotator, Location)"));
		}
	}

	// -------------------------------------------------------------------------
	// FTransform member access: Location, Rotation, Scale3D.
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_MemberAccess", ASTEST_AS(R"AS(
		FVector GetLocation()
		{
			FTransform T = FTransform(FVector(100, 200, 300));
			return T.Location;
		}

		FVector GetScale()
		{
			FTransform T = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 3, 4));
			return T.Scale3D;
		}

		FTransform SetLocation()
		{
			FTransform T = FTransform::Identity;
			T.Location = FVector(10, 20, 30);
			return T;
		}

		FTransform SetScale()
		{
			FTransform T = FTransform::Identity;
			T.Scale3D = FVector(5, 5, 5);
			return T;
		}

		FTransform SetRotation()
		{
			FTransform T = FTransform::Identity;
			T.Rotation = FQuat(FRotator(0, 90, 0));
			return T;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector GetLocation()"), FVector(100, 200, 300), TEXT("FTransform.Location getter"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector GetScale()"), FVector(2, 3, 4), TEXT("FTransform.Scale3D getter"));

		{
			FTransform Expected(FVector(10, 20, 30));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform SetLocation()"), Expected, TEXT("FTransform.Location setter"));
		}

		{
			FTransform Expected(FQuat::Identity, FVector::ZeroVector, FVector(5, 5, 5));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform SetScale()"), Expected, TEXT("FTransform.Scale3D setter"));
		}

		{
			FTransform Expected(FRotator(0, 90, 0), FVector::ZeroVector);
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform SetRotation()"), Expected, TEXT("FTransform.Rotation setter"));
		}
	}

	// -------------------------------------------------------------------------
	// FTransform composition: * operator (transform multiplication).
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformComposition)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_Composition", ASTEST_AS(R"AS(
		FTransform ComposeTransforms()
		{
			FTransform T1 = FTransform(FVector(100, 0, 0));
			FTransform T2 = FTransform(FVector(0, 200, 0));
			return T1 * T2;
		}

		FTransform ComposeWithScale()
		{
			FTransform T1 = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 2, 2));
			FTransform T2 = FTransform(FVector(10, 10, 10));
			return T1 * T2;
		}

		FTransform ComposeThree()
		{
			FTransform T1 = FTransform(FVector(100, 0, 0));
			FTransform T2 = FTransform(FVector(0, 100, 0));
			FTransform T3 = FTransform(FVector(0, 0, 100));
			return T1 * T2 * T3;
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
			FTransform T1(FVector(100, 0, 0));
			FTransform T2(FVector(0, 200, 0));
			FTransform Expected = T1 * T2;
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ComposeTransforms()"), Expected, TEXT("FTransform composition T1 * T2"));
		}

		{
			FTransform T1(FQuat::Identity, FVector::ZeroVector, FVector(2, 2, 2));
			FTransform T2(FVector(10, 10, 10));
			FTransform Expected = T1 * T2;
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ComposeWithScale()"), Expected, TEXT("FTransform composition with scale"));
		}

		{
			FTransform T1(FVector(100, 0, 0));
			FTransform T2(FVector(0, 100, 0));
			FTransform T3(FVector(0, 0, 100));
			FTransform Expected = T1 * T2 * T3;
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform ComposeThree()"), Expected, TEXT("FTransform composition of three transforms"));
		}
	}

	// -------------------------------------------------------------------------
	// FTransform methods: TransformPosition, TransformVector.
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformPositionAndVector)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_TransformPV", ASTEST_AS(R"AS(
		FVector TransformPosition()
		{
			FTransform T = FTransform(FVector(100, 0, 0));
			FVector Point = FVector(10, 0, 0);
			return T.TransformPosition(Point);
		}

		FVector TransformVector()
		{
			FTransform T = FTransform(FQuat::Identity, FVector(100, 0, 0), FVector(2, 2, 2));
			FVector Vec = FVector(10, 0, 0);
			return T.TransformVector(Vec);
		}

		FVector TransformPositionWithScale()
		{
			FTransform T = FTransform(FQuat::Identity, FVector(50, 50, 50), FVector(2, 2, 2));
			FVector Point = FVector(10, 20, 30);
			return T.TransformPosition(Point);
		}

		FVector TransformVectorNoTranslation()
		{
			FTransform T = FTransform(FVector(1000, 1000, 1000));
			FVector Vec = FVector(1, 0, 0);
			return T.TransformVector(Vec);
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
			FTransform T(FVector(100, 0, 0));
			FVector Expected = T.TransformPosition(FVector(10, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TransformPosition()"), Expected, TEXT("FTransform.TransformPosition()"));
		}

		{
			FTransform T(FQuat::Identity, FVector(100, 0, 0), FVector(2, 2, 2));
			FVector Expected = T.TransformVector(FVector(10, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TransformVector()"), Expected, TEXT("FTransform.TransformVector()"));
		}

		{
			FTransform T(FQuat::Identity, FVector(50, 50, 50), FVector(2, 2, 2));
			FVector Expected = T.TransformPosition(FVector(10, 20, 30));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TransformPositionWithScale()"), Expected, TEXT("FTransform.TransformPosition() with scale"));
		}

		{
			FTransform T(FVector(1000, 1000, 1000));
			FVector Expected = T.TransformVector(FVector(1, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector TransformVectorNoTranslation()"), Expected, TEXT("FTransform.TransformVector() ignores translation"));
		}
	}

	// -------------------------------------------------------------------------
	// FTransform inverse methods: Inverse, InverseTransformPosition.
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformInverse)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_Inverse", ASTEST_AS(R"AS(
		FTransform GetInverse()
		{
			FTransform T = FTransform(FVector(100, 200, 300));
			return T.Inverse();
		}

		FVector InverseTransformPosition()
		{
			FTransform T = FTransform(FVector(100, 0, 0));
			FVector WorldPoint = FVector(110, 0, 0);
			return T.InverseTransformPosition(WorldPoint);
		}

		FVector InverseTransformVector()
		{
			FTransform T = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 2, 2));
			FVector WorldVec = FVector(20, 0, 0);
			return T.InverseTransformVector(WorldVec);
		}

		FVector InverseRoundTrip()
		{
			FTransform T = FTransform(FVector(100, 200, 300));
			FVector Original = FVector(10, 20, 30);
			FVector World = T.TransformPosition(Original);
			return T.InverseTransformPosition(World);
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
			FTransform T(FVector(100, 200, 300));
			FTransform Expected = T.Inverse();
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform GetInverse()"), Expected, TEXT("FTransform.Inverse()"));
		}

		{
			FTransform T(FVector(100, 0, 0));
			FVector Expected = T.InverseTransformPosition(FVector(110, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector InverseTransformPosition()"), Expected, TEXT("FTransform.InverseTransformPosition()"));
		}

		{
			FTransform T(FQuat::Identity, FVector::ZeroVector, FVector(2, 2, 2));
			FVector Expected = T.InverseTransformVector(FVector(20, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector InverseTransformVector()"), Expected, TEXT("FTransform.InverseTransformVector()"));
		}

		{
			// Round trip should give back original
			FVector Expected(10, 20, 30);
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector InverseRoundTrip()"), Expected, TEXT("FTransform inverse round-trip"), 0.01);
		}
	}

	// -------------------------------------------------------------------------
	// FTransform interpolation: Lerp.
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformInterpolation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_Lerp", ASTEST_AS(R"AS(
		FTransform LerpTransforms()
		{
			FTransform A = FTransform(FVector(0, 0, 0));
			FTransform B = FTransform(FVector(100, 100, 100));
			return Math::Lerp(A, B, 0.5f);
		}

		FTransform LerpAtZero()
		{
			FTransform A = FTransform(FVector(100, 200, 300));
			FTransform B = FTransform(FVector(400, 500, 600));
			return Math::Lerp(A, B, 0.0f);
		}

		FTransform LerpAtOne()
		{
			FTransform A = FTransform(FVector(100, 200, 300));
			FTransform B = FTransform(FVector(400, 500, 600));
			return Math::Lerp(A, B, 1.0f);
		}

		FTransform LerpWithScale()
		{
			FTransform A = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, 1));
			FTransform B = FTransform(FQuat::Identity, FVector(100, 0, 0), FVector(3, 3, 3));
			return Math::Lerp(A, B, 0.5f);
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
			FTransform A(FVector(0, 0, 0));
			FTransform B(FVector(100, 100, 100));
			FTransform Expected;
			Expected.Blend(A, B, 0.5f);
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform LerpTransforms()"), Expected, TEXT("FTransform Lerp at 0.5"), 0.01);
		}

		{
			FTransform Expected(FVector(100, 200, 300));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform LerpAtZero()"), Expected, TEXT("FTransform Lerp at 0.0"), 0.01);
		}

		{
			FTransform Expected(FVector(400, 500, 600));
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform LerpAtOne()"), Expected, TEXT("FTransform Lerp at 1.0"), 0.01);
		}

		{
			FTransform A(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, 1));
			FTransform B(FQuat::Identity, FVector(100, 0, 0), FVector(3, 3, 3));
			FTransform Expected;
			Expected.Blend(A, B, 0.5f);
			ExpectTransformNearlyEqual(Engine, Module, TEXT("FTransform LerpWithScale()"), Expected, TEXT("FTransform Lerp with scale"), 0.01);
		}
	}

	// -------------------------------------------------------------------------
	// FTransform comparison: Equals.
	// -------------------------------------------------------------------------
	TEST_METHOD(TransformComparison)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformExpr_Comparison", ASTEST_AS(R"AS(
		bool CompareIdentity()
		{
			FTransform A = FTransform::Identity;
			FTransform B = FTransform::Identity;
			return A.Equals(B);
		}

		bool CompareEqual()
		{
			FTransform A = FTransform(FVector(100, 200, 300));
			FTransform B = FTransform(FVector(100, 200, 300));
			return A.Equals(B);
		}

		bool CompareNotEqual()
		{
			FTransform A = FTransform(FVector(100, 200, 300));
			FTransform B = FTransform(FVector(400, 500, 600));
			return A.Equals(B);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool CompareIdentity()"), true, TEXT("FTransform::Identity equals Identity"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool CompareEqual()"), true, TEXT("FTransform equals same values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool CompareNotEqual()"), false, TEXT("FTransform not equals different values"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
