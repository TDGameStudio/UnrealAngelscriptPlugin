#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageBoolExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript bool *expression usage* -- operators, literals,
// conversions, and logical operations.
//
// Bool operations:
//   - Logical: &&, ||, !, ^
//   - Equality: ==, !=
//   - Bitwise: &, |, ^ (as bit operations)
//   - No arithmetic: +, -, *, /, %
//   - No ordering: <, <=, >, >=
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageBoolExpressionTest,
	"Angelscript.TestModule.Coverage.BoolExpression",
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
		const T Result = Invoker.ExecuteAndGet<T>(T{});
		TestRunner->TestEqual(Message, Result, Expected);
	}

	// -------------------------------------------------------------------------
	// Local declarations: default, const, true/false.
	// -------------------------------------------------------------------------
	TEST_METHOD(LocalDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_LocalDecl", ASTEST_AS(R"AS(
		bool LocalDefaultInit()
		{
			bool Value = true;
			return Value;
		}

		bool LocalDeferredInit()
		{
			bool Value;
			Value = false;
			return Value;
		}

		bool LocalConst()
		{
			const bool Value = true;
			return Value;
		}

		bool LocalNoDefault()
		{
			bool Value;
			return Value;  // Should be false
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalDefaultInit()"), true, TEXT("local bool with default true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalDeferredInit()"), false, TEXT("local bool deferred init false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalConst()"), true, TEXT("local const bool"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalNoDefault()"), false, TEXT("local bool no default should be false"));
	}

	// -------------------------------------------------------------------------
	// Global const declarations.
	// -------------------------------------------------------------------------
	TEST_METHOD(GlobalConstDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_GlobalConst", ASTEST_AS(R"AS(
		const bool GConstTrue = true;
		const bool GConstFalse = false;

		bool GetGlobalTrue()
		{
			return GConstTrue;
		}

		bool GetGlobalFalse()
		{
			return GConstFalse;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool GetGlobalTrue()"), true, TEXT("global const bool true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool GetGlobalFalse()"), false, TEXT("global const bool false"));
	}

	// -------------------------------------------------------------------------
	// Logical operators: &&, ||, !, ^
	// -------------------------------------------------------------------------
	TEST_METHOD(LogicalOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Logical", ASTEST_AS(R"AS(
		bool OpLogicalAnd_TT() { return true && true; }
		bool OpLogicalAnd_TF() { return true && false; }
		bool OpLogicalAnd_FT() { return false && true; }
		bool OpLogicalAnd_FF() { return false && false; }

		bool OpLogicalOr_TT() { return true || true; }
		bool OpLogicalOr_TF() { return true || false; }
		bool OpLogicalOr_FT() { return false || true; }
		bool OpLogicalOr_FF() { return false || false; }

		bool OpLogicalNot_T() { return !true; }
		bool OpLogicalNot_F() { return !false; }

		bool OpLogicalXor_TT() { return true ^ true; }
		bool OpLogicalXor_TF() { return true ^ false; }
		bool OpLogicalXor_FT() { return false ^ true; }
		bool OpLogicalXor_FF() { return false ^ false; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// &&
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_TT()"), true, TEXT("true && true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_TF()"), false, TEXT("true && false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_FT()"), false, TEXT("false && true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_FF()"), false, TEXT("false && false"));

		// ||
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_TT()"), true, TEXT("true || true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_TF()"), true, TEXT("true || false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_FT()"), true, TEXT("false || true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_FF()"), false, TEXT("false || false"));

		// !
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalNot_T()"), false, TEXT("!true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalNot_F()"), true, TEXT("!false"));

		// ^
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalXor_TT()"), false, TEXT("true ^ true (xor)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalXor_TF()"), true, TEXT("true ^ false (xor)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalXor_FT()"), true, TEXT("false ^ true (xor)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalXor_FF()"), false, TEXT("false ^ false (xor)"));
	}

	// -------------------------------------------------------------------------
	// Equality operators: ==, !=
	// -------------------------------------------------------------------------
	TEST_METHOD(EqualityOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Equality", ASTEST_AS(R"AS(
		bool OpEquals_TT() { return true == true; }
		bool OpEquals_TF() { return true == false; }
		bool OpEquals_FT() { return false == true; }
		bool OpEquals_FF() { return false == false; }

		bool OpNotEquals_TT() { return true != true; }
		bool OpNotEquals_TF() { return true != false; }
		bool OpNotEquals_FT() { return false != true; }
		bool OpNotEquals_FF() { return false != false; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// ==
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_TT()"), true, TEXT("true == true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_TF()"), false, TEXT("true == false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_FT()"), false, TEXT("false == true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_FF()"), true, TEXT("false == false"));

		// !=
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_TT()"), false, TEXT("true != true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_TF()"), true, TEXT("true != false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_FT()"), true, TEXT("false != true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_FF()"), false, TEXT("false != false"));
	}

	// -------------------------------------------------------------------------
	// Bitwise operators: &, |, ^ (as bit operations on bool)
	// -------------------------------------------------------------------------
	TEST_METHOD(BitwiseOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Bitwise", ASTEST_AS(R"AS(
		bool OpBitAnd_TT() { return true & true; }
		bool OpBitAnd_TF() { return true & false; }
		bool OpBitAnd_FT() { return false & true; }
		bool OpBitAnd_FF() { return false & false; }

		bool OpBitOr_TT() { return true | true; }
		bool OpBitOr_TF() { return true | false; }
		bool OpBitOr_FT() { return false | true; }
		bool OpBitOr_FF() { return false | false; }

		bool OpBitXor_TT() { return true ^ true; }
		bool OpBitXor_TF() { return true ^ false; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// & (bitwise AND, same as logical AND for bool)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitAnd_TT()"), true, TEXT("true & true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitAnd_TF()"), false, TEXT("true & false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitAnd_FT()"), false, TEXT("false & true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitAnd_FF()"), false, TEXT("false & false"));

		// | (bitwise OR, same as logical OR for bool)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitOr_TT()"), true, TEXT("true | true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitOr_TF()"), true, TEXT("true | false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitOr_FT()"), true, TEXT("false | true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitOr_FF()"), false, TEXT("false | false"));

		// ^ (bitwise XOR)
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitXor_TT()"), false, TEXT("true ^ true (bitwise)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpBitXor_TF()"), true, TEXT("true ^ false (bitwise)"));
	}

	// -------------------------------------------------------------------------
	// Literals: true, false.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Literals", ASTEST_AS(R"AS(
		bool LiteralTrue() { return true; }
		bool LiteralFalse() { return false; }
		bool LiteralFromComparison() { return 5 > 3; }
		bool LiteralFromLogical() { return true && true; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralTrue()"), true, TEXT("true literal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralFalse()"), false, TEXT("false literal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralFromComparison()"), true, TEXT("bool from comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralFromLogical()"), true, TEXT("bool from logical expr"));
	}

	// -------------------------------------------------------------------------
	// Type conversions: bool ↔ int, bool ↔ float.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Conversion", ASTEST_AS(R"AS(
		int BoolToInt_T() { return int(true); }
		int BoolToInt_F() { return int(false); }

		bool IntToBool_0() { return bool(0); }
		bool IntToBool_1() { return bool(1); }
		bool IntToBool_Neg() { return bool(-5); }
		bool IntToBool_Large() { return bool(999); }

		float BoolToFloat_T() { return float(true); }
		float BoolToFloat_F() { return float(false); }

		bool FloatToBool_0() { return bool(0.0f); }
		bool FloatToBool_Pos() { return bool(1.5f); }
		bool FloatToBool_Neg() { return bool(-2.5f); }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// bool → int
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int BoolToInt_T()"), 1, TEXT("bool(true) -> int(1)"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int BoolToInt_F()"), 0, TEXT("bool(false) -> int(0)"));

		// int → bool
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBool_0()"), false, TEXT("int(0) -> bool(false)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBool_1()"), true, TEXT("int(1) -> bool(true)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBool_Neg()"), true, TEXT("int(-5) -> bool(true)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBool_Large()"), true, TEXT("int(999) -> bool(true)"));

		// bool → float
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float BoolToFloat_T()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("bool(true) -> float(1.0)"), FMath::IsNearlyEqual(Result, 1.0f, 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float BoolToFloat_F()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("bool(false) -> float(0.0)"), FMath::IsNearlyEqual(Result, 0.0f, 0.001f));
		}

		// float → bool
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatToBool_0()"), false, TEXT("float(0.0) -> bool(false)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatToBool_Pos()"), true, TEXT("float(1.5) -> bool(true)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatToBool_Neg()"), true, TEXT("float(-2.5) -> bool(true)"));
	}

	// -------------------------------------------------------------------------
	// Logical short-circuit evaluation.
	// -------------------------------------------------------------------------
	TEST_METHOD(LogicalShortCircuit)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_ShortCircuit", ASTEST_AS(R"AS(
		int Counter = 0;

		bool SideEffect()
		{
			Counter++;
			return true;
		}

		bool TestAndShortCircuit()
		{
			Counter = 0;
			bool result = false && SideEffect();
			return Counter == 0;  // SideEffect should not be called
		}

		bool TestOrShortCircuit()
		{
			Counter = 0;
			bool result = true || SideEffect();
			return Counter == 0;  // SideEffect should not be called
		}

		bool TestAndNoShortCircuit()
		{
			Counter = 0;
			bool result = true && SideEffect();
			return Counter == 1;  // SideEffect should be called
		}

		bool TestOrNoShortCircuit()
		{
			Counter = 0;
			bool result = false || SideEffect();
			return Counter == 1;  // SideEffect should be called
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestAndShortCircuit()"), true, TEXT("&& short-circuit (false && ...)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestOrShortCircuit()"), true, TEXT("|| short-circuit (true || ...)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestAndNoShortCircuit()"), true, TEXT("&& no short-circuit (true && ...)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestOrNoShortCircuit()"), true, TEXT("|| no short-circuit (false || ...)"));
	}

	// -------------------------------------------------------------------------
	// Class members (non-UPROPERTY).
	// -------------------------------------------------------------------------
	TEST_METHOD(ClassMembersNonProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_ClassMember", ASTEST_AS(R"AS(
		class BoolHolder
		{
			bool Value;

			BoolHolder()
			{
				Value = true;
			}

			bool GetValue() const
			{
				return Value;
			}

			void SetValue(bool v)
			{
				Value = v;
			}

			void Toggle()
			{
				Value = !Value;
			}
		}

		bool TestClassMemberAccess()
		{
			BoolHolder holder;
			return holder.Value;
		}

		bool TestClassMemberModify()
		{
			BoolHolder holder;
			holder.Value = false;
			return holder.GetValue();
		}

		bool TestClassMemberToggle()
		{
			BoolHolder holder;
			holder.Toggle();
			return holder.Value;  // Should be false
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestClassMemberAccess()"), true, TEXT("class member bool access"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestClassMemberModify()"), false, TEXT("class member bool modify"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestClassMemberToggle()"), false, TEXT("class member bool toggle"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
