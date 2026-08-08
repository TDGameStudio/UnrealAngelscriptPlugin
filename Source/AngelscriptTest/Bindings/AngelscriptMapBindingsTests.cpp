// ============================================================================
// AngelscriptMapBindingsTests.cpp
//
// TMap binding contract smoke. Broad TMap semantics live in Coverage
// (`03-containers`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptMapBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Map",
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

	TEST_METHOD(TMapContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASMap_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTMapContractSmoke()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 4);
				Values.Add(FName("Alpha"), 7);

				int Existing = 0;
				if (!Values.Find(FName("Alpha"), Existing) || Existing != 7)
				{
					return 0;
				}

				int MissingSentinel = 99;
				if (Values.Find(FName("Missing"), MissingSentinel) || MissingSentinel != 99)
				{
					return 0;
				}

				int& Gamma = Values.FindOrAdd(FName("Gamma"));
				Gamma = 33;
				int& Delta = Values.FindOrAdd(FName("Delta"), 11);
				Delta += 1;

				int GammaValue = 0;
				int DeltaValue = 0;
				return Values.Num() == 3
					&& Values.Find(FName("Gamma"), GammaValue)
					&& Values.Find(FName("Delta"), DeltaValue)
					&& GammaValue == 33
					&& DeltaValue == 12 ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTMapContractSmoke()"),
			TEXT("TMap Add/Find/FindOrAdd/ref-return bindings should dispatch"),
			1)));
	}

	TEST_METHOD(TMapMutationCollectionAndIterationSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASMap_MutationCollectionIteration"), ASTEST_AS(R"AS(
			int VerifyTMapMutationAndAssignment()
			{
				TMap<FName, int> Source;
				if (!Source.IsEmpty())
				{
					return 0;
				}

				Source.Add(FName("Alpha"), 4);
				Source.Add(FName("Beta"), 8);

				TMap<FName, int> Assigned;
				Assigned = Source;
				if (!(Assigned == Source)
					|| !Assigned.Contains(FName("Alpha"))
					|| Assigned[FName("Beta")] != 8)
				{
					return 0;
				}

				int RemovedValue = -1;
				if (!Assigned.RemoveAndCopyValue(FName("Alpha"), RemovedValue)
					|| RemovedValue != 4
					|| Assigned.Contains(FName("Alpha")))
				{
					return 0;
				}

				if (Assigned.Remove(FName("Missing"))
					|| !Assigned.Remove(FName("Beta"))
					|| !Assigned.IsEmpty())
				{
					return 0;
				}

				Source.Reset();
				return Source.IsEmpty() ? 1 : 0;
			}

			int VerifyTMapKeysValuesAndEmpty()
			{
				TMap<int, FString> Values;
				Values.Add(1, "One");
				Values.Add(2, "Two");
				Values.Add(3, "Three");

				TArray<int> Keys;
				TArray<FString> OutValues;
				Values.GetKeys(Keys);
				Values.GetValues(OutValues);
				if (Keys.Num() != 3
					|| !Keys.Contains(1)
					|| !Keys.Contains(2)
					|| !Keys.Contains(3)
					|| OutValues.Num() != 3
					|| !OutValues.Contains("One")
					|| !OutValues.Contains("Two")
					|| !OutValues.Contains("Three"))
				{
					return 0;
				}

				Values.Empty(8);
				return Values.IsEmpty() && Values.Num() == 0 ? 1 : 0;
			}

			int SumConstTMap(const TMap<FName, int>&in Values)
			{
				int Sum = 0;
				TMapConstIterator<FName, int> It = Values.Iterator();
				while (It.CanProceed)
				{
					It.Proceed();
					Sum += It.GetValue();
				}
				return Sum;
			}

			int VerifyTMapIterationSurface()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 1);
				Values.Add(FName("Beta"), 2);
				Values.Add(FName("Gamma"), 3);
				int ConstIteratorSum = SumConstTMap(Values);

				int ForeachSum = 0;
				int ForeachCount = 0;
				foreach (int Value, FName Key : Values)
				{
					ForeachSum += Value;
					if (Key != NAME_None)
					{
						ForeachCount++;
					}
				}

				int IteratorCount = 0;
				TMapIterator<FName, int> It = Values.Iterator();
				while (It.CanProceed)
				{
					It.Proceed();
					IteratorCount++;
					if (It.GetKey() == FName("Beta"))
					{
						It.RemoveCurrent();
					}
					else
					{
						It.SetValue(It.GetValue() + 10);
					}
				}

				int Alpha = 0;
				int Gamma = 0;
				return ForeachSum == 6
					&& ConstIteratorSum == 6
					&& ForeachCount == 3
					&& IteratorCount == 3
					&& Values.Num() == 2
					&& !Values.Contains(FName("Beta"))
					&& Values.Find(FName("Alpha"), Alpha)
					&& Values.Find(FName("Gamma"), Gamma)
					&& Alpha == 11
					&& Gamma == 13 ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTMapMutationAndAssignment()"),
			TEXT("TMap construction, assignment, equality, lookup, removal, and reset should dispatch"),
			1)));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTMapKeysValuesAndEmpty()"),
			TEXT("TMap GetKeys, GetValues, and Empty should preserve the collection contract"),
			1)));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTMapIterationSurface()"),
			TEXT("TMap foreach and explicit mutable iterator surfaces should dispatch"),
			1)));
	}
};

#endif
