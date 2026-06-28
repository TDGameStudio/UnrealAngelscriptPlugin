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
//   * TSetAdvancedOperations - Contains(), Remove(), Empty(), Reset()
//   * TSetIteration          - for-each loops
//   * TSetSetOperations      - Append(Set) and unsupported set-operation aliases
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
	// TSet advanced operations: Contains(), Remove(), Empty(), Reset()
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
				bool bContainsElement = false;

				UPROPERTY()
				bool bRemoved20 = false;

				UPROPERTY()
				bool bRemoved40 = false;

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

					// Test Contains()
					bContainsElement = TestSet.Contains(30);

					// Test Remove()
					bRemoved20 = TestSet.Remove(20);
					bRemoved40 = TestSet.Remove(40);

					// Test Empty()
					SizeBeforeEmpty = TestSet.Num();
					TestSet.Empty();
					SizeAfterEmpty = TestSet.Num();
				}
			}
			)AS"),
			TEXT("ACoverageTSetAdvancedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-advanced actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-advanced actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify Contains() worked
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainsElement"), true, TEXT("Contains() should locate existing element"))));

		// Verify Remove() worked
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemoved20"), true, TEXT("Remove() should remove element 20"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemoved40"), true, TEXT("Remove() should remove element 40"))));

		// Verify set size before empty
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeBeforeEmpty"), 3, TEXT("Set should have 3 elements before Empty()"))));

		// Verify Empty() worked
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeAfterEmpty"), 0, TEXT("Set should be empty after Empty()"))));

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::Find(const int)'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTSetFindUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetFindUnsupportedActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TSet<int> Values;
					Values.Add(1);
					Values.Find(1);
				}
			}
			)AS"),
			TEXT("TSet.Find() should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
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

				UPROPERTY()
				int ExplicitIteratorSum = 0;

				UPROPERTY()
				int ExplicitIteratorCount = 0;

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

					// Test explicit iterator API.
					TSetIterator<int> It = TestSet.Iterator();
					while (It.CanProceed)
					{
						ExplicitIteratorSum += It.Proceed();
						ExplicitIteratorCount++;
					}
				}
			}
			)AS"),
			TEXT("ACoverageTSetIterationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-iteration actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-iteration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify iteration worked
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Sum"), 120, TEXT("Sum should be 15+25+35+45=120"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IterationCount"), 4, TEXT("Should iterate over all 4 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MaxValue"), 45, TEXT("Max value should be 45"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExplicitIteratorSum"), 120, TEXT("explicit TSet iterator should traverse all values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExplicitIteratorCount"), 4, TEXT("explicit TSet iterator should visit all elements"))));
	}

	// -------------------------------------------------------------------------
	// TSet set operations: Append(Set) and unsupported aliases
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

					// Test Append(Set): A plus B = {1, 2, 3, 4, 5, 6, 7}
					UnionResult = SetA;
					UnionResult.Append(SetB);
				}
			}
			)AS"),
			TEXT("ACoverageTSetSetOperationsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-set-operations actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-set-operations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify Union result
		{
			int32 UnionSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("UnionResult"), UnionSize), TEXT("Should get UnionResult size")));
			ASSERT_THAT(AreEqual(7, UnionSize, TEXT("Union should have 7 elements")));
			ASSERT_THAT(IsTrue(SetContainsByPath<int32>(*TestRunner, Actor, TEXT("UnionResult"), 1), TEXT("UnionResult should contain SetA-only value")));
			ASSERT_THAT(IsTrue(SetContainsByPath<int32>(*TestRunner, Actor, TEXT("UnionResult"), 7), TEXT("UnionResult should contain SetB-only value")));
		}

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::Union(TSet<int>)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::Intersect(TSet<int>)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::Difference(TSet<int>)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::Includes(TSet<int>)'"));
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::FilterByPredicate(const int)'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTSet_SetOperationAliasesUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetSetOperationAliasesActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TSet<int> Values;
					TSet<int> Other;
					Values.Union(Other);
					Values.Intersect(Other);
					Values.Difference(Other);
					Values.Includes(Other);
					Values.FilterByPredicate(1);
				}
			}
			)AS"),
			TEXT("TSet set-operation aliases and predicate filtering should remain explicit unsupported boundaries"),
			MakeArrayView(ExpectedDiagnostics))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-element-types actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify FString set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StringSet"), SetSize), TEXT("Should get StringSet size")));
			ASSERT_THAT(AreEqual(4, SetSize, TEXT("StringSet should have 4 elements")));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStringContains"), true, TEXT("StringSet should contain 'Banana'"))));
		}

		// Verify FName set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("NameSet"), SetSize), TEXT("Should get NameSet size")));
			ASSERT_THAT(AreEqual(4, SetSize, TEXT("NameSet should have 4 elements")));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameContains"), true, TEXT("NameSet should contain 'Green'"))));
		}

		// Verify Enum set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("EnumSet"), SetSize), TEXT("Should get EnumSet size")));
			ASSERT_THAT(AreEqual(3, SetSize, TEXT("EnumSet should have 3 elements")));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bEnumContains"), true, TEXT("EnumSet should contain Beta"))));
		}

		// Verify FVector set
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("VectorSet"), SetSize), TEXT("Should get VectorSet size")));
			ASSERT_THAT(AreEqual(3, SetSize, TEXT("VectorSet should have 3 elements")));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bVectorContains"), true, TEXT("VectorSet should contain (0,1,0)"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify original set unchanged
		{
			int32 SetSize = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("OriginalSet"), SetSize), TEXT("Should get OriginalSet size")));
			ASSERT_THAT(AreEqual(3, SetSize, TEXT("OriginalSet should still have 3 elements")));
		}

		// Verify sum from const ref parameter
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SumFromConstRef"), 6, TEXT("Sum should be 1+2+3=6"))));

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
	// TSet Append(Array) and unsupported Array() conversion
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetArrayConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTSet_ArrayAppend"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTSetArrayAppend.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetArrayConversionActor : AActor
			{
				UPROPERTY()
				TSet<int> UniqueSet;

				UPROPERTY()
				TArray<int> SourceArray;

				UPROPERTY()
				int SetSize = 0;

				UPROPERTY()
				bool bSetContainsAll = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SourceArray.Add(100);
					SourceArray.Add(200);
					SourceArray.Add(300);
					SourceArray.Add(400);
					SourceArray.Add(400);

					UniqueSet.Append(SourceArray);
					SetSize = UniqueSet.Num();

					// Verify all elements present (order may vary)
					bSetContainsAll =
						UniqueSet.Contains(100) &&
						UniqueSet.Contains(200) &&
						UniqueSet.Contains(300) &&
						UniqueSet.Contains(400);
				}
			}
			)AS"),
			TEXT("ACoverageTSetArrayConversionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet-array-append actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-array-append actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify Append(Array) worked
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetSize"), 4, TEXT("Set should contain 4 unique elements after Append(Array)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetContainsAll"), true, TEXT("Set should contain all array elements"))));

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'TSet::Array()'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageTSetArrayConversionUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTSetArrayConversionUnsupportedActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TSet<int> Values;
					Values.Add(1);
					TArray<int> Converted = Values.Array();
				}
			}
			)AS"),
			TEXT("TSet.Array() should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet-reset-capacity actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify Reset() worked
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeBeforeReset"), 5, TEXT("Set should have 5 elements before Reset()"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SizeAfterReset"), 0, TEXT("Set should be empty after Reset()"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bReAddSuccess"), true, TEXT("Should be able to re-add elements after Reset()"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
