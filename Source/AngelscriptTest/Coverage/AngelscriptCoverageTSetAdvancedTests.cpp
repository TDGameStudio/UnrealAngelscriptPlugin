#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageTSetAdvancedTests
// -----------------------------------------------------------------------------
// Advanced TSet container operations coverage, extending the basic int tests
// from AngelscriptCoverageIntPropertyTests.cpp.
//
// Matrix coverage (from Documents/Coverage/Coverage_Containers.md):
//   * TSetAdvancedOperations - Remove(), Find(), Empty(), Reset()
//   * TSetIteration          - for-each loops
//   * TSetSetOperations      - Union(), Intersect(), Difference(), Includes()
//   * TSetElementTypes       - FString, FName, enum, FVector
//   * TSetAsParameter        - Parameters and return values
//
// Basic operations (Add, Contains, Num) are already covered in the int tests.
// This file focuses on the remaining high-priority TSet operations.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageTSetAdvancedTest,
	"Angelscript.TestModule.Coverage.TSetAdvanced",
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

	// -------------------------------------------------------------------------
	// TSet advanced operations: Remove(), Find(), Empty(), Reset()
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetAdvancedOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_Advanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetAdvanced.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetAdvancedActor : AActor
			{
				UPROPERTY()
				TSet<int> TestSet;

				UPROPERTY()
				bool bFoundElement = false;

				UPROPERTY()
				int RemovedCount = 0;

				UPROPERTY()
				int SizeBeforeEmpty = 0;

				UPROPERTY()
				int SizeAfterEmpty = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Setup test data
					TestSet.Add(10);
					TestSet.Add(20);
					TestSet.Add(30);
					TestSet.Add(40);
					TestSet.Add(50);

					// Test Find() - returns pointer
					int* FoundPtr = TestSet.Find(30);
					bFoundElement = (FoundPtr != nullptr);

					// Test Remove()
					RemovedCount = TestSet.Remove(20);
					RemovedCount += TestSet.Remove(40);

					// Test Empty()
					SizeBeforeEmpty = TestSet.Num();
					TestSet.Empty();
					SizeAfterEmpty = TestSet.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTSetAdvancedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-advanced actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-advanced actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify Find() worked
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFoundElement"), true, TEXT("Find() should locate existing element"));

		// Verify Remove() worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RemovedCount"), 2, TEXT("Remove() should remove 2 elements"));

		// Verify set size before empty
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeBeforeEmpty"), 3, TEXT("Set should have 3 elements before Empty()"));

		// Verify Empty() worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeAfterEmpty"), 0, TEXT("Set should be empty after Empty()"));
	}

	// -------------------------------------------------------------------------
	// TSet iteration: for-each loops
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetIteration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_Iteration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetIteration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetIterationActor : AActor
			{
				UPROPERTY()
				TSet<int> TestSet;

				UPROPERTY()
				int Sum = 0;

				UPROPERTY()
				int IterationCount = 0;

				UPROPERTY()
				int MaxValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestSet.Add(15);
					TestSet.Add(25);
					TestSet.Add(35);
					TestSet.Add(45);

					// Test for-each iteration
					for (int Value : TestSet)
					{
						Sum += Value;
						IterationCount++;
						if (Value > MaxValue)
						{
							MaxValue = Value;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageTSetIterationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-iteration actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-iteration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify iteration worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Sum"), 120, TEXT("Sum should be 15+25+35+45=120"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IterationCount"), 4, TEXT("Should iterate over all 4 elements"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MaxValue"), 45, TEXT("Max value should be 45"));
	}

	// -------------------------------------------------------------------------
	// TSet set operations: Union, Intersect, Difference, Includes
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetSetOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_SetOperations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetSetOperations.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetSetOperationsActor : AActor
			{
				UPROPERTY()
				TSet<int> SetA;

				UPROPERTY()
				TSet<int> SetB;

				UPROPERTY()
				TSet<int> UnionResult;

				UPROPERTY()
				TSet<int> IntersectResult;

				UPROPERTY()
				TSet<int> DifferenceResult;

				UPROPERTY()
				bool bSetAIncludesSubset = false;

				UPROPERTY()
				bool bSetAIncludesSetB = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Setup SetA: {1, 2, 3, 4, 5}
					SetA.Add(1);
					SetA.Add(2);
					SetA.Add(3);
					SetA.Add(4);
					SetA.Add(5);

					// Setup SetB: {3, 4, 5, 6, 7}
					SetB.Add(3);
					SetB.Add(4);
					SetB.Add(5);
					SetB.Add(6);
					SetB.Add(7);

					// Test Union: A ∪ B = {1, 2, 3, 4, 5, 6, 7}
					UnionResult = SetA;
					UnionResult.Append(SetB);

					// Test Intersect: A ∩ B = {3, 4, 5}
					IntersectResult = SetA;
					IntersectResult.Intersect(SetB);

					// Test Difference: A - B = {1, 2}
					DifferenceResult = SetA;
					DifferenceResult.Difference(SetB);

					// Test Includes (subset check)
					TSet<int> Subset;
					Subset.Add(2);
					Subset.Add(3);
					bSetAIncludesSubset = SetA.Includes(Subset);

					// SetA does not include SetB (SetB has 6 and 7)
					bSetAIncludesSetB = SetA.Includes(SetB);
				}
			}
			)AS"),
			TEXT("ACoverageTSetSetOperationsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-set-operations actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-set-operations actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify Union result
		{
			int32 UnionSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("UnionResult"), UnionSize), TEXT("Should get UnionResult size")));
			ASSERT_THAT(AreEqual(7, UnionSize, TEXT("Union should have 7 elements")));
		}

		// Verify Intersect result
		{
			int32 IntersectSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("IntersectResult"), IntersectSize), TEXT("Should get IntersectResult size")));
			ASSERT_THAT(AreEqual(3, IntersectSize, TEXT("Intersect should have 3 elements")));
		}

		// Verify Difference result
		{
			int32 DifferenceSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("DifferenceResult"), DifferenceSize), TEXT("Should get DifferenceResult size")));
			ASSERT_THAT(AreEqual(2, DifferenceSize, TEXT("Difference should have 2 elements")));
		}

		// Verify Includes() results
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetAIncludesSubset"), true, TEXT("SetA should include subset {2, 3}"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetAIncludesSetB"), false, TEXT("SetA should not include SetB"));
	}

	// -------------------------------------------------------------------------
	// TSet element types: FString, FName, enum, FVector
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetElementTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_ElementTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetElementTypes.as"),
			ASTEST_AS(R"AS(
			enum class ETestEnum
			{
				Alpha,
				Beta,
				Gamma,
				Delta
			}

			UCLASS()
			class ACoverageTSetElementTypesActor : AActor
			{
				UPROPERTY()
				TSet<FString> StringSet;

				UPROPERTY()
				TSet<FName> NameSet;

				UPROPERTY()
				TSet<ETestEnum> EnumSet;

				UPROPERTY()
				TSet<FVector> VectorSet;

				UPROPERTY()
				bool bStringContains = false;

				UPROPERTY()
				bool bNameContains = false;

				UPROPERTY()
				bool bEnumContains = false;

				UPROPERTY()
				bool bVectorContains = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// FString set
					StringSet.Add("Apple");
					StringSet.Add("Banana");
					StringSet.Add("Cherry");
					StringSet.Add("Date");
					bStringContains = StringSet.Contains("Banana");

					// FName set
					NameSet.Add(n"Red");
					NameSet.Add(n"Green");
					NameSet.Add(n"Blue");
					NameSet.Add(n"Yellow");
					bNameContains = NameSet.Contains(n"Green");

					// Enum set
					EnumSet.Add(ETestEnum::Alpha);
					EnumSet.Add(ETestEnum::Beta);
					EnumSet.Add(ETestEnum::Gamma);
					bEnumContains = EnumSet.Contains(ETestEnum::Beta);

					// FVector set
					VectorSet.Add(FVector(1.0, 0.0, 0.0));
					VectorSet.Add(FVector(0.0, 1.0, 0.0));
					VectorSet.Add(FVector(0.0, 0.0, 1.0));
					bVectorContains = VectorSet.Contains(FVector(0.0, 1.0, 0.0));
				}
			}
			)AS"),
			TEXT("ACoverageTSetElementTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-element-types actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-element-types actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify FString set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StringSet"), SetSize), TEXT("Should get StringSet size")));
			ASSERT_THAT(AreEqual(4, SetSize, TEXT("StringSet should have 4 elements")));
			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStringContains"), true, TEXT("StringSet should contain 'Banana'"));
		}

		// Verify FName set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("NameSet"), SetSize), TEXT("Should get NameSet size")));
			ASSERT_THAT(AreEqual(4, SetSize, TEXT("NameSet should have 4 elements")));
			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameContains"), true, TEXT("NameSet should contain 'Green'"));
		}

		// Verify Enum set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("EnumSet"), SetSize), TEXT("Should get EnumSet size")));
			ASSERT_THAT(AreEqual(3, SetSize, TEXT("EnumSet should have 3 elements")));
			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bEnumContains"), true, TEXT("EnumSet should contain Beta"));
		}

		// Verify FVector set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("VectorSet"), SetSize), TEXT("Should get VectorSet size")));
			ASSERT_THAT(AreEqual(3, SetSize, TEXT("VectorSet should have 3 elements")));
			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bVectorContains"), true, TEXT("VectorSet should contain (0,1,0)"));
		}
	}

	// -------------------------------------------------------------------------
	// TSet as parameters and return values
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_Parameter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetParameter.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetParameterActor : AActor
			{
				UPROPERTY()
				TSet<int> OriginalSet;

				UPROPERTY()
				TSet<int> ModifiedSet;

				UPROPERTY()
				TSet<int> ReturnedSet;

				UPROPERTY()
				int SumFromConstRef = 0;

				// Function taking const ref (read-only)
				int SumSetElements(const TSet<int>&in InSet)
				{
					int Sum = 0;
					for (int Value : InSet)
					{
						Sum += Value;
					}
					return Sum;
				}

				// Function taking mutable ref (can modify)
				void AddElementsToSet(TSet<int>&inout InSet, int Value1, int Value2)
				{
					InSet.Add(Value1);
					InSet.Add(Value2);
				}

				// Function returning TSet
				TSet<int> CreateEvenNumberSet(int MaxValue)
				{
					TSet<int> Result;
					for (int i = 0; i <= MaxValue; i += 2)
					{
						Result.Add(i);
					}
					return Result;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Setup original set
					OriginalSet.Add(1);
					OriginalSet.Add(2);
					OriginalSet.Add(3);

					// Test const ref parameter
					SumFromConstRef = SumSetElements(OriginalSet);

					// Test mutable ref parameter
					ModifiedSet = OriginalSet; // Copy
					AddElementsToSet(ModifiedSet, 4, 5);

					// Test return value
					ReturnedSet = CreateEvenNumberSet(10);
				}
			}
			)AS"),
			TEXT("ACoverageTSetParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-parameter actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-parameter actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify original set unchanged
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("OriginalSet"), SetSize), TEXT("Should get OriginalSet size")));
			ASSERT_THAT(AreEqual(3, SetSize, TEXT("OriginalSet should still have 3 elements")));
		}

		// Verify sum from const ref parameter
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumFromConstRef"), 6, TEXT("Sum should be 1+2+3=6"));

		// Verify modified set has new elements
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("ModifiedSet"), SetSize), TEXT("Should get ModifiedSet size")));
			ASSERT_THAT(AreEqual(5, SetSize, TEXT("ModifiedSet should have 5 elements after modification")));
		}

		// Verify returned set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("ReturnedSet"), SetSize), TEXT("Should get ReturnedSet size")));
			ASSERT_THAT(AreEqual(6, SetSize, TEXT("ReturnedSet should have 6 even numbers (0, 2, 4, 6, 8, 10)")));
		}
	}

	// -------------------------------------------------------------------------
	// TSet Array conversion and combined operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetArrayConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_ArrayConversion"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetArrayConversion.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetArrayConversionActor : AActor
			{
				UPROPERTY()
				TSet<int> UniqueSet;

				UPROPERTY()
				TArray<int> ConvertedArray;

				UPROPERTY()
				int ArraySize = 0;

				UPROPERTY()
				bool bArrayContainsAll = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create set with unique values
					UniqueSet.Add(100);
					UniqueSet.Add(200);
					UniqueSet.Add(300);
					UniqueSet.Add(400);

					// Convert to array
					ConvertedArray = UniqueSet.Array();
					ArraySize = ConvertedArray.Num();

					// Verify all elements present (order may vary)
					bArrayContainsAll =
						ConvertedArray.Contains(100) &&
						ConvertedArray.Contains(200) &&
						ConvertedArray.Contains(300) &&
						ConvertedArray.Contains(400);
				}
			}
			)AS"),
			TEXT("ACoverageTSetArrayConversionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-array-conversion actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-array-conversion actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify conversion worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArraySize"), 4, TEXT("Converted array should have 4 elements"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bArrayContainsAll"), true, TEXT("Array should contain all set elements"));
	}

	// -------------------------------------------------------------------------
	// TSet Reset and capacity operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetResetAndCapacity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_ResetCapacity"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetResetCapacity.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetResetCapacityActor : AActor
			{
				UPROPERTY()
				TSet<int> TestSet;

				UPROPERTY()
				int SizeBeforeReset = 0;

				UPROPERTY()
				int SizeAfterReset = 0;

				UPROPERTY()
				bool bReAddSuccess = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add elements
					TestSet.Add(10);
					TestSet.Add(20);
					TestSet.Add(30);
					TestSet.Add(40);
					TestSet.Add(50);

					SizeBeforeReset = TestSet.Num();

					// Test Reset (clears but may preserve capacity)
					TestSet.Reset();
					SizeAfterReset = TestSet.Num();

					// Verify we can re-add elements after reset
					TestSet.Add(100);
					TestSet.Add(200);
					bReAddSuccess = (TestSet.Num() == 2) && TestSet.Contains(100) && TestSet.Contains(200);
				}
			}
			)AS"),
			TEXT("ACoverageTSetResetCapacityActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-reset-capacity actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-reset-capacity actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify Reset() worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeBeforeReset"), 5, TEXT("Set should have 5 elements before Reset()"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeAfterReset"), 0, TEXT("Set should be empty after Reset()"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bReAddSuccess"), true, TEXT("Should be able to re-add elements after Reset()"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
