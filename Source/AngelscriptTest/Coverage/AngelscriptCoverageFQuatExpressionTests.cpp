#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFQuatExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FQuat *expression usage* -- operators, construction,
// methods, and rotation operations.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFQuatExpressionTest,
	"Angelscript.TestModule.Coverage.FQuatExpression",
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
		if constexpr (std::is_same_v<T, float>)
		{
			Result = static_cast<float>(Invoker.ExecuteAndGet<double>(0.0));
		}
		else if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
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

	// Helper for FQuat with tolerance
	void ExpectQuatNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FQuat& Expected, const TCHAR* Message, double Tolerance = 0.001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FQuat Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
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
	// FQuat construction: default, parameterized, constants, from rotator.
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_Construct", ASTEST_AS(R"AS(
		FQuat ConstructDefault()
		{
			return FQuat();
		}

		FQuat ConstructFourParams()
		{
			return FQuat(0, 0, 0, 1);
		}

		FQuat ConstructIdentity()
		{
			return FQuat::Identity;
		}

		FQuat ConstructFromRotator()
		{
			return FQuat(FRotator(0, 90, 0));
		}

		FQuat ConstructFromAxisAngle()
		{
			FVector axis = FVector::UpVector;
			float angleRad = 1.5708; // 90 degrees in radians
			return FQuat(axis, angleRad);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat ConstructDefault()"), FQuat::Identity, TEXT("FQuat() default"));
		ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat ConstructFourParams()"), FQuat(0, 0, 0, 1), TEXT("FQuat(0,0,0,1)"));
		ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat ConstructIdentity()"), FQuat::Identity, TEXT("FQuat::Identity"));
		ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat ConstructFromRotator()"), FQuat(FRotator(0, 90, 0)), TEXT("FQuat from FRotator"), 0.01);
		ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat ConstructFromAxisAngle()"), FQuat(FVector::UpVector, 1.5708f), TEXT("FQuat from axis-angle"), 0.01);
	}

	// -------------------------------------------------------------------------
	// FQuat member access: X, Y, Z, W.
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_MemberAccess", ASTEST_AS(R"AS(
		float GetX()
		{
			FQuat q = FQuat(0.1, 0.2, 0.3, 0.9);
			return q.X;
		}

		float GetY()
		{
			FQuat q = FQuat(0.1, 0.2, 0.3, 0.9);
			return q.Y;
		}

		float GetZ()
		{
			FQuat q = FQuat(0.1, 0.2, 0.3, 0.9);
			return q.Z;
		}

		float GetW()
		{
			FQuat q = FQuat(0.1, 0.2, 0.3, 0.9);
			return q.W;
		}

		FQuat SetX()
		{
			FQuat q = FQuat::Identity;
			q.X = 0.5;
			return q;
		}

		FQuat SetY()
		{
			FQuat q = FQuat::Identity;
			q.Y = 0.5;
			return q;
		}

		FQuat SetZ()
		{
			FQuat q = FQuat::Identity;
			q.Z = 0.5;
			return q;
		}

		FQuat SetW()
		{
			FQuat q = FQuat::Identity;
			q.W = 0.5;
			return q;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetX()"), 0.1f, TEXT("FQuat.X getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetY()"), 0.2f, TEXT("FQuat.Y getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetZ()"), 0.3f, TEXT("FQuat.Z getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetW()"), 0.9f, TEXT("FQuat.W getter"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat SetX()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FQuat.X setter"), Result.X, 0.5, 0.001);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat SetY()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FQuat.Y setter"), Result.Y, 0.5, 0.001);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat SetZ()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FQuat.Z setter"), Result.Z, 0.5, 0.001);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat SetW()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FQuat.W setter"), Result.W, 0.5, 0.001);
		}
	}

	// -------------------------------------------------------------------------
	// FQuat operators: multiplication (composition).
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatMultiplicationOperator)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_Multiply", ASTEST_AS(R"AS(
		FQuat MultiplyQuats()
		{
			FQuat q1 = FQuat(FRotator(0, 45, 0));
			FQuat q2 = FQuat(FRotator(0, 45, 0));
			return q1 * q2;
		}

		FVector RotateVectorWithOperator()
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FVector v = FVector(1, 0, 0);
			return q * v;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Multiply two quaternions (should result in 90-degree yaw rotation)
		{
			FQuat q1 = FQuat(FRotator(0, 45, 0));
			FQuat q2 = FQuat(FRotator(0, 45, 0));
			FQuat Expected = q1 * q2;
			ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat MultiplyQuats()"), Expected, TEXT("quat * quat"), 0.01);
		}

		// Rotate vector with quaternion operator
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FVector Expected = q.RotateVector(FVector(1, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector RotateVectorWithOperator()"), Expected, TEXT("quat * vector"), 0.01);
		}
	}

	// -------------------------------------------------------------------------
	// FQuat methods: Inverse, Normalize.
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatInverseAndNormalize)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_InverseNormalize", ASTEST_AS(R"AS(
		FQuat InverseQuat()
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			return q.Inverse();
		}

		FQuat NormalizeQuat()
		{
			FQuat q = FQuat(0.1, 0.2, 0.3, 0.9);
			return q.GetNormalized();
		}

		bool IsNormalized()
		{
			FQuat q = FQuat::Identity;
			return q.IsNormalized();
		}

		bool IsIdentityQuat()
		{
			FQuat q = FQuat::Identity;
			return q.IsIdentity();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Inverse
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FQuat Expected = q.Inverse();
			ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat InverseQuat()"), Expected, TEXT("FQuat.Inverse()"), 0.01);
		}

		// Normalize
		{
			FQuat q = FQuat(0.1, 0.2, 0.3, 0.9);
			FQuat Expected = q.GetNormalized();
			ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat NormalizeQuat()"), Expected, TEXT("FQuat.GetNormalized()"), 0.01);
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IsNormalized()"), true, TEXT("FQuat.IsNormalized()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IsIdentityQuat()"), true, TEXT("FQuat.IsIdentity()"));
	}

	// -------------------------------------------------------------------------
	// FQuat methods: RotateVector, UnrotateVector.
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatRotateVector)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_RotateVector", ASTEST_AS(R"AS(
		FVector RotateForwardBy90()
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FVector v = FVector(1, 0, 0);
			return q.RotateVector(v);
		}

		FVector UnrotateVector()
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FVector v = FVector(0, 1, 0);
			return q.UnrotateVector(v);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// RotateVector - rotate (1,0,0) by 90 degrees yaw should give approximately (0,1,0)
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FVector Expected = q.RotateVector(FVector(1, 0, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector RotateForwardBy90()"), Expected, TEXT("FQuat.RotateVector()"), 0.01);
		}

		// UnrotateVector
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			FVector Expected = q.UnrotateVector(FVector(0, 1, 0));
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector UnrotateVector()"), Expected, TEXT("FQuat.UnrotateVector()"), 0.01);
		}
	}

	// -------------------------------------------------------------------------
	// FQuat conversion: Rotator, Euler, GetAxisX/Y/Z.
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatConversionMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_Conversion", ASTEST_AS(R"AS(
		FRotator QuatToRotator()
		{
			FQuat q = FQuat(FRotator(0, 90, 0));
			return q.Rotator();
		}

		FVector QuatEuler()
		{
			FQuat q = FQuat(FRotator(10, 20, 30));
			return q.Euler();
		}

		FVector GetForwardAxis()
		{
			FQuat q = FQuat(FRotator(0, 0, 0));
			return q.GetAxisX();
		}

		FVector GetRightAxis()
		{
			FQuat q = FQuat(FRotator(0, 0, 0));
			return q.GetAxisY();
		}

		FVector GetUpAxis()
		{
			FQuat q = FQuat(FRotator(0, 0, 0));
			return q.GetAxisZ();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Rotator()
		ExpectRotatorNearlyEqual(Engine, Module, TEXT("FRotator QuatToRotator()"), FRotator(0, 90, 0), TEXT("FQuat.Rotator()"), 0.1);

		// Euler()
		{
			FQuat q = FQuat(FRotator(10, 20, 30));
			FVector Expected = q.Euler();
			ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector QuatEuler()"), Expected, TEXT("FQuat.Euler()"), 0.1);
		}

		// GetAxisX/Y/Z
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector GetForwardAxis()"), FVector::ForwardVector, TEXT("FQuat.GetAxisX()"), 0.01);
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector GetRightAxis()"), FVector::RightVector, TEXT("FQuat.GetAxisY()"), 0.01);
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector GetUpAxis()"), FVector::UpVector, TEXT("FQuat.GetAxisZ()"), 0.01);
	}

	// -------------------------------------------------------------------------
	// FQuat static methods: Slerp, MakeFromEuler.
	// -------------------------------------------------------------------------
	TEST_METHOD(QuatStaticMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_Static", ASTEST_AS(R"AS(
		FQuat SlerpQuats()
		{
			FQuat q1 = FQuat::Identity;
			FQuat q2 = FQuat(FRotator(0, 90, 0));
			return FQuat::Slerp(q1, q2, 0.5);
		}

		FQuat MakeFromEuler()
		{
			FVector euler = FVector(10, 20, 30);
			return FQuat::MakeFromEuler(euler);
		}

		FQuat FindBetweenVectors()
		{
			FVector v1 = FVector::ForwardVector;
			FVector v2 = FVector::RightVector;
			return FQuat::FindBetweenVectors(v1, v2);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Slerp
		{
			FQuat q1 = FQuat::Identity;
			FQuat q2 = FQuat(FRotator(0, 90, 0));
			FQuat Expected = FQuat::Slerp(q1, q2, 0.5f);
			ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat SlerpQuats()"), Expected, TEXT("FQuat::Slerp()"), 0.01);
		}

		// MakeFromEuler
		{
			FQuat Expected = FQuat::MakeFromEuler(FVector(10, 20, 30));
			ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat MakeFromEuler()"), Expected, TEXT("FQuat::MakeFromEuler()"), 0.01);
		}

		// FindBetweenVectors
		{
			FQuat Expected = FQuat::FindBetweenVectors(FVector::ForwardVector, FVector::RightVector);
			ExpectQuatNearlyEqual(Engine, Module, TEXT("FQuat FindBetweenVectors()"), Expected, TEXT("FQuat::FindBetweenVectors()"), 0.01);
		}
	}

	TEST_METHOD(QuatAdvancedOperatorsAndMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatExpr_AdvancedOps", ASTEST_AS(R"AS(
		bool CopyAssignAndEquality()
		{
			FQuat Source = FQuat(FVector::UpVector, 1.5707963267948966);
			FQuat Copy(Source);
			FQuat Assigned;
			Assigned = Copy;
			return Assigned == Source && Assigned.Equals(Source, 0.001);
		}

		bool CompoundAndScalarOperators()
		{
			FQuat QuarterTurn = FQuat(FVector::UpVector, 1.5707963267948966);
			FQuat Working = FQuat::Identity;
			Working *= QuarterTurn;
			FQuat Inflated = Working * 2.0;
			Inflated /= 2.0;
			FQuat AddedThenSubtracted = (Working + FQuat::Identity) - FQuat::Identity;
			return Inflated.Equals(QuarterTurn, 0.001)
				&& AddedThenSubtracted.Equals(QuarterTurn, 0.001);
		}

		bool AxisAngleAndDirectionMethods()
		{
			FQuat QuarterTurn = FQuat(FVector::UpVector, 1.5707963267948966);
			FVector Axis;
			float64 Angle = 0.0;
			QuarterTurn.ToAxisAndAngle(Axis, Angle);
			return Axis.Equals(FVector::UpVector, 0.001)
				&& Math::Abs(Angle - 1.5707963267948966) < 0.001
				&& QuarterTurn.GetForwardVector().Equals(QuarterTurn.GetAxisX(), 0.001)
				&& QuarterTurn.Vector().Equals(QuarterTurn.GetAxisX(), 0.001);
		}

		bool InterpolationAndErrorMethods()
		{
			FQuat Start = FQuat::Identity;
			FQuat End = FQuat(FVector::UpVector, 1.5707963267948966);
			FQuat Fast = FQuat::FastLerp(Start, End, 0.5).GetNormalized();
			FQuat Full = FQuat::SlerpFullPath(Start, End, 0.5);
			float64 Error = FQuat::Error(Fast, Full);
			return Fast.IsNormalized()
				&& Full.IsNormalized()
				&& Error < 0.1
				&& FQuat::ErrorAutoNormalize(Fast * 2.0, Full * 3.0) < 0.1;
		}

		bool SwingTwistAndTangents()
		{
			FQuat Rotation = FQuat(FVector::UpVector, 1.5707963267948966);
			FQuat Swing;
			FQuat Twist;
			Rotation.ToSwingTwist(FVector::UpVector, Swing, Twist);

			FQuat Tangent;
			FQuat::CalcTangents(FQuat::Identity, Rotation, FQuat(FVector::UpVector, 3.1415926535897932), 0.0, Tangent);

			return Twist.IsNormalized()
				&& Swing.IsNormalized()
				&& Math::Abs(Rotation.GetTwistAngle(FVector::UpVector) - 1.5707963267948966) < 0.001
				&& !Tangent.ContainsNaN();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("advanced FQuat expression module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool CopyAssignAndEquality()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("copy construction, assignment, and equality should work")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool CompoundAndScalarOperators()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("compound quaternion and scalar operators should work")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AxisAngleAndDirectionMethods()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("axis-angle and direction accessors should work")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool InterpolationAndErrorMethods()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("interpolation and error helpers should work")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool SwingTwistAndTangents()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("swing/twist and tangent helpers should work")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
