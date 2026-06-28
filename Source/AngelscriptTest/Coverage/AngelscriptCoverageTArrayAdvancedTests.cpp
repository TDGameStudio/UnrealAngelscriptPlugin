#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageTArrayAdvancedTests
// -----------------------------------------------------------------------------
// Advanced coverage for TArray operations in AngelScript.
// This file covers:
//   - Sort() - Array sorting
//   - Insert() / RemoveAt() - Insertion and removal by index
//   - FindIndex() - Search returning index
//   - Reserve() - Capacity pre-allocation
//   - For-each iteration - Range-based loops
//   - Different element types - FString, FVector, UObject references
//   - Nested containers - TArray<TArray<int>> unsupported boundary
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
	// TArray Sort operations
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
				}
			}
			)AS"),
			TEXT("ACoverageTArraySortActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-sort actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-sort actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify sorted array
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[0]"), 1, TEXT("Sorted array[0] should be 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[1]"), 2, TEXT("Sorted array[1] should be 2"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[2]"), 5, TEXT("Sorted array[2] should be 5"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[3]"), 8, TEXT("Sorted array[3] should be 8"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[4]"), 9, TEXT("Sorted array[4] should be 9"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-insert actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify final array: [10, 20, 30, 35]
		int32 NumElements = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Values"), NumElements), TEXT("Should get array length")));
		ASSERT_THAT(AreEqual(4, NumElements, TEXT("Array should have 4 elements after operations")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[0]"), 10, TEXT("Values[0] should be 10"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[1]"), 20, TEXT("Values[1] should be 20"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[2]"), 30, TEXT("Values[2] should be 30"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Values[3]"), 35, TEXT("Values[3] should be 35"))));
	}

	// -------------------------------------------------------------------------
	// TArray FindIndex operation - returns index or -1
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
					
					FindIndex1 = Values.FindIndex(100);  // Should be 0
					FindIndex2 = Values.FindIndex(200);  // Should be 1 (first occurrence)
					FindIndex3 = Values.FindIndex(300);  // Should be 2
					NotFoundIndex = Values.FindIndex(999);  // Should be -1
				}
			}
			)AS"),
			TEXT("ACoverageTArrayFindActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-find actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-find actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex1"), 0, TEXT("FindIndex(100) should return index 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex2"), 1, TEXT("FindIndex(200) should return index 1 (first occurrence)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex3"), 2, TEXT("FindIndex(300) should return index 2"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NotFoundIndex"), -1, TEXT("FindIndex(999) should return -1 when not found"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-reserve actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Reserve doesn't change Num(), only capacity
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FinalSize"), 10, TEXT("Array size should be 10 after adding 10 elements"))));

		// Verify some elements
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReservedArray[0]"), 0, TEXT("ReservedArray[0] should be 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReservedArray[5]"), 50, TEXT("ReservedArray[5] should be 50"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReservedArray[9]"), 90, TEXT("ReservedArray[9] should be 90"))));
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

				UPROPERTY()
				int ExplicitIteratorSum = 0;

				UPROPERTY()
				int ExplicitIteratorCount = 0;

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
					
					// Explicit iterator API coverage.
					TArrayIterator<int> It = Values.Iterator();
					while (It.CanProceed)
					{
						ExplicitIteratorSum += It.Proceed();
						ExplicitIteratorCount++;
					}

					// Copy modified values
					ModifiedArray = Values;
				}
			}
			)AS"),
			TEXT("ACoverageTArrayForEachActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray-foreach actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray-foreach actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Sum by value should be 1+2+3+4+5 = 15
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumByValue"), 15, TEXT("For-each by value sum should be 15"))));

		// Sum by reference should be 2+4+6+8+10 = 30
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumByReference"), 30, TEXT("For-each by reference sum should be 30"))));

		// Verify modified array values
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[0]"), 2, TEXT("ModifiedArray[0] should be 2"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[1]"), 4, TEXT("ModifiedArray[1] should be 4"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[2]"), 6, TEXT("ModifiedArray[2] should be 6"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[3]"), 8, TEXT("ModifiedArray[3] should be 8"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[4]"), 10, TEXT("ModifiedArray[4] should be 10"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExplicitIteratorSum"), 30, TEXT("explicit TArray iterator should traverse all values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExplicitIteratorCount"), 5, TEXT("explicit TArray iterator should visit all elements"))));
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
					
					// FindIndex operations
					FindIndexHello = StringArray.FindIndex("Hello");
					FindIndexNotFound = StringArray.FindIndex("Missing");
					
					// Insert at beginning
					StringArray.Insert("AAA", 0);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayStringActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<FString> actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<FString> actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// After sorting: ["AngelScript", "Hello", "Test", "World"]
		// After inserting "AAA" at 0: ["AAA", "AngelScript", "Hello", "Test", "World"]
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[0]"), FString(TEXT("AAA")), TEXT("StringArray[0] should be 'AAA'"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[1]"), FString(TEXT("AngelScript")), TEXT("StringArray[1] should be 'AngelScript'"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[2]"), FString(TEXT("Hello")), TEXT("StringArray[2] should be 'Hello'"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[3]"), FString(TEXT("Test")), TEXT("StringArray[3] should be 'Test'"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[4]"), FString(TEXT("World")), TEXT("StringArray[4] should be 'World'"))));

		// FindIndex is captured before inserting "AAA".
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndexHello"), 1, TEXT("FindIndex('Hello') should return 1 before insert"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndexNotFound"), -1, TEXT("FindIndex('Missing') should return -1"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<FVector> actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Final array: [(1,0,0), (0.5,0.5,0), (0,1,0)]
		int32 NumElements = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("VectorArray"), NumElements), TEXT("Should get array length")));
		ASSERT_THAT(AreEqual(3, NumElements, TEXT("VectorArray should have 3 elements")));

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].X"), 1.0, TEXT("VectorArray[0].X should be 1.0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].Y"), 0.0, TEXT("VectorArray[0].Y should be 0.0"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[1].X"), 0.5, TEXT("VectorArray[1].X should be 0.5"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[1].Y"), 0.5, TEXT("VectorArray[1].Y should be 0.5"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].X"), 0.0, TEXT("VectorArray[2].X should be 0.0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].Y"), 1.0, TEXT("VectorArray[2].Y should be 1.0"))));
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
					
					// Test FindIndex
					int SelfIndex = ActorReferences.FindIndex(this);
					if (SelfIndex != 0)
					{
						ActorCount = -1;  // Signal error
					}
				}
			}
			)AS"),
			TEXT("ACoverageTArrayActorRefsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<AActor> actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<AActor> actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorCount"), 3, TEXT("ActorReferences should have 3 actors"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsSelf"), true, TEXT("ActorReferences should contain self"))));
	}

	// -------------------------------------------------------------------------
	// Nested containers: TArray<TArray<int>> is rejected by this fork.
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayNestedContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Containers cannot be nested in other containers"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTArrayNestedUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayNestedActor : AActor
			{
				UPROPERTY()
				TArray<TArray<int>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<int>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTArrayDeepNestedUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayDeepNestedActor : AActor
			{
				UPROPERTY()
				TArray<TArray<TArray<int>>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<TArray<int>>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Unsupported TArray API aliases from older matrix drafts.
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayUnsupportedApiAliases)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::Find(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::FindLast(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::Reverse()'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::RemoveAll(const int)'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTArrayUnsupportedApiAliases"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayUnsupportedApiActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TArray<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.Find(1);
					Values.FindLast(1);
					Values.Reverse();
					Values.RemoveAll(1);
				}
			}
			)AS"),
			TEXT("Legacy TArray aliases Find/FindLast/Reverse/RemoveAll should remain explicit unsupported boundaries"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// TArray Append and array merging
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayAppendAndMerge)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Append"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayAppend.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayAppendActor : AActor
			{
				UPROPERTY()
				int MergedSize;

				UPROPERTY()
				TArray<int> MergedArray;

				UPROPERTY()
				int AppendEmptyResult;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray Append Test ===");

					// Test basic Append
					TArray<int> Array1;
					Array1.Add(1);
					Array1.Add(2);
					Array1.Add(3);
					Print("Array1 initial size: " + Array1.Num());
					for (int i = 0; i < Array1.Num(); i++)
						Print("  Array1[" + i + "] = " + Array1[i]);

					TArray<int> Array2;
					Array2.Add(4);
					Array2.Add(5);
					Print("Array2 size: " + Array2.Num());
					for (int i = 0; i < Array2.Num(); i++)
						Print("  Array2[" + i + "] = " + Array2[i]);

					Array1.Append(Array2);
					Print("After Append, Array1 size: " + Array1.Num());
					MergedSize = Array1.Num();

					// Copy to property for verification
					for (int i = 0; i < Array1.Num(); i++)
					{
						MergedArray.Add(Array1[i]);
						Print("  MergedArray[" + i + "] = " + Array1[i]);
					}

					// Test appending empty array
					TArray<int> EmptyArray;
					int BeforeSize = Array1.Num();
					Array1.Append(EmptyArray);
					AppendEmptyResult = (Array1.Num() == BeforeSize) ? 1 : 0;
					Print("Append empty array - size unchanged: " + AppendEmptyResult);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayAppendActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray Append actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray Append actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedSize"), 5, TEXT("Merged array should have 5 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedArray[0]"), 1, TEXT("MergedArray[0] should be 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedArray[1]"), 2, TEXT("MergedArray[1] should be 2"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedArray[2]"), 3, TEXT("MergedArray[2] should be 3"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedArray[3]"), 4, TEXT("MergedArray[3] should be 4"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedArray[4]"), 5, TEXT("MergedArray[4] should be 5"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AppendEmptyResult"), 1, TEXT("Appending empty array should not change size"))));
	}

	// -------------------------------------------------------------------------
	// TArray AddUnique and RemoveAll operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayAddUniqueAndRemoveAll)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_UniqueRemove"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayUniqueRemove.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayUniqueRemoveActor : AActor
			{
				UPROPERTY()
				int UniqueArraySize;

				UPROPERTY()
				int RemovedCount;

				UPROPERTY()
				int FinalSize;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray AddUnique and Remove Test ===");

					// Test AddUnique
					TArray<int> Numbers;
					Numbers.AddUnique(5);
					Numbers.AddUnique(10);
					Numbers.AddUnique(5);  // Duplicate, should not add
					Numbers.AddUnique(15);
					Numbers.AddUnique(10); // Duplicate, should not add

					Print("After AddUnique operations:");
					Print("  Numbers size: " + Numbers.Num());
					for (int i = 0; i < Numbers.Num(); i++)
						Print("  Numbers[" + i + "] = " + Numbers[i]);

					UniqueArraySize = Numbers.Num();

					// Test Remove - remove all instances of a value
					TArray<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.Add(3);
					Values.Add(2);
					Values.Add(4);
					Values.Add(2);
					Values.Add(5);

					Print("Before Remove(2):");
					Print("  Values size: " + Values.Num());
					for (int i = 0; i < Values.Num(); i++)
						Print("  Values[" + i + "] = " + Values[i]);

					int Removed = Values.Remove(2);

					Print("After Remove(2):");
					Print("  Removed count: " + Removed);
					Print("  Values size: " + Values.Num());
					for (int i = 0; i < Values.Num(); i++)
						Print("  Values[" + i + "] = " + Values[i]);

					RemovedCount = Removed;
					FinalSize = Values.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTArrayUniqueRemoveActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray AddUnique/Remove actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray AddUnique/Remove actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("UniqueArraySize"), 3, TEXT("AddUnique should result in 3 unique elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RemovedCount"), 3, TEXT("Remove should remove 3 instances of value 2"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FinalSize"), 4, TEXT("Array should have 4 elements after Remove"))));
	}

	// -------------------------------------------------------------------------
	// TArray SetNum and capacity management
	// -------------------------------------------------------------------------
	TEST_METHOD(TArraySetNumAndCapacity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_SetNum"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArraySetNum.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArraySetNumActor : AActor
			{
				UPROPERTY()
				int ExpandedSize;

				UPROPERTY()
				int ShrunkSize;

				UPROPERTY()
				int LastElement;

				UPROPERTY()
				int EmptyResetSize;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray SetNum and Capacity Test ===");

					// Test SetNum to expand array
					TArray<int> Numbers;
					Numbers.Add(1);
					Numbers.Add(2);
					Numbers.Add(3);
					Print("Initial size: " + Numbers.Num());

					Numbers.SetNum(10);
					Print("After SetNum(10): " + Numbers.Num());
					ExpandedSize = Numbers.Num();

					// Verify existing elements remain
					Print("  Numbers[0] = " + Numbers[0]);
					Print("  Numbers[1] = " + Numbers[1]);
					Print("  Numbers[2] = " + Numbers[2]);

					// New elements are default-initialized (0 for int)
					Print("  Numbers[9] = " + Numbers[9]);
					LastElement = Numbers[9];

					// Test SetNum to shrink array
					Numbers.SetNum(2);
					Print("After SetNum(2): " + Numbers.Num());
					ShrunkSize = Numbers.Num();

					// Test Empty and Reset
					TArray<int> TestArray;
					for (int i = 0; i < 100; i++)
						TestArray.Add(i);

					Print("TestArray size before Empty: " + TestArray.Num());
					TestArray.Empty();
					Print("TestArray size after Empty: " + TestArray.Num());

					TestArray.Add(1);
					TestArray.Reset();
					EmptyResetSize = TestArray.Num();
					Print("TestArray size after Reset: " + EmptyResetSize);
				}
			}
			)AS"),
			TEXT("ACoverageTArraySetNumActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray SetNum actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray SetNum actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExpandedSize"), 10, TEXT("SetNum(10) should expand array to 10 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ShrunkSize"), 2, TEXT("SetNum(2) should shrink array to 2 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastElement"), 0, TEXT("New elements should be default-initialized to 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EmptyResetSize"), 0, TEXT("Reset should clear array"))));
	}

	// -------------------------------------------------------------------------
	// TArray Swap elements
	// -------------------------------------------------------------------------
	TEST_METHOD(TArraySwapElements)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Swap"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArraySwap.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArraySwapActor : AActor
			{
				UPROPERTY()
				TArray<int> SwappedArray;

				UPROPERTY()
				TArray<FString> StringSwapped;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray Swap Test ===");

					// Test Swap with int array
					TArray<int> Numbers;
					Numbers.Add(10);
					Numbers.Add(20);
					Numbers.Add(30);
					Numbers.Add(40);
					Numbers.Add(50);

					Print("Before Swap:");
					for (int i = 0; i < Numbers.Num(); i++)
						Print("  Numbers[" + i + "] = " + Numbers[i]);

					// Swap first and last elements
					Numbers.Swap(0, 4);
					Print("After Swap(0, 4):");
					for (int i = 0; i < Numbers.Num(); i++)
					{
						Print("  Numbers[" + i + "] = " + Numbers[i]);
						SwappedArray.Add(Numbers[i]);
					}

					// Test Swap with FString array
					TArray<FString> Words;
					Words.Add("First");
					Words.Add("Second");
					Words.Add("Third");

					Print("Before String Swap:");
					for (int i = 0; i < Words.Num(); i++)
						Print("  Words[" + i + "] = " + Words[i]);

					Words.Swap(0, 2);
					Print("After String Swap(0, 2):");
					for (int i = 0; i < Words.Num(); i++)
					{
						Print("  Words[" + i + "] = " + Words[i]);
						StringSwapped.Add(Words[i]);
					}
				}
			}
			)AS"),
			TEXT("ACoverageTArraySwapActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray Swap actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray Swap actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		// Verify swapped int array
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SwappedArray[0]"), 50, TEXT("SwappedArray[0] should be 50"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SwappedArray[1]"), 20, TEXT("SwappedArray[1] should be 20"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SwappedArray[2]"), 30, TEXT("SwappedArray[2] should be 30"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SwappedArray[3]"), 40, TEXT("SwappedArray[3] should be 40"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SwappedArray[4]"), 10, TEXT("SwappedArray[4] should be 10"))));

		// Verify swapped string array
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringSwapped[0]"), TEXT("Third"), TEXT("StringSwapped[0] should be 'Third'"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringSwapped[1]"), TEXT("Second"), TEXT("StringSwapped[1] should be 'Second'"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringSwapped[2]"), TEXT("First"), TEXT("StringSwapped[2] should be 'First'"))));
	}

	// -------------------------------------------------------------------------
	// TArray advanced search operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayAdvancedSearch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Search"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArraySearch.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArraySearchActor : AActor
			{
				UPROPERTY()
				int FindLastResult;

				UPROPERTY()
				int ContainsTrue;

				UPROPERTY()
				int ContainsFalse;

				UPROPERTY()
				int IsValidTrue;

				UPROPERTY()
				int IsValidFalse;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray Advanced Search Test ===");

					// Test FindLast - find last occurrence
					TArray<int> Numbers;
					Numbers.Add(5);
					Numbers.Add(10);
					Numbers.Add(5);
					Numbers.Add(15);
					Numbers.Add(5);
					Numbers.Add(20);

					Print("Array contents:");
					for (int i = 0; i < Numbers.Num(); i++)
						Print("  Numbers[" + i + "] = " + Numbers[i]);

					int LastIndex = -1;
					for (int i = 0; i < Numbers.Num(); i++)
					{
						if (Numbers[i] == 5)
							LastIndex = i;
					}
					Print("manual last index for 5 returned: " + LastIndex);
					FindLastResult = LastIndex;

					// Test Contains
					bool HasFive = Numbers.Contains(5);
					bool HasHundred = Numbers.Contains(100);
					Print("Contains(5): " + HasFive);
					Print("Contains(100): " + HasHundred);
					ContainsTrue = HasFive ? 1 : 0;
					ContainsFalse = HasHundred ? 1 : 0;

					// Test IsValidIndex
					bool Valid5 = Numbers.IsValidIndex(5);
					bool Valid10 = Numbers.IsValidIndex(10);
					Print("IsValidIndex(5): " + Valid5);
					Print("IsValidIndex(10): " + Valid10);
					IsValidTrue = Valid5 ? 1 : 0;
					IsValidFalse = Valid10 ? 1 : 0;
				}
			}
			)AS"),
			TEXT("ACoverageTArraySearchActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray Search actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray Search actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindLastResult"), 4, TEXT("Manual last-index search should return index 4"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ContainsTrue"), 1, TEXT("Contains(5) should return true"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ContainsFalse"), 0, TEXT("Contains(100) should return false"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IsValidTrue"), 1, TEXT("IsValidIndex(5) should return true"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IsValidFalse"), 0, TEXT("IsValidIndex(10) should return false"))));
	}

	// -------------------------------------------------------------------------
	// TArray with FName elements
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayWithFName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_FName"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayFName.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayFNameActor : AActor
			{
				UPROPERTY()
				TArray<FName> Names;

				UPROPERTY()
				int FindIndex;

				UPROPERTY()
				int SortedCount;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray<FName> Test ===");

					// Add FNames
					Names.Add(n"Player");
					Names.Add(n"Enemy");
					Names.Add(n"Weapon");
					Names.Add(n"Item");
					Names.Add(n"Enemy");  // Duplicate

					Print("FName array contents:");
					for (int i = 0; i < Names.Num(); i++)
						Print("  Names[" + i + "] = " + Names[i]);

					// Test FindIndex
					int EnemyIndex = Names.FindIndex(n"Enemy");
					Print("FindIndex(n\"Enemy\") returned: " + EnemyIndex);
					FindIndex = EnemyIndex;

					// Test Sort
					Names.Sort();
					Print("After Sort:");
					for (int i = 0; i < Names.Num(); i++)
						Print("  Names[" + i + "] = " + Names[i]);

					SortedCount = Names.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTArrayFNameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray<FName> actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray<FName> actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindIndex"), 1, TEXT("FindIndex(n\"Enemy\") should return index 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SortedCount"), 5, TEXT("Sorted array should still have 5 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("Names[0]"), TEXT("Enemy"), TEXT("First sorted name"))));
	}

	// -------------------------------------------------------------------------
	// TArray edge cases - empty arrays
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayEdgeCasesEmpty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_EdgeCases"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayEdgeCases.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayEdgeCasesActor : AActor
			{
				UPROPERTY()
				int EmptyArraySize;

				UPROPERTY()
				int FindInEmpty;

				UPROPERTY()
				int ContainsInEmpty;

				UPROPERTY()
				int SingleElementSize;

				UPROPERTY()
				int SingleElementValue;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray Edge Cases Test ===");

					// Test operations on empty array
					TArray<int> EmptyArray;
					Print("Empty array size: " + EmptyArray.Num());
					EmptyArraySize = EmptyArray.Num();

					int FindResult = EmptyArray.FindIndex(5);
					Print("FindIndex(5) in empty array: " + FindResult);
					FindInEmpty = FindResult;

					bool ContainsResult = EmptyArray.Contains(5);
					Print("Contains(5) in empty array: " + ContainsResult);
					ContainsInEmpty = ContainsResult ? 1 : 0;

					// Sort empty array (should not crash)
					EmptyArray.Sort();
					Print("Sort on empty array succeeded");

					// Test single element array
					TArray<int> SingleArray;
					SingleArray.Add(42);
					Print("Single element array size: " + SingleArray.Num());
					SingleElementSize = SingleArray.Num();

					// Sort single element
					SingleArray.Sort();
					SingleElementValue = SingleArray[0];
					Print("Single element after sort: " + SingleElementValue);

				}
			}
			)AS"),
			TEXT("ACoverageTArrayEdgeCasesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray edge cases actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray edge cases actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EmptyArraySize"), 0, TEXT("Empty array should have size 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FindInEmpty"), -1, TEXT("FindIndex in empty array should return -1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ContainsInEmpty"), 0, TEXT("Contains in empty array should return false"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SingleElementSize"), 1, TEXT("Single element array should have size 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SingleElementValue"), 42, TEXT("Single element value should be preserved"))));
	}

	// -------------------------------------------------------------------------
	// TArray bulk operations and performance patterns
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayBulkOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Bulk"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayBulk.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayBulkActor : AActor
			{
				UPROPERTY()
				int BulkAddedSize;

				UPROPERTY()
				int ReservedCapacity;

				UPROPERTY()
				int MergedSize;

				UPROPERTY()
				int DuplicateRemoved;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray Bulk Operations Test ===");

					// Test bulk adding with Reserve for performance
					TArray<int> LargeArray;
					LargeArray.Reserve(100);
					Print("Reserved capacity for 100 elements");

					for (int i = 0; i < 100; i++)
					{
						LargeArray.Add(i);
					}
					Print("Added 100 elements, size: " + LargeArray.Num());
					BulkAddedSize = LargeArray.Num();

					// Test appending multiple arrays
					TArray<int> Array1;
					for (int i = 0; i < 50; i++)
						Array1.Add(i);

					TArray<int> Array2;
					for (int i = 50; i < 100; i++)
						Array2.Add(i);

					TArray<int> Array3;
					for (int i = 100; i < 150; i++)
						Array3.Add(i);

					Print("Array1 size: " + Array1.Num());
					Print("Array2 size: " + Array2.Num());
					Print("Array3 size: " + Array3.Num());

					// Merge all arrays
					TArray<int> Merged;
					Merged.Append(Array1);
					Merged.Append(Array2);
					Merged.Append(Array3);
					Print("Merged array size: " + Merged.Num());
					MergedSize = Merged.Num();

					// Test removing duplicates pattern
					TArray<int> WithDuplicates;
					WithDuplicates.Add(1);
					WithDuplicates.Add(2);
					WithDuplicates.Add(3);
					WithDuplicates.Add(2);
					WithDuplicates.Add(4);
					WithDuplicates.Add(3);
					WithDuplicates.Add(5);
					WithDuplicates.Add(1);

					Print("Array with duplicates size: " + WithDuplicates.Num());

					// Remove duplicates by using AddUnique to a new array
					TArray<int> NoDuplicates;
					for (int i = 0; i < WithDuplicates.Num(); i++)
					{
						NoDuplicates.AddUnique(WithDuplicates[i]);
					}

					Print("Array without duplicates size: " + NoDuplicates.Num());
					for (int i = 0; i < NoDuplicates.Num(); i++)
						Print("  NoDuplicates[" + i + "] = " + NoDuplicates[i]);

					DuplicateRemoved = NoDuplicates.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTArrayBulkActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray bulk operations actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray bulk operations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BulkAddedSize"), 100, TEXT("Bulk add should result in 100 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MergedSize"), 150, TEXT("Merged arrays should have 150 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DuplicateRemoved"), 5, TEXT("Array without duplicates should have 5 unique elements"))));
	}

	// -------------------------------------------------------------------------
	// TArray with duplicate elements handling
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayDuplicateHandling)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_Duplicates"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayDuplicates.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayDuplicatesActor : AActor
			{
				UPROPERTY()
				int FirstOccurrence;

				UPROPERTY()
				int LastOccurrence;

				UPROPERTY()
				int TotalOccurrences;

				UPROPERTY()
				int AfterRemoveSize;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TArray Duplicate Handling Test ===");

					TArray<int> Numbers;
					Numbers.Add(5);
					Numbers.Add(10);
					Numbers.Add(5);
					Numbers.Add(15);
					Numbers.Add(5);
					Numbers.Add(20);
					Numbers.Add(5);

					Print("Array with duplicates:");
					for (int i = 0; i < Numbers.Num(); i++)
						Print("  Numbers[" + i + "] = " + Numbers[i]);

					// Find first occurrence
					int First = Numbers.FindIndex(5);
					Print("First occurrence of 5: " + First);
					FirstOccurrence = First;

					// Find last occurrence manually.
					int Last = -1;
					for (int i = 0; i < Numbers.Num(); i++)
					{
						if (Numbers[i] == 5)
							Last = i;
					}
					Print("Last occurrence of 5: " + Last);
					LastOccurrence = Last;

					// Count occurrences manually
					int Count = 0;
					for (int i = 0; i < Numbers.Num(); i++)
					{
						if (Numbers[i] == 5)
							Count++;
					}
					Print("Total occurrences of 5: " + Count);
					TotalOccurrences = Count;

					// Remove all occurrences
					int Removed = Numbers.Remove(5);
					Print("Removed " + Removed + " occurrences");
					Print("Array size after Remove: " + Numbers.Num());
					AfterRemoveSize = Numbers.Num();

					Print("Array after removing duplicates:");
					for (int i = 0; i < Numbers.Num(); i++)
						Print("  Numbers[" + i + "] = " + Numbers[i]);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayDuplicatesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray duplicate handling actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TArray duplicate handling actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstOccurrence"), 0, TEXT("First occurrence should be at index 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastOccurrence"), 6, TEXT("Last occurrence should be at index 6"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TotalOccurrences"), 4, TEXT("Should find 4 occurrences"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AfterRemoveSize"), 3, TEXT("Array should have 3 elements after Remove"))));
	}

	// -------------------------------------------------------------------------
	// TArray UPROPERTY specifiers and metadata
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayPropertySpecifiersAndMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTArray_PropertySpecifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTArrayPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayPropertySpecifierActor : AActor
			{
				UPROPERTY(BlueprintReadWrite)
				TArray<int> BlueprintValues;

				UPROPERTY(meta = (ClampMin = "0", ClampMax = "10"))
				TArray<int> ClampedValues;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BlueprintValues.Add(1);
					BlueprintValues.Add(2);
					ClampedValues.Add(3);
				}
			}
			)AS"),
			TEXT("ACoverageTArrayPropertySpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TArray property specifier actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FArrayProperty* BlueprintValues = FindFProperty<FArrayProperty>(ScriptClass, TEXT("BlueprintValues"));
		ASSERT_THAT(IsNotNull(BlueprintValues, TEXT("BlueprintValues should be an array property")));
		if (BlueprintValues == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BlueprintValues->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite array should be BlueprintVisible")));
		ASSERT_THAT(IsFalse(BlueprintValues->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadWrite array should not be BlueprintReadOnly")));

		const FArrayProperty* ClampedValues = FindFProperty<FArrayProperty>(ScriptClass, TEXT("ClampedValues"));
		ASSERT_THAT(IsNotNull(ClampedValues, TEXT("ClampedValues should be an array property")));
		if (ClampedValues == nullptr)
		{
			return;
		}
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(
			FString(TEXT("0")),
			ClampedValues->GetMetaData(TEXT("ClampMin")),
			TEXT("TArray ClampMin meta should round-trip")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("10")),
			ClampedValues->GetMetaData(TEXT("ClampMax")),
			TEXT("TArray ClampMax meta should round-trip")));
#endif
	}

	// -------------------------------------------------------------------------
	// Unsupported TArray algorithms from UE API surface
	// -------------------------------------------------------------------------
	TEST_METHOD(TArrayUnsupportedAlgorithms)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::StableSort()'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::FilterByPredicate(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::FindByKey(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::FindByPredicate(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::Heapify()'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::HeapPop()'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::HeapPush(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::LowerBound(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TArray::UpperBound(const int)'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTArrayUnsupportedAlgorithms"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTArrayUnsupportedAlgorithmsActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TArray<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.StableSort();
					Values.FilterByPredicate(1);
					Values.FindByKey(1);
					Values.FindByPredicate(1);
					Values.Heapify();
					Values.HeapPop();
					Values.HeapPush(3);
					Values.LowerBound(1);
					Values.UpperBound(2);
				}
			}
			)AS"),
			TEXT("Unbound TArray algorithms should remain explicit unsupported boundaries"),
			MakeArrayView(ExpectedDiagnostics))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
