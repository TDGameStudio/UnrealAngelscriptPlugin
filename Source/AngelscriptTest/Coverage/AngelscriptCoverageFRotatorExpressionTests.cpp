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

#if WITH_ANGELSCRIPT_UNITTESTS

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
		if constexpr (std::is_same_v<T, float>)
		{
			const double Result = Invoker.ExecuteAndGet<double>(0.0);
			ASSERT_THAT(IsNear(static_cast<double>(Expected), Result, 0.0001, Message));
			return;
		}
		else if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
			|| std::is_same_v<T, double>)
		{
			const T Result = Invoker.ExecuteAndGet<T>(T{});
			if constexpr (std::is_floating_point_v<T>)
			{
				ASSERT_THAT(IsNear(Expected, Result, static_cast<T>(0.0001), Message));
			}
			else
			{
				ASSERT_THAT(AreEqual(Expected, Result, Message));
			}
		}
		else
		{
			T Result{};
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator construction module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator arithmetic module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpAdd()"), FRotator(15, 30, 45), TEXT("rotator addition"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpSubtract()"), FRotator(90, 180, 270), TEXT("rotator subtraction"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpMultiplyScalar()"), FRotator(20, 40, 60), TEXT("rotator * scalar"));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpCompoundAdd()"), FRotator(15, 25, 35), TEXT("rotator += "));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpCompoundSubtract()"), FRotator(90, 80, 70), TEXT("rotator -= "));
		ExpectGlobalReturn<FRotator>(Engine, Module, TEXT("FRotator OpCompoundMultiply()"), FRotator(30, 60, 90), TEXT("rotator *= "));
	}

	TEST_METHOD(RotatorUnsupportedOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedFragments = { TEXT("Function 'opNeg()' not found") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovFRotatorExpr_UnaryNegateUnsupported"),
			ASTEST_AS(R"AS(
			FRotator TryNegate()
			{
				FRotator Rotator = FRotator(10, 20, 30);
				return -Rotator;
			}
			)AS"),
			TEXT("FRotator unary negation is not bound on the current AS surface"),
			MakeArrayView(ExpectedFragments))));
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator comparison module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator member access module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
			return r.Clamp();
		}

		bool IsZero()
		{
			FRotator r = FRotator::ZeroRotator;
			return r.IsZero();
		}

		bool IsNearlyZero()
		{
			FRotator r = FRotator(0.000001, 0.000001, 0.000001);
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator normalization module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
			ASSERT_THAT(IsTrue(Result.Equals(FRotator(100, 200, 100).Clamp(), 0.001), TEXT("FRotator Clamp")));
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator conversion module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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

		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator static method module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		// MakeFromEuler
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator MakeFromEuler()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FRotator Expected = FRotator::MakeFromEuler(FVector(10, 20, 30));
			TestRunner->TestTrue(TEXT("FRotator::MakeFromEuler()"), Result.Equals(Expected, 0.001));
		}
	}

	TEST_METHOD(RotatorUnsupportedStaticMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedFragments = { TEXT("No matching signatures to 'FRotator::Lerp") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovFRotatorExpr_LerpUnsupported"),
			ASTEST_AS(R"AS(
			FRotator TryLerp()
			{
				FRotator A = FRotator(0, 0, 0);
				FRotator B = FRotator(90, 90, 90);
				return FRotator::Lerp(A, B, 0.5);
			}
			)AS"),
			TEXT("FRotator::Lerp is not bound on the current AS surface"),
			MakeArrayView(ExpectedFragments))));
	}

	TEST_METHOD(RotatorDeclarationsAndConfirmedMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorExpr_DeclarationsMethods", ASTEST_AS(R"AS(
		const FRotator GlobalConstRotator = FRotator::ZeroRotator;

		float LocalDefaultIsZero()
		{
			FRotator r;
			return r.Pitch + r.Yaw + r.Roll;
		}

		float LocalScalarConstructorSum()
		{
			FRotator r = FRotator(7);
			return r.Pitch + r.Yaw + r.Roll;
		}

		float LocalCopyConstructorSum()
		{
			FRotator Source = FRotator(1, 2, 3);
			FRotator Copy = FRotator(Source);
			return Copy.Pitch + Copy.Yaw + Copy.Roll;
		}

		float LocalConstValue()
		{
			const FRotator r = FRotator(10, 20, 30);
			return r.Pitch + r.Yaw + r.Roll;
		}

		float GlobalConstValue()
		{
			return GlobalConstRotator.Pitch + GlobalConstRotator.Yaw + GlobalConstRotator.Roll;
		}

		FRotator NormalizeMutates()
		{
			FRotator r = FRotator(0, 450, 0);
			r.Normalize();
			return r;
		}

		FRotator InverseRotator()
		{
			return FRotator(0, 90, 0).GetInverse();
		}

		float AxisHelpers()
		{
			return FRotator::NormalizeAxis(450) + FRotator::ClampAxis(-90);
		}

		bool WindingAndRemainder()
		{
			FRotator Winding;
			FRotator Remainder;
			FRotator(0, 450, 0).GetWindingAndRemainder(Winding, Remainder);
			return Winding.Equals(FRotator(0, 360, 0), 0.001) && Remainder.Equals(FRotator(0, 90, 0), 0.001);
		}

		float ManhattanDistance()
		{
			return FRotator(10, 20, 30).GetManhattanDistance(FRotator(5, 5, 5));
		}

		FVector RightVector()
		{
			return FRotator(0, 0, 0).GetRightVector();
		}

		FVector UpVector()
		{
			return FRotator(0, 0, 0).GetUpVector();
		}

		bool DeltaRoundTrip()
		{
			FRotator Origin = FRotator::ZeroRotator;
			FRotator Target = FRotator(0, 90, 0);
			FRotator Delta = FRotator::GetDelta(Origin, Target);
			return FRotator::ApplyDelta(Origin, Delta).Equals(Target, 0.05);
		}

		bool RelativeRoundTrip()
		{
			FRotator Parent = FRotator(0, 30, 0);
			FRotator Child = FRotator(0, 75, 0);
			FRotator Relative = FRotator::GetRelative(Parent, Child);
			return FRotator::ApplyRelative(Parent, Relative).Equals(Child, 0.05);
		}

		class FPlainRotatorHolder
		{
			FRotator Value;

			FPlainRotatorHolder()
			{
				Value = FRotator(2, 4, 6);
			}
		}

		int PlainClassMemberValueRaisesBoundary()
		{
			FPlainRotatorHolder Holder;
			return Holder.Value.Pitch + Holder.Value.Yaw + Holder.Value.Roll;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator declaration/method module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float LocalDefaultIsZero()"));
			ASSERT_THAT(IsNear(0.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator local default declaration should be zero")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float LocalScalarConstructorSum()"));
			ASSERT_THAT(IsNear(21.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator scalar constructor should fill all components")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float LocalCopyConstructorSum()"));
			ASSERT_THAT(IsNear(6.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator copy constructor should preserve components")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float LocalConstValue()"));
			ASSERT_THAT(IsNear(60.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator local const declaration should expose members")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float GlobalConstValue()"));
			ASSERT_THAT(IsNear(0.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator global const declaration should expose members")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator NormalizeMutates()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("NormalizeMutates should execute")));
			ASSERT_THAT(IsTrue(Result.Equals(FRotator(0, 90, 0), 0.001), TEXT("FRotator.Normalize() should mutate in place")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator InverseRotator()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("InverseRotator should execute")));
			ASSERT_THAT(IsTrue(Result.Equals(FRotator(0, 90, 0).GetInverse(), 0.001), TEXT("FRotator.GetInverse() should match native inverse")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AxisHelpers()"));
			ASSERT_THAT(IsNear(360.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator axis helpers should normalize and clamp angles")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool WindingAndRemainder()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("FRotator.GetWindingAndRemainder() should populate out rotators")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float ManhattanDistance()"));
			ASSERT_THAT(IsNear(45.0, Invoker.ExecuteAndGet<double>(-1.0), 0.01, TEXT("FRotator.GetManhattanDistance() should sum component deltas")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector RightVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("RightVector should execute")));
			ASSERT_THAT(IsTrue(Result.Equals(FVector::RightVector, 0.001), TEXT("FRotator.GetRightVector() should expose right axis")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector UpVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("UpVector should execute")));
			ASSERT_THAT(IsTrue(Result.Equals(FVector::UpVector, 0.001), TEXT("FRotator.GetUpVector() should expose up axis")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool DeltaRoundTrip()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("FRotator GetDelta/ApplyDelta should round-trip")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool RelativeRoundTrip()"));
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndGet<bool>(false), TEXT("FRotator GetRelative/ApplyRelative should round-trip")));
		}
		{
			asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int PlainClassMemberValueRaisesBoundary()"));
			ASSERT_THAT(IsNotNull(Function, TEXT("FRotator plain script class boundary function should exist")));
			if (Function != nullptr)
			{
				ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
					*TestRunner,
					Engine,
					*Function,
					TEXT("FRotator plain script class member boundary"),
					TEXT("Null pointer access"),
					TEXT("int PlainClassMemberValueRaisesBoundary() | Line"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
