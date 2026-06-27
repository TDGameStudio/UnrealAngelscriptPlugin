#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageConditionalTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript conditional statements and expressions:
//   - if / if-else / if-else if-else (nested)
//   - Ternary operator (? :)
//   - switch / case / default / break (enum switch, fallthrough)
//   - Various condition expressions (&&, ||, !, comparison, null check)
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptCoverageConditionalTests_NS
{
	TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageConditionalTest,
		"Angelscript.TestModule.Coverage.Conditional",
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
			if (Module == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("%s: backing module failed to build"), Message));
				return;
			}

			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
			const T Result = Invoker.CallAndReturn<T>();
			TestRunner->TestEqual(Message, Result, Expected);
		}

		// -------------------------------------------------------------------------
		// if / if-else / if-else if-else
		// -------------------------------------------------------------------------
		TEST_METHOD(IfBasic)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_IfBasic", ASTEST_AS(R"AS(
			// Simple if
			int SimpleIf(bool Condition)
			{
				int Result = 0;
				if (Condition)
				{
					Result = 10;
				}
				return Result;
			}

			// if-else
			int IfElse(bool Condition)
			{
				if (Condition)
				{
					return 1;
				}
				else
				{
					return 2;
				}
			}

			// if-else if
			int IfElseIf(int Value)
			{
				if (Value < 0)
				{
					return -1;
				}
				else if (Value > 0)
				{
					return 1;
				}
				else if (Value == 0)
				{
					return 0;
				}
				return 999;
			}

			// if-else if-else (complete chain)
			int IfElseIfElse(int Value)
			{
				if (Value < 10)
				{
					return 1;
				}
				else if (Value < 20)
				{
					return 2;
				}
				else if (Value < 30)
				{
					return 3;
				}
				else
				{
					return 4;
				}
			}

			// Single line if (no braces)
			int SingleLineIf(bool Condition)
			{
				int Result = 0;
				if (Condition)
					Result = 5;
				return Result;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SimpleIf(bool)"), 10, TEXT("simple if true"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int IfElse(bool)"), 1, TEXT("if-else true branch"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int IfElseIf(int)"), 1, TEXT("if-else if positive"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int IfElseIfElse(int)"), 2, TEXT("if-else if-else chain"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SingleLineIf(bool)"), 5, TEXT("single line if"));
		}

		// -------------------------------------------------------------------------
		// Nested if
		// -------------------------------------------------------------------------
		TEST_METHOD(IfNested)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_IfNested", ASTEST_AS(R"AS(
			int NestedIf(int X, int Y)
			{
				if (X > 0)
				{
					if (Y > 0)
					{
						return 1;
					}
					else
					{
						return 2;
					}
				}
				else
				{
					if (Y > 0)
					{
						return 3;
					}
					else
					{
						return 4;
					}
				}
			}

			int DeepNested(int Value)
			{
				if (Value > 0)
				{
					if (Value > 10)
					{
						if (Value > 20)
						{
							return 3;
						}
						return 2;
					}
					return 1;
				}
				return 0;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedIf(int, int)"), 1, TEXT("nested if both positive"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int DeepNested(int)"), 3, TEXT("deep nested if"));
		}

		// -------------------------------------------------------------------------
		// Various condition expressions
		// -------------------------------------------------------------------------
		TEST_METHOD(IfConditions)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_IfConditions", ASTEST_AS(R"AS(
			// bool variable
			int BoolVariable()
			{
				bool bFlag = true;
				if (bFlag)
					return 1;
				return 0;
			}

			// Comparison expression
			int ComparisonExpr(int Value)
			{
				if (Value > 10)
					return 1;
				if (Value < 5)
					return 2;
				if (Value == 7)
					return 3;
				if (Value != 8)
					return 4;
				return 0;
			}

			// Logical AND (short circuit)
			int LogicalAnd(int A, int B)
			{
				if (A > 0 && B > 0)
					return 1;
				return 0;
			}

			// Logical OR (short circuit)
			int LogicalOr(int A, int B)
			{
				if (A > 0 || B > 0)
					return 1;
				return 0;
			}

			// Logical NOT
			int LogicalNot(bool Flag)
			{
				if (!Flag)
					return 1;
				return 0;
			}

			// Complex expression
			int ComplexExpr(int A, int B, bool C)
			{
				if ((A > 0) && (B < 10) || C)
					return 1;
				return 0;
			}

			// Null check
			int NullCheck(UObject Obj)
			{
				if (Obj != nullptr)
					return 1;
				return 0;
			}

			// IsValid check
			int IsValidCheck(UObject Obj)
			{
				if (IsValid(Obj))
					return 1;
				return 0;
			}

			// Function return value
			bool IsReady()
			{
				return true;
			}

			int FunctionReturnCheck()
			{
				if (IsReady())
					return 1;
				return 0;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BoolVariable()"), 1, TEXT("bool variable condition"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ComparisonExpr(int)"), 1, TEXT("comparison expression"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int LogicalAnd(int, int)"), 1, TEXT("logical AND"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int LogicalOr(int, int)"), 1, TEXT("logical OR"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int LogicalNot(bool)"), 1, TEXT("logical NOT"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ComplexExpr(int, int, bool)"), 1, TEXT("complex expression"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NullCheck(UObject)"), 0, TEXT("null check"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int IsValidCheck(UObject)"), 0, TEXT("IsValid check"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int FunctionReturnCheck()"), 1, TEXT("function return check"));
		}

		// -------------------------------------------------------------------------
		// Ternary operator
		// -------------------------------------------------------------------------
		TEST_METHOD(TernaryOperator)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_Ternary", ASTEST_AS(R"AS(
			// Basic ternary
			int BasicTernary(bool Condition)
			{
				int X = Condition ? 10 : 20;
				return X;
			}

			// Nested ternary
			int NestedTernary(int Value)
			{
				int X = Value > 0 ? 1 : (Value < 0 ? -1 : 0);
				return X;
			}

			// Return ternary
			int ReturnTernary(bool Flag)
			{
				return Flag ? 1 : 0;
			}

			// Ternary with expressions
			int TernaryExpressions(int A, int B)
			{
				return (A > B) ? (A + B) : (A - B);
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BasicTernary(bool)"), 10, TEXT("basic ternary true"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedTernary(int)"), 1, TEXT("nested ternary positive"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ReturnTernary(bool)"), 1, TEXT("return ternary"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int TernaryExpressions(int, int)"), 30, TEXT("ternary with expressions"));
		}

		// -------------------------------------------------------------------------
		// switch / case / default / break
		// -------------------------------------------------------------------------
		TEST_METHOD(SwitchBasic)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_SwitchBasic", ASTEST_AS(R"AS(
			// Basic switch
			int BasicSwitch(int Value)
			{
				switch (Value)
				{
					case 1:
						return 10;
					case 2:
						return 20;
					case 3:
						return 30;
					default:
						return 0;
				}
			}

			// Switch with break
			int SwitchWithBreak(int Value)
			{
				int Result = 0;
				switch (Value)
				{
					case 1:
						Result = 10;
						break;
					case 2:
						Result = 20;
						break;
					default:
						Result = 99;
						break;
				}
				return Result;
			}

			// Switch without default
			int SwitchNoDefault(int Value)
			{
				int Result = 0;
				switch (Value)
				{
					case 1:
						Result = 1;
						break;
					case 2:
						Result = 2;
						break;
				}
				return Result;
			}

			// Multiple cases (fallthrough)
			int MultipleCase(int Value)
			{
				switch (Value)
				{
					case 1:
					case 2:
					case 3:
						return 123;
					case 4:
					case 5:
						return 45;
					default:
						return 0;
				}
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BasicSwitch(int)"), 10, TEXT("basic switch case 1"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchWithBreak(int)"), 20, TEXT("switch with break"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchNoDefault(int)"), 1, TEXT("switch no default"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleCase(int)"), 123, TEXT("multiple case fallthrough"));
		}

		// -------------------------------------------------------------------------
		// switch fallthrough behavior
		// -------------------------------------------------------------------------
		TEST_METHOD(SwitchFallthrough)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_SwitchFallthrough", ASTEST_AS(R"AS(
			// Fallthrough behavior
			int Fallthrough(int Value)
			{
				int Result = 0;
				switch (Value)
				{
					case 1:
						Result += 1;
						// fallthrough
					case 2:
						Result += 2;
						// fallthrough
					case 3:
						Result += 3;
						break;
					default:
						Result = 0;
				}
				return Result;
			}

			// Partial fallthrough
			int PartialFallthrough(int Value)
			{
				int Result = 0;
				switch (Value)
				{
					case 1:
						Result += 10;
						break;
					case 2:
						Result += 20;
						// fallthrough
					case 3:
						Result += 30;
						break;
					default:
						Result = 0;
				}
				return Result;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int Fallthrough(int)"), 6, TEXT("fallthrough from case 1"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int PartialFallthrough(int)"), 50, TEXT("partial fallthrough case 2"));
		}

		// -------------------------------------------------------------------------
		// switch with enum
		// -------------------------------------------------------------------------
		TEST_METHOD(SwitchEnum)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_SwitchEnum", ASTEST_AS(R"AS(
			// Switch on enum
			int SwitchOnEnum(ECollisionChannel Channel)
			{
				switch (Channel)
				{
					case ECollisionChannel::ECC_WorldStatic:
						return 1;
					case ECollisionChannel::ECC_WorldDynamic:
						return 2;
					case ECollisionChannel::ECC_Pawn:
						return 3;
					case ECollisionChannel::ECC_Visibility:
						return 4;
					default:
						return 0;
				}
			}

			// Enum switch without default (complete coverage)
			int EnumSwitchComplete(ENetRole Role)
			{
				switch (Role)
				{
					case ENetRole::ROLE_None:
						return 0;
					case ENetRole::ROLE_SimulatedProxy:
						return 1;
					case ENetRole::ROLE_AutonomousProxy:
						return 2;
					case ENetRole::ROLE_Authority:
						return 3;
				}
				return -1;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchOnEnum(ECollisionChannel)"), 1, TEXT("switch on enum"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int EnumSwitchComplete(ENetRole)"), 0, TEXT("enum switch complete coverage"));
		}

		// -------------------------------------------------------------------------
		// switch with various types
		// -------------------------------------------------------------------------
		TEST_METHOD(SwitchTypes)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovConditional_SwitchTypes", ASTEST_AS(R"AS(
			// Switch on int8
			int SwitchInt8(int8 Value)
			{
				switch (Value)
				{
					case 1:
						return 10;
					case 2:
						return 20;
					default:
						return 0;
				}
			}

			// Switch on int16
			int SwitchInt16(int16 Value)
			{
				switch (Value)
				{
					case 100:
						return 1;
					case 200:
						return 2;
					default:
						return 0;
				}
			}

			// Switch on int64
			int SwitchInt64(int64 Value)
			{
				switch (Value)
				{
					case 1000:
						return 1;
					case 2000:
						return 2;
					default:
						return 0;
				}
			}

			// Switch on uint8
			int SwitchUInt8(uint8 Value)
			{
				switch (Value)
				{
					case 5:
						return 50;
					case 10:
						return 100;
					default:
						return 0;
				}
			}

			// Switch on uint
			int SwitchUInt(uint Value)
			{
				switch (Value)
				{
					case 42:
						return 1;
					case 100:
						return 2;
					default:
						return 0;
				}
			}

			// Switch on bool (valid but not recommended)
			int SwitchBool(bool Value)
			{
				switch (Value)
				{
					case true:
						return 1;
					case false:
						return 0;
				}
				return -1;
			}

			// Switch on FName
			int SwitchFName(FName Name)
			{
				switch (Name)
				{
					case n"Alpha":
						return 1;
					case n"Beta":
						return 2;
					case n"Gamma":
						return 3;
					default:
						return 0;
				}
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchInt8(int8)"), 10, TEXT("switch on int8"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchInt16(int16)"), 1, TEXT("switch on int16"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchInt64(int64)"), 1, TEXT("switch on int64"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchUInt8(uint8)"), 50, TEXT("switch on uint8"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchUInt(uint)"), 1, TEXT("switch on uint"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchBool(bool)"), 1, TEXT("switch on bool"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int SwitchFName(FName)"), 1, TEXT("switch on FName"));
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
