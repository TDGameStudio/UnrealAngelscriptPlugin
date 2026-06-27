#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFloatExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript float-family *expression usage* -- the script-side
// half of the float matrix. This file covers:
//
//   * Local/global declarations
//   * Arithmetic operators (+ - * / %)
//   * Comparison operators (== != < <= > >=)
//   * Compound assignment (+=, -=, *=, /=, %=)
//   * Increment/decrement (++, --)
//   * Literals (decimal, scientific notation, f suffix)
//   * Type conversions (float↔double, float↔int)
//   * Class members (non-UPROPERTY)
//
// Test patterns:
//   - Pattern B: Global functions returning values
//   - Pattern F: ExpectGlobalReturn helper
//
// float family under test:
//   float / double
//
// Note: Float has NO bitwise or shift operators (unlike int).
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFloatExpressionTest,
	"Angelscript.TestModule.Coverage.FloatExpression",
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

	// Helper: build module + expect global return
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		const T Result = Invoker.ExecuteAndGet<T>(T{});

		// For float/double, use tolerance-based comparison
		if constexpr (std::is_same_v<T, float>)
		{
			TestRunner->TestTrue(Message, FMath::IsNearlyEqual(Result, Expected, 0.0001f));
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			TestRunner->TestTrue(Message, FMath::IsNearlyEqual(Result, Expected, 0.00001));
		}
		else
		{
			TestRunner->TestEqual(Message, Result, Expected);
		}
	}

	// -------------------------------------------------------------------------
	// Local declaration contexts: default init, deferred init, const, auto.
	// -------------------------------------------------------------------------
	TEST_METHOD(LocalDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_LocalDecl", ASTEST_AS(R"AS(
		float LocalDefaultInit()
		{
			float Value = 5.5f;
			return Value;
		}

		float LocalDeferredInit()
		{
			float Value;
			Value = 7.7f;
			return Value;
		}

		float LocalConst()
		{
			const float Value = 9.9f;
			return Value;
		}

		float LocalAutoFloat()
		{
			auto Value = 11.11f;
			return Value;
		}

		double LocalAutoDouble()
		{
			auto Value = 13.13;
			return Value;
		}

		double LocalDouble()
		{
			double Value = 1.234567890123456;
			return Value;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalDefaultInit()"),  5.5f,  TEXT("local float with default initializer"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalDeferredInit()"), 7.7f,  TEXT("local float declared then assigned"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalConst()"),        9.9f,  TEXT("local const float"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalAutoFloat()"),   11.11f, TEXT("auto infers float from f suffix"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LocalAutoDouble()"), 13.13,  TEXT("auto infers double from no suffix"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LocalDouble()"),     1.234567890123456, TEXT("local double"));
	}

	// -------------------------------------------------------------------------
	// Module-level const globals (float / double).
	// -------------------------------------------------------------------------
	TEST_METHOD(GlobalConstDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_GlobalConst", ASTEST_AS(R"AS(
		const float GConstFloat = 3.14159f;
		const double GConstDouble = 2.718281828459045;

		float GetGlobalFloat()
		{
			return GConstFloat;
		}

		double GetGlobalDouble()
		{
			return GConstDouble;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetGlobalFloat()"),   3.14159f, TEXT("global const float"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double GetGlobalDouble()"), 2.718281828459045, TEXT("global const double"));
	}

	// -------------------------------------------------------------------------
	// Arithmetic operators: + - * / % and unary minus.
	// Note: float supports % (modulo), unlike some languages.
	// -------------------------------------------------------------------------
	TEST_METHOD(ArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Arithmetic", ASTEST_AS(R"AS(
		float OpAdd()      { return 1.5f + 2.5f; }
		float OpSubtract() { return 5.0f - 2.0f; }
		float OpMultiply() { return 3.0f * 4.0f; }
		float OpDivide()   { return 10.0f / 2.5f; }
		float OpModulo()   { return 7.5f % 2.0f; }
		float OpUnaryMinus() { return -(4.2f); }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpAdd()"),        4.0f,  TEXT("float addition"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpSubtract()"),   3.0f,  TEXT("float subtraction"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpMultiply()"),  12.0f,  TEXT("float multiplication"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpDivide()"),     4.0f,  TEXT("float division"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpModulo()"),     1.5f,  TEXT("float modulo"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpUnaryMinus()"), -4.2f, TEXT("float unary minus"));
	}

	// -------------------------------------------------------------------------
	// Comparison operators: == != < <= > >=
	// Note: Direct == comparison can be unreliable due to precision, but
	// we test it for coverage (in real code, use IsNearlyEqual).
	// -------------------------------------------------------------------------
	TEST_METHOD(ComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Comparison", ASTEST_AS(R"AS(
		bool OpEqual()        { return 3.14f == 3.14f; }
		bool OpNotEqual()     { return 3.14f != 2.71f; }
		bool OpLess()         { return 2.0f < 3.0f; }
		bool OpLessEqual()    { return 2.0f <= 2.0f; }
		bool OpGreater()      { return 5.0f > 3.0f; }
		bool OpGreaterEqual() { return 5.0f >= 5.0f; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEqual()"),        true,  TEXT("float =="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEqual()"),     true,  TEXT("float !="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLess()"),         true,  TEXT("float <"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLessEqual()"),    true,  TEXT("float <="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpGreater()"),      true,  TEXT("float >"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpGreaterEqual()"), true,  TEXT("float >="));
	}

	// -------------------------------------------------------------------------
	// Compound assignment and increment/decrement operators.
	// -------------------------------------------------------------------------
	TEST_METHOD(CompoundAssignmentOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_CompoundAssign", ASTEST_AS(R"AS(
		float OpAddAssign()
		{
			float x = 10.0f;
			x += 5.0f;
			return x;
		}

		float OpSubtractAssign()
		{
			float x = 10.0f;
			x -= 3.0f;
			return x;
		}

		float OpMultiplyAssign()
		{
			float x = 4.0f;
			x *= 2.5f;
			return x;
		}

		float OpDivideAssign()
		{
			float x = 20.0f;
			x /= 4.0f;
			return x;
		}

		float OpModuloAssign()
		{
			float x = 10.0f;
			x %= 3.0f;
			return x;
		}

		float OpPreIncrement()
		{
			float x = 5.0f;
			++x;
			return x;
		}

		float OpPostIncrement()
		{
			float x = 5.0f;
			float y = x++;
			return y;
		}

		float OpPreDecrement()
		{
			float x = 5.0f;
			--x;
			return x;
		}

		float OpPostDecrement()
		{
			float x = 5.0f;
			float y = x--;
			return y;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpAddAssign()"),      15.0f, TEXT("float +="));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpSubtractAssign()"),  7.0f, TEXT("float -="));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpMultiplyAssign()"), 10.0f, TEXT("float *="));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpDivideAssign()"),    5.0f, TEXT("float /="));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpModuloAssign()"),    1.0f, TEXT("float %="));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPreIncrement()"),    6.0f, TEXT("float ++x"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPostIncrement()"),   5.0f, TEXT("float x++"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPreDecrement()"),    4.0f, TEXT("float --x"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPostDecrement()"),   5.0f, TEXT("float x--"));
	}

	// -------------------------------------------------------------------------
	// Float literals: decimal, scientific notation, f suffix.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Literals", ASTEST_AS(R"AS(
		float LiteralDecimal()      { return 123.456f; }
		float LiteralScientific()   { return 1.5e2f; }
		double LiteralNoSuffix()    { return 3.14159; }
		float LiteralWithSuffix()   { return 2.71828f; }
		float LiteralIntToFloat()   { return 42; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralDecimal()"),    123.456f, TEXT("decimal literal"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralScientific()"),  150.0f,  TEXT("scientific notation 1.5e2"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LiteralNoSuffix()"),   3.14159, TEXT("no suffix defaults to double"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralWithSuffix()"),  2.71828f, TEXT("f suffix is float"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralIntToFloat()"),  42.0f,    TEXT("int literal converts to float"));
	}

	// -------------------------------------------------------------------------
	// Type conversions: float↔double, float↔int.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Conversion", ASTEST_AS(R"AS(
		double FloatToDouble()
		{
			float f = 3.14159f;
			return double(f);
		}

		float DoubleToFloat()
		{
			double d = 2.718281828459045;
			return float(d);
		}

		float IntToFloat()
		{
			int i = 42;
			return float(i);
		}

		int FloatToInt()
		{
			float f = 9.99f;
			return int(f);
		}

		double IntToDouble()
		{
			int i = 123;
			return double(i);
		}

		int DoubleToInt()
		{
			double d = 456.789;
			return int(d);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<double>(Engine, Module, TEXT("double FloatToDouble()"), 3.14159, TEXT("float widens to double"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DoubleToFloat()"), 2.71828f, TEXT("double truncates to float"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float IntToFloat()"), 42.0f, TEXT("int converts to float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int FloatToInt()"), 9, TEXT("float truncates to int"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double IntToDouble()"), 123.0, TEXT("int converts to double"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int DoubleToInt()"), 456, TEXT("double truncates to int"));
	}

	// -------------------------------------------------------------------------
	// Class members (non-UPROPERTY): script-visible float fields without
	// reflection, accessed directly within script code.
	// -------------------------------------------------------------------------
	TEST_METHOD(ClassMembersNonProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_ClassMember", ASTEST_AS(R"AS(
		class FloatHolder
		{
			float Value;
			double BigValue;

			FloatHolder()
			{
				Value = 3.14159f;
				BigValue = 2.718281828459045;
			}

			float GetValue() const
			{
				return Value;
			}

			void SetValue(float v)
			{
				Value = v;
			}

			double GetBigValue() const
			{
				return BigValue;
			}
		}

		float TestClassMemberAccess()
		{
			FloatHolder holder;
			return holder.Value;
		}

		float TestClassMemberModify()
		{
			FloatHolder holder;
			holder.Value = 1.41421f;
			return holder.GetValue();
		}

		double TestClassMemberDouble()
		{
			FloatHolder holder;
			return holder.BigValue;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestClassMemberAccess()"), 3.14159f, TEXT("class member float direct access"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestClassMemberModify()"), 1.41421f, TEXT("class member float modify"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestClassMemberDouble()"), 2.718281828459045, TEXT("class member double"));
	}

	// -------------------------------------------------------------------------
	// Special values: NaN, Inf, -Inf, and precision comparison.
	// Covers: NaN generation/detection, Inf generation/detection, IsNearlyEqual.
	// -------------------------------------------------------------------------
	TEST_METHOD(SpecialValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_SpecialValues", ASTEST_AS(R"AS(
		// NaN generation and detection (float)
		bool TestFloatNaNGeneration()
		{
			float nanValue = 0.0f / 0.0f;
			return Math::IsNaN(nanValue);
		}

		bool TestFloatNaNComparison()
		{
			float nanValue = 0.0f / 0.0f;
			// NaN should never equal itself
			return !(nanValue == nanValue);
		}

		// NaN generation and detection (double)
		bool TestDoubleNaNGeneration()
		{
			double nanValue = 0.0 / 0.0;
			return Math::IsNaN(nanValue);
		}

		bool TestDoubleNaNComparison()
		{
			double nanValue = 0.0 / 0.0;
			// NaN should never equal itself
			return !(nanValue == nanValue);
		}

		// Positive infinity generation and detection (float)
		bool TestFloatInfGeneration()
		{
			float infValue = 1.0f / 0.0f;
			return !Math::IsFinite(infValue) && infValue > 0.0f;
		}

		bool TestFloatInfComparison()
		{
			float infValue = 1.0f / 0.0f;
			return infValue > 1000000.0f;
		}

		// Negative infinity generation and detection (float)
		bool TestFloatNegInfGeneration()
		{
			float negInfValue = -1.0f / 0.0f;
			return !Math::IsFinite(negInfValue) && negInfValue < 0.0f;
		}

		bool TestFloatNegInfComparison()
		{
			float negInfValue = -1.0f / 0.0f;
			return negInfValue < -1000000.0f;
		}

		// Positive infinity generation and detection (double)
		bool TestDoubleInfGeneration()
		{
			double infValue = 1.0 / 0.0;
			return !Math::IsFinite(infValue) && infValue > 0.0;
		}

		bool TestDoubleInfComparison()
		{
			double infValue = 1.0 / 0.0;
			return infValue > 1000000.0;
		}

		// Negative infinity generation and detection (double)
		bool TestDoubleNegInfGeneration()
		{
			double negInfValue = -1.0 / 0.0;
			return !Math::IsFinite(negInfValue) && negInfValue < 0.0;
		}

		bool TestDoubleNegInfComparison()
		{
			double negInfValue = -1.0 / 0.0;
			return negInfValue < -1000000.0;
		}

		// IsFinite with normal values (float)
		bool TestFloatIsFiniteNormal()
		{
			return Math::IsFinite(123.456f);
		}

		// IsFinite with normal values (double)
		bool TestDoubleIsFiniteNormal()
		{
			return Math::IsFinite(123.456);
		}

		// Precision comparison using Math::IsNearlyEqual (float)
		bool TestFloatPrecisionComparison()
		{
			float a = 1.0f / 3.0f;
			float b = 0.333333f;
			return Math::IsNearlyEqual(a, b, 0.00001f);
		}

		bool TestFloatPrecisionComparisonFails()
		{
			float a = 1.0f;
			float b = 1.1f;
			return !Math::IsNearlyEqual(a, b, 0.01f);
		}

		// Precision comparison using Math::IsNearlyEqual (double)
		bool TestDoublePrecisionComparison()
		{
			double a = 1.0 / 3.0;
			double b = 0.333333333333;
			return Math::IsNearlyEqual(a, b, 0.000001);
		}

		bool TestDoublePrecisionComparisonFails()
		{
			double a = 1.0;
			double b = 1.1;
			return !Math::IsNearlyEqual(a, b, 0.01);
		}

		// Test -0.0 vs 0.0 (float)
		bool TestFloatNegativeZeroEquality()
		{
			float positiveZero = 0.0f;
			float negativeZero = -0.0f;
			// In floating point, -0.0 == 0.0
			return positiveZero == negativeZero;
		}

		// Test -0.0 vs 0.0 (double)
		bool TestDoubleNegativeZeroEquality()
		{
			double positiveZero = 0.0;
			double negativeZero = -0.0;
			// In floating point, -0.0 == 0.0
			return positiveZero == negativeZero;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// NaN tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNaNGeneration()"), true, TEXT("float NaN generation via 0.0f/0.0f"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNaNComparison()"), true, TEXT("float NaN != NaN"));

		// NaN tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNGeneration()"), true, TEXT("double NaN generation via 0.0/0.0"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNComparison()"), true, TEXT("double NaN != NaN"));

		// Positive Infinity tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatInfGeneration()"), true, TEXT("float Inf generation via 1.0f/0.0f"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatInfComparison()"), true, TEXT("float Inf > large number"));

		// Negative Infinity tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegInfGeneration()"), true, TEXT("float -Inf generation via -1.0f/0.0f"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegInfComparison()"), true, TEXT("float -Inf < large negative number"));

		// Positive Infinity tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleInfGeneration()"), true, TEXT("double Inf generation via 1.0/0.0"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleInfComparison()"), true, TEXT("double Inf > large number"));

		// Negative Infinity tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegInfGeneration()"), true, TEXT("double -Inf generation via -1.0/0.0"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegInfComparison()"), true, TEXT("double -Inf < large negative number"));

		// IsFinite tests
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatIsFiniteNormal()"), true, TEXT("float IsFinite with normal value"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleIsFiniteNormal()"), true, TEXT("double IsFinite with normal value"));

		// Precision comparison tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatPrecisionComparison()"), true, TEXT("float IsNearlyEqual for close values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatPrecisionComparisonFails()"), true, TEXT("float IsNearlyEqual fails for distant values"));

		// Precision comparison tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoublePrecisionComparison()"), true, TEXT("double IsNearlyEqual for close values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoublePrecisionComparisonFails()"), true, TEXT("double IsNearlyEqual fails for distant values"));

		// -0.0 vs 0.0 tests
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegativeZeroEquality()"), true, TEXT("float -0.0 == 0.0"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegativeZeroEquality()"), true, TEXT("double -0.0 == 0.0"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
