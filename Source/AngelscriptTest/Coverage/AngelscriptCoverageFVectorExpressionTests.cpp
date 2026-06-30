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
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector expression module should compile before invoking globals")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		T Result{};
		if constexpr (std::is_same_v<T, float>)
		{
			const double Actual = Invoker.ExecuteAndGet<double>(0.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(static_cast<double>(Expected), Actual, 0.0001), Message));
		}
		else if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
			|| std::is_same_v<T, double>)
		{
			Result = Invoker.ExecuteAndGet<T>(T{});
			if constexpr (std::is_floating_point_v<T>)
			{
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Expected, Result, static_cast<T>(0.0001)), Message));
			}
			else
			{
				ASSERT_THAT(AreEqual(Expected, Result, Message));
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}
	}

	// Helper for FVector with tolerance
	void ExpectVectorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message, double Tolerance = 0.0001)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector expression module should compile before invoking globals")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FVector Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		ASSERT_THAT(IsTrue(Result.Equals(Expected, Tolerance), Message));
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

		FVector ConstructForwardVector()
		{
			return FVector::ForwardVector;
		}

		FVector ConstructRightVector()
		{
			return FVector::RightVector;
		}

		FVector ConstructUpVector()
		{
			return FVector::UpVector;
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
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructForwardVector()"), FVector::ForwardVector, TEXT("FVector::ForwardVector"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructRightVector()"), FVector::RightVector, TEXT("FVector::RightVector"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ConstructUpVector()"), FVector::UpVector, TEXT("FVector::UpVector"));

		{
			const TArray<FString> ExpectedDiagnostics = {
				TEXT("No matching signatures to 'FVector::UnitX()'"),
				TEXT("No matching signatures to 'FVector::UnitY()'"),
				TEXT("No matching signatures to 'FVector::UnitZ()'")
			};
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFVectorExpr_UnitFunctionsUnsupported"),
				ASTEST_AS(R"AS(
				void TryUnsupportedUnitFunctions()
				{
					FVector UnitX = FVector::UnitX();
					FVector UnitY = FVector::UnitY();
					FVector UnitZ = FVector::UnitZ();
				}
				)AS"),
				TEXT("FVector UnitX/UnitY/UnitZ function aliases should remain explicit unsupported boundaries; use ForwardVector/RightVector/UpVector"),
				MakeArrayView(ExpectedDiagnostics))));
		}
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
			return a.DotProduct(b);
		}

		float DotProductGeneral()
		{
			FVector a = FVector(2, 3, 4);
			FVector b = FVector(5, 6, 7);
			return a.DotProduct(b);
		}

		FVector CrossProduct()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return a.CrossProduct(b);
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
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DotProductGeneral()"), 56.0f, TEXT("DotProduct method"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector CrossProduct()"), FVector(0, 0, 1), TEXT("CrossProduct method"));

		{
			const TArray<FString> ExpectedDiagnostics = {
				TEXT("No matching operator that takes the types 'FVector' and 'FVector' found"),
				TEXT("No matching signatures to 'FVector::DotProduct(FVector, FVector)'"),
				TEXT("No matching signatures to 'FVector::CrossProduct(FVector, FVector)'")
			};
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFVectorExpr_StaticAndOperatorProductsUnsupported"),
				ASTEST_AS(R"AS(
				void TryUnsupportedVectorProducts()
				{
					FVector A = FVector(1, 0, 0);
					FVector B = FVector(0, 1, 0);
					float Dot = A | B;
					FVector Cross = A ^ B;
					float StaticDot = FVector::DotProduct(A, B);
					FVector StaticCross = FVector::CrossProduct(A, B);
				}
				)AS"),
				TEXT("FVector product operator/static aliases should remain explicit unsupported boundaries; use member methods"),
				MakeArrayView(ExpectedDiagnostics))));
		}
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
			return v.Size();
		}

		float VectorSquaredLength()
		{
			FVector v = FVector(3, 4, 0);
			return v.SizeSquared();
		}

		FVector VectorNormalize()
		{
			FVector v = FVector(5, 0, 0);
			return v.GetSafeNormal();
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
			return a.Distance(b);
		}

		float VectorDot()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(4, 5, 6);
			return a.DotProduct(b);
		}

		FVector VectorCross()
		{
			FVector a = FVector(1, 0, 0);
			FVector b = FVector(0, 1, 0);
			return a.CrossProduct(b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorLength()"), 5.0f, TEXT("vector Size()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorSquaredLength()"), 25.0f, TEXT("vector SizeSquared()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector VectorNormalize()"), FVector(1, 0, 0), TEXT("vector GetSafeNormal()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool VectorIsZero()"), true, TEXT("vector IsZero()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool VectorIsNearlyZero()"), true, TEXT("vector IsNearlyZero()"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorDistance()"), 5.0f, TEXT("vector member Distance()"));
		// 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float VectorDot()"), 32.0f, TEXT("vector DotProduct()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector VectorCross()"), FVector(0, 0, 1), TEXT("vector CrossProduct()"));

		{
			const TArray<FString> ExpectedDiagnostics = {
				TEXT("No matching signatures to 'FVector::Length()'"),
				TEXT("No matching signatures to 'FVector::SquaredLength()'"),
				TEXT("No matching signatures to 'FVector::GetNormalized()'"),
				TEXT("No matching signatures to 'FVector::Distance(FVector, FVector)'"),
				TEXT("No matching signatures to 'FVector::Dot(FVector)'"),
				TEXT("No matching signatures to 'FVector::Cross(FVector)'")
			};
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFVectorExpr_MethodAliasesUnsupported"),
				ASTEST_AS(R"AS(
				void TryUnsupportedVectorMethodAliases()
				{
					FVector A = FVector(3, 4, 0);
					FVector B = FVector(1, 0, 0);
					float Length = A.Length();
					float SquaredLength = A.SquaredLength();
					FVector Normal = A.GetNormalized();
					float Distance = FVector::Distance(A, B);
					float Dot = A.Dot(B);
					FVector Cross = A.Cross(B);
				}
				)AS"),
				TEXT("FVector legacy aliases should remain explicit unsupported boundaries; use Size/SizeSquared/GetSafeNormal/member methods"),
				MakeArrayView(ExpectedDiagnostics))));
		}
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

	TEST_METHOD(FVectorDeclarationsAndIndexAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_DeclarationsIndex", ASTEST_AS(R"AS(
		const FVector GlobalConstVector = FVector::ZeroVector;

		float LocalDefaultIsZero()
		{
			FVector v;
			return v.X + v.Y + v.Z;
		}

		float LocalDefaultValue()
		{
			FVector v = FVector(1, 2, 3);
			return v.X + v.Y + v.Z;
		}

		float LocalConstValue()
		{
			const FVector v = FVector(1, 0, 0);
			return v.X;
		}

		float GlobalConstValue()
		{
			return GlobalConstVector.X + GlobalConstVector.Y + GlobalConstVector.Z;
		}

		float IndexRead()
		{
			FVector v = FVector(4, 5, 6);
			return v[0] + v[1] + v[2];
		}

		FVector IndexWrite()
		{
			FVector v = FVector::ZeroVector;
			v[0] = 7;
			v[1] = 8;
			v[2] = 9;
			return v;
		}

		class FPlainVectorHolder
		{
			FVector Value;

			FPlainVectorHolder()
			{
				Value = FVector(2, 4, 6);
			}
		}

		int PlainClassMemberValueRaisesBoundary()
		{
			FPlainVectorHolder Holder;
			return Holder.Value.X + Holder.Value.Y + Holder.Value.Z;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalDefaultIsZero()"), 0.0f, TEXT("FVector local default declaration should be zero"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalDefaultValue()"), 6.0f, TEXT("FVector local initialized declaration"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float LocalConstValue()"), 1.0f, TEXT("FVector local const declaration"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GlobalConstValue()"), 0.0f, TEXT("FVector global const declaration"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float IndexRead()"), 15.0f, TEXT("FVector index read"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector IndexWrite()"), FVector(7, 8, 9), TEXT("FVector index write"));
		{
			ASSERT_THAT(IsNotNull(Module, TEXT("FVector declaration/index module should compile before plain class boundary check")));
			if (Module != nullptr)
			{
				asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int PlainClassMemberValueRaisesBoundary()"));
				ASSERT_THAT(IsNotNull(Function, TEXT("FVector plain script class boundary function should exist")));
				if (Function != nullptr)
				{
					ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
						*TestRunner,
						Engine,
						*Function,
						TEXT("FVector plain script class member boundary"),
						TEXT("Null pointer access"),
						TEXT("int PlainClassMemberValueRaisesBoundary() | Line"))));
				}
			}
		}
	}

	TEST_METHOD(FVectorExtendedOperatorsAndMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorExpr_ExtendedMethods", ASTEST_AS(R"AS(
		FVector OpComponentMultiply()
		{
			return FVector(2, 3, 4) * FVector(5, 6, 7);
		}

		FVector OpComponentDivide()
		{
			return FVector(20, 30, 40) / FVector(2, 3, 4);
		}

		FVector OpCompoundComponentMultiply()
		{
			FVector v = FVector(2, 3, 4);
			v *= FVector(5, 6, 7);
			return v;
		}

		FVector OpCompoundComponentDivide()
		{
			FVector v = FVector(20, 30, 40);
			v /= FVector(2, 3, 4);
			return v;
		}

		bool NormalizeMutates()
		{
			FVector v = FVector(10, 0, 0);
			bool bNormalized = v.Normalize();
			return bNormalized && v.Equals(FVector(1, 0, 0), 0.001);
		}

		bool UnitAndNormalizedChecks()
		{
			FVector v = FVector(1, 0, 0);
			return v.IsUnit() && v.IsNormalized();
		}

		float DistSquaredMethod()
		{
			return FVector(1, 2, 3).DistSquared(FVector(4, 6, 3));
		}

		FVector ProjectOnToVector()
		{
			return FVector(3, 4, 0).ProjectOnTo(FVector(1, 0, 0));
		}

		FVector ProjectOnToNormalVector()
		{
			return FVector(3, 4, 0).ProjectOnToNormal(FVector(0, 1, 0));
		}

		FVector LerpVector()
		{
			return Math::Lerp(FVector(0, 0, 0), FVector(10, 20, 30), 0.25);
		}

		FVector ClampSizeVector()
		{
			return FVector(10, 0, 0).GetClampedToSize(0, 5);
		}

		FVector ClampMaxSizeVector()
		{
			return FVector(0, 12, 0).GetClampedToMaxSize(3);
		}

		FVector RotateAroundZ()
		{
			return FVector(1, 0, 0).RotateAngleAxis(90, FVector(0, 0, 1));
		}

		bool DirectionAndLengthOutParams()
		{
			FVector Direction;
			float64 Length = 0;
			FVector(0, 3, 4).ToDirectionAndLength(Direction, Length);
			return Direction.Equals(FVector(0, 0.6, 0.8), 0.001) && Length > 4.999 && Length < 5.001;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpComponentMultiply()"), FVector(10, 18, 28), TEXT("FVector component multiply"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpComponentDivide()"), FVector(10, 10, 10), TEXT("FVector component divide"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpCompoundComponentMultiply()"), FVector(10, 18, 28), TEXT("FVector component *= "));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector OpCompoundComponentDivide()"), FVector(10, 10, 10), TEXT("FVector component /= "));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool NormalizeMutates()"), true, TEXT("FVector.Normalize() should mutate and report success"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool UnitAndNormalizedChecks()"), true, TEXT("FVector IsUnit/IsNormalized"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float DistSquaredMethod()"), 25.0f, TEXT("FVector.DistSquared()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ProjectOnToVector()"), FVector(3, 0, 0), TEXT("FVector.ProjectOnTo()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ProjectOnToNormalVector()"), FVector(0, 4, 0), TEXT("FVector.ProjectOnToNormal()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector LerpVector()"), FVector(2.5, 5, 7.5), TEXT("Math::Lerp FVector"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ClampSizeVector()"), FVector(5, 0, 0), TEXT("FVector.GetClampedToSize()"));
		ExpectGlobalReturn<FVector>(Engine, Module, TEXT("FVector ClampMaxSizeVector()"), FVector(0, 3, 0), TEXT("FVector.GetClampedToMaxSize()"));
		ExpectVectorNearlyEqual(Engine, Module, TEXT("FVector RotateAroundZ()"), FVector(0, 1, 0), TEXT("FVector.RotateAngleAxis()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool DirectionAndLengthOutParams()"), true, TEXT("FVector.ToDirectionAndLength() out params"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
