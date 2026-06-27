#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageContainerNestedTests
// -----------------------------------------------------------------------------
// Coverage for nested container types in AngelScript.
// This file covers:
//   - TArray<TArray<int>>            - Nested arrays (2D matrix)
//   - TArray<TMap<int, FString>>     - Array of Maps
//   - TMap<int, TArray<int>>         - Map of Arrays (one-to-many)
//   - TArray<TSet<int>>              - Array of Sets
//   - TMap<int, TMap<FString, float>> - Map of Maps (2D mapping)
//
// These nested containers are essential for:
//   - Multi-dimensional data structures
//   - Complex data relationships
//   - Grouped/hierarchical data storage
//
// Detailed coverage matrix: Documents/Coverage/Coverage_Containers.md (Section 4)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageContainerNestedTest,
	"Angelscript.TestModule.Coverage.ContainerNested",
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
	// TArray<TArray<int>> - Nested arrays (2D matrix)
	// -------------------------------------------------------------------------
	TEST_METHOD(NestedArrays_TwoDimensionalMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNestedArray"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNestedArray.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNestedArrayActor : AActor
			{
				UPROPERTY()
				TArray<TArray<int>> Matrix;

				UPROPERTY()
				int SumRow0 = 0;

				UPROPERTY()
				int SumRow1 = 0;

				UPROPERTY()
				int ElementAt_1_2 = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create a 3x3 matrix
					// Row 0: [1, 2, 3]
					TArray<int> Row0;
					Row0.Add(1);
					Row0.Add(2);
					Row0.Add(3);
					Matrix.Add(Row0);

					// Row 1: [4, 5, 6]
					TArray<int> Row1;
					Row1.Add(4);
					Row1.Add(5);
					Row1.Add(6);
					Matrix.Add(Row1);

					// Row 2: [7, 8, 9]
					TArray<int> Row2;
					Row2.Add(7);
					Row2.Add(8);
					Row2.Add(9);
					Matrix.Add(Row2);

					// Access elements
					SumRow0 = Matrix[0][0] + Matrix[0][1] + Matrix[0][2];  // 1+2+3 = 6
					SumRow1 = Matrix[1][0] + Matrix[1][1] + Matrix[1][2];  // 4+5+6 = 15
					ElementAt_1_2 = Matrix[1][2];  // 6

					// Modify an element
					Matrix[2][1] = 99;  // Change 8 to 99
				}
			}
			)AS"),
			TEXT("ACoverageNestedArrayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Nested array class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Nested array actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify matrix dimensions
		int32 OuterNum = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Matrix"), OuterNum), TEXT("Should get outer array length")));
		ASSERT_THAT(AreEqual(3, OuterNum, TEXT("Matrix should have 3 rows")));

		// Verify row 0 elements
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[0][0]"), 1, TEXT("Matrix[0][0] should be 1"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[0][1]"), 2, TEXT("Matrix[0][1] should be 2"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[0][2]"), 3, TEXT("Matrix[0][2] should be 3"));

		// Verify sums
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumRow0"), 6, TEXT("Sum of row 0 should be 6"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumRow1"), 15, TEXT("Sum of row 1 should be 15"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ElementAt_1_2"), 6, TEXT("Element[1][2] should be 6"));

		// Verify modified element
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[2][1]"), 99, TEXT("Modified element should be 99"));
	}

	// -------------------------------------------------------------------------
	// TArray<TMap<int, FString>> - Array of Maps
	// -------------------------------------------------------------------------
	TEST_METHOD(ArrayOfMaps)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageArrayOfMaps"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageArrayOfMaps.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageArrayOfMapsActor : AActor
			{
				UPROPERTY()
				TArray<TMap<int, FString>> Dictionaries;

				UPROPERTY()
				FString ValueFromMap0 = "";

				UPROPERTY()
				FString ValueFromMap1 = "";

				UPROPERTY()
				int TotalKeys = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create first map: {1: "Apple", 2: "Banana"}
					TMap<int, FString> Map0;
					Map0.Add(1, "Apple");
					Map0.Add(2, "Banana");
					Dictionaries.Add(Map0);

					// Create second map: {10: "Cat", 20: "Dog"}
					TMap<int, FString> Map1;
					Map1.Add(10, "Cat");
					Map1.Add(20, "Dog");
					Map1.Add(30, "Elephant");
					Dictionaries.Add(Map1);

					// Access elements
					ValueFromMap0 = Dictionaries[0][1];  // "Apple"
					ValueFromMap1 = Dictionaries[1][20]; // "Dog"

					// Count total keys
					TotalKeys = Dictionaries[0].Num() + Dictionaries[1].Num();  // 2 + 3 = 5

					// Modify a value
					Dictionaries[0][2] = "Orange";  // Change "Banana" to "Orange"
				}
			}
			)AS"),
			TEXT("ACoverageArrayOfMapsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Array of Maps class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Array of Maps actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify array size
		int32 NumMaps = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Dictionaries"), NumMaps), TEXT("Should get array length")));
		ASSERT_THAT(AreEqual(2, NumMaps, TEXT("Should have 2 maps")));

		// Verify accessed values
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ValueFromMap0"), TEXT("Apple"), TEXT("Value from Map0 should be Apple"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ValueFromMap1"), TEXT("Dog"), TEXT("Value from Map1 should be Dog"));

		// Verify total keys
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TotalKeys"), 5, TEXT("Total keys should be 5"));

		// Verify modified value
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Dictionaries[0][2]"), TEXT("Orange"), TEXT("Modified value should be Orange"));

		// Verify unmodified value
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Dictionaries[1][10]"), TEXT("Cat"), TEXT("Unmodified value should be Cat"));
	}

	// -------------------------------------------------------------------------
	// TMap<int, TArray<int>> - Map of Arrays (one-to-many mapping)
	// -------------------------------------------------------------------------
	TEST_METHOD(MapOfArrays_OneToMany)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMapOfArrays"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMapOfArrays.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMapOfArraysActor : AActor
			{
				UPROPERTY()
				TMap<int, TArray<int>> GroupedData;

				UPROPERTY()
				int Group1Size = 0;

				UPROPERTY()
				int Group2Size = 0;

				UPROPERTY()
				int FirstElementOfGroup1 = 0;

				UPROPERTY()
				int SumOfGroup2 = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Group 1: Key=1, Values=[10, 20, 30]
					TArray<int> Group1;
					Group1.Add(10);
					Group1.Add(20);
					Group1.Add(30);
					GroupedData.Add(1, Group1);

					// Group 2: Key=2, Values=[100, 200, 300, 400]
					TArray<int> Group2;
					Group2.Add(100);
					Group2.Add(200);
					Group2.Add(300);
					Group2.Add(400);
					GroupedData.Add(2, Group2);

					// Access and compute
					Group1Size = GroupedData[1].Num();
					Group2Size = GroupedData[2].Num();
					FirstElementOfGroup1 = GroupedData[1][0];

					// Sum all elements in Group 2
					for (int Val : GroupedData[2])
					{
						SumOfGroup2 += Val;
					}

					// Add new element to Group 1
					GroupedData[1].Add(40);

					// Create new group with empty array
					TArray<int> Group3;
					GroupedData.Add(3, Group3);
					GroupedData[3].Add(999);
				}
			}
			)AS"),
			TEXT("ACoverageMapOfArraysActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Map of Arrays class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Map of Arrays actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify group sizes (before modification)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Group1Size"), 3, TEXT("Group1 initial size should be 3"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Group2Size"), 4, TEXT("Group2 size should be 4"));

		// Verify first element
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstElementOfGroup1"), 10, TEXT("First element of Group1 should be 10"));

		// Verify sum
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumOfGroup2"), 1000, TEXT("Sum of Group2 should be 1000"));

		// Verify Group1 after adding new element (now has 4 elements)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("GroupedData[1][3]"), 40, TEXT("New element in Group1 should be 40"));

		// Verify new Group3
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("GroupedData[3][0]"), 999, TEXT("Group3 first element should be 999"));
	}

	// -------------------------------------------------------------------------
	// TArray<TSet<int>> - Array of Sets
	// -------------------------------------------------------------------------
	TEST_METHOD(ArrayOfSets)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageArrayOfSets"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageArrayOfSets.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageArrayOfSetsActor : AActor
			{
				UPROPERTY()
				TArray<TSet<int>> SetCollection;

				UPROPERTY()
				int Set0Size = 0;

				UPROPERTY()
				int Set1Size = 0;

				UPROPERTY()
				bool bSet0Contains5 = false;

				UPROPERTY()
				bool bSet1Contains100 = false;

				UPROPERTY()
				int TotalUniqueElements = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create first set: {1, 2, 3}
					TSet<int> Set0;
					Set0.Add(1);
					Set0.Add(2);
					Set0.Add(3);
					Set0.Add(2);  // Duplicate, should be ignored
					SetCollection.Add(Set0);

					// Create second set: {10, 20, 30, 40}
					TSet<int> Set1;
					Set1.Add(10);
					Set1.Add(20);
					Set1.Add(30);
					Set1.Add(40);
					SetCollection.Add(Set1);

					// Query sizes
					Set0Size = SetCollection[0].Num();
					Set1Size = SetCollection[1].Num();

					// Check membership
					bSet0Contains5 = SetCollection[0].Contains(5);
					bSet1Contains100 = SetCollection[1].Contains(100);

					// Add new element to Set0
					SetCollection[0].Add(5);

					// Count total unique elements
					TotalUniqueElements = SetCollection[0].Num() + SetCollection[1].Num();

					// Remove element from Set1
					SetCollection[1].Remove(10);
				}
			}
			)AS"),
			TEXT("ACoverageArrayOfSetsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Array of Sets class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Array of Sets actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify initial sizes (duplicates should be ignored)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Set0Size"), 3, TEXT("Set0 should have 3 unique elements"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Set1Size"), 4, TEXT("Set1 should have 4 elements"));

		// Verify membership checks (before modification)
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSet0Contains5"), false, TEXT("Set0 should not contain 5 initially"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSet1Contains100"), false, TEXT("Set1 should not contain 100"));

		// Verify after adding 5 to Set0 and removing 10 from Set1
		// Total = 4 (Set0: 1,2,3,5) + 3 (Set1: 20,30,40) = 7
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TotalUniqueElements"), 7, TEXT("Total unique elements should be 7"));
	}

	// -------------------------------------------------------------------------
	// TMap<int, TMap<FString, float>> - Map of Maps (2D mapping)
	// -------------------------------------------------------------------------
	TEST_METHOD(MapOfMaps_TwoDimensionalMapping)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMapOfMaps"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMapOfMaps.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMapOfMapsActor : AActor
			{
				UPROPERTY()
				TMap<int, TMap<FString, float>> NestedMap;

				UPROPERTY()
				float PlayerScore = 0.0f;

				UPROPERTY()
				float EnemyHealth = 0.0f;

				UPROPERTY()
				int Player1StatsCount = 0;

				UPROPERTY()
				bool bContainsPlayer2 = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Player 1 stats: {PlayerID=1: {"Score": 100.0, "Health": 75.5}}
					TMap<FString, float> Player1Stats;
					Player1Stats.Add("Score", 100.0f);
					Player1Stats.Add("Health", 75.5f);
					NestedMap.Add(1, Player1Stats);

					// Player 2 stats: {PlayerID=2: {"Score": 200.0, "Health": 50.0, "Armor": 30.0}}
					TMap<FString, float> Player2Stats;
					Player2Stats.Add("Score", 200.0f);
					Player2Stats.Add("Health", 50.0f);
					Player2Stats.Add("Armor", 30.0f);
					NestedMap.Add(2, Player2Stats);

					// Access nested values
					PlayerScore = NestedMap[1]["Score"];     // 100.0
					EnemyHealth = NestedMap[2]["Health"];    // 50.0

					// Query structure
					Player1StatsCount = NestedMap[1].Num();  // 2
					bContainsPlayer2 = NestedMap.Contains(2);

					// Modify nested value
					NestedMap[1]["Score"] = 150.0f;

					// Add new stat to existing player
					NestedMap[1].Add("Mana", 100.0f);

					// Create new player
					TMap<FString, float> Player3Stats;
					Player3Stats.Add("Score", 300.0f);
					NestedMap.Add(3, Player3Stats);
				}
			}
			)AS"),
			TEXT("ACoverageMapOfMapsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Map of Maps class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Map of Maps actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify accessed values
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("PlayerScore"), 100.0f, TEXT("Player score should be 100.0"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("EnemyHealth"), 50.0f, TEXT("Enemy health should be 50.0"));

		// Verify stats count
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Player1StatsCount"), 2, TEXT("Player1 should have 2 stats initially"));

		// Verify contains check
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsPlayer2"), true, TEXT("Should contain Player2"));

		// Verify modified score
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NestedMap[1][\"Score\"]"), 150.0f, TEXT("Modified score should be 150.0"));

		// Verify new stat added to Player1
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NestedMap[1][\"Mana\"]"), 100.0f, TEXT("Mana stat should be 100.0"));

		// Verify Player2 unchanged
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NestedMap[2][\"Armor\"]"), 30.0f, TEXT("Player2 Armor should be 30.0"));

		// Verify Player3 created
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NestedMap[3][\"Score\"]"), 300.0f, TEXT("Player3 score should be 300.0"));
	}

	// -------------------------------------------------------------------------
	// Complex nested iteration and traversal
	// -------------------------------------------------------------------------
	TEST_METHOD(NestedContainerIteration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNestedIteration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNestedIteration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNestedIterationActor : AActor
			{
				UPROPERTY()
				TArray<TArray<int>> Matrix;

				UPROPERTY()
				int TotalSum = 0;

				UPROPERTY()
				int MaxValue = 0;

				UPROPERTY()
				TMap<int, TArray<int>> Groups;

				UPROPERTY()
				int AllGroupsSum = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create 2x3 matrix
					TArray<int> Row0;
					Row0.Add(1);
					Row0.Add(2);
					Row0.Add(3);
					Matrix.Add(Row0);

					TArray<int> Row1;
					Row1.Add(4);
					Row1.Add(5);
					Row1.Add(6);
					Matrix.Add(Row1);

					// Iterate and sum all elements
					for (TArray<int> Row : Matrix)
					{
						for (int Val : Row)
						{
							TotalSum += Val;
							if (Val > MaxValue)
								MaxValue = Val;
						}
					}

					// Create groups
					TArray<int> Group1;
					Group1.Add(10);
					Group1.Add(20);
					Groups.Add(1, Group1);

					TArray<int> Group2;
					Group2.Add(30);
					Group2.Add(40);
					Group2.Add(50);
					Groups.Add(2, Group2);

					// Iterate over map of arrays
					for (auto& Pair : Groups)
					{
						for (int Val : Pair.Value)
						{
							AllGroupsSum += Val;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageNestedIterationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Nested iteration class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Nested iteration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify matrix iteration results
		// TotalSum = 1+2+3+4+5+6 = 21
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TotalSum"), 21, TEXT("Total sum should be 21"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MaxValue"), 6, TEXT("Max value should be 6"));

		// Verify map of arrays iteration
		// AllGroupsSum = 10+20+30+40+50 = 150
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AllGroupsSum"), 150, TEXT("All groups sum should be 150"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
