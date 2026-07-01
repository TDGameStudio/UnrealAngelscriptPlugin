#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#include <type_traits>

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathNamespaceFunctions
// -----------------------------------------------------------------------------
// Coverage for Math:: namespace functions identified in Coverage_MathStructs.md
// Section 8: Math namespace functions (trigonometric, power/root, rounding, etc.)
//
// Test patterns: Pattern B (global functions)
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMathNamespaceFunctionsTest,
	"Angelscript.TestModule.Coverage.MathNamespaceFunctions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

private:
	// Helper
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message, double Tolerance = 0.001)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math namespace module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math namespace global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		T Result{};
		if constexpr (std::is_same_v<T, float>)
		{
			// AS `float` is double-backed on this fork (asEP_FLOAT_IS_FLOAT64=1):
			// read the return register as double before narrowing to float.
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

		if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual((double)Result, (double)Expected, Tolerance), Message));
		}
		else
		{
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}
	}

	void ExpectGlobalFloatRange(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, float MinValue, float MaxValue, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math namespace module should compile before executing range function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math namespace range function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const float Result = static_cast<float>(Invoker.ExecuteAndGet<double>(0.0));
		ASSERT_THAT(IsTrue(Result >= MinValue && Result <= MaxValue, Message));
	}

	void ExpectGlobalFloatSatisfies(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, TFunctionRef<bool(float)> Predicate, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math namespace module should compile before executing predicate function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math namespace predicate function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const float Result = static_cast<float>(Invoker.ExecuteAndGet<double>(0.0));
		ASSERT_THAT(IsTrue(Predicate(Result), Message));
	}

	template <typename T>
	void ExpectGlobalStructSatisfies(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, TFunctionRef<bool(const T&)> Predicate, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math namespace module should compile before extracting struct")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math namespace struct function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		T Result{};
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		ASSERT_THAT(IsTrue(Predicate(Result), Message));
	}

public:
	// -------------------------------------------------------------------------
	// Trigonometric functions: Sin, Cos, Tan, Asin, Acos, Atan, Atan2
	// -------------------------------------------------------------------------
	TEST_METHOD(TrigonometricFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_Trig", ASTEST_AS(R"AS(
		float TestSin()
		{
			return Math::Sin(Math::DegreesToRadians(90.0));
		}

		float TestCos()
		{
			return Math::Cos(0.0);
		}

		float TestTan()
		{
			return Math::Tan(Math::DegreesToRadians(45.0));
		}

		float TestAsin()
		{
			return Math::RadiansToDegrees(Math::Asin(1.0));
		}

		float TestAcos()
		{
			return Math::RadiansToDegrees(Math::Acos(0.0));
		}

		float TestAtan()
		{
			return Math::RadiansToDegrees(Math::Atan(1.0));
		}

		float TestAtan2()
		{
			return Math::RadiansToDegrees(Math::Atan2(1.0, 1.0));
		}

		float TestFastAsin()
		{
			return Math::RadiansToDegrees(Math::FastAsin(0.5));
		}

		float TestSinh()
		{
			return Math::Sinh(0.0);
		}

		float TestFloatOverload()
		{
			float Angle = Math::DegreesToRadians(30.0f);
			return Math::Sin(Angle) + Math::Cos(Angle) + Math::Tan(0.0f);
		}

		float TestDegreesToRadians()
		{
			return Math::DegreesToRadians(180.0);
		}

		float TestRadiansToDegrees()
		{
			return Math::RadiansToDegrees(PI);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSin()"), 1.0f, TEXT("Math::Sin()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestCos()"), 1.0f, TEXT("Math::Cos()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestTan()"), 1.0f, TEXT("Math::Tan()"), 0.01);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAsin()"), 90.0f, TEXT("Math::Asin()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAcos()"), 90.0f, TEXT("Math::Acos()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAtan()"), 45.0f, TEXT("Math::Atan()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAtan2()"), 45.0f, TEXT("Math::Atan2()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFastAsin()"), 30.0f, TEXT("Math::FastAsin()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSinh()"), 0.0f, TEXT("Math::Sinh()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFloatOverload()"), 1.366f, TEXT("Math trig float32 overloads"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDegreesToRadians()"), PI, TEXT("Math::DegreesToRadians()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestRadiansToDegrees()"), 180.0f, TEXT("Math::RadiansToDegrees()"), 0.001);
	}

	// -------------------------------------------------------------------------
	// Power and root functions: Pow, Sqrt, Exp, Log, Log2
	// -------------------------------------------------------------------------
	TEST_METHOD(PowerAndRootFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_PowerRoot", ASTEST_AS(R"AS(
		float TestPow()
		{
			return Math::Pow(2.0, 3.0);
		}

		float TestSquare()
		{
			return Math::Square(5.0);
		}

		float TestSqrt()
		{
			return Math::Sqrt(16.0);
		}

		float TestInvSqrt()
		{
			return Math::InvSqrt(4.0);
		}

		float TestExp()
		{
			return Math::Exp(0.0);
		}

		float TestLoge()
		{
			return Math::Loge(Math::Exp(2.0));
		}

		float TestLogX()
		{
			return Math::LogX(2.0, 8.0);
		}

		float TestLog2()
		{
			return Math::Log2(8.0);
		}

		float TestExp2()
		{
			return Math::Exp2(3.0);
		}

		float TestFloatPowSqrtLog()
		{
			return Math::Pow(3.0f, 2.0f) + Math::Sqrt(25.0f) + Math::Log2(16.0f);
		}

		float TestInvSqrtEst()
		{
			return Math::InvSqrtEst(4.0f);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestPow()"), 8.0f, TEXT("Math::Pow()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSquare()"), 25.0f, TEXT("Math::Square()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSqrt()"), 4.0f, TEXT("Math::Sqrt()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestInvSqrt()"), 0.5f, TEXT("Math::InvSqrt()"), 0.01);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestExp()"), 1.0f, TEXT("Math::Exp()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestLoge()"), 2.0f, TEXT("Math::Loge()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestLogX()"), 3.0f, TEXT("Math::LogX()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestLog2()"), 3.0f, TEXT("Math::Log2()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestExp2()"), 8.0f, TEXT("Math::Exp2()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFloatPowSqrtLog()"), 18.0f, TEXT("Math power/root float32 overloads"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestInvSqrtEst()"), 0.5f, TEXT("Math::InvSqrtEst()"), 0.05);
	}

	// -------------------------------------------------------------------------
	// Rounding functions: Floor, Ceil, Round, Trunc
	// -------------------------------------------------------------------------
	TEST_METHOD(RoundingFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_Rounding", ASTEST_AS(R"AS(
		float TestFloor()
		{
			return Math::FloorToFloat(3.7);
		}

		int32 TestFloorToInt()
		{
			return Math::FloorToInt(3.7);
		}

		float TestCeil()
		{
			return Math::CeilToFloat(3.2);
		}

		int32 TestCeilToInt()
		{
			return Math::CeilToInt(3.2);
		}

		float TestRound()
		{
			return Math::RoundToFloat(3.5);
		}

		int32 TestRoundToInt()
		{
			return Math::RoundToInt(3.5);
		}

		float TestTrunc()
		{
			return Math::TruncToFloat(3.9);
		}

		int32 TestTruncToInt()
		{
			return Math::TruncToInt(3.9);
		}

		float TestFractional()
		{
			return Math::Fractional(3.7);
		}

		float TestModulo()
		{
			return Math::Fmod(10.0, 3.0);
		}

		float TestNegativeRounding()
		{
			return Math::FloorToFloat(-3.2) + Math::CeilToFloat(-3.8) + Math::RoundToFloat(-3.5) + Math::TruncToFloat(-3.9);
		}

		float TestToFloatRounding()
		{
			return Math::FloorToFloat(3.7f) + Math::CeilToFloat(3.2f) + Math::RoundToFloat(3.5f) + Math::TruncToFloat(3.9f);
		}

		double TestToDoubleRounding()
		{
			return Math::FloorToDouble(3.7) + Math::CeilToDouble(3.2) + Math::RoundToDouble(3.5) + Math::TruncToDouble(3.9);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFloor()"), 3.0f, TEXT("Math::Floor()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestFloorToInt()"), 3, TEXT("Math::FloorToInt()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestCeil()"), 4.0f, TEXT("Math::Ceil()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestCeilToInt()"), 4, TEXT("Math::CeilToInt()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestRound()"), 4.0f, TEXT("Math::Round()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestRoundToInt()"), 4, TEXT("Math::RoundToInt()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestTrunc()"), 3.0f, TEXT("Math::Trunc()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestTruncToInt()"), 3, TEXT("Math::TruncToInt()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFractional()"), 0.7f, TEXT("Math::Fractional()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestModulo()"), 1.0f, TEXT("Math::Fmod()"), 0.001);
		// FloorToFloat(-3.2)=-4, CeilToFloat(-3.8)=-3, RoundToFloat(-3.5)=floor(-3.0)=-3,
		// TruncToFloat(-3.9)=-3  =>  sum = -13.0
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestNegativeRounding()"), -13.0f, TEXT("Math rounding handles negative inputs"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestToFloatRounding()"), 14.0f, TEXT("Math *ToFloat rounding helpers"), 0.001);
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestToDoubleRounding()"), 14.0, TEXT("Math *ToDouble rounding helpers"), 0.001);
	}

	// -------------------------------------------------------------------------
	// Absolute value and sign functions: Abs, Sign
	// -------------------------------------------------------------------------
	TEST_METHOD(AbsoluteAndSignFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_AbsSign", ASTEST_AS(R"AS(
		float TestAbsFloat()
		{
			return Math::Abs(-5.5);
		}

		int32 TestAbsInt()
		{
			return Math::Abs(-10);
		}

		float TestSignFloat()
		{
			return Math::Sign(-5.5);
		}

		int32 TestSignInt()
		{
			return Math::Sign(10);
		}

		float TestSignZero()
		{
			return Math::Sign(0.0);
		}

		float TestAbsAndSignFloat32()
		{
			return Math::Abs(-2.25f) + Math::Sign(12.0f) + Math::Sign(-12.0f);
		}

		double TestAbsAndSignFloat64()
		{
			return Math::Abs(-8.5) + Math::Sign(12.0) + Math::Sign(-12.0);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAbsFloat()"), 5.5f, TEXT("Math::Abs() float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestAbsInt()"), 10, TEXT("Math::Abs() int"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSignFloat()"), -1.0f, TEXT("Math::Sign() float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestSignInt()"), 1, TEXT("Math::Sign() int"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSignZero()"), 0.0f, TEXT("Math::Sign() zero"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAbsAndSignFloat32()"), 2.25f, TEXT("Math::Abs()/Sign() float32 overloads"), 0.001);
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestAbsAndSignFloat64()"), 8.5, TEXT("Math::Abs()/Sign() float64 overloads"), 0.001);
	}

	// -------------------------------------------------------------------------
	// Min/Max/Clamp functions
	// -------------------------------------------------------------------------
	TEST_METHOD(MinMaxClampFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_MinMaxClamp", ASTEST_AS(R"AS(
		float TestMinFloat()
		{
			return Math::Min(5.0, 3.0);
		}

		int32 TestMinInt()
		{
			return Math::Min(5, 3);
		}

		float TestMaxFloat()
		{
			return Math::Max(5.0, 3.0);
		}

		int32 TestMaxInt()
		{
			return Math::Max(5, 3);
		}

		float TestClampFloat()
		{
			return Math::Clamp(15.0, 0.0, 10.0);
		}

		int32 TestClampInt()
		{
			return Math::Clamp(-5, 0, 10);
		}

		float TestClampInRange()
		{
			return Math::Clamp(5.0, 0.0, 10.0);
		}

		double TestClampDouble()
		{
			return Math::Clamp(-2.5, -1.0, 4.0);
		}

		float TestMax3()
		{
			return Math::Max3(1.0f, 7.0f, 3.0f);
		}

		bool TestWithinRanges()
		{
			return Math::IsWithin(5.0, 0.0, 10.0)
				&& !Math::IsWithin(10.0, 0.0, 10.0)
				&& Math::IsWithinInclusive(10.0, 0.0, 10.0);
		}

		float TestClampAngleAndUnwind()
		{
			return Math::ClampAngle(30.0f, -45.0f, 45.0f) + Math::UnwindDegrees(450.0f);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestMinFloat()"), 3.0f, TEXT("Math::Min() float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestMinInt()"), 3, TEXT("Math::Min() int"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestMaxFloat()"), 5.0f, TEXT("Math::Max() float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestMaxInt()"), 5, TEXT("Math::Max() int"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestClampFloat()"), 10.0f, TEXT("Math::Clamp() float clamped to max"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestClampInt()"), 0, TEXT("Math::Clamp() int clamped to min"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestClampInRange()"), 5.0f, TEXT("Math::Clamp() float in range"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestClampDouble()"), -1.0, TEXT("Math::Clamp() double clamped to min"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestMax3()"), 7.0f, TEXT("Math::Max3() float"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestWithinRanges()"), true, TEXT("Math::IsWithin()/IsWithinInclusive()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestClampAngleAndUnwind()"), 120.0f, TEXT("Math angle clamp/unwind helpers"), 0.001);
	}

	TEST_METHOD(SpecialValueClassificationFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_SpecialValues", ASTEST_AS(R"AS(
		bool TestFloatNaNConstant()
		{
			float NanValue = NAN_flt;
			return Math::IsNaN(NanValue);
		}

		bool TestDoubleNaNConstant()
		{
			double NanValue = NAN_dbl;
			return Math::IsNaN(NanValue);
		}

		bool TestFloatGeneratedNaN()
		{
			float NanValue = NAN_flt * 1.0f;
			return Math::IsNaN(NanValue) && !Math::IsFinite(NanValue);
		}

		bool TestDoubleGeneratedNaN()
		{
			double NanValue = NAN_dbl * 1.0;
			return Math::IsNaN(NanValue) && !Math::IsFinite(NanValue);
		}

		bool TestFloatPositiveInfinity()
		{
			float InfValue = Math::Exp(1000.0f);
			return !Math::IsNaN(InfValue) && !Math::IsFinite(InfValue) && InfValue > 0.0f;
		}

		bool TestFloatNegativeInfinity()
		{
			float InfValue = -Math::Exp(1000.0f);
			return !Math::IsNaN(InfValue) && !Math::IsFinite(InfValue) && InfValue < 0.0f;
		}

		bool TestDoublePositiveInfinity()
		{
			double InfValue = Math::Exp(1000.0);
			return !Math::IsNaN(InfValue) && !Math::IsFinite(InfValue) && InfValue > 0.0;
		}

		bool TestDoubleNegativeInfinity()
		{
			double InfValue = -Math::Exp(1000.0);
			return !Math::IsNaN(InfValue) && !Math::IsFinite(InfValue) && InfValue < 0.0;
		}

		bool TestFloatFiniteValue()
		{
			return Math::IsFinite(123.456f) && !Math::IsNaN(123.456f);
		}

		bool TestDoubleFiniteValue()
		{
			return Math::IsFinite(123.456) && !Math::IsNaN(123.456);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNaNConstant()"), true, TEXT("Math::IsNaN detects NAN_flt"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNConstant()"), true, TEXT("Math::IsNaN detects NAN_dbl"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatGeneratedNaN()"), true, TEXT("Math::IsNaN detects NAN_flt expression result"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleGeneratedNaN()"), true, TEXT("Math::IsNaN detects NAN_dbl expression result"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatPositiveInfinity()"), true, TEXT("Math::IsFinite rejects float overflow Inf"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegativeInfinity()"), true, TEXT("Math::IsFinite rejects float overflow -Inf"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoublePositiveInfinity()"), true, TEXT("Math::IsFinite rejects double overflow Inf"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegativeInfinity()"), true, TEXT("Math::IsFinite rejects double overflow -Inf"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatFiniteValue()"), true, TEXT("Math::IsFinite accepts finite float"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleFiniteValue()"), true, TEXT("Math::IsFinite accepts finite double"));
	}

	// -------------------------------------------------------------------------
	// Interpolation functions: Lerp, Smoothstep, InterpEaseIn/Out
	// -------------------------------------------------------------------------
	TEST_METHOD(InterpolationFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_Interp", ASTEST_AS(R"AS(
		float TestLerpFloat()
		{
			return Math::Lerp(0.0, 10.0, 0.5);
		}

		float TestLerpStableFloat()
		{
			return Math::LerpStable(0.0, 10.0, 0.5);
		}

		float TestInterpEaseIn()
		{
			return Math::EaseIn(0.0, 10.0, 0.5, 2.0);
		}

		float TestInterpEaseOut()
		{
			return Math::EaseOut(0.0, 10.0, 0.5, 2.0);
		}

		float TestInterpEaseInOut()
		{
			return Math::EaseInOut(0.0, 10.0, 0.5, 2.0);
		}

		float TestSmoothStep()
		{
			return Math::SmoothStep(0.0, 10.0, 0.5);
		}

		FVector TestVectorLerp()
		{
			return Math::Lerp(FVector(0, 0, 0), FVector(10, 20, 30), 0.5);
		}

		FVector TestVectorComponentLerp()
		{
			return Math::VLerp(FVector(0, 0, 0), FVector(10, 20, 30), FVector(0.1, 0.5, 1.0));
		}

		FVector TestVectorEaseIn()
		{
			return Math::EaseIn(FVector(0, 0, 0), FVector(10, 20, 30), 0.5f, 2.0f);
		}

		FVector TestVectorEaseOut()
		{
			return Math::EaseOut(FVector(0, 0, 0), FVector(10, 20, 30), 0.5f, 2.0f);
		}

		FVector TestVectorEaseInOut()
		{
			return Math::EaseInOut(FVector(0, 0, 0), FVector(10, 20, 30), 0.5f, 2.0f);
		}

		float TestFInterpConstantTo()
		{
			return Math::FInterpConstantTo(0.0f, 10.0f, 0.5f, 4.0f);
		}

		float TestFInterpTo()
		{
			return Math::FInterpTo(0.0f, 10.0f, 0.5f, 1.0f);
		}

		float TestMappedRange()
		{
			return Math::GetMappedRangeValueClamped(FVector2D(0, 10), FVector2D(0, 100), 15.0)
				+ Math::GetMappedRangeValueUnclamped(FVector2D(0, 10), FVector2D(0, 100), 15.0);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestLerpFloat()"), 5.0f, TEXT("Math::Lerp() float"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestLerpStableFloat()"), 5.0f, TEXT("Math::LerpStable() float"));

		// Ease functions return values depend on implementation details, just verify they execute within the interpolation range.
		ExpectGlobalFloatRange(Engine, Module, TEXT("float TestInterpEaseIn()"), 0.0f, 10.0f, TEXT("Math::EaseIn() executes"));
		ExpectGlobalFloatRange(Engine, Module, TEXT("float TestInterpEaseOut()"), 0.0f, 10.0f, TEXT("Math::EaseOut() executes"));
		ExpectGlobalFloatRange(Engine, Module, TEXT("float TestInterpEaseInOut()"), 0.0f, 10.0f, TEXT("Math::EaseInOut() executes"));
		ExpectGlobalFloatRange(Engine, Module, TEXT("float TestSmoothStep()"), 0.0f, 10.0f, TEXT("Math::SmoothStep() executes"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorLerp()"),
			[](const FVector& Result) { return Result.Equals(FVector(5, 10, 15), 0.001); },
			TEXT("Math::Lerp() vector overload"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorComponentLerp()"),
			[](const FVector& Result) { return Result.Equals(FVector(1, 10, 30), 0.001); },
			TEXT("Math::VLerp() component alpha overload"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorEaseIn()"),
			[](const FVector& Result) { return Result.Equals(FVector(2.5, 5, 7.5), 0.001); },
			TEXT("Math::EaseIn() vector overload"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorEaseOut()"),
			[](const FVector& Result) { return Result.Equals(FVector(7.5, 15, 22.5), 0.001); },
			TEXT("Math::EaseOut() vector overload"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorEaseInOut()"),
			[](const FVector& Result) { return Result.Equals(FVector(5, 10, 15), 0.001); },
			TEXT("Math::EaseInOut() vector overload"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFInterpConstantTo()"), 2.0f, TEXT("Math::FInterpConstantTo()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestFInterpTo()"), 5.0f, TEXT("Math::FInterpTo()"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestMappedRange()"), 250.0f, TEXT("Math mapped range helpers"), 0.001);
	}

	TEST_METHOD(ScalarCurveAndUtilityFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_ScalarCurvesUtilities", ASTEST_AS(R"AS(
		float TestSinusoidalInOut()
		{
			return Math::SinusoidalIn(0.0f, 10.0f, 0.0f)
				+ Math::SinusoidalOut(0.0f, 10.0f, 1.0f)
				+ Math::SinusoidalInOut(0.0f, 10.0f, 0.5f);
		}

		float TestExpoAndCircularFamilies()
		{
			return Math::ExpoIn(0.0f, 10.0f, 1.0f)
				+ Math::ExpoOut(0.0f, 10.0f, 0.0f)
				+ Math::ExpoInOut(0.0f, 10.0f, 0.5f)
				+ Math::CircularIn(0.0f, 10.0f, 0.0f)
				+ Math::CircularOut(0.0f, 10.0f, 1.0f)
				+ Math::CircularInOut(0.0f, 10.0f, 0.5f);
		}

		float TestCubicInterp()
		{
			return Math::CubicInterp(0.0f, 0.0f, 10.0f, 0.0f, 0.5f);
		}

		float TestScalarUtilities()
		{
			return Math::GridSnap(12.3f, 5.0f)
				+ Math::NormalizeToRange(15.0, 10.0, 20.0)
				+ Math::MakePulsatingValue(0.25, 1.0f, 0.0f);
		}

		bool TestScalarPredicates()
		{
			return Math::IsNearlyEqual(1.0f, 1.00001f, 0.001f)
				&& Math::IsNearlyZero(0.00001f, 0.001f)
				&& Math::IsPowerOfTwo(64);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSinusoidalInOut()"), 15.0f, TEXT("Math sinusoidal interpolation family"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestExpoAndCircularFamilies()"), 30.0f, TEXT("Math expo/circular interpolation families"), 0.001);
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestCubicInterp()"), 5.0f, TEXT("Math::CubicInterp() scalar"), 0.001);
		ExpectGlobalFloatSatisfies(
			Engine,
			Module,
			TEXT("float TestScalarUtilities()"),
			[](float Result) { return Result >= 10.5f && Result <= 11.5f; },
			TEXT("Math scalar utility helpers should execute and keep pulse output normalized"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestScalarPredicates()"), true, TEXT("Math scalar predicate helpers"));
	}

	// -------------------------------------------------------------------------
	// Random functions: FRand, RandRange, RandBool
	// -------------------------------------------------------------------------
	TEST_METHOD(RandomFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_Random", ASTEST_AS(R"AS(
		float TestFRand()
		{
			return Math::FRand();
		}

		float TestRandRange()
		{
			return Math::RandRange(10.0, 20.0);
		}

		int32 TestRandRangeInt()
		{
			return Math::RandRange(10, 20);
		}

		bool TestRandBool()
		{
			return Math::RandBool();
		}

		int32 TestRandHelper()
		{
			return Math::RandHelper(5);
		}

		int32 TestRand()
		{
			return Math::Rand();
		}

		FVector TestVRand()
		{
			return Math::VRand();
		}

		FVector2D TestRandPointInCircle()
		{
			return Math::RandPointInCircle(5.0f);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Random functions - verify they execute and return values in expected ranges
		ExpectGlobalFloatRange(Engine, Module, TEXT("float TestFRand()"), 0.0f, 1.0f, TEXT("Math::FRand() returns [0,1]"));
		ExpectGlobalFloatRange(Engine, Module, TEXT("float TestRandRange()"), 10.0f, 20.0f, TEXT("Math::RandRange() float in range"));

		ASSERT_THAT(IsNotNull(Module, TEXT("math random module should compile before executing int range function")));
		if (Module == nullptr)
		{
			return;
		}
		FASGlobalFunctionInvoker RandRangeIntInvoker(*TestRunner, Engine, *Module, TEXT("int32 TestRandRangeInt()"));
		ASSERT_THAT(IsTrue(RandRangeIntInvoker.IsValid(), TEXT("TestRandRangeInt should resolve and prepare")));
		if (!RandRangeIntInvoker.IsValid())
		{
			return;
		}
		const int32 RandRangeIntResult = RandRangeIntInvoker.ExecuteAndGet<int32>(0);
		ASSERT_THAT(IsTrue(RandRangeIntResult >= 10 && RandRangeIntResult <= 20, TEXT("Math::RandRange() int in range")));

		FASGlobalFunctionInvoker RandBoolInvoker(*TestRunner, Engine, *Module, TEXT("bool TestRandBool()"));
		ASSERT_THAT(IsTrue(RandBoolInvoker.IsValid(), TEXT("TestRandBool should resolve and prepare")));
		if (!RandBoolInvoker.IsValid())
		{
			return;
		}
		const bool RandBoolResult = RandBoolInvoker.ExecuteAndGet<bool>(false);
		TestRunner->AddInfo(FString::Printf(TEXT("Math::RandBool() returned: %s"), RandBoolResult ? TEXT("true") : TEXT("false")));

		FASGlobalFunctionInvoker RandHelperInvoker(*TestRunner, Engine, *Module, TEXT("int32 TestRandHelper()"));
		ASSERT_THAT(IsTrue(RandHelperInvoker.IsValid(), TEXT("TestRandHelper should resolve and prepare")));
		if (!RandHelperInvoker.IsValid())
		{
			return;
		}
		const int32 RandHelperResult = RandHelperInvoker.ExecuteAndGet<int32>(-1);
		ASSERT_THAT(IsTrue(RandHelperResult >= 0 && RandHelperResult < 5, TEXT("Math::RandHelper() returns [0, Max)")));

		FASGlobalFunctionInvoker RandInvoker(*TestRunner, Engine, *Module, TEXT("int32 TestRand()"));
		ASSERT_THAT(IsTrue(RandInvoker.IsValid(), TEXT("TestRand should resolve and prepare")));
		if (!RandInvoker.IsValid())
		{
			return;
		}
		const int32 RandResult = RandInvoker.ExecuteAndGet<int32>(0);
		ASSERT_THAT(IsTrue(RandResult >= 0, TEXT("Math::Rand() returns non-negative integer")));

		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVRand()"),
			[](const FVector& Result) { return FMath::IsNearlyEqual(Result.Size(), 1.0, 0.001); },
			TEXT("Math::VRand() returns a unit vector"));
		ExpectGlobalStructSatisfies<FVector2D>(
			Engine,
			Module,
			TEXT("FVector2D TestRandPointInCircle()"),
			[](const FVector2D& Result) { return Result.Size() <= 5.001; },
			TEXT("Math::RandPointInCircle() stays within radius"));
	}

	// -------------------------------------------------------------------------
	// Vector-specific Math functions
	// -------------------------------------------------------------------------
	TEST_METHOD(VectorMathFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_Vector", ASTEST_AS(R"AS(
		float TestVectorLength()
		{
			FVector v = FVector(3, 4, 0);
			return v.Size();
		}

		FVector TestVectorNormalize()
		{
			FVector v = FVector(10, 0, 0);
			return v.GetSafeNormal();
		}

		bool TestVectorNormalizeInPlace()
		{
			FVector v = FVector(0, 6, 8);
			return v.Normalize() && v.Equals(FVector(0, 0.6, 0.8), 0.001);
		}

		double TestVectorMemberDotProduct()
		{
			return FVector(1, 2, 3).DotProduct(FVector(4, 5, 6));
		}

		FVector TestVectorMemberCrossProduct()
		{
			return FVector(1, 0, 0).CrossProduct(FVector(0, 1, 0));
		}

		double TestVectorMixinAngularDistance()
		{
			return Math::RadiansToDegrees(FVector(1, 0, 0).AngularDistance(FVector(0, 1, 0)));
		}

		FVector TestVectorMixinConstrainToDirection()
		{
			return FVector(3, 4, 0).ConstrainToDirection(FVector(1, 0, 0));
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestVectorLength()"), 5.0f, TEXT("Vector.Length()"));

		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorNormalize()"),
			[](const FVector& Result) { return Result.Equals(FVector(1, 0, 0), 0.001); },
			TEXT("Vector.GetSafeNormal()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestVectorNormalizeInPlace()"), true, TEXT("FVector.Normalize() in-place"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestVectorMemberDotProduct()"), 32.0, TEXT("FVector.DotProduct() member"));

		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorMemberCrossProduct()"),
			[](const FVector& Result) { return Result.Equals(FVector(0, 0, 1), 0.001); },
			TEXT("FVector.CrossProduct() member"));

		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestVectorMixinAngularDistance()"), 90.0, TEXT("FVector.AngularDistance() mixin"), 0.001);

		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestVectorMixinConstrainToDirection()"),
			[](const FVector& Result) { return Result.Equals(FVector(3, 0, 0), 0.001); },
			TEXT("FVector.ConstrainToDirection() mixin"));
	}

	TEST_METHOD(UnsupportedVectorMathNamespaceBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compile each alias in isolation: a single combined expression only surfaces
		// the first unresolved-symbol diagnostic before the compiler bails out.
		auto ExpectAliasUnsupported = [this, &Engine](const TCHAR* ModuleSuffix, const FString& Source, const TCHAR* DiagnosticToken)
		{
			TArray<FString> ExpectedDiagnostics;
			ExpectedDiagnostics.Add(DiagnosticToken);
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				*FString::Printf(TEXT("ASCovMath_UnsupportedVectorMathNamespace_%s"), ModuleSuffix),
				Source,
				*FString::Printf(TEXT("unsupported Math::%s alias should remain a compile-failure boundary"), DiagnosticToken),
				MakeArrayView(ExpectedDiagnostics))));
		};

		ExpectAliasUnsupported(TEXT("DotProduct"), ASTEST_AS(R"AS(
		float Trigger()
		{
			return Math::DotProduct(FVector(1, 0, 0), FVector(0, 1, 0));
		}
		)AS"), TEXT("DotProduct"));

		ExpectAliasUnsupported(TEXT("CrossProduct"), ASTEST_AS(R"AS(
		float Trigger()
		{
			return Math::CrossProduct(FVector(1, 0, 0), FVector(0, 1, 0)).Z;
		}
		)AS"), TEXT("CrossProduct"));

		ExpectAliasUnsupported(TEXT("VectorLength"), ASTEST_AS(R"AS(
		float Trigger()
		{
			return Math::VectorLength(FVector(3, 4, 0));
		}
		)AS"), TEXT("VectorLength"));

		ExpectAliasUnsupported(TEXT("VectorNormalize"), ASTEST_AS(R"AS(
		float Trigger()
		{
			return Math::VectorNormalize(FVector(10, 0, 0)).X;
		}
		)AS"), TEXT("VectorNormalize"));
	}

	TEST_METHOD(GeometricMathFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_GeometricHelpers", ASTEST_AS(R"AS(
		bool TestLineBoxIntersection()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FVector start = FVector(-5, 5, 5);
			FVector end = FVector(15, 5, 5);
			return Math::LineBoxIntersection(box, start, end, end - start);
		}

		bool TestSphereAABBIntersection()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			return Math::SphereAABBIntersection(FVector(5, 5, 20), 121.0, box);
		}

		FVector TestRayPlaneIntersection()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			return Math::RayPlaneIntersection(FVector(0, 0, -5), FVector(0, 0, 1), plane);
		}

		FVector TestLinePlaneIntersectionWithOriginNormal()
		{
			return Math::LinePlaneIntersection(
				FVector(0, 0, -5),
				FVector(0, 0, 5),
				FVector::ZeroVector,
				FVector(0, 0, 1));
		}

		FVector TestClosestPointOnLine()
		{
			return Math::ClosestPointOnLine(FVector(0, 0, 0), FVector(10, 0, 0), FVector(3, 4, 0));
		}

		bool TestSegmentIntersection2DOutParam()
		{
			FVector intersection;
			bool hit = Math::SegmentIntersection2D(
				FVector(0, 0, 0),
				FVector(10, 10, 0),
				FVector(0, 10, 0),
				FVector(10, 0, 0),
				intersection);
			return hit && intersection.Equals(FVector(5, 5, 0), 0.001);
		}

		bool TestIsPointInBoxHelpers()
		{
			return Math::IsPointInBox(FVector(1, 2, 3), FVector::ZeroVector, FVector(5, 5, 5))
				&& Math::IsPointInBoxWithTransform(FVector(1, 2, 3), FTransform::Identity, FVector(5, 5, 5));
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestLineBoxIntersection()"), true, TEXT("Math::LineBoxIntersection()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestSphereAABBIntersection()"), true, TEXT("Math::SphereAABBIntersection()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestRayPlaneIntersection()"),
			[](const FVector& Result) { return Result.Equals(FVector::ZeroVector, 0.001); },
			TEXT("Math::RayPlaneIntersection()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestLinePlaneIntersectionWithOriginNormal()"),
			[](const FVector& Result) { return Result.Equals(FVector::ZeroVector, 0.001); },
			TEXT("Math::LinePlaneIntersection() origin/normal overload"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestClosestPointOnLine()"),
			[](const FVector& Result) { return Result.Equals(FVector(3, 0, 0), 0.001); },
			TEXT("Math::ClosestPointOnLine()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestSegmentIntersection2DOutParam()"), true, TEXT("Math::SegmentIntersection2D() out param"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsPointInBoxHelpers()"), true, TEXT("Math::IsPointInBox helpers"));
	}

	TEST_METHOD(VectorMethodMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_VectorMethods", ASTEST_AS(R"AS(
		float TestSizeSquared()
		{
			FVector SourceVector = FVector(3, 4, 12);
			return SourceVector.SizeSquared();
		}

		float TestSize2D()
		{
			FVector SourceVector = FVector(3, 4, 12);
			return SourceVector.Size2D();
		}

		float TestDistance()
		{
			FVector SourceVector = FVector(1, 2, 3);
			return SourceVector.Distance(FVector(4, 6, 3));
		}

		float TestDistSquared()
		{
			FVector SourceVector = FVector(1, 2, 3);
			return SourceVector.DistSquared(FVector(4, 6, 3));
		}

		bool TestIsNearlyZero()
		{
			return FVector(0.00001, 0.0, 0.0).IsNearlyZero(0.001);
		}

		bool TestIsZero()
		{
			return FVector::ZeroVector.IsZero();
		}

		bool TestIsUnit()
		{
			return FVector(1, 0, 0).IsUnit();
		}

		FVector TestGetClampedToSize()
		{
			FVector SourceVector = FVector(10, 0, 0);
			return SourceVector.GetClampedToSize(0, 5);
		}

		FVector TestProjectOnTo()
		{
			FVector SourceVector = FVector(3, 4, 0);
			return SourceVector.ProjectOnTo(FVector(1, 0, 0));
		}

		FVector TestProjectOnToNormal()
		{
			FVector SourceVector = FVector(3, 4, 0);
			return SourceVector.ProjectOnToNormal(FVector(1, 0, 0));
		}

		FVector TestRotateAngleAxis()
		{
			FVector SourceVector = FVector(1, 0, 0);
			return SourceVector.RotateAngleAxis(90, FVector(0, 0, 1));
		}

		bool TestRotation()
		{
			FRotator Rotation = FVector(1, 0, 0).Rotation();
			return Math::Abs(Rotation.Pitch) < 0.01 && Math::Abs(Rotation.Yaw) < 0.01 && Math::Abs(Rotation.Roll) < 0.01;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector method matrix module should compile")));

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSizeSquared()"), 169.0f, TEXT("FVector.SizeSquared()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestSize2D()"), 5.0f, TEXT("FVector.Size2D()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDistance()"), 5.0f, TEXT("FVector.Distance()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDistSquared()"), 25.0f, TEXT("FVector.DistSquared()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNearlyZero()"), true, TEXT("FVector.IsNearlyZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsZero()"), true, TEXT("FVector.IsZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsUnit()"), true, TEXT("FVector.IsUnit()"));

		auto ExpectVectorReturn = [this, &Engine, Module](const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message)
		{
			ExpectGlobalStructSatisfies<FVector>(
				Engine,
				Module,
				Declaration,
				[Expected](const FVector& Result) { return Result.Equals(Expected, 0.001); },
				Message);
		};

		ExpectVectorReturn(TEXT("FVector TestGetClampedToSize()"), FVector(5, 0, 0), TEXT("FVector.GetClampedToSize()"));
		ExpectVectorReturn(TEXT("FVector TestProjectOnTo()"), FVector(3, 0, 0), TEXT("FVector.ProjectOnTo()"));
		ExpectVectorReturn(TEXT("FVector TestProjectOnToNormal()"), FVector(3, 0, 0), TEXT("FVector.ProjectOnToNormal()"));
		ExpectVectorReturn(TEXT("FVector TestRotateAngleAxis()"), FVector(0, 1, 0), TEXT("FVector.RotateAngleAxis()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestRotation()"), true, TEXT("FVector.Rotation()"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
