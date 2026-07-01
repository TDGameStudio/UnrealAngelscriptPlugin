#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

#include <cmath>
#include <type_traits>

// -----------------------------------------------------------------------------
// AngelscriptCoverageFloatExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript float-family *expression usage* -- the script-side
// half of the float matrix. This file covers:
//
//   * Local/global declarations
//   * Arithmetic operators (+ - * /)
//   * Comparison operators (== != < <= > >=)
//   * Compound assignment (+=, -=, *=, /=)
//   * Increment/decrement (++, --)
//   * Literals (decimal, scientific notation, f suffix)
//   * Type conversions (float <-> double, float <-> int)
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

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFloatExpressionTest,
	"Angelscript.TestModule.Coverage.FloatExpression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	template <typename T>
	T ExecuteGlobalReturnValue(FAngelscriptEngine& Engine, FASGlobalFunctionInvoker& Invoker, const TCHAR* Declaration)
	{
		if constexpr (std::is_same_v<T, float>)
		{
			asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
			if (ScriptEngine == nullptr)
			{
				return 0.0f;
			}

			const FString DeclarationString(Declaration);
			const bool bExplicitFloat32Return = DeclarationString.StartsWith(TEXT("float32 "));
			const bool bReadReturnAsDouble = !bExplicitFloat32Return && ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
			if (bReadReturnAsDouble)
			{
				return static_cast<float>(Invoker.ExecuteAndGet<double>(0.0));
			}

			return Invoker.ExecuteAndGet<float>(0.0f);
		}
		else
		{
			return Invoker.ExecuteAndGet<T>(T{});
		}
	}

	// Helper: build module + expect global return
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("Float expression module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Float expression global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}
		const T Result = ExecuteGlobalReturnValue<T>(Engine, Invoker, Declaration);

		if constexpr (std::is_same_v<T, float>)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, Expected, 0.0001f), Message));
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, Expected, 0.00001), Message));
		}
		else
		{
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}
	}

	template <typename T, typename PredicateType>
	void ExpectGlobalReturnSatisfies(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, PredicateType Predicate, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("Float expression module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Float expression global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const T Result = ExecuteGlobalReturnValue<T>(Engine, Invoker, Declaration);
		ASSERT_THAT(IsTrue(Predicate(Result), Message));
	}

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

	// -------------------------------------------------------------------------
	// Local declaration contexts: default init, deferred init, const, auto.
	// -------------------------------------------------------------------------
	TEST_METHOD(LocalDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TestRunner->AddExpectedError(TEXT("'Value' may not be initialized"), EAutomationExpectedErrorFlags::Contains, 2);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_LocalDecl", ASTEST_AS(R"AS(
		float LocalNoDefault()
		{
			float Value;
			return Value;
		}

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

		double LocalDoubleNoDefault()
		{
			double Value;
			return Value;
		}

		double LocalDoubleConst()
		{
			const double Value = 10.25;
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

		ASSERT_THAT(IsNotNull(Module, TEXT("Float local declaration module should compile before checking uninitialized boundary functions")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float LocalNoDefault()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("local float without initializer should compile and remain resolvable as a warning boundary")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double LocalDoubleNoDefault()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("local double without initializer should compile and remain resolvable as a warning boundary")));
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalDefaultInit()"),  5.5f,  TEXT("local float with default initializer"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalDeferredInit()"), 7.7f,  TEXT("local float declared then assigned"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalConst()"),        9.9f,  TEXT("local const float"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LocalDoubleConst()"), 10.25, TEXT("local const double"));
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
	// Arithmetic operators: + - * / and unary minus.
	// -------------------------------------------------------------------------
	TEST_METHOD(ArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Arithmetic", ASTEST_AS(R"AS(
		float OpAdd()
		{
			return 1.5f + 2.5f;
		}

		float OpSubtract()
		{
			return 5.0f - 2.0f;
		}

		float OpMultiply()
		{
			return 3.0f * 4.0f;
		}

		float OpDivide()
		{
			return 10.0f / 2.5f;
		}

		float OpUnaryMinus()
		{
			return -(4.2f);
		}

		double OpDoubleAdd()
		{
			return 1.25 + 2.75;
		}

		double OpDoubleSubtract()
		{
			return 5.5 - 1.25;
		}

		double OpDoubleMultiply()
		{
			return 2.5 * 4.0;
		}

		double OpDoubleDivide()
		{
			return 9.0 / 2.0;
		}

		double OpDoubleUnaryMinus()
		{
			return -(6.75);
		}
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
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpUnaryMinus()"), -4.2f, TEXT("float unary minus"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleAdd()"),        4.0,  TEXT("double addition"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleSubtract()"),   4.25, TEXT("double subtraction"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleMultiply()"),  10.0,  TEXT("double multiplication"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleDivide()"),     4.5,  TEXT("double division"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleUnaryMinus()"), -6.75, TEXT("double unary minus"));
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
		bool OpEqual()
		{
			return 3.14f == 3.14f;
		}

		bool OpNotEqual()
		{
			return 3.14f != 2.71f;
		}

		bool OpLess()
		{
			return 2.0f < 3.0f;
		}

		bool OpLessEqual()
		{
			return 2.0f <= 2.0f;
		}

		bool OpGreater()
		{
			return 5.0f > 3.0f;
		}

		bool OpGreaterEqual()
		{
			return 5.0f >= 5.0f;
		}

		bool OpDoubleEqual()
		{
			return 3.25 == 3.25;
		}

		bool OpDoubleNotEqual()
		{
			return 3.25 != 4.25;
		}

		bool OpDoubleLess()
		{
			return 2.25 < 3.25;
		}

		bool OpDoubleLessEqual()
		{
			return 2.25 <= 2.25;
		}

		bool OpDoubleGreater()
		{
			return 5.25 > 3.25;
		}

		bool OpDoubleGreaterEqual()
		{
			return 5.25 >= 5.25;
		}
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
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpDoubleEqual()"),        true, TEXT("double =="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpDoubleNotEqual()"),     true, TEXT("double !="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpDoubleLess()"),         true, TEXT("double <"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpDoubleLessEqual()"),    true, TEXT("double <="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpDoubleGreater()"),      true, TEXT("double >"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpDoubleGreaterEqual()"), true, TEXT("double >="));
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

		double OpDoubleAddAssign()
		{
			double x = 10.0;
			x += 5.5;
			return x;
		}

		double OpDoubleSubtractAssign()
		{
			double x = 10.0;
			x -= 3.25;
			return x;
		}

		double OpDoubleMultiplyAssign()
		{
			double x = 4.0;
			x *= 2.5;
			return x;
		}

		double OpDoubleDivideAssign()
		{
			double x = 20.0;
			x /= 4.0;
			return x;
		}

		double OpDoublePreIncrement()
		{
			double x = 5.0;
			++x;
			return x;
		}

		double OpDoublePostIncrement()
		{
			double x = 5.0;
			double y = x++;
			return y;
		}

		double OpDoublePreDecrement()
		{
			double x = 5.0;
			--x;
			return x;
		}

		double OpDoublePostDecrement()
		{
			double x = 5.0;
			double y = x--;
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
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPreIncrement()"),    6.0f, TEXT("float ++x"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPostIncrement()"),   5.0f, TEXT("float x++"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPreDecrement()"),    4.0f, TEXT("float --x"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float OpPostDecrement()"),   5.0f, TEXT("float x--"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleAddAssign()"),      15.5,  TEXT("double +="));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleSubtractAssign()"),  6.75, TEXT("double -="));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleMultiplyAssign()"), 10.0,  TEXT("double *="));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoubleDivideAssign()"),    5.0,  TEXT("double /="));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoublePreIncrement()"),    6.0,  TEXT("double ++x"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoublePostIncrement()"),   5.0,  TEXT("double x++"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoublePreDecrement()"),    4.0,  TEXT("double --x"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double OpDoublePostDecrement()"),   5.0,  TEXT("double x--"));
	}

	TEST_METHOD(TernaryOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Ternary", ASTEST_AS(R"AS(
		float PickPositiveFloat()
		{
			float Value = 2.0f;
			return Value > 0.0f ? 7.5f : -7.5f;
		}

		float PickNegativeFloat()
		{
			float Value = -2.0f;
			return Value > 0.0f ? 7.5f : -7.5f;
		}

		double PickPositiveDouble()
		{
			double Value = 2.0;
			return Value > 0.0 ? 8.25 : -8.25;
		}

		double PickNegativeDouble()
		{
			double Value = -2.0;
			return Value > 0.0 ? 8.25 : -8.25;
		}

		double PickMixedFloat32DoublePromotes()
		{
			bool bUseFloat32 = true;
			return bUseFloat32 ? float32(1.25f) : 2.5;
		}

		float32 PickExplicitFloat32Branch()
		{
			bool bUseFirst = false;
			return bUseFirst ? float32(9.5f) : float32(3.75f);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float PickPositiveFloat()"), 7.5f, TEXT("float ternary true branch"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float PickNegativeFloat()"), -7.5f, TEXT("float ternary false branch"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double PickPositiveDouble()"), 8.25, TEXT("double ternary true branch"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double PickNegativeDouble()"), -8.25, TEXT("double ternary false branch"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double PickMixedFloat32DoublePromotes()"), 1.25, TEXT("mixed float32/double ternary promotes to double"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float32 PickExplicitFloat32Branch()"), 3.75f, TEXT("explicit float32 ternary preserves branch value"));
	}

	// -------------------------------------------------------------------------
	// Float literals: decimal, scientific notation, f suffix.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Literals", ASTEST_AS(R"AS(
		float LiteralDecimal()
		{
			return 123.456f;
		}

		float LiteralScientific()
		{
			return 1.5e2f;
		}

		float LiteralScientificSmall()
		{
			return 1e-2f;
		}

		double LiteralNoSuffix()
		{
			return 3.14159;
		}

		double LiteralDoubleScientific()
		{
			return 1.5e3;
		}

		double LiteralDoubleScientificSmall()
		{
			return 1e-2;
		}

		float32 LiteralFloat32ScientificUppercase()
		{
			return float32(6.25E-1);
		}

		double LiteralDoubleScientificPositiveSign()
		{
			return 2.5e+2;
		}

		float LiteralWithSuffix()
		{
			return 2.71828f;
		}

		float LiteralNegative()
		{
			return -1.5f;
		}

		float LiteralIntToFloat()
		{
			return 42;
		}

		double LiteralIntToDouble()
		{
			return 43;
		}
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
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralScientificSmall()"), 0.01f, TEXT("scientific notation 1e-2f"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LiteralNoSuffix()"),   3.14159, TEXT("no suffix defaults to double"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LiteralDoubleScientific()"), 1500.0, TEXT("double scientific notation 1.5e3"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LiteralDoubleScientificSmall()"), 0.01, TEXT("double scientific notation 1e-2"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float32 LiteralFloat32ScientificUppercase()"), 0.625f, TEXT("float32 scientific notation with uppercase exponent"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LiteralDoubleScientificPositiveSign()"), 250.0, TEXT("double scientific notation with explicit positive exponent"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralWithSuffix()"),  2.71828f, TEXT("f suffix is float"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralNegative()"), -1.5f, TEXT("negative float literal via unary minus"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LiteralIntToFloat()"),  42.0f,    TEXT("int literal converts to float"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double LiteralIntToDouble()"), 43.0, TEXT("int literal converts to double"));
	}

	TEST_METHOD(FloatLiteralUnsupportedSuffixBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedDiagnostics = {
			TEXT("Expected ';'"),
			TEXT("Instead found identifier 'd'"),
		};
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovFloatExpr_DoubleSuffixUnsupported"),
			ASTEST_AS(R"AS(
			double LiteralDoubleSuffix()
			{
				return 1.25d;
			}
			)AS"),
			TEXT("double literal d suffix should remain unsupported; no-suffix literals are the supported double path"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Type conversions: float <-> double, float <-> int.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_Conversion", ASTEST_AS(R"AS(
		double FloatToDouble()
		{
			float f = 3.14159f;
			return f;
		}

		float DoubleToFloat()
		{
			double d = 2.718281828459045;
			return float(d);
		}

		float32 DoubleToFloat32()
		{
			float64 d = 2.718281828459045;
			return float32(d);
		}

		float64 Float32ToFloat64()
		{
			float32 f = 0.125f;
			return f;
		}

		float IntToFloat()
		{
			int i = 42;
			return i;
		}

		int FloatToInt()
		{
			float f = 9.99f;
			return int(f);
		}

		double IntToDouble()
		{
			int i = 123;
			return i;
		}

		int DoubleToInt()
		{
			double d = 456.789;
			return int(d);
		}

		int NegativeFloatToInt()
		{
			float f = -9.99f;
			return int(f);
		}

		int NegativeDoubleToInt()
		{
			double d = -456.789;
			return int(d);
		}

		float32 NegativeIntToFloat32()
		{
			int i = -42;
			return i;
		}

		int Float32ToInt()
		{
			float32 f = -3.75f;
			return int(f);
		}

		bool DoubleToFloatPrecisionLoss()
		{
			float64 d = 16777217.0;
			float32 f = float32(d);
			return float64(f) != d;
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
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float32 DoubleToFloat32()"), 2.71828f, TEXT("double explicitly narrows to float32"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("float64 Float32ToFloat64()"), 0.125, TEXT("float32 widens to float64"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float IntToFloat()"), 42.0f, TEXT("int converts to float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int FloatToInt()"), 9, TEXT("float truncates to int"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double IntToDouble()"), 123.0, TEXT("int converts to double"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int DoubleToInt()"), 456, TEXT("double truncates to int"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int NegativeFloatToInt()"), -9, TEXT("negative float truncates toward zero"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int NegativeDoubleToInt()"), -456, TEXT("negative double truncates toward zero"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float32 NegativeIntToFloat32()"), -42.0f, TEXT("negative int converts to float32"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int Float32ToInt()"), -3, TEXT("float32 truncates to int toward zero"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool DoubleToFloatPrecisionLoss()"), true, TEXT("double to float32 explicit conversion can lose precision"));
	}

	// -------------------------------------------------------------------------
	// Class members (non-UPROPERTY): script-visible float fields without
	// reflection, accessed directly within script code.
	// -------------------------------------------------------------------------
	// Script class members are a current execution boundary in this fork for
	// float / double, matching bool and FString-family expression coverage.
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

		ASSERT_THAT(IsNotNull(Module, TEXT("Float class member boundary module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASCovFloatExpr_ClassMember"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("TestClassMember"), EAutomationExpectedErrorFlags::Contains, 3);

		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("float TestClassMemberAccess()"),
			TEXT("float class member direct access currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("float TestClassMemberModify()"),
			TEXT("float class member modify currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("double TestClassMemberDouble()"),
			TEXT("double class member access currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
	}

	// -------------------------------------------------------------------------
	// Special values: NaN constants, Inf/-Inf generation, detection, and precision comparison.
	// Covers the bound NAN_flt / NAN_dbl constants and generated Inf/-Inf values without relying on divide-by-zero behavior.
	// -------------------------------------------------------------------------
	TEST_METHOD(SpecialValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatExpr_SpecialValues", ASTEST_AS(R"AS(
		// NaN generation and detection (float)
		bool TestFloatNaNConstant()
		{
			float nanValue = NAN_flt;
			return Math::IsNaN(nanValue);
		}

		bool TestFloatNaNGeneration()
		{
			float nanValue = NAN_flt * 1.0f;
			return Math::IsNaN(nanValue);
		}

		bool TestFloatNaNComparison()
		{
			float nanValue = NAN_flt;
			// NaN should never equal itself
			return !(nanValue == nanValue);
		}

		bool TestFloat32NaNFromSqrt()
		{
			float32 nanValue = Math::Sqrt(float32(-1.0f));
			return Math::IsNaN(nanValue) && !Math::IsFinite(nanValue);
		}

		bool TestFloatIsFiniteRejectsNaN()
		{
			return !Math::IsFinite(NAN_flt);
		}

		float ReturnFloatNaNConstant()
		{
			return NAN_flt;
		}

		// NaN generation and detection (double)
		bool TestDoubleNaNConstant()
		{
			double nanValue = NAN_dbl;
			return Math::IsNaN(nanValue);
		}

		bool TestDoubleNaNGeneration()
		{
			double nanValue = NAN_dbl * 1.0;
			return Math::IsNaN(nanValue);
		}

		bool TestDoubleNaNComparison()
		{
			double nanValue = NAN_dbl;
			// NaN should never equal itself
			return !(nanValue == nanValue);
		}

		bool TestDoubleNaNFromSqrt()
		{
			double nanValue = Math::Sqrt(-1.0);
			return Math::IsNaN(nanValue) && !Math::IsFinite(nanValue);
		}

		bool TestDoubleIsFiniteRejectsNaN()
		{
			return !Math::IsFinite(NAN_dbl);
		}

		double ReturnDoubleNaNConstant()
		{
			return NAN_dbl;
		}

		// Positive infinity generation and detection (float)
		bool TestFloatInfGeneration()
		{
			float infValue = Math::Exp(1000.0f);
			return !Math::IsFinite(infValue) && infValue > 0.0f;
		}

		bool TestFloatInfComparison()
		{
			float infValue = Math::Exp(1000.0f);
			return infValue > 1000000.0f;
		}

		float ReturnFloatInfGenerated()
		{
			return Math::Exp(1000.0f);
		}

		// Negative infinity generation and detection (float)
		bool TestFloatNegInfGeneration()
		{
			float negInfValue = -Math::Exp(1000.0f);
			return !Math::IsFinite(negInfValue) && negInfValue < 0.0f;
		}

		bool TestFloatNegInfComparison()
		{
			float negInfValue = -Math::Exp(1000.0f);
			return negInfValue < -1000000.0f;
		}

		float ReturnFloatNegInfGenerated()
		{
			return -Math::Exp(1000.0f);
		}

		// Positive infinity generation and detection (double)
		bool TestDoubleInfGeneration()
		{
			double infValue = Math::Exp(1000.0);
			return !Math::IsFinite(infValue) && infValue > 0.0;
		}

		bool TestDoubleInfComparison()
		{
			double infValue = Math::Exp(1000.0);
			return infValue > 1000000.0;
		}

		double ReturnDoubleInfGenerated()
		{
			return Math::Exp(1000.0);
		}

		// Negative infinity generation and detection (double)
		bool TestDoubleNegInfGeneration()
		{
			double negInfValue = -Math::Exp(1000.0);
			return !Math::IsFinite(negInfValue) && negInfValue < 0.0;
		}

		bool TestDoubleNegInfComparison()
		{
			double negInfValue = -Math::Exp(1000.0);
			return negInfValue < -1000000.0;
		}

		double ReturnDoubleNegInfGenerated()
		{
			return -Math::Exp(1000.0);
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

		bool TestFloatEpsilonBoundary()
		{
			float a = 1.0f;
			float b = a + 0.000001f;
			return Math::IsNearlyEqual(a, b, 0.00001f) && !Math::IsNearlyEqual(a, b, 0.0000001f);
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

		bool TestDoubleEpsilonBoundary()
		{
			double a = 1.0;
			double b = a + 0.000000001;
			return Math::IsNearlyEqual(a, b, 0.00000001) && !Math::IsNearlyEqual(a, b, 0.0000000001);
		}

		// Test -0.0 vs 0.0 (float)
		bool TestFloatNegativeZeroEquality()
		{
			float positiveZero = 0.0f;
			float negativeZero = -0.0f;
			return positiveZero == negativeZero;
		}

		bool TestFloatNegativeZeroSign()
		{
			float negativeZero = -0.0f;
			return negativeZero == 0.0f && !Math::IsNaN(negativeZero);
		}

		// Test -0.0 vs 0.0 (double)
		bool TestDoubleNegativeZeroEquality()
		{
			double positiveZero = 0.0;
			double negativeZero = -0.0;
			return positiveZero == negativeZero;
		}

		bool TestDoubleNegativeZeroSign()
		{
			double negativeZero = -0.0;
			return negativeZero == 0.0 && !Math::IsNaN(negativeZero);
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
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNaNConstant()"), true, TEXT("float NaN constant via NAN_flt"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNaNGeneration()"), true, TEXT("float NaN generation via NAN_flt expression"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNaNComparison()"), true, TEXT("float NaN != NaN"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloat32NaNFromSqrt()"), true, TEXT("float32 NaN generated through Math::Sqrt"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatIsFiniteRejectsNaN()"), true, TEXT("float IsFinite rejects NaN"));
		ExpectGlobalReturnSatisfies<float>(Engine, Module, TEXT("float ReturnFloatNaNConstant()"),
			[](float Result) { return std::isnan(Result); },
			TEXT("float NAN_flt should return NaN to C++"));

		// NaN tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNConstant()"), true, TEXT("double NaN constant via NAN_dbl"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNGeneration()"), true, TEXT("double NaN generation via NAN_dbl expression"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNComparison()"), true, TEXT("double NaN != NaN"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNaNFromSqrt()"), true, TEXT("double NaN generated through Math::Sqrt"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleIsFiniteRejectsNaN()"), true, TEXT("double IsFinite rejects NaN"));
		ExpectGlobalReturnSatisfies<double>(Engine, Module, TEXT("double ReturnDoubleNaNConstant()"),
			[](double Result) { return std::isnan(Result); },
			TEXT("double NAN_dbl should return NaN to C++"));

		// Positive Infinity tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatInfGeneration()"), true, TEXT("float Inf generation via finite overflow"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatInfComparison()"), true, TEXT("float Inf > large number"));
		ExpectGlobalReturnSatisfies<float>(Engine, Module, TEXT("float ReturnFloatInfGenerated()"),
			[](float Result) { return std::isinf(Result) && Result > 0.0f; },
			TEXT("float overflow should return positive Inf to C++"));

		// Negative Infinity tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegInfGeneration()"), true, TEXT("float -Inf generation via finite overflow"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegInfComparison()"), true, TEXT("float -Inf < large negative number"));
		ExpectGlobalReturnSatisfies<float>(Engine, Module, TEXT("float ReturnFloatNegInfGenerated()"),
			[](float Result) { return std::isinf(Result) && Result < 0.0f; },
			TEXT("float overflow should return negative Inf to C++"));

		// Positive Infinity tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleInfGeneration()"), true, TEXT("double Inf generation via finite overflow"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleInfComparison()"), true, TEXT("double Inf > large number"));
		ExpectGlobalReturnSatisfies<double>(Engine, Module, TEXT("double ReturnDoubleInfGenerated()"),
			[](double Result) { return std::isinf(Result) && Result > 0.0; },
			TEXT("double overflow should return positive Inf to C++"));

		// Negative Infinity tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegInfGeneration()"), true, TEXT("double -Inf generation via finite overflow"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegInfComparison()"), true, TEXT("double -Inf < large negative number"));
		ExpectGlobalReturnSatisfies<double>(Engine, Module, TEXT("double ReturnDoubleNegInfGenerated()"),
			[](double Result) { return std::isinf(Result) && Result < 0.0; },
			TEXT("double overflow should return negative Inf to C++"));

		// IsFinite tests
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatIsFiniteNormal()"), true, TEXT("float IsFinite with normal value"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleIsFiniteNormal()"), true, TEXT("double IsFinite with normal value"));

		// Precision comparison tests (float)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatPrecisionComparison()"), true, TEXT("float IsNearlyEqual for close values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatPrecisionComparisonFails()"), true, TEXT("float IsNearlyEqual fails for distant values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatEpsilonBoundary()"), true, TEXT("float near-equal boundary honors tolerance"));

		// Precision comparison tests (double)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoublePrecisionComparison()"), true, TEXT("double IsNearlyEqual for close values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoublePrecisionComparisonFails()"), true, TEXT("double IsNearlyEqual fails for distant values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleEpsilonBoundary()"), true, TEXT("double near-equal boundary honors tolerance"));

		// -0.0 vs 0.0 tests
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegativeZeroEquality()"), true, TEXT("float -0.0 == 0.0"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFloatNegativeZeroSign()"), true, TEXT("float -0.0 remains finite-equivalent"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegativeZeroEquality()"), true, TEXT("double -0.0 == 0.0"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestDoubleNegativeZeroSign()"), true, TEXT("double -0.0 remains finite-equivalent"));
	}

	TEST_METHOD(MathNaNInfNamesRemainUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString NaNSource = ASTEST_AS(R"AS(
			bool TryMathNaNName()
			{
				float NanValue = Math::NaN;
				return Math::IsNaN(NanValue);
			}
			)AS");
		const TArray<FString> NaNDiagnostics = { TEXT("NaN") };

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovFloatExpr_MathNaNNameUnsupported"),
			NaNSource,
			TEXT("Math::NaN name should remain an explicit unsupported boundary"),
			MakeArrayView(NaNDiagnostics))));

		const FString InfSource = ASTEST_AS(R"AS(
			bool TryMathInfName()
			{
				float InfValue = Math::Inf;
				return !Math::IsFinite(InfValue);
			}
			)AS");
		const TArray<FString> InfDiagnostics = { TEXT("Inf") };

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovFloatExpr_MathInfNameUnsupported"),
			InfSource,
			TEXT("Math::Inf name should remain an explicit unsupported boundary"),
			MakeArrayView(InfDiagnostics))));

		const FString NegInfSource = ASTEST_AS(R"AS(
			bool TryNegativeMathInfName()
			{
				float NegInfValue = -Math::Inf;
				return !Math::IsFinite(NegInfValue);
			}
			)AS");
		const TArray<FString> NegInfDiagnostics = { TEXT("Inf") };

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovFloatExpr_NegativeMathInfNameUnsupported"),
			NegInfSource,
			TEXT("-Math::Inf expression should remain an explicit unsupported boundary"),
			MakeArrayView(NegInfDiagnostics))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
