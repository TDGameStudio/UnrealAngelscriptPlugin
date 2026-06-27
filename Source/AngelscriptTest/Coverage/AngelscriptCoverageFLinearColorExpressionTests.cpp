#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFLinearColorExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FLinearColor *expression usage* -- operators, construction,
// methods, and color operations.
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFLinearColorExpressionTest,
	"Angelscript.TestModule.Coverage.FLinearColorExpression",
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

	// Helper for FLinearColor with tolerance
	void ExpectColorNearlyEqual(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const FLinearColor& Expected, const TCHAR* Message, float Tolerance = 0.0001f)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		FLinearColor Result;
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		TestRunner->TestTrue(Message, Result.Equals(Expected, Tolerance));
	}

	// -------------------------------------------------------------------------
	// FLinearColor construction: default, parameterized, predefined colors.
	// -------------------------------------------------------------------------
	TEST_METHOD(LinearColorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorExpr_Construct", ASTEST_AS(R"AS(
		FLinearColor ConstructDefault()
		{
			return FLinearColor();
		}

		FLinearColor ConstructFourParams()
		{
			return FLinearColor(0.5, 0.6, 0.7, 0.8);
		}

		FLinearColor ConstructThreeParams()
		{
			return FLinearColor(0.2, 0.4, 0.6);
		}

		FLinearColor ConstructWhite()
		{
			return FLinearColor::White;
		}

		FLinearColor ConstructBlack()
		{
			return FLinearColor::Black;
		}

		FLinearColor ConstructRed()
		{
			return FLinearColor::Red;
		}

		FLinearColor ConstructGreen()
		{
			return FLinearColor::Green;
		}

		FLinearColor ConstructBlue()
		{
			return FLinearColor::Blue;
		}

		FLinearColor ConstructYellow()
		{
			return FLinearColor::Yellow;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructDefault()"), FLinearColor(0, 0, 0, 0), TEXT("FLinearColor() default"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructFourParams()"), FLinearColor(0.5f, 0.6f, 0.7f, 0.8f), TEXT("FLinearColor(R,G,B,A)"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructThreeParams()"), FLinearColor(0.2f, 0.4f, 0.6f, 1.0f), TEXT("FLinearColor(R,G,B) default alpha"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructWhite()"), FLinearColor::White, TEXT("FLinearColor::White"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructBlack()"), FLinearColor::Black, TEXT("FLinearColor::Black"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructRed()"), FLinearColor::Red, TEXT("FLinearColor::Red"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructGreen()"), FLinearColor::Green, TEXT("FLinearColor::Green"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructBlue()"), FLinearColor::Blue, TEXT("FLinearColor::Blue"));
		ExpectGlobalReturn<FLinearColor>(Engine, Module, TEXT("FLinearColor ConstructYellow()"), FLinearColor::Yellow, TEXT("FLinearColor::Yellow"));
	}

	// -------------------------------------------------------------------------
	// FLinearColor arithmetic operators: +, -, *, /.
	// -------------------------------------------------------------------------
	TEST_METHOD(LinearColorArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorExpr_Arithmetic", ASTEST_AS(R"AS(
		FLinearColor OpAdd()
		{
			FLinearColor a = FLinearColor(0.1, 0.2, 0.3, 0.4);
			FLinearColor b = FLinearColor(0.5, 0.4, 0.3, 0.2);
			return a + b;
		}

		FLinearColor OpSubtract()
		{
			FLinearColor a = FLinearColor(1.0, 0.8, 0.6, 0.4);
			FLinearColor b = FLinearColor(0.2, 0.3, 0.1, 0.1);
			return a - b;
		}

		FLinearColor OpMultiplyScalar()
		{
			FLinearColor c = FLinearColor(0.2, 0.4, 0.6, 0.8);
			return c * 2.0;
		}

		FLinearColor OpMultiplyColor()
		{
			FLinearColor a = FLinearColor(0.5, 0.5, 0.5, 1.0);
			FLinearColor b = FLinearColor(0.8, 0.6, 0.4, 1.0);
			return a * b;
		}

		FLinearColor OpDivideScalar()
		{
			FLinearColor c = FLinearColor(1.0, 0.8, 0.6, 0.4);
			return c / 2.0;
		}

		FLinearColor OpCompoundAdd()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			c += FLinearColor(0.2, 0.1, 0.1, 0.1);
			return c;
		}

		FLinearColor OpCompoundMultiply()
		{
			FLinearColor c = FLinearColor(0.5, 0.4, 0.3, 0.2);
			c *= 2.0;
			return c;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpAdd()"), FLinearColor(0.6f, 0.6f, 0.6f, 0.6f), TEXT("color addition"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpSubtract()"), FLinearColor(0.8f, 0.5f, 0.5f, 0.3f), TEXT("color subtraction"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpMultiplyScalar()"), FLinearColor(0.4f, 0.8f, 1.2f, 1.6f), TEXT("color * scalar"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpMultiplyColor()"), FLinearColor(0.4f, 0.3f, 0.2f, 1.0f), TEXT("color * color"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpDivideScalar()"), FLinearColor(0.5f, 0.4f, 0.3f, 0.2f), TEXT("color / scalar"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpCompoundAdd()"), FLinearColor(0.3f, 0.3f, 0.4f, 0.5f), TEXT("color += "));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor OpCompoundMultiply()"), FLinearColor(1.0f, 0.8f, 0.6f, 0.4f), TEXT("color *= "));
	}

	// -------------------------------------------------------------------------
	// FLinearColor comparison operators: ==, !=.
	// -------------------------------------------------------------------------
	TEST_METHOD(LinearColorComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorExpr_Comparison", ASTEST_AS(R"AS(
		bool OpEquals_True()
		{
			FLinearColor a = FLinearColor(0.5, 0.6, 0.7, 0.8);
			FLinearColor b = FLinearColor(0.5, 0.6, 0.7, 0.8);
			return a == b;
		}

		bool OpEquals_False()
		{
			FLinearColor a = FLinearColor(0.5, 0.6, 0.7, 0.8);
			FLinearColor b = FLinearColor(0.5, 0.6, 0.8, 0.8);
			return a == b;
		}

		bool OpNotEquals_True()
		{
			FLinearColor a = FLinearColor::Red;
			FLinearColor b = FLinearColor::Blue;
			return a != b;
		}

		bool OpNotEquals_False()
		{
			FLinearColor a = FLinearColor::White;
			FLinearColor b = FLinearColor::White;
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

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_True()"), true, TEXT("color == (equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_False()"), false, TEXT("color == (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_True()"), true, TEXT("color != (not equal)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_False()"), false, TEXT("color != (equal)"));
	}

	// -------------------------------------------------------------------------
	// FLinearColor methods: ToFColor, Desaturate, etc.
	// -------------------------------------------------------------------------
	TEST_METHOD(LinearColorMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorExpr_Methods", ASTEST_AS(R"AS(
		FColor ToFColorSRGB()
		{
			FLinearColor c = FLinearColor::White;
			return c.ToFColor(true);
		}

		FColor ToFColorLinear()
		{
			FLinearColor c = FLinearColor(1.0, 0.5, 0.0, 1.0);
			return c.ToFColor(false);
		}

		FLinearColor Desaturate()
		{
			FLinearColor c = FLinearColor::Red;
			return c.Desaturate(0.5);
		}

		float GetLuminance()
		{
			FLinearColor c = FLinearColor::White;
			return c.GetLuminance();
		}

		FLinearColor LerpColors()
		{
			FLinearColor a = FLinearColor::Black;
			FLinearColor b = FLinearColor::White;
			return FLinearColor::LerpUsingHSV(a, b, 0.5);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// ToFColor with sRGB
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor ToFColorSRGB()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("ToFColor(true) R"), Result.R, (uint8)255);
			TestRunner->TestEqual(TEXT("ToFColor(true) G"), Result.G, (uint8)255);
			TestRunner->TestEqual(TEXT("ToFColor(true) B"), Result.B, (uint8)255);
			TestRunner->TestEqual(TEXT("ToFColor(true) A"), Result.A, (uint8)255);
		}

		// ToFColor without sRGB
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor ToFColorLinear()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("ToFColor(false) R"), Result.R, (uint8)255);
		}

		// Desaturate
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor Desaturate()"));
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("Desaturate reduces saturation"), Result.R < 1.0f && Result.G > 0.0f);
		}

		// GetLuminance
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float GetLuminance()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("GetLuminance of white"), Result > 0.9f);
		}

		// LerpUsingHSV
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor LerpColors()"));
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("LerpUsingHSV produces intermediate color"), Result.R > 0.0f && Result.R < 1.0f);
		}
	}

	// -------------------------------------------------------------------------
	// FLinearColor member access: R, G, B, A.
	// -------------------------------------------------------------------------
	TEST_METHOD(LinearColorMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorExpr_MemberAccess", ASTEST_AS(R"AS(
		float GetR()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			return c.R;
		}

		float GetG()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			return c.G;
		}

		float GetB()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			return c.B;
		}

		float GetA()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			return c.A;
		}

		FLinearColor SetR()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			c.R = 0.9;
			return c;
		}

		FLinearColor SetG()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			c.G = 0.8;
			return c;
		}

		FLinearColor SetB()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			c.B = 0.7;
			return c;
		}

		FLinearColor SetA()
		{
			FLinearColor c = FLinearColor(0.1, 0.2, 0.3, 0.4);
			c.A = 1.0;
			return c;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetR()"), 0.1f, TEXT("FLinearColor.R getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetG()"), 0.2f, TEXT("FLinearColor.G getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetB()"), 0.3f, TEXT("FLinearColor.B getter"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float GetA()"), 0.4f, TEXT("FLinearColor.A getter"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor SetR()"), FLinearColor(0.9f, 0.2f, 0.3f, 0.4f), TEXT("FLinearColor.R setter"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor SetG()"), FLinearColor(0.1f, 0.8f, 0.3f, 0.4f), TEXT("FLinearColor.G setter"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor SetB()"), FLinearColor(0.1f, 0.2f, 0.7f, 0.4f), TEXT("FLinearColor.B setter"));
		ExpectColorNearlyEqual(Engine, Module, TEXT("FLinearColor SetA()"), FLinearColor(0.1f, 0.2f, 0.3f, 1.0f), TEXT("FLinearColor.A setter"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
