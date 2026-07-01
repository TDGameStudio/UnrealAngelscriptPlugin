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
// AngelscriptCoverageTMapAdvancedTests
// -----------------------------------------------------------------------------
// Advanced TMap container operations coverage, extending the basic int tests
// from AngelscriptCoverageIntPropertyTests.cpp.
//
// Matrix coverage (from OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md):
//   * TMapAdvancedOperations - Find(out), Remove(), GetKeys(), index access
//   * TMapIteration          - Iterator() key-value traversal
//   * TMapKeyTypes           - FString, FName, enum keys
//   * TMapValueTypes         - FVector values and nested-container rejection
//   * TMapUserStructValues   - user USTRUCT values
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
	// TMap advanced operations: Find(out), Remove(), GetKeys(), index access
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
				FString FoundValue;

				UPROPERTY()
				bool bRemovedKey = false;

				UPROPERTY()
				bool bContainsRemovedKey = true;

				UPROPERTY()
				TArray<int> Keys;

				UPROPERTY()
				TArray<FString> Values;

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

					// Test Find() - copies the value to an out parameter.
					bFoundKey = TestMap.Find(2, FoundValue);

					// Test Remove()
					bRemovedKey = TestMap.Remove(3);
					bContainsRemovedKey = TestMap.Contains(3);

					// Test GetKeys()
					TestMap.GetKeys(Keys);

					// Test GetValues()
					TestMap.GetValues(Values);

					// Test index access (Map[Key])
					IndexAccessValue = TestMap[1];

					// Add a new key after index-read coverage.
					TestMap.Add(5, "Five");
				}
			}
			)AS"),
			TEXT("ACoverageTMapAdvancedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-advanced actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-advanced actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify Find(out) worked
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFoundKey"), true, TEXT("Find(out) should locate existing key"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("FoundValue"), FString(TEXT("Two")), TEXT("Find(out) should copy the found value"))));

		// Verify Remove() worked
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemovedKey"), true, TEXT("Remove() should remove existing key"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsRemovedKey"), false, TEXT("Removed key should no longer be present"))));

		// Verify map size after removal
		int32 MapSize = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("TestMap"), MapSize), TEXT("Should get map size")));
		ASSERT_THAT(AreEqual(4, MapSize, TEXT("Map should have 4 elements after remove and index-add")));

		// Verify GetKeys() populated the array
		int32 KeysCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Keys"), KeysCount), TEXT("Should get keys array size")));
		ASSERT_THAT(AreEqual(3, KeysCount, TEXT("GetKeys() should return 3 keys (before index-add)")));

		int32 ValuesCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Values"), ValuesCount), TEXT("Should get values array size")));
		ASSERT_THAT(AreEqual(3, ValuesCount, TEXT("GetValues() should return 3 values (before index-add)")));

		// Verify index access read
		FString IndexValue;
		ASSERT_THAT(IsTrue(GetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("IndexAccessValue"), IndexValue),
			TEXT("Should read IndexAccessValue")));
		ASSERT_THAT(AreEqual(FString(TEXT("One")), IndexValue, TEXT("Index access should read correct value")));

		// Verify the explicitly added key worked
		FString NewValue;
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("TestMap"), 5, NewValue),
			TEXT("Should find newly added key")));
		ASSERT_THAT(AreEqual(FString(TEXT("Five")), NewValue, TEXT("Index-added value should be correct")));
	}

	// -------------------------------------------------------------------------
	// TMap iteration: explicit iterator over key-value pairs
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

					// Test explicit iterator over key-value pairs.
					TMapIterator<int, FString> It = TestMap.Iterator();
					while (It.CanProceed)
					{
						It.Proceed();
						KeySum += It.GetKey();
						ConcatenatedValues += It.GetValue();
						IterationCount++;
					}
				}
			}
			)AS"),
			TEXT("ACoverageTMapIterationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-iteration actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-iteration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify iteration worked
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeySum"), 60, TEXT("Key sum should be 10+20+30=60"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IterationCount"), 3, TEXT("Should iterate over all 3 pairs"))));

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-key-types actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
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
	// TMap value types: FVector and nested-container boundary
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
				FVector FirstVectorValue;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// FVector values
					IntToVectorMap.Add(1, FVector(1.0, 2.0, 3.0));
					IntToVectorMap.Add(2, FVector(4.0, 5.0, 6.0));
					IntToVectorMap.Add(3, FVector(7.0, 8.0, 9.0));
					FirstVectorValue = IntToVectorMap[1];
				}
			}
			)AS"),
			TEXT("ACoverageTMapValueTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap-value-types actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-value-types actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify TMap<int, FVector>
		{
			int32 MapSize = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToVectorMap"), MapSize), TEXT("Should get IntToVectorMap size")));
			ASSERT_THAT(AreEqual(3, MapSize, TEXT("IntToVectorMap should have 3 entries")));

			// Verify FVector map value access via AS; property paths do not index TMap keys.
			FVector VectorValue;
			ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("FirstVectorValue"), VectorValue),
				TEXT("Should read FVector from map")));
			ASSERT_THAT(IsTrue(VectorValue.Equals(FVector(1.0, 2.0, 3.0), 0.001), TEXT("IntToVectorMap[1] should be (1,2,3)")));
		}

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Containers cannot be nested in other containers"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTMap_ArrayValueUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapArrayValueActor : AActor
			{
				UPROPERTY()
				TMap<FString, TArray<int>> StringToArrayMap;
			}
			)AS"),
			TEXT("TMap<FString,TArray<int>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// TMap value type: user-defined USTRUCT values
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapUserStructValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_UserStructValues"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapUserStructValues.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FCoverageTMapPayload
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;

				UPROPERTY()
				bool bComplete = false;
			}

			UCLASS()
			class ACoverageTMapUserStructValuesActor : AActor
			{
				UPROPERTY()
				TMap<int, FCoverageTMapPayload> StructValues;

				UPROPERTY()
				FCoverageTMapPayload FoundPayload;

				UPROPERTY()
				FCoverageTMapPayload OverwrittenPayload;

				UPROPERTY()
				bool bFoundPayload = false;

				UPROPERTY()
				int PayloadCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FCoverageTMapPayload First;
					First.Score = 11;
					First.Label = "First";
					First.bComplete = false;

					FCoverageTMapPayload Second;
					Second.Score = 22;
					Second.Label = "Second";
					Second.bComplete = true;

					FCoverageTMapPayload Replacement;
					Replacement.Score = 33;
					Replacement.Label = "Replacement";
					Replacement.bComplete = true;

					StructValues.Add(1, First);
					StructValues.Add(2, Second);
					bFoundPayload = StructValues.Find(2, FoundPayload);

					StructValues.Add(1, Replacement);
					OverwrittenPayload = StructValues[1];
					PayloadCount = StructValues.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTMapUserStructValuesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap user-struct-value actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FMapProperty* StructValuesProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructValues"));
		ASSERT_THAT(IsNotNull(StructValuesProperty, TEXT("TMap<int,FCoverageTMapPayload> should reflect as FMapProperty")));
		if (StructValuesProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(StructValuesProperty->KeyProp),
			TEXT("TMap user-struct-value key should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructValuesProperty->ValueProp),
			TEXT("TMap user-struct-value value should reflect as FStructProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap user-struct-value actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		int32 MapSize = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructValues"), MapSize), TEXT("Should get StructValues size")));
		ASSERT_THAT(AreEqual(2, MapSize, TEXT("StructValues should retain two entries after overwrite")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PayloadCount"), 2,
			TEXT("AS-side TMap.Num should report two user-struct entries"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFoundPayload"), true,
			TEXT("TMap.Find should locate a user USTRUCT value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FoundPayload.Score"), 22,
			TEXT("TMap.Find should copy user USTRUCT int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("FoundPayload.Label"), FString(TEXT("Second")),
			TEXT("TMap.Find should copy user USTRUCT string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundPayload.bComplete"), true,
			TEXT("TMap.Find should copy user USTRUCT bool fields"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OverwrittenPayload.Score"), 33,
			TEXT("TMap index access should return overwritten user USTRUCT int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("OverwrittenPayload.Label"), FString(TEXT("Replacement")),
			TEXT("TMap index access should return overwritten user USTRUCT string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverwrittenPayload.bComplete"), true,
			TEXT("TMap index access should return overwritten user USTRUCT bool fields"))));
	}

	// -------------------------------------------------------------------------
	// TMap key overwrite plus stable lookup/removal/key-value array operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapOverwriteRemoveContainsKeysValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_OverwriteLookup"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapOverwriteLookup.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapOverwriteLookupActor : AActor
			{
				UPROPERTY()
				TMap<int, FString> Values;

				UPROPERTY()
				FString OverwrittenValue;

				UPROPERTY()
				bool bContainsOverwrittenKey = false;

				UPROPERTY()
				bool bRemovedExistingKey = false;

				UPROPERTY()
				bool bRemovedMissingKey = true;

				UPROPERTY()
				bool bContainsRemovedKey = true;

				UPROPERTY()
				TArray<int> Keys;

				UPROPERTY()
				TArray<FString> OutValues;

				UPROPERTY()
				int FinalSize = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Values.Add(1, "One");
					Values.Add(2, "Two");
					Values.Add(2, "TwoUpdated");
					Values.Add(3, "Three");

					bContainsOverwrittenKey = Values.Contains(2);
					Values.Find(2, OverwrittenValue);

					bRemovedExistingKey = Values.Remove(1);
					bRemovedMissingKey = Values.Remove(99);
					bContainsRemovedKey = Values.Contains(1);

					Values.GetKeys(Keys);
					Values.GetValues(OutValues);
					FinalSize = Values.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTMapOverwriteLookupActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap overwrite lookup actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap overwrite lookup actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsOverwrittenKey"), true, TEXT("Contains should find overwritten key"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("OverwrittenValue"), FString(TEXT("TwoUpdated")), TEXT("Add should overwrite existing key value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemovedExistingKey"), true, TEXT("Remove should return true for an existing key"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemovedMissingKey"), false, TEXT("Remove should return false for a missing key"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsRemovedKey"), false, TEXT("Contains should return false after removal"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FinalSize"), 2, TEXT("Map should keep overwritten key and remove one key"))));

		int32 KeysCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Keys"), KeysCount), TEXT("Should get keys array size")));
		ASSERT_THAT(AreEqual(2, KeysCount, TEXT("GetKeys should return remaining keys")));

		int32 ValuesCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("OutValues"), ValuesCount), TEXT("Should get values array size")));
		ASSERT_THAT(AreEqual(2, ValuesCount, TEXT("GetValues should return remaining values")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap-FindOrAdd actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify initial and final sizes
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InitialSize"), 1, TEXT("Initial map size should be 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FinalSize"), 2, TEXT("Final map size should be 2 after FindOrAdd"))));

		// Verify FindOrAdd modified existing value
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Key10Value"), 150, TEXT("FindOrAdd on existing key should modify value (100+50=150)"))));

		// Verify FindOrAdd added new key with modified default
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Key20Value"), 200, TEXT("FindOrAdd on new key should add default and modify (0+200=200)"))));
	}

	// -------------------------------------------------------------------------
	// TMap Remove operations and cleanup
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapRemoveOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_Remove"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapRemove.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapRemoveActor : AActor
			{
				UPROPERTY()
				int InitialSize;

				UPROPERTY()
				int AfterRemoveSize;

				UPROPERTY()
				int AfterEmptySize;

				UPROPERTY()
				bool bRemovedExisting;

				UPROPERTY()
				bool bContainsRemovedKey;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TMap Remove Operations Test ===");

					TMap<int, FString> Items;
					Items.Add(1, "One");
					Items.Add(2, "Two");
					Items.Add(3, "Three");
					Items.Add(4, "Four");
					Items.Add(5, "Five");

					Print("Initial map size: " + Items.Num());
					InitialSize = Items.Num();

					// Test Remove - returns whether the key was removed.
					bool bRemoved = Items.Remove(3);
					Print("Removed key 3: " + bRemoved);
					Print("Size after Remove: " + Items.Num());
					bRemovedExisting = bRemoved;
					AfterRemoveSize = Items.Num();

					// Verify key no longer exists
					bool HasThree = Items.Contains(3);
					Print("Contains(3) after Remove: " + HasThree);
					bContainsRemovedKey = HasThree;

					// Test Empty
					Items.Empty();
					Print("Size after Empty: " + Items.Num());
					AfterEmptySize = Items.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTMapRemoveActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap Remove actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap Remove actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InitialSize"), 5, TEXT("Initial map should have 5 entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemovedExisting"), true, TEXT("Remove should return true for an existing key"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsRemovedKey"), false, TEXT("Removed key should no longer be present"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AfterRemoveSize"), 4, TEXT("Map should have 4 entries after Remove"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AfterEmptySize"), 0, TEXT("Map should be empty after Empty()"))));
	}

	// -------------------------------------------------------------------------
	// TMap with TArray values is rejected by this fork.
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapWithArrayValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Containers cannot be nested in other containers"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTMapArrayValuesUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapArrayValuesActor : AActor
			{
				UPROPERTY()
				TMap<int, TArray<int>> Groups;
			}
			)AS"),
			TEXT("TMap<int,TArray<int>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// TMap edge cases - empty map operations
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapEdgeCases)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTMap_EdgeCases"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTMapEdgeCases.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapEdgeCasesActor : AActor
			{
				UPROPERTY()
				int EmptySize;

				UPROPERTY()
				int ContainsResult;

				UPROPERTY()
				int SingleEntrySize;

				UPROPERTY()
				bool bRemovedFromEmpty;

				UPROPERTY()
				int AfterSingleRemoveSize;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TMap Edge Cases Test ===");

					// Test empty map operations
					TMap<int, FString> EmptyMap;
					Print("Empty map size: " + EmptyMap.Num());
					EmptySize = EmptyMap.Num();

					bool HasKey = EmptyMap.Contains(5);
					Print("Contains(5) in empty map: " + HasKey);
					ContainsResult = HasKey ? 1 : 0;

					// Remove from empty map (should not crash)
					bool bRemoved = EmptyMap.Remove(5);
					Print("Remove(5) from empty map returned: " + bRemoved);
					bRemovedFromEmpty = bRemoved;

					// Test single entry map
					TMap<int, FString> SingleMap;
					SingleMap.Add(42, "Answer");
					Print("Single entry map size: " + SingleMap.Num());
					SingleEntrySize = SingleMap.Num();

					// Remove the only entry
					SingleMap.Remove(42);
					Print("After removing only entry: " + SingleMap.Num());
					AfterSingleRemoveSize = SingleMap.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTMapEdgeCasesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap edge cases actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap edge cases actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EmptySize"), 0, TEXT("Empty map should have size 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ContainsResult"), 0, TEXT("Contains in empty map should return false"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemovedFromEmpty"), false, TEXT("Remove from empty map should return false"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SingleEntrySize"), 1, TEXT("Single entry map should have size 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AfterSingleRemoveSize"), 0, TEXT("Removing the only entry should empty the map"))));
	}

	// -------------------------------------------------------------------------
	// Unsupported TMap API aliases from UE API surface
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapUnsupportedApiAliases)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::GenerateKeyArray(int[])'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::GenerateValueArray(FString[])'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::FindRef(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::FindChecked(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::Reserve(const int)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::Shrink()'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::Append(TMap<int,FString>)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TMap::FilterByPredicate(const int)'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTMapUnsupportedApiAliases"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTMapUnsupportedApiActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TMap<int, FString> Values;
					TMap<int, FString> Other;
					TArray<int> Keys;
					TArray<FString> OutValues;
					Values.Add(1, "One");
					Values.GenerateKeyArray(Keys);
					Values.GenerateValueArray(OutValues);
					Values.FindRef(1);
					Values.FindChecked(1);
					Values.Reserve(8);
					Values.Shrink();
					Values.Append(Other);
					Values.FilterByPredicate(1);
				}
			}
			)AS"),
			TEXT("Unbound TMap UE aliases should remain explicit unsupported boundaries"),
			MakeArrayView(ExpectedDiagnostics))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
