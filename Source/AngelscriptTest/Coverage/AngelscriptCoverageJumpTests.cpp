#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageJumpTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript jump statements:
//   - break (exit loop and switch)
//   - continue (skip iteration)
//   - return (early return, multiple return points)
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptCoverageJumpTests_NS
{
	TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageJumpTest,
		"Angelscript.TestModule.Coverage.Jump",
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
		template <typename T, typename... ArgTypes>
		void ExpectGlobalReturn(
			FAngelscriptEngine& Engine,
			asIScriptModule* Module,
			const TCHAR* Declaration,
			const T& Expected,
			const TCHAR* Message,
			ArgTypes... Args)
		{
			ASSERT_THAT(IsNotNull(Module, TEXT("jump module should compile before executing global function")));
			if (Module == nullptr)
			{
				return;
			}

			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("jump global function should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			(Invoker.AddArg(Args), ...);
			const T Result = Invoker.ExecuteAndGet<T>();
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}

		// -------------------------------------------------------------------------
		// break in loops
		// -------------------------------------------------------------------------
		TEST_METHOD(BreakInLoop)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovJump_BreakInLoop", ASTEST_AS(R"AS(
			// Break in for loop
			int BreakInFor()
			{
				int Sum = 0;
				for (int i = 0; i < 100; i++)
				{
					if (i >= 5)
						break;
					Sum += i;
				}
				return Sum;
			}

			// Break in while loop
			int BreakInWhile()
			{
				int Sum = 0;
				int i = 0;
				while (i < 100)
				{
					if (i >= 7)
						break;
					Sum += i;
					i++;
				}
				return Sum;
			}

			// Break in do-while loop
			int BreakInDoWhile()
			{
				int Sum = 0;
				int i = 0;
				do
				{
					if (i >= 4)
						break;
					Sum += i;
					i++;
				} while (i < 100);
				return Sum;
			}

			// Multiple breaks in loop
			int MultipleBreaks()
			{
				int Sum = 0;
				for (int i = 0; i < 100; i++)
				{
					if (i < 0)
						break;
					if (i > 10)
						break;
					Sum += i;
				}
				return Sum;
			}

			// Break in nested loop (only exits inner)
			int BreakInNested()
			{
				int Count = 0;
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 5; j++)
					{
						if (j >= 2)
							break;
						Count++;
					}
				}
				return Count;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakInFor()"), 10, TEXT("break in for loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakInWhile()"), 21, TEXT("break in while loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakInDoWhile()"), 6, TEXT("break in do-while loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleBreaks()"), 55, TEXT("multiple breaks in loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakInNested()"), 6, TEXT("break in nested loop"));
		}

		// -------------------------------------------------------------------------
		// break in switch
		// -------------------------------------------------------------------------
		TEST_METHOD(BreakInSwitch)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			TestRunner->AddExpectedError(
				TEXT("Non-empty switch case with fallthrough to the next case"),
				EAutomationExpectedErrorFlags::Contains,
				1);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovJump_BreakInSwitch", ASTEST_AS(R"AS(
			// Break in switch
			int BreakInSwitch(int Value)
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
					case 3:
						Result = 30;
						break;
					default:
						Result = 99;
						break;
				}
				return Result;
			}

			// Break prevents fallthrough
			int BreakPreventsFallthrough(int Value)
			{
				int Result = 0;
				switch (Value)
				{
					case 1:
						Result += 1;
						break;
					case 2:
						Result += 2;
						// No break - fallthrough
					case 3:
						Result += 3;
						break;
					default:
						Result = 0;
				}
				return Result;
			}

			// Multiple breaks in switch
			int MultipleBreaksInSwitch(int Value)
			{
				int Result = 0;
				switch (Value)
				{
					case 1:
						Result = 10;
						if (Result > 5)
							break;
						Result = 20;
						break;
					case 2:
						Result = 30;
						break;
					default:
						break;
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakInSwitch(int)"), 10, TEXT("break in switch"), 1);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakPreventsFallthrough(int)"), 5, TEXT("break prevents fallthrough"), 2);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleBreaksInSwitch(int)"), 10, TEXT("multiple breaks in switch"), 1);
		}

		// -------------------------------------------------------------------------
		// continue in loops
		// -------------------------------------------------------------------------
		TEST_METHOD(ContinueInLoop)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovJump_ContinueInLoop", ASTEST_AS(R"AS(
			// Continue in for loop
			int ContinueInFor()
			{
				int Sum = 0;
				for (int i = 0; i < 10; i++)
				{
					if (i % 2 == 0)
						continue;
					Sum += i;
				}
				return Sum;
			}

			// Continue in while loop
			int ContinueInWhile()
			{
				int Sum = 0;
				int i = 0;
				while (i < 10)
				{
					i++;
					if (i % 2 == 0)
						continue;
					Sum += i;
				}
				return Sum;
			}

			// Continue in do-while loop
			int ContinueInDoWhile()
			{
				int Sum = 0;
				int i = 0;
				do
				{
					i++;
					if (i % 3 == 0)
						continue;
					Sum += i;
				} while (i < 10);
				return Sum;
			}

			// Multiple continues
			int MultipleContinues()
			{
				int Sum = 0;
				for (int i = 0; i < 20; i++)
				{
					if (i < 5)
						continue;
					if (i > 15)
						continue;
					Sum += i;
				}
				return Sum;
			}

			// Continue in nested loop (only affects inner)
			int ContinueInNested()
			{
				int Count = 0;
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 5; j++)
					{
						if (j % 2 == 0)
							continue;
						Count++;
					}
				}
				return Count;
			}

			// Continue with complex condition
			int ContinueComplexCondition()
			{
				int Sum = 0;
				for (int i = 0; i < 20; i++)
				{
					if (i % 2 == 0 || i > 15)
						continue;
					Sum += i;
				}
				return Sum;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ContinueInFor()"), 25, TEXT("continue in for loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ContinueInWhile()"), 25, TEXT("continue in while loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ContinueInDoWhile()"), 37, TEXT("continue in do-while loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleContinues()"), 110, TEXT("multiple continues"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ContinueInNested()"), 6, TEXT("continue in nested loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ContinueComplexCondition()"), 64, TEXT("continue with complex condition"));
		}

		// -------------------------------------------------------------------------
		// Early return
		// -------------------------------------------------------------------------
		TEST_METHOD(ReturnEarly)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovJump_ReturnEarly", ASTEST_AS(R"AS(
			// Early return from function
			int EarlyReturn(int Value)
			{
				if (Value < 0)
					return -1;
				if (Value > 100)
					return 100;
				return Value;
			}

			// Early return in loop
			int EarlyReturnInLoop()
			{
				for (int i = 0; i < 10; i++)
				{
					if (i == 5)
						return i;
				}
				return -1;
			}

			// Early return void function
			void EarlyReturnVoid(int Value)
			{
				if (Value < 0)
					return;
				if (Value > 10)
					return;
				// Do something
			}

			// Guard clause pattern
			int GuardClause(int Value)
			{
				if (Value < 0)
					return 0;
				if (Value > 100)
					return 100;

				int Result = Value * 2;
				return Result;
			}

			// Early return with nested conditions
			int EarlyReturnNested(int A, int B)
			{
				if (A > 0)
				{
					if (B > 0)
						return 1;
					return 2;
				}
				return 3;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int EarlyReturn(int)"), -1, TEXT("early return negative"), -5);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int EarlyReturnInLoop()"), 5, TEXT("early return in loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int GuardClause(int)"), 0, TEXT("guard clause pattern"), -5);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int EarlyReturnNested(int, int)"), 1, TEXT("early return nested"), 1, 1);
		}

		// -------------------------------------------------------------------------
		// Multiple return points
		// -------------------------------------------------------------------------
		TEST_METHOD(MultipleReturns)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovJump_MultipleReturns", ASTEST_AS(R"AS(
			// Multiple return points
			int MultipleReturnPoints(int Value)
			{
				if (Value < 0)
					return -1;
				else if (Value == 0)
					return 0;
				else if (Value < 10)
					return 1;
				else if (Value < 100)
					return 2;
				else
					return 3;
			}

			// Return from switch cases
			int ReturnFromSwitch(int Value)
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

			// Return with expression
			int ReturnExpression(int A, int B)
			{
				return A + B;
			}

			// Return ternary
			int ReturnTernary(int Value)
			{
				return Value > 0 ? 1 : -1;
			}

			// Multiple returns in complex logic
			int ComplexMultipleReturns(int X, int Y, int Z)
			{
				if (X > 0)
				{
					if (Y > 0)
					{
						if (Z > 0)
							return 1;
						else
							return 2;
					}
					else
					{
						return 3;
					}
				}
				else
				{
					if (Y > 0)
						return 4;
					else
						return 5;
				}
			}

			// Return in nested loops
			int ReturnInNestedLoops()
			{
				for (int i = 0; i < 10; i++)
				{
					for (int j = 0; j < 10; j++)
					{
						if (i == 3 && j == 5)
							return i * 10 + j;
					}
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleReturnPoints(int)"), -1, TEXT("multiple return points"), -1);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ReturnFromSwitch(int)"), 10, TEXT("return from switch"), 1);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ReturnExpression(int, int)"), 30, TEXT("return expression"), 10, 20);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ReturnTernary(int)"), 1, TEXT("return ternary"), 1);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ComplexMultipleReturns(int, int, int)"), 1, TEXT("complex multiple returns"), 1, 1, 1);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ReturnInNestedLoops()"), 35, TEXT("return in nested loops"));
		}

		// -------------------------------------------------------------------------
		// Combined jump statements
		// -------------------------------------------------------------------------
		TEST_METHOD(CombinedJumps)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovJump_CombinedJumps", ASTEST_AS(R"AS(
			// Break and continue together
			int BreakAndContinue()
			{
				int Sum = 0;
				for (int i = 0; i < 20; i++)
				{
					if (i > 15)
						break;
					if (i % 2 == 0)
						continue;
					Sum += i;
				}
				return Sum;
			}

			// Break, continue, and return
			int AllThreeJumps(int Limit)
			{
				int Sum = 0;
				for (int i = 0; i < 100; i++)
				{
					if (Sum > Limit)
						return Sum;
					if (i > 50)
						break;
					if (i % 3 == 0)
						continue;
					Sum += i;
				}
				return Sum;
			}

			// Nested with various jumps
			int NestedVariousJumps()
			{
				int Count = 0;
				for (int i = 0; i < 5; i++)
				{
					if (i == 0)
						continue;
					for (int j = 0; j < 5; j++)
					{
						if (j == 2)
							continue;
						if (j == 4)
							break;
						Count++;
					}
					if (Count > 10)
						return Count;
				}
				return Count;
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BreakAndContinue()"), 64, TEXT("break and continue together"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int AllThreeJumps(int)"), 817, TEXT("break continue and return"), 800);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedVariousJumps()"), 12, TEXT("nested various jumps"));
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
