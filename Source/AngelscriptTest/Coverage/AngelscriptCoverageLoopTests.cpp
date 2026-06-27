#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageLoopTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript loop statements:
//   - for loops (count, decrement, step, infinite)
//   - for-each (value, reference, const reference)
//   - while loops
//   - do-while loops
//   - nested loops
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptCoverageLoopTests_NS
{
	TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageLoopTest,
		"Angelscript.TestModule.Coverage.Loop",
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
		// Basic for loops
		// -------------------------------------------------------------------------
		TEST_METHOD(ForBasic)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_ForBasic", ASTEST_AS(R"AS(
			// Count up
			int ForCountUp()
			{
				int Sum = 0;
				for (int i = 0; i < 10; i++)
				{
					Sum += i;
				}
				return Sum;
			}

			// Count down
			int ForCountDown()
			{
				int Sum = 0;
				for (int i = 10; i >= 0; i--)
				{
					Sum += i;
				}
				return Sum;
			}

			// Step by 2
			int ForStepTwo()
			{
				int Sum = 0;
				for (int i = 0; i < 10; i += 2)
				{
					Sum += i;
				}
				return Sum;
			}

			// Empty body
			int ForEmptyBody()
			{
				int Count = 0;
				for (int i = 0; i < 5; i++)
					Count++;
				return Count;
			}

			// Multiple loop variables
			int ForMultipleVars()
			{
				int Sum = 0;
				for (int i = 0, j = 10; i < 5; i++, j--)
				{
					Sum += i + j;
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForCountUp()"), 45, TEXT("for count up 0-9"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForCountDown()"), 55, TEXT("for count down 10-0"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForStepTwo()"), 20, TEXT("for step by 2"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForEmptyBody()"), 5, TEXT("for empty body"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForMultipleVars()"), 50, TEXT("for multiple variables"));
		}

		// -------------------------------------------------------------------------
		// For loop variations
		// -------------------------------------------------------------------------
		TEST_METHOD(ForVariations)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_ForVariations", ASTEST_AS(R"AS(
			// No initialization
			int ForNoInit()
			{
				int i = 0;
				int Sum = 0;
				for (; i < 5; i++)
				{
					Sum += i;
				}
				return Sum;
			}

			// No condition (infinite loop with break)
			int ForNoCondition()
			{
				int Sum = 0;
				int i = 0;
				for (;;)
				{
					Sum += i;
					i++;
					if (i >= 5)
						break;
				}
				return Sum;
			}

			// No increment
			int ForNoIncrement()
			{
				int Sum = 0;
				for (int i = 0; i < 5;)
				{
					Sum += i;
					i++;
				}
				return Sum;
			}

			// Empty for (all parts missing)
			int ForAllEmpty()
			{
				int Sum = 0;
				int i = 0;
				for (;;)
				{
					Sum += i;
					i++;
					if (i >= 3)
						break;
				}
				return Sum;
			}

			// Decrement with step
			int ForDecrementStep()
			{
				int Sum = 0;
				for (int i = 20; i > 0; i -= 3)
				{
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForNoInit()"), 10, TEXT("for no initialization"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForNoCondition()"), 10, TEXT("for no condition"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForNoIncrement()"), 10, TEXT("for no increment"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForAllEmpty()"), 3, TEXT("for all empty"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForDecrementStep()"), 77, TEXT("for decrement with step"));
		}

		// -------------------------------------------------------------------------
		// for-each loops
		// -------------------------------------------------------------------------
		TEST_METHOD(ForEach)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_ForEach", ASTEST_AS(R"AS(
			// For-each with value (copy)
			int ForEachValue()
			{
				TArray<int> Arr;
				Arr.Add(1);
				Arr.Add(2);
				Arr.Add(3);
				Arr.Add(4);
				Arr.Add(5);

				int Sum = 0;
				for (int Val : Arr)
				{
					Sum += Val;
				}
				return Sum;
			}

			// For-each with reference (can modify)
			int ForEachReference()
			{
				TArray<int> Arr;
				Arr.Add(1);
				Arr.Add(2);
				Arr.Add(3);

				for (int& Val : Arr)
				{
					Val *= 2;
				}

				int Sum = 0;
				for (int Val : Arr)
				{
					Sum += Val;
				}
				return Sum;
			}

			// For-each with const reference (read-only)
			int ForEachConstRef()
			{
				TArray<int> Arr;
				Arr.Add(10);
				Arr.Add(20);
				Arr.Add(30);

				int Sum = 0;
				for (const int& Val : Arr)
				{
					Sum += Val;
				}
				return Sum;
			}

			// For-each with TSet
			int ForEachSet()
			{
				TSet<int> Set;
				Set.Add(5);
				Set.Add(10);
				Set.Add(15);

				int Sum = 0;
				for (int Val : Set)
				{
					Sum += Val;
				}
				return Sum;
			}

			// For-each with TMap
			int ForEachMap()
			{
				TMap<int, int> Map;
				Map.Add(1, 10);
				Map.Add(2, 20);
				Map.Add(3, 30);

				int Sum = 0;
				for (auto& Pair : Map)
				{
					Sum += Pair.Key + Pair.Value;
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForEachValue()"), 15, TEXT("for-each value copy"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForEachReference()"), 12, TEXT("for-each reference modify"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForEachConstRef()"), 60, TEXT("for-each const reference"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForEachSet()"), 30, TEXT("for-each TSet"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForEachMap()"), 66, TEXT("for-each TMap"));
		}

		// -------------------------------------------------------------------------
		// Nested for loops
		// -------------------------------------------------------------------------
		TEST_METHOD(ForNested)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_ForNested", ASTEST_AS(R"AS(
			// Nested for loops
			int NestedFor()
			{
				int Sum = 0;
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 3; j++)
					{
						Sum += i * 10 + j;
					}
				}
				return Sum;
			}

			// Triple nested
			int TripleNested()
			{
				int Count = 0;
				for (int i = 0; i < 2; i++)
				{
					for (int j = 0; j < 2; j++)
					{
						for (int k = 0; k < 2; k++)
						{
							Count++;
						}
					}
				}
				return Count;
			}

			// Nested with arrays
			int NestedWithArrays()
			{
				TArray<int> Outer;
				Outer.Add(1);
				Outer.Add(2);

				int Sum = 0;
				for (int Val1 : Outer)
				{
					TArray<int> Inner;
					Inner.Add(10);
					Inner.Add(20);

					for (int Val2 : Inner)
					{
						Sum += Val1 + Val2;
					}
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedFor()"), 99, TEXT("nested for loops"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int TripleNested()"), 8, TEXT("triple nested loops"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedWithArrays()"), 66, TEXT("nested with arrays"));
		}

		// -------------------------------------------------------------------------
		// while loops
		// -------------------------------------------------------------------------
		TEST_METHOD(WhileBasic)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_WhileBasic", ASTEST_AS(R"AS(
			// Basic while
			int BasicWhile()
			{
				int Sum = 0;
				int i = 0;
				while (i < 5)
				{
					Sum += i;
					i++;
				}
				return Sum;
			}

			// While with complex condition
			int WhileComplexCondition()
			{
				int Sum = 0;
				int i = 0;
				while (i < 10 && Sum < 20)
				{
					Sum += i;
					i++;
				}
				return Sum;
			}

			// Infinite while with break
			int WhileInfinite()
			{
				int Sum = 0;
				int i = 0;
				while (true)
				{
					Sum += i;
					i++;
					if (i >= 5)
						break;
				}
				return Sum;
			}

			// Nested while
			int NestedWhile()
			{
				int Sum = 0;
				int i = 0;
				while (i < 3)
				{
					int j = 0;
					while (j < 2)
					{
						Sum += i + j;
						j++;
					}
					i++;
				}
				return Sum;
			}

			// While with continue
			int WhileWithContinue()
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
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BasicWhile()"), 10, TEXT("basic while loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int WhileComplexCondition()"), 21, TEXT("while complex condition"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int WhileInfinite()"), 10, TEXT("infinite while with break"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedWhile()"), 9, TEXT("nested while loops"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int WhileWithContinue()"), 25, TEXT("while with continue"));
		}

		// -------------------------------------------------------------------------
		// do-while loops
		// -------------------------------------------------------------------------
		TEST_METHOD(DoWhileBasic)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_DoWhileBasic", ASTEST_AS(R"AS(
			// Basic do-while (executes at least once)
			int BasicDoWhile()
			{
				int Sum = 0;
				int i = 0;
				do
				{
					Sum += i;
					i++;
				} while (i < 5);
				return Sum;
			}

			// Do-while executes at least once even if condition is false
			int DoWhileOnce()
			{
				int Count = 0;
				do
				{
					Count++;
				} while (false);
				return Count;
			}

			// Do-while with break
			int DoWhileWithBreak()
			{
				int Sum = 0;
				int i = 0;
				do
				{
					Sum += i;
					i++;
					if (i >= 3)
						break;
				} while (true);
				return Sum;
			}

			// Do-while with continue
			int DoWhileWithContinue()
			{
				int Sum = 0;
				int i = 0;
				do
				{
					i++;
					if (i % 2 == 0)
						continue;
					Sum += i;
				} while (i < 10);
				return Sum;
			}

			// Nested do-while
			int NestedDoWhile()
			{
				int Sum = 0;
				int i = 0;
				do
				{
					int j = 0;
					do
					{
						Sum += i + j;
						j++;
					} while (j < 2);
					i++;
				} while (i < 2);
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BasicDoWhile()"), 10, TEXT("basic do-while"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int DoWhileOnce()"), 1, TEXT("do-while executes at least once"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int DoWhileWithBreak()"), 3, TEXT("do-while with break"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int DoWhileWithContinue()"), 25, TEXT("do-while with continue"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedDoWhile()"), 4, TEXT("nested do-while"));
		}

		// -------------------------------------------------------------------------
		// Infinite loops with break
		// -------------------------------------------------------------------------
		TEST_METHOD(InfiniteLoops)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovLoop_InfiniteLoops", ASTEST_AS(R"AS(
			// Infinite for with break
			int InfiniteFor()
			{
				int Sum = 0;
				for (;;)
				{
					Sum++;
					if (Sum >= 10)
						break;
				}
				return Sum;
			}

			// Infinite while with break
			int InfiniteWhile()
			{
				int Sum = 0;
				while (true)
				{
					Sum++;
					if (Sum >= 7)
						break;
				}
				return Sum;
			}

			// Infinite do-while with break
			int InfiniteDoWhile()
			{
				int Sum = 0;
				do
				{
					Sum++;
					if (Sum >= 5)
						break;
				} while (true);
				return Sum;
			}

			// Multiple breaks
			int MultipleBreaks()
			{
				int Sum = 0;
				for (int i = 0; i < 100; i++)
				{
					if (i > 20)
						break;
					if (i % 2 == 0)
						Sum += i;
					if (Sum > 50)
						break;
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

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int InfiniteFor()"), 10, TEXT("infinite for with break"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int InfiniteWhile()"), 7, TEXT("infinite while with break"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int InfiniteDoWhile()"), 5, TEXT("infinite do-while with break"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleBreaks()"), 56, TEXT("multiple breaks"));
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
