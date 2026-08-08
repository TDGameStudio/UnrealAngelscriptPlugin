// ============================================================================
// AngelscriptSetBindingsAdvancedTests.cpp
//
// TSet advanced binding contract smoke. Broad TSet semantics live in Coverage
// (`03-containers`); this file only proves the advanced entrypoints are exposed.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSetAdvancedBindingsTest,
	"Angelscript.TestModule.Bindings.SetAdvanced",
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

	TEST_METHOD(TSetAdvancedContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSetAdvanced_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTSetAdvancedContractSmoke()
			{
				TArray<int> SourceArray;
				SourceArray.Add(4);
				SourceArray.Add(7);
				SourceArray.Add(7);

				TSet<int> Values;
				Values.Append(SourceArray);

				TSet<int> Extra;
				Extra.Add(1);
				Extra.Add(4);
				Values.Append(Extra);

				TSet<int> Copy = Values;
				Copy.Remove(4);

				TSet<int> Assigned;
				Assigned.Add(99);
				Assigned = Copy;
				Assigned.Empty(8);

				return Values.Num() == 3
					&& Values.Contains(1)
					&& Values.Contains(4)
					&& Values.Contains(7)
					&& Copy.Num() == 2
					&& !Copy.Contains(4)
					&& Assigned.IsEmpty() ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTSetAdvancedContractSmoke()"),
			TEXT("TSet Append, copy, assignment, Remove, and Empty bindings should dispatch"),
			1)));
	}

	TEST_METHOD(TSetIterationResetAndEquality)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSetAdvanced_IterationResetAndEquality"), ASTEST_AS(R"AS(
			int SumWithConstIterator(const TSet<int>&in Values)
			{
				TSetConstIterator<int> Iterator = Values.Iterator();
				int Sum = 0;
				int VisitCount = 0;
				while (Iterator.CanProceed)
				{
					Sum += Iterator.Proceed();
					VisitCount++;
				}
				return Sum * 10 + VisitCount;
			}

			int VerifyTSetIterationResetAndEquality()
			{
				TSet<int> Values;
				Values.Add(2);
				Values.Add(4);
				Values.Add(8);

				int RangeSum = 0;
				for (int Value : Values)
				{
					RangeSum += Value;
				}

				TSetIterator<int> Original = Values.Iterator();
				TSetIterator<int> Copied = Original;
				TSetIterator<int> Assigned = Values.Iterator();
				Assigned = Copied;

				int IteratorSum = 0;
				int IteratorVisitCount = 0;
				while (Original.CanProceed)
				{
					IteratorSum += Original.Proceed();
					IteratorVisitCount++;
				}
				while (Copied.CanProceed)
				{
					IteratorSum += Copied.Proceed();
					IteratorVisitCount++;
				}
				while (Assigned.CanProceed)
				{
					IteratorSum += Assigned.Proceed();
					IteratorVisitCount++;
				}

				TSet<int> Permuted;
				Permuted.Add(8);
				Permuted.Add(2);
				Permuted.Add(4);

				TSet<int> RemoveTarget = Values;
				bool bRemovedExisting = RemoveTarget.Remove(4);
				bool bRemovedMissing = RemoveTarget.Remove(99);

				TSet<int> ResetTarget = Values;
				ResetTarget.Reset();

				return RangeSum == 14
					&& IteratorSum == 42
					&& IteratorVisitCount == 9
					&& SumWithConstIterator(Values) == 143
					&& Values == Permuted
					&& bRemovedExisting
					&& !bRemovedMissing
					&& RemoveTarget.Num() == 2
					&& ResetTarget.IsEmpty() ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTSetIterationResetAndEquality()"),
			TEXT("TSet range, mutable/const iterator, equality, Remove result, and Reset bindings should dispatch"),
			1)));
	}
};

#endif
