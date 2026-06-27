#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFVector2DExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector2D *expression usage* -- operators, construction,
// methods, and vector operations.
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
			return FVector2D(10, 20);
		}

		FVector2D ConstructSingleValue()
		{
			return FVector2D(7);
		}

		FVector2D ConstructZeroVector()
		{
			return FVector2D::ZeroVector;
		}

		FVector2D ConstructUnitVector()
		{
			return FVector2D::UnitVector;
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
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructTwoParams()"), FVector2D(10, 20), TEXT("FVector2D(10,20)"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructSingleValue()"), FVector2D(7, 7), TEXT("FVector2D(7) single value"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructZeroVector()"), FVector2D::ZeroVector, TEXT("FVector2D::ZeroVector"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D ConstructUnitVector()"), FVector2D::UnitVector, TEXT("FVector2D::UnitVector"));
	}

	// -------------------------------------------------------------------------
	// FVector2D arithmetic operators: +, -, *, /.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_Arithmetic", ASTEST_AS(R"AS(
		FVector2D OpAdd()
		{
			FVector2D a = FVector2D(1, 2);
			FVector2D b = FVector2D(3, 4);
			return a + b;
		}

		FVector2D OpSubtract()
		{
			FVector2D a = FVector2D(10, 20);
			FVector2D b = FVector2D(1, 5);
			return a - b;
		}

		FVector2D OpMultiplyScalar()
		{
			FVector2D v = FVector2D(2, 3);
			return v * 3.0;
		}

		FVector2D OpDivideScalar()
		{
			FVector2D v = FVector2D(20, 40);
			return v / 2.0;
		}

		FVector2D OpNegate()
		{
			FVector2D v = FVector2D(5, 10);
			return -v;
		}

		FVector2D OpCompoundAdd()
		{
			FVector2D v = FVector2D(1, 2);
			v += FVector2D(2, 3);
			return v;
		}

		FVector2D OpCompoundSubtract()
		{
			FVector2D v = FVector2D(10, 10);
			v -= FVector2D(3, 5);
			return v;
		}

		FVector2D OpCompoundMultiply()
		{
			FVector2D v = FVector2D(2, 4);
			v *= 2.0;
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

		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpAdd()"), FVector2D(4, 6), TEXT("vector2d addition"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpSubtract()"), FVector2D(9, 15), TEXT("vector2d subtraction"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpMultiplyScalar()"), FVector2D(6, 9), TEXT("vector2d * scalar"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpDivideScalar()"), FVector2D(10, 20), TEXT("vector2d / scalar"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpNegate()"), FVector2D(-5, -10), TEXT("vector2d negation"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundAdd()"), FVector2D(3, 5), TEXT("vector2d += "));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundSubtract()"), FVector2D(7, 5), TEXT("vector2d -= "));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D OpCompoundMultiply()"), FVector2D(4, 8), TEXT("vector2d *= "));
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
			FVector2D a = FVector2D(5, 10);
			FVector2D b = FVector2D(5, 10);
			return a == b;
		}

		bool OpEquals_False()
		{
			FVector2D a = FVector2D(5, 10);
			FVector2D b = FVector2D(5, 11);
			return a == b;
		}

		bool OpNotEquals_True()
		{
			FVector2D a = FVector2D(1, 2);
			FVector2D b = FVector2D(3, 4);
			return a != b;
		}

		bool OpNotEquals_False()
		{
			FVector2D a = FVector2D(7, 8);
			FVector2D b = FVector2D(7, 8);
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
	// FVector2D dot product.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DDotProduct)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_Dot", ASTEST_AS(R"AS(
		float DotProduct()
		{
			FVector2D a = FVector2D(2, 3);
			FVector2D b = FVector2D(4, 5);
			return a | b;  // Dot product
		}

		float DotProductMethod()
		{
			FVector2D a = FVector2D(1, 0);
			FVector2D b = FVector2D(0, 1);
			return FVector2D::DotProduct(a, b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// 2*4 + 3*5 = 8 + 15 = 23
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProduct()"), 23.0f, TEXT("dot product operator"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductMethod()"), 0.0f, TEXT("DotProduct method (orthogonal)"));
	}

	// -------------------------------------------------------------------------
	// FVector2D methods: Length, Normalize, Distance.
	// -------------------------------------------------------------------------
	TEST_METHOD(Vector2DMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DExpr_Methods", ASTEST_AS(R"AS(
		float VectorLength()
		{
			FVector2D v = FVector2D(3, 4);
			return v.Length();
		}

		float VectorSquaredLength()
		{
			FVector2D v = FVector2D(3, 4);
			return v.SquaredLength();
		}

		FVector2D VectorNormalize()
		{
			FVector2D v = FVector2D(5, 0);
			return v.GetNormalized();
		}

		bool VectorIsZero()
		{
			FVector2D v = FVector2D::ZeroVector;
			return v.IsZero();
		}

		bool VectorIsNearlyZero()
		{
			FVector2D v = FVector2D(0.00001, 0.00001);
			return v.IsNearlyZero();
		}

		float VectorDistance()
		{
			FVector2D a = FVector2D(0, 0);
			FVector2D b = FVector2D(3, 4);
			return FVector2D::Distance(a, b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorLength()"), 5.0f, TEXT("vector2d Length()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorSquaredLength()"), 25.0f, TEXT("vector2d SquaredLength()"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D VectorNormalize()"), FVector2D(1, 0), TEXT("vector2d GetNormalized()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool VectorIsZero()"), true, TEXT("vector2d IsZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool VectorIsNearlyZero()"), true, TEXT("vector2d IsNearlyZero()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorDistance()"), 5.0f, TEXT("vector2d Distance()"));
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
			FVector2D v = FVector2D(10, 20);
			return v.X;
		}

		float GetY()
		{
			FVector2D v = FVector2D(10, 20);
			return v.Y;
		}

		FVector2D SetX()
		{
			FVector2D v = FVector2D(10, 20);
			v.X = 100;
			return v;
		}

		FVector2D SetY()
		{
			FVector2D v = FVector2D(10, 20);
			v.Y = 200;
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

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetX()"), 10.0f, TEXT("FVector2D.X getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetY()"), 20.0f, TEXT("FVector2D.Y getter"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D SetX()"), FVector2D(100, 20), TEXT("FVector2D.X setter"));
		ExpectGlobalReturn<FVector2D>(Engine, Module, TEXT("FVector2D SetY()"), FVector2D(10, 200), TEXT("FVector2D.Y setter"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
