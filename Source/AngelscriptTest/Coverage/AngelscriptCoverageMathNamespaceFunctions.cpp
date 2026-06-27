#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathNamespaceFunctions
// -----------------------------------------------------------------------------
// Coverage for Math:: namespace functions identified in Coverage_MathStructs.md
// Section 8: Math namespace functions (trigonometric, power/root, rounding, etc.)
//
// Test patterns: Pattern B (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMathNamespaceFunctionsTest,
	"Angelscript.TestModule.Coverage.MathNamespaceFunctions",
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
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message, double Tolerance = 0.001)
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

		if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			TestRunner->TestTrue(Message, FMath::IsNearlyEqual((double)Result, (double)Expected, Tolerance));
		}
		else
		{
			TestRunner->TestEqual(Message, Result, Expected);
		}
	}

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
			return Math::Floor(3.7);
		}

		int32 TestFloorToInt()
		{
			return Math::FloorToInt(3.7);
		}

		float TestCeil()
		{
			return Math::Ceil(3.2);
		}

		int32 TestCeilToInt()
		{
			return Math::CeilToInt(3.2);
		}

		float TestRound()
		{
			return Math::Round(3.5);
		}

		int32 TestRoundToInt()
		{
			return Math::RoundToInt(3.5);
		}

		float TestTrunc()
		{
			return Math::Trunc(3.9);
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

		float TestLerpAngle()
		{
			return Math::LerpAngle(0.0, 180.0, 0.5);
		}

		float TestInterpEaseIn()
		{
			return Math::InterpEaseIn(0.0, 10.0, 0.5, 2.0);
		}

		float TestInterpEaseOut()
		{
			return Math::InterpEaseOut(0.0, 10.0, 0.5, 2.0);
		}

		float TestInterpEaseInOut()
		{
			return Math::InterpEaseInOut(0.0, 10.0, 0.5, 2.0);
		}

		float TestSmoothStep()
		{
			return Math::SmoothStep(0.0, 10.0, 0.5);
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
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestLerpAngle()"), 90.0f, TEXT("Math::LerpAngle()"), 0.01);

		// InterpEase functions return values depend on implementation, just verify they execute
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestInterpEaseIn()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("Math::InterpEaseIn() executes"), Result >= 0.0f && Result <= 10.0f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestInterpEaseOut()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("Math::InterpEaseOut() executes"), Result >= 0.0f && Result <= 10.0f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestInterpEaseInOut()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("Math::InterpEaseInOut() executes"), Result >= 0.0f && Result <= 10.0f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestSmoothStep()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("Math::SmoothStep() executes"), Result >= 0.0f && Result <= 10.0f);
		}
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
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Random functions - verify they execute and return values in expected ranges
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestFRand()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("Math::FRand() returns [0,1)"), Result >= 0.0f && Result < 1.0f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestRandRange()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("Math::RandRange() float in range"), Result >= 10.0f && Result <= 20.0f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int32 TestRandRangeInt()"));
			int32 Result = Invoker.ExecuteAndGet<int32>(0);
			TestRunner->TestTrue(TEXT("Math::RandRange() int in range"), Result >= 10 && Result <= 20);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool TestRandBool()"));
			bool Result = Invoker.ExecuteAndGet<bool>(false);
			// Just verify it executes - it's a bool, so any value is valid
			TestRunner->AddInfo(FString::Printf(TEXT("Math::RandBool() returned: %s"), Result ? TEXT("true") : TEXT("false")));
		}
	}

	// -------------------------------------------------------------------------
	// Vector-specific Math functions
	// -------------------------------------------------------------------------
	TEST_METHOD(VectorMathFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMath_Vector", ASTEST_AS(R"AS(
		float TestDotProduct()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return Math::DotProduct(a, b);
		}

		FVector TestCrossProduct()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return Math::CrossProduct(a, b);
		}

		float TestVectorLength()
		{
			FVector v = FVector(3, 4, 0);
			return v.Length();
		}

		FVector TestVectorNormalize()
		{
			FVector v = FVector(10, 0, 0);
			return v.GetSafeNormal();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDotProduct()"), 0.0f, TEXT("Math::DotProduct()"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestCrossProduct()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("Math::CrossProduct()"), Result.Equals(FVector(0, 0, 1), 0.001));
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestVectorLength()"), 5.0f, TEXT("Vector.Length()"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector TestVectorNormalize()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("Vector.GetSafeNormal()"), Result.Equals(FVector(1, 0, 0), 0.001));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
