// ============================================================================
// AngelscriptIteratorBindingsTests.cpp
//
// TSet/TMap iterator binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.Iterator.FAngelscriptIteratorBindingsTest.*
//
// Sections:
//   SetIterator        — TSetIterator sum via CanProceed/Proceed
//   MapIterator        — TMapIterator sum + key enumeration
//   MapIteratorPairing — empty-map guard, copy semantics, key/value
//                        correspondence preservation
//
// CQTest adaptation notes:
//   Three original legacy automation classes merged into one
//   TEST_CLASS with three TEST_METHODs.  Each `int Entry()` has been renamed
//   to a descriptive function returning 1 (pass) or 0 (fail).  The pairing
//   test is split into empty-map guard + full traversal-with-copy.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptIteratorBindingsTest,
	"Angelscript.TestModule.Bindings.Iterator",
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

	// ====================================================================
	// Section: SetIterator
	// ====================================================================

	TEST_METHOD(SetIterator)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIterator_SetIter"), ASTEST_AS(R"AS(
			int SetIter_SumElements()
			{
				TSet<int> Values;
				Values.Add(2);
				Values.Add(5);

				TSetIterator<int> It = Values.Iterator();
				int Sum = 0;
				while (It.CanProceed)
				{
					Sum += It.Proceed();
				}

				return (Sum == 7) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SetIter_SumElements()"), TEXT("TSet iterator should sum all elements via CanProceed/Proceed"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: MapIterator
	// ====================================================================

	TEST_METHOD(MapIterator)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIterator_MapIter"), ASTEST_AS(R"AS(
			int MapIter_SumValuesAndCountKeys()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 2);
				Values.Add(FName("Beta"), 5);

				TMapIterator<FName, int> It = Values.Iterator();
				int Sum = 0;
				int KeyCount = 0;
				while (It.CanProceed)
				{
					It.Proceed();
					if (It.GetKey() == FName("Alpha") || It.GetKey() == FName("Beta"))
					{
						KeyCount += 1;
					}
					Sum += It.GetValue();
				}

				return (Sum == 7 && KeyCount == 2) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapIter_SumValuesAndCountKeys()"), TEXT("TMap iterator should sum values and enumerate keys"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: MapIteratorPairing
	// ====================================================================

	TEST_METHOD(MapIteratorPairing)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIterator_MapIterPair"), ASTEST_AS(R"AS(
			bool MatchesExpectedPair(FName Key, int Value)
			{
				if (Key == FName("Alpha"))
				{
					return Value == 2;
				}
				if (Key == FName("Beta"))
				{
					return Value == 9;
				}
				if (Key == FName("Gamma"))
				{
					return Value == 17;
				}
				return false;
			}

			int MapIterPair_EmptyMapDoesNotProceed()
			{
				TMap<FName, int> Empty;
				return (!Empty.Iterator().CanProceed) ? 1 : 0;
			}

			int MapIterPair_CopyPreservesCorrespondence()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 2);
				Values.Add(FName("Beta"), 9);
				Values.Add(FName("Gamma"), 17);

				TMapIterator<FName, int> It = Values.Iterator();
				if (!It.CanProceed)
				{
					return 0;
				}

				It.Proceed();
				if (!MatchesExpectedPair(It.GetKey(), It.GetValue()))
				{
					return 0;
				}

				TMapIterator<FName, int> Copy = It;

				TMap<FName, int> OriginalRemaining;
				TMap<FName, int> CopyRemaining;

				OriginalRemaining.Add(It.GetKey(), It.GetValue());
				CopyRemaining.Add(Copy.GetKey(), Copy.GetValue());

				while (It.CanProceed)
				{
					It.Proceed();
					if (!MatchesExpectedPair(It.GetKey(), It.GetValue()))
					{
						return 0;
					}
					OriginalRemaining.Add(It.GetKey(), It.GetValue());
				}

				while (Copy.CanProceed)
				{
					Copy.Proceed();
					if (!MatchesExpectedPair(Copy.GetKey(), Copy.GetValue()))
					{
						return 0;
					}
					CopyRemaining.Add(Copy.GetKey(), Copy.GetValue());
				}

				if (It.CanProceed || Copy.CanProceed)
				{
					return 0;
				}

				if (OriginalRemaining.Num() != 3 || CopyRemaining.Num() != 3)
				{
					return 0;
				}

				int Value = 0;
				if (!OriginalRemaining.Find(FName("Alpha"), Value) || Value != 2)
				{
					return 0;
				}
				if (!OriginalRemaining.Find(FName("Beta"), Value) || Value != 9)
				{
					return 0;
				}
				if (!OriginalRemaining.Find(FName("Gamma"), Value) || Value != 17)
				{
					return 0;
				}

				if (!CopyRemaining.Find(FName("Alpha"), Value) || Value != 2)
				{
					return 0;
				}
				if (!CopyRemaining.Find(FName("Beta"), Value) || Value != 9)
				{
					return 0;
				}
				if (!CopyRemaining.Find(FName("Gamma"), Value) || Value != 17)
				{
					return 0;
				}

				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapIterPair_EmptyMapDoesNotProceed()"), TEXT("empty TMap iterator should not proceed"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapIterPair_CopyPreservesCorrespondence()"), TEXT("TMap iterator copy should preserve key/value correspondence and remaining state"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
