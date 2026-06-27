#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFRotatorExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FRotator *expression usage* -- operators, construction,
// methods, and rotation operations.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFRotatorExpressionTest,
	"Angelscript.TestModule.Coverage.FRotatorExpression",
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

	// Helper for FRotator with tolerance
	void ExpectRotatorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FRotator& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FRotator Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// Helper for FVector with tolerance
	void ExpectVectorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FVector Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// -------------------------------------------------------------------------
	// FRotator construction: default, parameterized, constants.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_Construct", ASTEST_AS(R"AS(
		FRotator ConstructDefault()
		{
			return FRotator();
		}

		FRotator ConstructThreeParams()
		{
			return FRotator(10, 20, 30);
		}

		FRotator ConstructZeroRotator()
		{
			return FRotator::ZeroRotator;
		}

		FRotator ConstructPitchOnly()
		{
			return FRotator(45, 0, 0);
		}

		FRotator ConstructYawOnly()
		{
			return FRotator(0, 90, 0);
		}

		FRotator ConstructRollOnly()
		{
			return FRotator(0, 0, 180);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator ConstructDefault()"), FRotator::ZeroRotator, TEXT("FRotator() default"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator ConstructThreeParams()"), FRotator(10, 20, 30), TEXT("FRotator(10,20,30)"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator ConstructZeroRotator()"), FRotator::ZeroRotator, TEXT("FRotator::ZeroRotator"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator ConstructPitchOnly()"), FRotator(45, 0, 0), TEXT("FRotator pitch only"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator ConstructYawOnly()"), FRotator(0, 90, 0), TEXT("FRotator yaw only"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator ConstructRollOnly()"), FRotator(0, 0, 180), TEXT("FRotator roll only"));
	}

	// -------------------------------------------------------------------------
	// FRotator arithmetic operators: +, -, *.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_Arithmetic", ASTEST_AS(R"AS(
		FRotator OpAdd()
		{
			FRotator a = FRotator(10, 20, 30);
			FRotator b = FRotator(5, 10, 15);
			return a + b;
		}

		FRotator OpSubtract()
		{
			FRotator a = FRotator(100, 200, 300);
			FRotator b = FRotator(10, 20, 30);
			return a - b;
		}

		FRotator OpMultiplyScalar()
		{
			FRotator r = FRotator(10, 20, 30);
			return r * 2.0;
		}

		FRotator OpNegate()
		{
			FRotator r = FRotator(10, 20, 30);
			return -r;
		}

		FRotator OpCompoundAdd()
		{
			FRotator r = FRotator(10, 20, 30);
			r += FRotator(5, 5, 5);
			return r;
		}

		FRotator OpCompoundSubtract()
		{
			FRotator r = FRotator(100, 100, 100);
			r -= FRotator(10, 20, 30);
			return r;
		}

		FRotator OpCompoundMultiply()
		{
			FRotator r = FRotator(10, 20, 30);
			r *= 3.0;
			return r;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpAdd()"), FRotator(15, 30, 45), TEXT("rotator addition"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpSubtract()"), FRotator(90, 180, 270), TEXT("rotator subtraction"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpMultiplyScalar()"), FRotator(20, 40, 60), TEXT("rotator * scalar"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpNegate()"), FRotator(-10, -20, -30), TEXT("rotator negation"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpCompoundAdd()"), FRotator(15, 25, 35), TEXT("rotator += "));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpCompoundSubtract()"), FRotator(90, 80, 70), TEXT("rotator -= "));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpCompoundMultiply()"), FRotator(30, 60, 90), TEXT("rotator *= "));
	}

	// -------------------------------------------------------------------------
	// FRotator comparison operators: ==, !=.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_Comparison", ASTEST_AS(R"AS(
		bool OpEquals_True()
		{
			FRotator a = FRotator(10, 20, 30);
			FRotator b = FRotator(10, 20, 30);
			return a == b;
		}

		bool OpEquals_False()
		{
			FRotator a = FRotator(10, 20, 30);
			FRotator b = FRotator(40, 50, 60);
			return a == b;
		}

		bool OpNotEquals_True()
		{
			FRotator a = FRotator(10, 20, 30);
			FRotator b = FRotator(40, 50, 60);
			return a != b;
		}

		bool OpNotEquals_False()
		{
			FRotator a = FRotator(10, 20, 30);
			FRotator b = FRotator(10, 20, 30);
			return a != b;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_True()"), true, TEXT("rotator == (equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_False()"), false, TEXT("rotator == (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_True()"), true, TEXT("rotator != (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_False()"), false, TEXT("rotator != (equal)"));
	}

	// -------------------------------------------------------------------------
	// FRotator member access: Pitch, Yaw, Roll.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_MemberAccess", ASTEST_AS(R"AS(
		float GetPitch()
		{
			FRotator r = FRotator(10, 20, 30);
			return r.Pitch;
		}

		float GetYaw()
		{
			FRotator r = FRotator(10, 20, 30);
			return r.Yaw;
		}

		float GetRoll()
		{
			FRotator r = FRotator(10, 20, 30);
			return r.Roll;
		}

		FRotator SetPitch()
		{
			FRotator r = FRotator(10, 20, 30);
			r.Pitch = 45;
			return r;
		}

		FRotator SetYaw()
		{
			FRotator r = FRotator(10, 20, 30);
			r.Yaw = 90;
			return r;
		}

		FRotator SetRoll()
		{
			FRotator r = FRotator(10, 20, 30);
			r.Roll = 180;
			return r;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetPitch()"), 10.0f, TEXT("FRotator.Pitch getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetYaw()"), 20.0f, TEXT("FRotator.Yaw getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetRoll()"), 30.0f, TEXT("FRotator.Roll getter"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator SetPitch()"), FRotator(45, 20, 30), TEXT("FRotator.Pitch setter"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator SetYaw()"), FRotator(10, 90, 30), TEXT("FRotator.Yaw setter"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator SetRoll()"), FRotator(10, 20, 180), TEXT("FRotator.Roll setter"));
	}

	// -------------------------------------------------------------------------
	// FRotator methods: Normalize, Clamp, GetNormalized.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorNormalizationMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_Normalize", ASTEST_AS(R"AS(
		FRotator NormalizeRotator()
		{
			FRotator r = FRotator(400, 720, -400);
			return r.GetNormalized();
		}

		FRotator ClampRotator()
		{
			FRotator r = FRotator(100, 200, 100);
			return r.GetClamped();
		}

		bool IsZero()
		{
			FRotator r = FRotator::ZeroRotator;
			return r.IsZero();
		}

		bool IsNearlyZero()
		{
			FRotator r = FRotator(0.0001, 0.0001, 0.0001);
			return r.IsNearlyZero();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Normalize - angles should be in [-180, 180] range
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator NormalizeRotator()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			// 400 -> 40, 720 -> 0, -400 -> -40
			TestRunner->TestTrue(TEXT("FRotator GetNormalized"), Result.Equals(FRotator(40, 0, -40), 0.001));
		}

		// Clamp
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator ClampRotator()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			// Test that clamping occurred
			TestRunner->TestTrue(TEXT("FRotator GetClamped"), Result.Pitch >= -90.0 && Result.Pitch <= 90.0);
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IsZero()"), true, TEXT("rotator IsZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IsNearlyZero()"), true, TEXT("rotator IsNearlyZero()"));
	}

	// -------------------------------------------------------------------------
	// FRotator conversion methods: Vector, Quaternion, Euler.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorConversionMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_Conversion", ASTEST_AS(R"AS(
		FVector RotatorToVector()
		{
			FRotator r = FRotator(0, 0, 0);
			return r.Vector();
		}

		FQuat RotatorToQuaternion()
		{
			FRotator r = FRotator(0, 90, 0);
			return r.Quaternion();
		}

		FVector RotatorEuler()
		{
			FRotator r = FRotator(10, 20, 30);
			return r.Euler();
		}

		FVector RotateVector()
		{
			FRotator r = FRotator(0, 90, 0);
			FVector v = FVector(1, 0, 0);
			return r.RotateVector(v);
		}

		FVector UnrotateVector()
		{
			FRotator r = FRotator(0, 90, 0);
			FVector v = FVector(0, 1, 0);
			return r.UnrotateVector(v);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Vector() - forward direction
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector RotatorToVector()"), FVector::ForwardVector, TEXT("FRotator.Vector()"), 0.01);

		// Quaternion()
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat RotatorToQuaternion()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FQuat Expected = FRotator(0, 90, 0).Quaternion();
			TestRunner->TestTrue(TEXT("FRotator.Quaternion()"), Result.Equals(Expected, 0.001));
		}

		// Euler()
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector RotatorEuler()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FVector Expected = FRotator(10, 20, 30).Euler();
			TestRunner->TestTrue(TEXT("FRotator.Euler()"), Result.Equals(Expected, 0.001));
		}

		// RotateVector()
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector RotateVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FVector Expected = FRotator(0, 90, 0).RotateVector(FVector(1, 0, 0));
			TestRunner->TestTrue(TEXT("FRotator.RotateVector()"), Result.Equals(Expected, 0.01));
		}

		// UnrotateVector()
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector UnrotateVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FVector Expected = FRotator(0, 90, 0).UnrotateVector(FVector(0, 1, 0));
			TestRunner->TestTrue(TEXT("FRotator.UnrotateVector()"), Result.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// FRotator static methods: MakeFromEuler, Lerp.
	// -------------------------------------------------------------------------
	TEST_METHOD(RotatorStaticMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_Static", ASTEST_AS(R"AS(
		FRotator MakeFromEuler()
		{
			FVector euler = FVector(10, 20, 30);
			return FRotator::MakeFromEuler(euler);
		}

		FRotator LerpRotators()
		{
			FRotator a = FRotator(0, 0, 0);
			FRotator b = FRotator(90, 90, 90);
			return FRotator::Lerp(a, b, 0.5);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// MakeFromEuler
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator MakeFromEuler()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FRotator Expected = FRotator::MakeFromEuler(FVector(10, 20, 30));
			TestRunner->TestTrue(TEXT("FRotator::MakeFromEuler()"), Result.Equals(Expected, 0.001));
		}

		// Lerp
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator LerpRotators()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FRotator Expected = FMath::Lerp(FRotator(0, 0, 0), FRotator(90, 90, 90), 0.5f);
			TestRunner->TestTrue(TEXT("FRotator::Lerp()"), Result.Equals(Expected, 0.001));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
