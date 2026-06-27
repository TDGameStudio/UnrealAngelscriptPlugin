#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFVector2DExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector2D *expression usage* -- operators, construction,
// methods, and 2D vector operations.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFVector2DExpressionTest,
	"Angelscript.TestModule.Coverage.FVector2DExpression",
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

	// Helper for FVector2D with tolerance
	void ExpectVector2DNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FVector2D& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FVector2D Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// -------------------------------------------------------------------------
	// FVector2D construction: default, parameterized, constants.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_Construct", ASTEST_AS(R"AS(
		FVector2D ConstructDefault()
		{
			return FVector2D();
		}

		FVector2D ConstructTwoParams()
		{
			return FVector2D(3.5, 7.2);
		}

		FVector2D ConstructSingleValue()
		{
			return FVector2D(5.0);
		}

		FVector2D ConstructZeroVector()
		{
			return FVector2D::ZeroVector;
		}

		FVector2D ConstructOneVector()
		{
			return FVector2D::One;
		}

		FVector2D ConstructUnitX()
		{
			return FVector2D::UnitX;
		}

		FVector2D ConstructUnitY()
		{
			return FVector2D::UnitY;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructDefault()"), FVector2D::ZeroVector, TEXT("FVector2D() default"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructTwoParams()"), FVector2D(3.5, 7.2), TEXT("FVector2D(X,Y)"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructSingleValue()"), FVector2D(5.0, 5.0), TEXT("FVector2D(V) single value"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructZeroVector()"), FVector2D::ZeroVector, TEXT("FVector2D::ZeroVector"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructOneVector()"), FVector2D::One(), TEXT("FVector2D::One"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructUnitX()"), FVector2D::UnitX(), TEXT("FVector2D::UnitX"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructUnitY()"), FVector2D::UnitY(), TEXT("FVector2D::UnitY"));
	}

	// -------------------------------------------------------------------------
	// FVector2D arithmetic operators: +, -, *, /, unary -, compound assignment.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_Arithmetic", ASTEST_AS(R"AS(
		FVector2D OpAdd()
		{
			FVector2D a = FVector2D(1.5, 2.5);
			FVector2D b = FVector2D(3.0, 4.0);
			return a + b;
		}

		FVector2D OpSubtract()
		{
			FVector2D a = FVector2D(10.0, 20.0);
			FVector2D b = FVector2D(3.0, 5.0);
			return a - b;
		}

		FVector2D OpMultiplyScalar()
		{
			FVector2D v = FVector2D(2.0, 3.0);
			return v * 4.0;
		}

		FVector2D OpDivideScalar()
		{
			FVector2D v = FVector2D(20.0, 40.0);
			return v / 2.0;
		}

		FVector2D OpNegate()
		{
			FVector2D v = FVector2D(5.0, 10.0);
			return -v;
		}

		FVector2D OpCompoundAdd()
		{
			FVector2D v = FVector2D(1.0, 2.0);
			v += FVector2D(3.0, 4.0);
			return v;
		}

		FVector2D OpCompoundSubtract()
		{
			FVector2D v = FVector2D(10.0, 15.0);
			v -= FVector2D(2.0, 5.0);
			return v;
		}

		FVector2D OpCompoundMultiply()
		{
			FVector2D v = FVector2D(3.0, 6.0);
			v *= 2.0;
			return v;
		}

		FVector2D OpCompoundDivide()
		{
			FVector2D v = FVector2D(20.0, 40.0);
			v /= 4.0;
			return v;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpAdd()"), FVector2D(4.5, 6.5), TEXT("vector2d addition"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpSubtract()"), FVector2D(7.0, 15.0), TEXT("vector2d subtraction"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpMultiplyScalar()"), FVector2D(8.0, 12.0), TEXT("vector2d * scalar"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpDivideScalar()"), FVector2D(10.0, 20.0), TEXT("vector2d / scalar"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpNegate()"), FVector2D(-5.0, -10.0), TEXT("vector2d negation"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundAdd()"), FVector2D(4.0, 6.0), TEXT("vector2d += "));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundSubtract()"), FVector2D(8.0, 10.0), TEXT("vector2d -= "));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundMultiply()"), FVector2D(6.0, 12.0), TEXT("vector2d *= "));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundDivide()"), FVector2D(5.0, 10.0), TEXT("vector2d /= "));
	}

	// -------------------------------------------------------------------------
	// FVector2D comparison operators: ==, !=.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_Comparison", ASTEST_AS(R"AS(
		bool OpEquals_True()
		{
			FVector2D a = FVector2D(1.5, 2.5);
			FVector2D b = FVector2D(1.5, 2.5);
			return a == b;
		}

		bool OpEquals_False()
		{
			FVector2D a = FVector2D(1.5, 2.5);
			FVector2D b = FVector2D(3.0, 4.0);
			return a == b;
		}

		bool OpNotEquals_True()
		{
			FVector2D a = FVector2D(1.0, 2.0);
			FVector2D b = FVector2D(3.0, 4.0);
			return a != b;
		}

		bool OpNotEquals_False()
		{
			FVector2D a = FVector2D(5.5, 6.5);
			FVector2D b = FVector2D(5.5, 6.5);
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

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_True()"), true, TEXT("vector2d == (equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_False()"), false, TEXT("vector2d == (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_True()"), true, TEXT("vector2d != (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_False()"), false, TEXT("vector2d != (equal)"));
	}

	// -------------------------------------------------------------------------
	// FVector2D dot product using | operator.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DDotProduct)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_DotProduct", ASTEST_AS(R"AS(
		float DotProductOperator()
		{
			FVector2D a = FVector2D(1.0, 0.0);
			FVector2D b = FVector2D(0.0, 1.0);
			return a | b;
		}

		float DotProductOrthogonal()
		{
			FVector2D a = FVector2D::UnitX();
			FVector2D b = FVector2D::UnitY();
			return a | b;
		}

		float DotProductParallel()
		{
			FVector2D a = FVector2D(3.0, 4.0);
			FVector2D b = FVector2D(6.0, 8.0);
			return a | b;
		}

		float DotProductGeneral()
		{
			FVector2D a = FVector2D(2.0, 3.0);
			FVector2D b = FVector2D(4.0, 5.0);
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

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductOperator()"), 0.0f, TEXT("dot product operator (orthogonal)"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductOrthogonal()"), 0.0f, TEXT("dot product UnitX | UnitY"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductParallel()"), 50.0f, TEXT("dot product parallel vectors"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductGeneral()"), 23.0f, TEXT("dot product general case"));
	}

	// -------------------------------------------------------------------------
	// FVector2D member access: X, Y.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_MemberAccess", ASTEST_AS(R"AS(
		float GetX()
		{
			FVector2D v = FVector2D(10.5, 20.5);
			return v.X;
		}

		float GetY()
		{
			FVector2D v = FVector2D(10.5, 20.5);
			return v.Y;
		}

		FVector2D SetX()
		{
			FVector2D v = FVector2D(10.0, 20.0);
			v.X = 100.0;
			return v;
		}

		FVector2D SetY()
		{
			FVector2D v = FVector2D(10.0, 20.0);
			v.Y = 200.0;
			return v;
		}

		FVector2D SetBoth()
		{
			FVector2D v = FVector2D(1.0, 2.0);
			v.X = 99.0;
			v.Y = 88.0;
			return v;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetX()"), 10.5f, TEXT("FVector2D.X getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetY()"), 20.5f, TEXT("FVector2D.Y getter"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D SetX()"), FVector2D(100.0, 20.0), TEXT("FVector2D.X setter"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D SetY()"), FVector2D(10.0, 200.0), TEXT("FVector2D.Y setter"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D SetBoth()"), FVector2D(99.0, 88.0), TEXT("FVector2D.X and Y setter"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
