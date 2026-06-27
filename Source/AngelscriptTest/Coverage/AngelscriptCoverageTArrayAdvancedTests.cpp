#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageTArrayAdvancedTests
// -----------------------------------------------------------------------------
// Advanced coverage for TArray operations in AngelScript.
// This file covers:
//   - Sort() / Reverse() - Array sorting and reversal
//   - Insert() / RemoveAt() - Insertion and removal by index
//   - Find() - Search returning index
//   - Reserve() - Capacity pre-allocation
//   - For-each iteration - Range-based loops
//   - Different element types - FString, FVector, UObject references
//   - Nested containers - TArray<TArray<int>>
//
// Basic operations (Add, Contains, Num, indexing) are already covered in
// AngelscriptCoverageIntPropertyTests.cpp.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_Containers.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageTArrayAdvancedTest,
	"Angelscript.TestModule.Coverage.TArrayAdvanced",
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
	// TArray Sort and Reverse operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TArraySortAndReverse)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_SortReverse"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArraySortReverse.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArraySortActor : AActor
			{
				UPROPERTY()
				TArray<int> IntArray;

				UPROPERTY()
				TArray<int> ReversedArray;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test Sort
					IntArray.Add(5);
					IntArray.Add(2);
					IntArray.Add(8);
					IntArray.Add(1);
					IntArray.Add(9);
					
					IntArray.Sort();
					
					// Test Reverse
					ReversedArray.Add(1);
					ReversedArray.Add(2);
					ReversedArray.Add(3);
					ReversedArray.Add(4);
					
					ReversedArray.Reverse();
				}
			}
			)AS"),
			TEXT("ACoverageTArraySortActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-sort actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-sort actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify sorted array
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[0]"), 1, TEXT("Sorted array[0] should be 1"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[1]"), 2, TEXT("Sorted array[1] should be 2"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[2]"), 5, TEXT("Sorted array[2] should be 5"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[3]"), 8, TEXT("Sorted array[3] should be 8"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[4]"), 9, TEXT("Sorted array[4] should be 9"));

		// Verify reversed array
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReversedArray[0]"), 4, TEXT("Reversed array[0] should be 4"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReversedArray[1]"), 3, TEXT("Reversed array[1] should be 3"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReversedArray[2]"), 2, TEXT("Reversed array[2] should be 2"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReversedArray[3]"), 1, TEXT("Reversed array[3] should be 1"));
	}

	// -------------------------------------------------------------------------
	// TArray Insert and RemoveAt operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayInsertAndRemoveAt)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_InsertRemove"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayInsertRemove.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayInsertActor : AActor
			{
				UPROPERTY()
				TArray<int> Values;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Initial array: [10, 20, 30]
					Values.Add(10);
					Values.Add(20);
					Values.Add(30);
					
					// Insert 15 at index 1: [10, 15, 20, 30]
					Values.Insert(15, 1);
					
					// Insert 5 at index 0: [5, 10, 15, 20, 30]
					Values.Insert(5, 0);
					
					// Insert 35 at end (index 5): [5, 10, 15, 20, 30, 35]
					Values.Insert(35, 5);
					
					// RemoveAt index 2 (removes 15): [5, 10, 20, 30, 35]
					Values.RemoveAt(2);
					
					// RemoveAt index 0 (removes 5): [10, 20, 30, 35]
					Values.RemoveAt(0);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayInsertActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-insert actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-insert actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify final array: [10, 20, 30, 35]
		int32 NumElements = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Values"), NumElements), TEXT("Should get array length")));
		ASSERT_THAT(AreEqual(4, NumElements, TEXT("Array should have 4 elements after operations")));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[0]"), 10, TEXT("Values[0] should be 10"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[1]"), 20, TEXT("Values[1] should be 20"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[2]"), 30, TEXT("Values[2] should be 30"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[3]"), 35, TEXT("Values[3] should be 35"));
	}

	// -------------------------------------------------------------------------
	// TArray Find operation - returns index or -1
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayFind)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Find"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayFind.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayFindActor : AActor
			{
				UPROPERTY()
				int FindIndex1;

				UPROPERTY()
				int FindIndex2;

				UPROPERTY()
				int FindIndex3;

				UPROPERTY()
				int NotFoundIndex;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TArray<int> Values;
					Values.Add(100);
					Values.Add(200);
					Values.Add(300);
					Values.Add(200);  // Duplicate
					
					FindIndex1 = Values.Find(100);  // Should be 0
					FindIndex2 = Values.Find(200);  // Should be 1 (first occurrence)
					FindIndex3 = Values.Find(300);  // Should be 2
					NotFoundIndex = Values.Find(999);  // Should be -1
				}
			}
			)AS"),
			TEXT("ACoverageTArrayFindActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-find actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-find actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex1"), 0, TEXT("Find(100) should return index 0"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex2"), 1, TEXT("Find(200) should return index 1 (first occurrence)"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex3"), 2, TEXT("Find(300) should return index 2"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NotFoundIndex"), -1, TEXT("Find(999) should return -1 when not found"));
	}

	// -------------------------------------------------------------------------
	// TArray Reserve - pre-allocate capacity
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayReserve)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Reserve"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayReserve.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayReserveActor : AActor
			{
				UPROPERTY()
				TArray<int> ReservedArray;

				UPROPERTY()
				int FinalSize;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Reserve space for 100 elements
					ReservedArray.Reserve(100);
					
					// Add some elements
					for (int i = 0; i < 10; i++)
					{
						ReservedArray.Add(i * 10);
					}
					
					FinalSize = ReservedArray.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTArrayReserveActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-reserve actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-reserve actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Reserve doesn't change Num(), only capacity
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FinalSize"), 10, TEXT("Array size should be 10 after adding 10 elements"));

		// Verify some elements
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReservedArray[0]"), 0, TEXT("ReservedArray[0] should be 0"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReservedArray[5]"), 50, TEXT("ReservedArray[5] should be 50"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReservedArray[9]"), 90, TEXT("ReservedArray[9] should be 90"));
	}

	// -------------------------------------------------------------------------
	// TArray for-each iteration
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayForEachIteration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_ForEach"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayForEach.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayForEachActor : AActor
			{
				UPROPERTY()
				int SumByValue;

				UPROPERTY()
				int SumByReference;

				UPROPERTY()
				TArray<int> ModifiedArray;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TArray<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.Add(3);
					Values.Add(4);
					Values.Add(5);
					
					// For-each by value (read-only iteration)
					SumByValue = 0;
					for (int Val : Values)
					{
						SumByValue += Val;
					}
					
					// For-each by reference (can modify)
					SumByReference = 0;
					for (int& Val : Values)
					{
						Val *= 2;  // Double each value
						SumByReference += Val;
					}
					
					// Copy modified values
					ModifiedArray = Values;
				}
			}
			)AS"),
			TEXT("ACoverageTArrayForEachActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-foreach actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-foreach actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Sum by value should be 1+2+3+4+5 = 15
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumByValue"), 15, TEXT("For-each by value sum should be 15"));

		// Sum by reference should be 2+4+6+8+10 = 30
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumByReference"), 30, TEXT("For-each by reference sum should be 30"));

		// Verify modified array values
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[0]"), 2, TEXT("ModifiedArray[0] should be 2"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[1]"), 4, TEXT("ModifiedArray[1] should be 4"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[2]"), 6, TEXT("ModifiedArray[2] should be 6"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[3]"), 8, TEXT("ModifiedArray[3] should be 8"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[4]"), 10, TEXT("ModifiedArray[4] should be 10"));
	}

	// -------------------------------------------------------------------------
	// TArray with FString elements
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayFString)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_FString"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayFString.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayStringActor : AActor
			{
				UPROPERTY()
				TArray<FString> StringArray;

				UPROPERTY()
				int FindIndexHello;

				UPROPERTY()
				int FindIndexNotFound;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StringArray.Add("Hello");
					StringArray.Add("World");
					StringArray.Add("AngelScript");
					StringArray.Add("Test");
					
					// Sort strings alphabetically
					StringArray.Sort();
					
					// Find operations
					FindIndexHello = StringArray.Find("Hello");
					FindIndexNotFound = StringArray.Find("Missing");
					
					// Insert at beginning
					StringArray.Insert("AAA", 0);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayStringActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<FString> actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<FString> actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// After sorting: ["AngelScript", "Hello", "Test", "World"]
		// After inserting "AAA" at 0: ["AAA", "AngelScript", "Hello", "Test", "World"]
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[0]"), FString(TEXT("AAA")), TEXT("StringArray[0] should be 'AAA'"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[1]"), FString(TEXT("AngelScript")), TEXT("StringArray[1] should be 'AngelScript'"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[2]"), FString(TEXT("Hello")), TEXT("StringArray[2] should be 'Hello'"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[3]"), FString(TEXT("Test")), TEXT("StringArray[3] should be 'Test'"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[4]"), FString(TEXT("World")), TEXT("StringArray[4] should be 'World'"));

		// Find should return index 2 (after insert, "Hello" is at index 2)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndexHello"), 2, TEXT("Find('Hello') should return 2"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndexNotFound"), -1, TEXT("Find('Missing') should return -1"));
	}

	// -------------------------------------------------------------------------
	// TArray with FVector elements
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayFVector)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_FVector"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayFVector.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayVectorActor : AActor
			{
				UPROPERTY()
				TArray<FVector> VectorArray;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VectorArray.Add(FVector(1, 0, 0));
					VectorArray.Add(FVector(0, 1, 0));
					VectorArray.Add(FVector(0, 0, 1));
					
					// Insert at index 1
					VectorArray.Insert(FVector(0.5, 0.5, 0), 1);
					
					// Remove last element
					VectorArray.RemoveAt(VectorArray.Num() - 1);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayVectorActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<FVector> actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<FVector> actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Final array: [(1,0,0), (0.5,0.5,0), (0,1,0)]
		int32 NumElements = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("VectorArray"), NumElements), TEXT("Should get array length")));
		ASSERT_THAT(AreEqual(3, NumElements, TEXT("VectorArray should have 3 elements")));

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].X"), 1.0, TEXT("VectorArray[0].X should be 1.0"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].Y"), 0.0, TEXT("VectorArray[0].Y should be 0.0"));

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[1].X"), 0.5, TEXT("VectorArray[1].X should be 0.5"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[1].Y"), 0.5, TEXT("VectorArray[1].Y should be 0.5"));

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].X"), 0.0, TEXT("VectorArray[2].X should be 0.0"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].Y"), 1.0, TEXT("VectorArray[2].Y should be 1.0"));
	}

	// -------------------------------------------------------------------------
	// TArray with UObject references (AActor references)
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayUObjectReferences)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_UObject"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayUObject.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayActorRefsActor : AActor
			{
				UPROPERTY()
				TArray<AActor> ActorReferences;

				UPROPERTY()
				int ActorCount;

				UPROPERTY()
				bool bContainsSelf;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add self to array
					ActorReferences.Add(this);
					
					// Spawn and add some child actors
					AActor Child1 = SpawnActor(AActor::StaticClass(), FVector::ZeroVector);
					AActor Child2 = SpawnActor(AActor::StaticClass(), FVector(100, 0, 0));
					
					ActorReferences.Add(Child1);
					ActorReferences.Add(Child2);
					
					ActorCount = ActorReferences.Num();
					
					// Test Contains
					bContainsSelf = ActorReferences.Contains(this);
					
					// Test Find
					int SelfIndex = ActorReferences.Find(this);
					if (SelfIndex != 0)
					{
						ActorCount = -1;  // Signal error
					}
				}
			}
			)AS"),
			TEXT("ACoverageTArrayActorRefsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<AActor> actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<AActor> actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorCount"), 3, TEXT("ActorReferences should have 3 actors"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsSelf"), true, TEXT("ActorReferences should contain self"));
	}

	// -------------------------------------------------------------------------
	// Nested containers: TArray<TArray<int>>
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayNestedContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Nested"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayNested.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayNestedActor : AActor
			{
				UPROPERTY()
				TArray<TArray<int>> Matrix;

				UPROPERTY()
				int RowCount;

				UPROPERTY()
				int TotalElements;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create a 3x3 matrix
					for (int i = 0; i < 3; i++)
					{
						TArray<int> Row;
						for (int j = 0; j < 3; j++)
						{
							Row.Add(i * 3 + j);
						}
						Matrix.Add(Row);
					}
					
					RowCount = Matrix.Num();
					
					// Count total elements
					TotalElements = 0;
					for (const TArray<int>& Row : Matrix)
					{
						TotalElements += Row.Num();
					}
					
					// Modify nested array
					Matrix[1][1] = 999;
				}
			}
			)AS"),
			TEXT("ACoverageTArrayNestedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<TArray<int>> actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<TArray<int>> actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RowCount"), 3, TEXT("Matrix should have 3 rows"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TotalElements"), 9, TEXT("Matrix should have 9 total elements"));

		// Verify some matrix values
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[0][0]"), 0, TEXT("Matrix[0][0] should be 0"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[0][1]"), 1, TEXT("Matrix[0][1] should be 1"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[0][2]"), 2, TEXT("Matrix[0][2] should be 2"));
		
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[1][0]"), 3, TEXT("Matrix[1][0] should be 3"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[1][1]"), 999, TEXT("Matrix[1][1] should be 999 (modified)"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[1][2]"), 5, TEXT("Matrix[1][2] should be 5"));
		
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[2][0]"), 6, TEXT("Matrix[2][0] should be 6"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[2][1]"), 7, TEXT("Matrix[2][1] should be 7"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Matrix[2][2]"), 8, TEXT("Matrix[2][2] should be 8"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
