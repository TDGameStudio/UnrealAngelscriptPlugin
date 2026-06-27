#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFVectorExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector *expression usage* -- operators, construction,
// methods, and vector operations.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFVectorExpressionTest,
	"Angelscript.TestModule.Coverage.FVectorExpression",
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

	// Helper for FVector with tolerance
	void ExpectVectorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FVector Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// -------------------------------------------------------------------------
	// FVector construction: default, parameterized, constants.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_Construct", ASTEST_AS(R"AS(
		FVector ConstructDefault()
		{
			return FVector();
		}

		FVector ConstructThreeParams()
		{
			return FVector(1, 2, 3);
		}

		FVector ConstructSingleValue()
		{
			return FVector(5);
		}

		FVector ConstructZeroVector()
		{
			return FVector::ZeroVector;
		}

		FVector ConstructOneVector()
		{
			return FVector::OneVector;
		}

		FVector ConstructUnitX()
		{
			return FVector::UnitX();
		}

		FVector ConstructUnitY()
		{
			return FVector::UnitY();
		}

		FVector ConstructUnitZ()
		{
			return FVector::UnitZ();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructDefault()"), FVector::ZeroVector, TEXT("FVector() default"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructThreeParams()"), FVector(1, 2, 3), TEXT("FVector(1,2,3)"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructSingleValue()"), FVector(5, 5, 5), TEXT("FVector(5) single value"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructZeroVector()"), FVector::ZeroVector, TEXT("FVector::ZeroVector"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructOneVector()"), FVector::OneVector, TEXT("FVector::OneVector"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructUnitX()"), FVector::UnitX(), TEXT("FVector::UnitX()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructUnitY()"), FVector::UnitY(), TEXT("FVector::UnitY()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructUnitZ()"), FVector::UnitZ(), TEXT("FVector::UnitZ()"));
	}

	// -------------------------------------------------------------------------
	// FVector arithmetic operators: +, -, *, /, unary -, compound assignment.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_Arithmetic", ASTEST_AS(R"AS(
		FVector OpAdd()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(4, 5, 6);
			return a + b;
		}

		FVector OpSubtract()
		{
			FVector a = FVector(10, 20, 30);
			FVector b = FVector(1, 2, 3);
			return a - b;
		}

		FVector OpMultiplyScalar()
		{
			FVector v = FVector(2, 3, 4);
			return v * 3.0;
		}

		FVector OpDivideScalar()
		{
			FVector v = FVector(20, 40, 60);
			return v / 2.0;
		}

		FVector OpNegate()
		{
			FVector v = FVector(5, 10, 15);
			return -v;
		}

		FVector OpCompoundAdd()
		{
			FVector v = FVector(1, 2, 3);
			v += FVector(2, 3, 4);
			return v;
		}

		FVector OpCompoundSubtract()
		{
			FVector v = FVector(10, 10, 10);
			v -= FVector(3, 5, 7);
			return v;
		}

		FVector OpCompoundMultiply()
		{
			FVector v = FVector(2, 4, 6);
			v *= 2.0;
			return v;
		}

		FVector OpCompoundDivide()
		{
			FVector v = FVector(20, 40, 60);
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

		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpAdd()"), FVector(5, 7, 9), TEXT("vector addition"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpSubtract()"), FVector(9, 18, 27), TEXT("vector subtraction"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpMultiplyScalar()"), FVector(6, 9, 12), TEXT("vector * scalar"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpDivideScalar()"), FVector(10, 20, 30), TEXT("vector / scalar"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpNegate()"), FVector(-5, -10, -15), TEXT("vector negation"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpCompoundAdd()"), FVector(3, 5, 7), TEXT("vector += "));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpCompoundSubtract()"), FVector(7, 5, 3), TEXT("vector -= "));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpCompoundMultiply()"), FVector(4, 8, 12), TEXT("vector *= "));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpCompoundDivide()"), FVector(5, 10, 15), TEXT("vector /= "));
	}

	// -------------------------------------------------------------------------
	// FVector comparison operators: ==, !=.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_Comparison", ASTEST_AS(R"AS(
		bool OpEquals_True()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(1, 2, 3);
			return a == b;
		}

		bool OpEquals_False()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(4, 5, 6);
			return a == b;
		}

		bool OpNotEquals_True()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(4, 5, 6);
			return a != b;
		}

		bool OpNotEquals_False()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(1, 2, 3);
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

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_True()"), true, TEXT("vector == (equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_False()"), false, TEXT("vector == (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_True()"), true, TEXT("vector != (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_False()"), false, TEXT("vector != (equal)"));
	}

	// -------------------------------------------------------------------------
	// FVector dot product and cross product.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorDotAndCross)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_DotCross", ASTEST_AS(R"AS(
		float DotProduct()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return a | b;
		}

		float DotProductMethod()
		{
			FVector a = FVector(2, 3, 4);
			FVector b = FVector(5, 6, 7);
			return FVector::DotProduct(a, b);
		}

		FVector CrossProduct()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return a ^ b;
		}

		FVector CrossProductMethod()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return FVector::CrossProduct(a, b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProduct()"), 0.0f, TEXT("dot product operator (orthogonal)"));
		// 2*5 + 3*6 + 4*7 = 10 + 18 + 28 = 56
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductMethod()"), 56.0f, TEXT("DotProduct method"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector CrossProduct()"), FVector(0, 0, 1), TEXT("cross product operator"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector CrossProductMethod()"), FVector(0, 0, 1), TEXT("CrossProduct method"));
	}

	// -------------------------------------------------------------------------
	// FVector methods: Length, SquaredLength, GetNormalized, IsZero, Distance, etc.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_Methods", ASTEST_AS(R"AS(
		float VectorLength()
		{
			FVector v = FVector(3, 4, 0);
			return v.Length();
		}

		float VectorSquaredLength()
		{
			FVector v = FVector(3, 4, 0);
			return v.SquaredLength();
		}

		FVector VectorNormalize()
		{
			FVector v = FVector(5, 0, 0);
			return v.GetNormalized();
		}

		bool VectorIsZero()
		{
			FVector v = FVector::ZeroVector;
			return v.IsZero();
		}

		bool VectorIsNearlyZero()
		{
			FVector v = FVector(0.00001, 0.00001, 0.00001);
			return v.IsNearlyZero();
		}

		float VectorDistance()
		{
			FVector a = FVector(0, 0, 0);
			FVector b = FVector(3, 4, 0);
			return FVector::Distance(a, b);
		}

		float VectorDot()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(4, 5, 6);
			return a.Dot(b);
		}

		FVector VectorCross()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return a.Cross(b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorLength()"), 5.0f, TEXT("vector Length()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorSquaredLength()"), 25.0f, TEXT("vector SquaredLength()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector VectorNormalize()"), FVector(1, 0, 0), TEXT("vector GetNormalized()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool VectorIsZero()"), true, TEXT("vector IsZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool VectorIsNearlyZero()"), true, TEXT("vector IsNearlyZero()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorDistance()"), 5.0f, TEXT("vector Distance()"));
		// 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorDot()"), 32.0f, TEXT("vector Dot()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector VectorCross()"), FVector(0, 0, 1), TEXT("vector Cross()"));
	}

	// -------------------------------------------------------------------------
	// FVector member access: X, Y, Z.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_MemberAccess", ASTEST_AS(R"AS(
		float GetX()
		{
			FVector v = FVector(10, 20, 30);
			return v.X;
		}

		float GetY()
		{
			FVector v = FVector(10, 20, 30);
			return v.Y;
		}

		float GetZ()
		{
			FVector v = FVector(10, 20, 30);
			return v.Z;
		}

		FVector SetX()
		{
			FVector v = FVector(10, 20, 30);
			v.X = 100;
			return v;
		}

		FVector SetY()
		{
			FVector v = FVector(10, 20, 30);
			v.Y = 200;
			return v;
		}

		FVector SetZ()
		{
			FVector v = FVector(10, 20, 30);
			v.Z = 300;
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

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetX()"), 10.0f, TEXT("FVector.X getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetY()"), 20.0f, TEXT("FVector.Y getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetZ()"), 30.0f, TEXT("FVector.Z getter"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector SetX()"), FVector(100, 20, 30), TEXT("FVector.X setter"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector SetY()"), FVector(10, 200, 30), TEXT("FVector.Y setter"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector SetZ()"), FVector(10, 20, 300), TEXT("FVector.Z setter"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
