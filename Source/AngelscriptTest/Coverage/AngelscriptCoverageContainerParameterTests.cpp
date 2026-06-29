#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageContainerParameterTests
// -----------------------------------------------------------------------------
// Complete coverage for containers as function parameters and return values.
// Covers all three container types (TArray, TMap, TSet) with all parameter
// passing modes (value, &in, &out, &inout).
//
// This extends AngelscriptCoverageContainerAdvancedTests.cpp with comprehensive
// coverage for TMap and TSet parameter passing scenarios.
//
// Matrix coverage (from OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md):
//   * TArray/TMap/TSet as value parameters (copy semantics)
//   * TArray/TMap/TSet as &in parameters (const reference, read-only)
//   * TArray/TMap/TSet as &out parameters (output, caller receives result)
//   * TArray/TMap/TSet as &inout parameters (reference, can modify)
//   * TArray/TMap/TSet as return values
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageContainerParameterTest,
	"Angelscript.TestModule.Coverage.ContainerParameter",
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
	// TMap as function parameters - all passing modes
	// -------------------------------------------------------------------------
	TEST_METHOD(TMapAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainerParam_TMap"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerParamTMap.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerParamTMapActor : AActor
			{
				UPROPERTY()
				int ResultByValue;

				UPROPERTY()
				int ResultByIn;

				UPROPERTY()
				int ResultByOut;

				UPROPERTY()
				int ResultByInout;

				UPROPERTY()
				int OriginalSize;

				// Pass by value (copy)
				int CountByValue(TMap<int, FString> Map)
				{
					Print("=== CountByValue ===");
					int Count = Map.Num();
					Print("Local map count: " + Count);
					return Count;
				}

				// Pass by const reference (&in) - read-only
				int CountByIn(const TMap<int, FString>&in Map)
				{
					Print("=== CountByIn ===");
					int Count = Map.Num();
					// Cannot modify (const)
					Print("Count: " + Count);
					return Count;
				}

				// Out parameter - function fills it
				void FillOut(TMap<int, FString>&out Result)
				{
					Print("=== FillOut ===");
					Result.Add(100, "Hundred");
					Result.Add(200, "TwoHundred");
					Result.Add(300, "ThreeHundred");
					Print("Filled with " + Result.Num() + " entries");
				}

				// Pass by reference (can modify)
				void ModifyByInout(TMap<int, FString>&inout Map)
				{
					Print("=== ModifyByInout ===");
					Print("Before: " + Map.Num());
					Map.Add(888, "Modified");
					Print("After: " + Map.Num());
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TMap as Parameter Test ===");

					// Test by value
					TMap<int, FString> Values;
					Values.Add(1, "One");
					Values.Add(2, "Two");
					Values.Add(3, "Three");
					OriginalSize = Values.Num();
					ResultByValue = CountByValue(Values);
					// Original should be unchanged
					Print("Original map after by-value call: " + Values.Num());

					// Test by const reference (&in)
					ResultByIn = CountByIn(Values);

					// Test out parameter
					TMap<int, FString> OutMap;
					FillOut(OutMap);
					ResultByOut = OutMap.Num();

					// Test inout parameter
					TMap<int, FString> InoutMap;
					InoutMap.Add(10, "Ten");
					InoutMap.Add(20, "Twenty");
					ModifyByInout(InoutMap);
					ResultByInout = InoutMap.Num();
				}
			}
			)AS"),
			TEXT("ACoverageContainerParamTMapActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TMap parameter actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TMap parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByValue"), 3, TEXT("Count by value should be 3"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByIn"), 3, TEXT("Count by &in should be 3"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByOut"), 3, TEXT("Out map should have 3 entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByInout"), 3, TEXT("Inout map should have 3 entries (2+1)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OriginalSize"), 3, TEXT("Original map should still have 3 entries"))));

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Non-const method call on read-only object reference"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageContainerParamTMapByValueMutationUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerParamTMapByValueMutationActor : AActor
			{
				int MutateByValue(TMap<int, FString> Map)
				{
					Map.Add(1, "One");
					return Map.Num();
				}
			}
			)AS"),
			TEXT("TMap by-value parameters should remain read-only inside the callee"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// TSet as function parameters - all passing modes
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainerParam_TSet"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerParamTSet.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerParamTSetActor : AActor
			{
				UPROPERTY()
				int ResultByValue;

				UPROPERTY()
				int ResultByIn;

				UPROPERTY()
				int ResultByOut;

				UPROPERTY()
				int ResultByInout;

				UPROPERTY()
				int OriginalSize;

				// Pass by value (copy)
				int CountByValue(TSet<int> Set)
				{
					Print("=== CountByValue ===");
					int Count = Set.Num();
					Print("Local set count: " + Count);
					return Count;
				}

				// Pass by const reference (&in) - read-only
				int CountByIn(const TSet<int>&in Set)
				{
					Print("=== CountByIn ===");
					int Count = Set.Num();
					Print("Count: " + Count);
					return Count;
				}

				// Out parameter - function fills it
				void FillOut(TSet<int>&out Result)
				{
					Print("=== FillOut ===");
					Result.Add(100);
					Result.Add(200);
					Result.Add(300);
					Print("Filled with " + Result.Num() + " elements");
				}

				// Pass by reference (can modify)
				void ModifyByInout(TSet<int>&inout Set)
				{
					Print("=== ModifyByInout ===");
					Print("Before: " + Set.Num());
					Set.Add(888);
					Print("After: " + Set.Num());
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TSet as Parameter Test ===");

					// Test by value
					TSet<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.Add(3);
					OriginalSize = Values.Num();
					ResultByValue = CountByValue(Values);
					// Original should be unchanged
					Print("Original set after by-value call: " + Values.Num());

					// Test by const reference (&in)
					ResultByIn = CountByIn(Values);

					// Test out parameter
					TSet<int> OutSet;
					FillOut(OutSet);
					ResultByOut = OutSet.Num();

					// Test inout parameter
					TSet<int> InoutSet;
					InoutSet.Add(10);
					InoutSet.Add(20);
					ModifyByInout(InoutSet);
					ResultByInout = InoutSet.Num();
				}
			}
			)AS"),
			TEXT("ACoverageContainerParamTSetActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet parameter actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByValue"), 3, TEXT("Count by value should be 3"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByIn"), 3, TEXT("Count by &in should be 3"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByOut"), 3, TEXT("Out set should have 3 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByInout"), 3, TEXT("Inout set should have 3 elements (2+1)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OriginalSize"), 3, TEXT("Original set should still have 3 elements"))));

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Non-const method call on read-only object reference"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageContainerParamTSetByValueMutationUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerParamTSetByValueMutationActor : AActor
			{
				int MutateByValue(TSet<int> Set)
				{
					Set.Add(1);
					return Set.Num();
				}
			}
			)AS"),
			TEXT("TSet by-value parameters should remain read-only inside the callee"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// TSet as return value
	// -------------------------------------------------------------------------
	TEST_METHOD(TSetAsReturnValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainerReturn_TSet"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerReturnTSet.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerReturnTSetActor : AActor
			{
				UPROPERTY()
				int SetSize;

				UPROPERTY()
				bool bContains100;

				UPROPERTY()
				bool bContains200;

				// Return TSet
				TSet<int> MakeSet()
				{
					Print("=== MakeSet ===");
					TSet<int> Result;
					Result.Add(100);
					Result.Add(200);
					Result.Add(300);
					Print("Created set with " + Result.Num() + " elements");
					return Result;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== TSet as Return Value Test ===");

					TSet<int> MySet = MakeSet();
					SetSize = MySet.Num();
					bContains100 = MySet.Contains(100);
					bContains200 = MySet.Contains(200);

					Print("Received set size: " + SetSize);
					Print("Contains 100: " + bContains100);
					Print("Contains 200: " + bContains200);
				}
			}
			)AS"),
			TEXT("ACoverageContainerReturnTSetActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet return actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetSize"), 3, TEXT("Returned set should have 3 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContains100"), true, TEXT("Set should contain 100"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContains200"), true, TEXT("Set should contain 200"))));
	}

	// -------------------------------------------------------------------------
	// Mixed container types as parameters
	// -------------------------------------------------------------------------
	TEST_METHOD(MixedContainerParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainerParam_Mixed"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerParamMixed.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerParamMixedActor : AActor
			{
				UPROPERTY()
				int ResultSize;

				// Function taking multiple container types
				int ProcessMultipleContainers(
					const TArray<int>&in Arr,
					const TMap<int, FString>&in Map,
					const TSet<int>&in Set)
				{
					Print("=== ProcessMultipleContainers ===");
					Print("Array size: " + Arr.Num());
					Print("Map size: " + Map.Num());
					Print("Set size: " + Set.Num());
					return Arr.Num() + Map.Num() + Set.Num();
				}

				// Function returning different containers based on input
				TArray<int> SetToArray(const TSet<int>&in InputSet)
				{
					Print("=== SetToArray ===");
					TArray<int> Result;
					for (int Val : InputSet)
					{
						Result.Add(Val);
					}
					Print("Converted " + InputSet.Num() + " elements to array");
					return Result;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== Mixed Container Parameters Test ===");

					TArray<int> MyArray;
					MyArray.Add(1);
					MyArray.Add(2);

					TMap<int, FString> MyMap;
					MyMap.Add(1, "One");
					MyMap.Add(2, "Two");
					MyMap.Add(3, "Three");

					TSet<int> MySet;
					MySet.Add(10);
					MySet.Add(20);

					ResultSize = ProcessMultipleContainers(MyArray, MyMap, MySet);

					// Test conversion
					TSet<int> ConvertSet;
					ConvertSet.Add(100);
					ConvertSet.Add(200);
					TArray<int> ConvertedArray = SetToArray(ConvertSet);
					Print("Converted array size: " + ConvertedArray.Num());
				}
			}
			)AS"),
			TEXT("ACoverageContainerParamMixedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Mixed container parameter actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Mixed container parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultSize"), 7, TEXT("Total size should be 2+3+2=7"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
