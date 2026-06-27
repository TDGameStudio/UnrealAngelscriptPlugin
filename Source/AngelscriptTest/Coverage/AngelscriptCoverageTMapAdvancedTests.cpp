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
// AngelscriptCoverageTMapAdvancedTests
// -----------------------------------------------------------------------------
// Advanced TMap container operations coverage, extending the basic int tests
// from AngelscriptCoverageIntPropertyTests.cpp.
//
// Matrix coverage (from Documents/Coverage/Coverage_Containers.md):
//   * TMapAdvancedOperations - Find(), Remove(), GetKeys(), index access
//   * TMapIteration          - for-each over key-value pairs
//   * TMapKeyTypes           - FString, FName, enum keys
//   * TMapValueTypes         - FVector, TArray<int> values
//   * TMapAdvancedLookup     - FindOrAdd()
//
// Basic operations (Add, Contains, Num) are already covered in the int tests.
// This file focuses on the remaining high-priority TMap operations.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageTMapAdvancedTest,
	"Angelscript.TestModule.Coverage.TMapAdvanced",
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
	// TMap advanced operations: Find(), Remove(), GetKeys(), index access
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapAdvancedOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_Advanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapAdvanced.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapAdvancedActor : AActor
			{
				UPROPERTY()
				TMap<int, FString> TestMap;

				UPROPERTY()
				bool bFoundKey = false;

				UPROPERTY()
				bool bRemovedKey = false;

				UPROPERTY()
				TArray<int> Keys;

				UPROPERTY()
				FString IndexAccessValue;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Setup test data
					TestMap.Add(1, "One");
					TestMap.Add(2, "Two");
					TestMap.Add(3, "Three");
					TestMap.Add(4, "Four");

					// Test Find() - returns pointer
					FString* FoundPtr = TestMap.Find(2);
					bFoundKey = (FoundPtr != nullptr);

					// Test Remove()
					int RemoveCount = TestMap.Remove(3);
					bRemovedKey = (RemoveCount > 0);

					// Test GetKeys()
					TestMap.GetKeys(Keys);

					// Test index access (Map[Key])
					IndexAccessValue = TestMap[1];

					// Test index access for non-existent key (should add default)
					TestMap[5] = "Five";
				}
			}
			)AS"),
			TEXT("ACoverageTMapAdvancedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-advanced actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-advanced actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify Find() worked
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFoundKey"), true, TEXT("Find() should locate existing key"));

		// Verify Remove() worked
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemovedKey"), true, TEXT("Remove() should remove existing key"));

		// Verify map size after removal
		int32 MapSize = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("TestMap"), MapSize), TEXT("Should get map size")));
		ASSERT_THAT(AreEqual(4, MapSize, TEXT("Map should have 4 elements after remove and index-add")));

		// Verify GetKeys() populated the array
		int32 KeysCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Keys"), KeysCount), TEXT("Should get keys array size")));
		ASSERT_THAT(AreEqual(3, KeysCount, TEXT("GetKeys() should return 3 keys (before index-add)")));

		// Verify index access read
		FString IndexValue;
		ASSERT_THAT(IsTrue(GetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("IndexAccessValue"), IndexValue),
			TEXT("Should read IndexAccessValue")));
		ASSERT_THAT(AreEqual(FString(TEXT("One")), IndexValue, TEXT("Index access should read correct value")));

		// Verify index access for new key worked
		FString NewValue;
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("TestMap"), 5, NewValue),
			TEXT("Should find newly added key")));
		ASSERT_THAT(AreEqual(FString(TEXT("Five")), NewValue, TEXT("Index-added value should be correct")));
	}

	// -------------------------------------------------------------------------
	// TMap iteration: for-each over key-value pairs
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapIteration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_Iteration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapIteration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapIterationActor : AActor
			{
				UPROPERTY()
				TMap<int, FString> TestMap;

				UPROPERTY()
				int KeySum = 0;

				UPROPERTY()
				FString ConcatenatedValues;

				UPROPERTY()
				int IterationCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestMap.Add(10, "A");
					TestMap.Add(20, "B");
					TestMap.Add(30, "C");

					// Test for-each iteration over pairs
					for (auto& Pair : TestMap)
					{
						KeySum += Pair.Key;
						ConcatenatedValues += Pair.Value;
						IterationCount++;
					}
				}
			}
			)AS"),
			TEXT("ACoverageTMapIterationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-iteration actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-iteration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify iteration worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeySum"), 60, TEXT("Key sum should be 10+20+30=60"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IterationCount"), 3, TEXT("Should iterate over all 3 pairs"));

		// Verify concatenated values (order may vary in TMap, so check length)
		FString ConcatValue;
		ASSERT_THAT(IsTrue(GetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ConcatenatedValues"), ConcatValue),
			TEXT("Should read concatenated values")));
		ASSERT_THAT(AreEqual(3, ConcatValue.Len(), TEXT("Concatenated string should have 3 characters")));
		ASSERT_THAT(IsTrue(ConcatValue.Contains(TEXT("A")) && ConcatValue.Contains(TEXT("B")) && ConcatValue.Contains(TEXT("C")),
			TEXT("Concatenated string should contain all values")));
	}

	// -------------------------------------------------------------------------
	// TMap key types: FString, FName, enum
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapKeyTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_KeyTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapKeyTypes.as"),
			ASTEST_AS(R"AS(
			enum class ETestEnum
			{
				First,
				Second,
				Third
			}

			UCLASS()
			class ACoverageTMapKeyTypesActor : AActor
			{
				UPROPERTY()
				TMap<FString, int> StringKeyMap;

				UPROPERTY()
				TMap<FName, int> NameKeyMap;

				UPROPERTY()
				TMap<ETestEnum, FString> EnumKeyMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// FString keys
					StringKeyMap.Add("Alpha", 100);
					StringKeyMap.Add("Beta", 200);
					StringKeyMap.Add("Gamma", 300);

					// FName keys
					NameKeyMap.Add(n"Red", 1);
					NameKeyMap.Add(n"Green", 2);
					NameKeyMap.Add(n"Blue", 3);

					// Enum keys
					EnumKeyMap.Add(ETestEnum::First, "FirstValue");
					EnumKeyMap.Add(ETestEnum::Second, "SecondValue");
					EnumKeyMap.Add(ETestEnum::Third, "ThirdValue");
				}
			}
			)AS"),
			TEXT("ACoverageTMapKeyTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-key-types actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-key-types actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify FString key map
		{
			int32 MapSize = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringKeyMap"), MapSize), TEXT("Should get StringKeyMap size")));
			ASSERT_THAT(AreEqual(3, MapSize, TEXT("StringKeyMap should have 3 entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringKeyMap"), FString(TEXT("Beta")), Value),
				TEXT("Should find FString key 'Beta'")));
			ASSERT_THAT(AreEqual(200, Value, TEXT("StringKeyMap['Beta'] should be 200")));
		}

		// Verify FName key map
		{
			int32 MapSize = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameKeyMap"), MapSize), TEXT("Should get NameKeyMap size")));
			ASSERT_THAT(AreEqual(3, MapSize, TEXT("NameKeyMap should have 3 entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, Actor, TEXT("NameKeyMap"), FName(TEXT("Green")), Value),
				TEXT("Should find FName key 'Green'")));
			ASSERT_THAT(AreEqual(2, Value, TEXT("NameKeyMap['Green'] should be 2")));
		}

		// Verify enum key map
		{
			int32 MapSize = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("EnumKeyMap"), MapSize), TEXT("Should get EnumKeyMap size")));
			ASSERT_THAT(AreEqual(3, MapSize, TEXT("EnumKeyMap should have 3 entries")));

			// Note: Enum keys are stored as their underlying integer type
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<uint8, FStrProperty, FString>(*TestRunner, Actor, TEXT("EnumKeyMap"), static_cast<uint8>(1), Value),
				TEXT("Should find enum key ETestEnum::Second (value 1)")));
			ASSERT_THAT(AreEqual(FString(TEXT("SecondValue")), Value, TEXT("EnumKeyMap[Second] should be 'SecondValue'")));
		}
	}

	// -------------------------------------------------------------------------
	// TMap value types: FVector, TArray<int>
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapValueTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_ValueTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapValueTypes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapValueTypesActor : AActor
			{
				UPROPERTY()
				TMap<int, FVector> IntToVectorMap;

				UPROPERTY()
				TMap<FString, TArray<int>> StringToArrayMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// FVector values
					IntToVectorMap.Add(1, FVector(1.0, 2.0, 3.0));
					IntToVectorMap.Add(2, FVector(4.0, 5.0, 6.0));
					IntToVectorMap.Add(3, FVector(7.0, 8.0, 9.0));

					// TArray<int> values
					TArray<int> Array1;
					Array1.Add(10);
					Array1.Add(20);
					StringToArrayMap.Add("First", Array1);

					TArray<int> Array2;
					Array2.Add(30);
					Array2.Add(40);
					Array2.Add(50);
					StringToArrayMap.Add("Second", Array2);
				}
			}
			)AS"),
			TEXT("ACoverageTMapValueTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-value-types actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-value-types actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify TMap<int, FVector>
		{
			int32 MapSize = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToVectorMap"), MapSize), TEXT("Should get IntToVectorMap size")));
			ASSERT_THAT(AreEqual(3, MapSize, TEXT("IntToVectorMap should have 3 entries")));

			// Verify FVector value via path (map lookups return references, but we can verify via nested path)
			FVector VectorValue;
			ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("IntToVectorMap[1]"), VectorValue),
				TEXT("Should read FVector from map")));
			ASSERT_THAT(IsTrue(VectorValue.Equals(FVector(1.0, 2.0, 3.0), 0.001), TEXT("IntToVectorMap[1] should be (1,2,3)")));
		}

		// Verify TMap<FString, TArray<int>>
		{
			int32 MapSize = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToArrayMap"), MapSize), TEXT("Should get StringToArrayMap size")));
			ASSERT_THAT(AreEqual(2, MapSize, TEXT("StringToArrayMap should have 2 entries")));

			// Verify nested array size
			int32 ArraySize = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StringToArrayMap[First]"), ArraySize),
				TEXT("Should get array size from map value")));
			ASSERT_THAT(AreEqual(2, ArraySize, TEXT("StringToArrayMap['First'] should have 2 elements")));

			// Verify nested array element
			int32 Element = 0;
			ASSERT_THAT(IsTrue(GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToArrayMap[First][1]"), Element),
				TEXT("Should read nested array element")));
			ASSERT_THAT(AreEqual(20, Element, TEXT("StringToArrayMap['First'][1] should be 20")));

			// Verify second array
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StringToArrayMap[Second]"), ArraySize),
				TEXT("Should get second array size")));
			ASSERT_THAT(AreEqual(3, ArraySize, TEXT("StringToArrayMap['Second'] should have 3 elements")));
		}
	}

	// -------------------------------------------------------------------------
	// TMap FindOrAdd: find existing or add default and return reference
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapFindOrAdd)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_FindOrAdd"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapFindOrAdd.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapFindOrAddActor : AActor
			{
				UPROPERTY()
				TMap<int, int> CounterMap;

				UPROPERTY()
				int InitialSize = 0;

				UPROPERTY()
				int FinalSize = 0;

				UPROPERTY()
				int Key10Value = 0;

				UPROPERTY()
				int Key20Value = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Pre-populate with one entry
					CounterMap.Add(10, 100);
					InitialSize = CounterMap.Num();

					// FindOrAdd on existing key - should return reference to existing value
					CounterMap.FindOrAdd(10) += 50;

					// FindOrAdd on non-existent key - should add default (0) and return reference
					CounterMap.FindOrAdd(20) += 200;

					FinalSize = CounterMap.Num();
					Key10Value = CounterMap[10];
					Key20Value = CounterMap[20];
				}
			}
			)AS"),
			TEXT("ACoverageTMapFindOrAddActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-FindOrAdd actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-FindOrAdd actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify initial and final sizes
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InitialSize"), 1, TEXT("Initial map size should be 1"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FinalSize"), 2, TEXT("Final map size should be 2 after FindOrAdd"));

		// Verify FindOrAdd modified existing value
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Key10Value"), 150, TEXT("FindOrAdd on existing key should modify value (100+50=150)"));

		// Verify FindOrAdd added new key with modified default
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Key20Value"), 200, TEXT("FindOrAdd on new key should add default and modify (0+200=200)"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
