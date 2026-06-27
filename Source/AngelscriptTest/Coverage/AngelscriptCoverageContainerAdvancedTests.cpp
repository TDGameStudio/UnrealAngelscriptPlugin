#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageContainerAdvancedTests
// -----------------------------------------------------------------------------
// Advanced container usage patterns: parameters, return values, nested
// combinations, and container usage scenarios.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageContainerAdvancedTest,
	"Angelscript.TestModule.Coverage.ContainerAdvanced",
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
	// Container as function parameters - value, reference, out
	// -------------------------------------------------------------------------
	TEST_METHOD(ContainerAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_Parameter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerParameter.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerParameterActor : AActor
			{
				UPROPERTY()
				int ResultByValue;

				UPROPERTY()
				int ResultByRef;

				UPROPERTY()
				int ResultByOut;

				// Pass by value (copy)
				int SumByValue(TArray<int> Arr)
				{
					Print("=== SumByValue ===");
					int Sum = 0;
					for (int Val : Arr)
						Sum += Val;
					Print("Sum: " + Sum);
					return Sum;
				}

				// Pass by reference (can modify)
				void ModifyByRef(TArray<int>&inout Arr)
				{
					Print("=== ModifyByRef ===");
					Print("Before: " + Arr.Num());
					Arr.Add(999);
					Print("After: " + Arr.Num());
				}

				// Out parameter
				void FillOut(TArray<int>&out Result)
				{
					Print("=== FillOut ===");
					Result.Add(100);
					Result.Add(200);
					Result.Add(300);
					Print("Filled with " + Result.Num() + " elements");
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== Container as Parameter Test ===");

					// Test by value
					TArray<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.Add(3);
					ResultByValue = SumByValue(Values);
					Print("Original array still: " + Values.Num());

					// Test by reference
					TArray<int> RefArray;
					RefArray.Add(10);
					RefArray.Add(20);
					ModifyByRef(RefArray);
					ResultByRef = RefArray.Num();

					// Test out parameter
					TArray<int> OutArray;
					FillOut(OutArray);
					ResultByOut = OutArray.Num();
				}
			}
			)AS"),
			TEXT("ACoverageContainerParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Container parameter actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Container parameter actor should spawn")));

		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByValue"), 6, TEXT("Sum by value should be 6"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByRef"), 3, TEXT("Array modified by ref should have 3 elements"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByOut"), 3, TEXT("Out array should have 3 elements"));
	}

	// -------------------------------------------------------------------------
	// Container as return value
	// -------------------------------------------------------------------------
	TEST_METHOD(ContainerAsReturnValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_Return"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerReturn.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerReturnActor : AActor
			{
				UPROPERTY()
				int ArraySize;

				UPROPERTY()
				int MapSize;

				// Return TArray
				TArray<int> MakeArray()
				{
					Print("=== MakeArray ===");
					TArray<int> Result;
					Result.Add(10);
					Result.Add(20);
					Result.Add(30);
					Print("Created array with " + Result.Num() + " elements");
					return Result;
				}

				// Return TMap
				TMap<int, FString> MakeMap()
				{
					Print("=== MakeMap ===");
					TMap<int, FString> Result;
					Result.Add(1, "One");
					Result.Add(2, "Two");
					Print("Created map with " + Result.Num() + " entries");
					return Result;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== Container as Return Value Test ===");

					TArray<int> MyArray = MakeArray();
					ArraySize = MyArray.Num();
					Print("Received array size: " + ArraySize);

					TMap<int, FString> MyMap = MakeMap();
					MapSize = MyMap.Num();
					Print("Received map size: " + MapSize);
				}
			}
			)AS"),
			TEXT("ACoverageContainerReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Container return actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Container return actor should spawn")));

		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArraySize"), 3, TEXT("Returned array should have 3 elements"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapSize"), 2, TEXT("Returned map should have 2 entries"));
	}

	// -------------------------------------------------------------------------
	// More nested container combinations
	// -------------------------------------------------------------------------
	TEST_METHOD(NestedContainerCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_Nested"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerNested.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerNestedActor : AActor
			{
				UPROPERTY()
				int ArrayOfMapsSize;

				UPROPERTY()
				int FirstMapSize;

				UPROPERTY()
				int NestedValue;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== Nested Container Combinations Test ===");

					// TArray<TMap<int, FString>>
					TArray<TMap<int, FString>> ArrayOfMaps;

					TMap<int, FString> Map1;
					Map1.Add(1, "A");
					Map1.Add(2, "B");
					ArrayOfMaps.Add(Map1);

					TMap<int, FString> Map2;
					Map2.Add(3, "C");
					Map2.Add(4, "D");
					ArrayOfMaps.Add(Map2);

					Print("Array of maps size: " + ArrayOfMaps.Num());
					ArrayOfMapsSize = ArrayOfMaps.Num();
					FirstMapSize = ArrayOfMaps[0].Num();

					// Access nested value
					FString Value = ArrayOfMaps[0][1];
					Print("ArrayOfMaps[0][1]: " + Value);
					NestedValue = (Value == "A") ? 1 : 0;
				}
			}
			)AS"),
			TEXT("ACoverageContainerNestedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Nested container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Nested container actor should spawn")));

		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayOfMapsSize"), 2, TEXT("Array should contain 2 maps"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstMapSize"), 2, TEXT("First map should have 2 entries"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedValue"), 1, TEXT("Nested value should be 'A'"));
	}

	// -------------------------------------------------------------------------
	// Non-UPROPERTY containers (local/temporary)
	// -------------------------------------------------------------------------
	TEST_METHOD(NonUPropertyContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_NonUProp"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerNonUProp.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerNonUPropActor : AActor
			{
				UPROPERTY()
				int LocalArrayResult;

				UPROPERTY()
				int TempMapResult;

				void ProcessLocalArray()
				{
					Print("=== ProcessLocalArray ===");
					// Local container (not UPROPERTY)
					TArray<int> LocalArray;
					LocalArray.Add(100);
					LocalArray.Add(200);
					LocalArray.Add(300);
					Print("Local array size: " + LocalArray.Num());
					LocalArrayResult = LocalArray.Num();
				}

				void UseTempMap()
				{
					Print("=== UseTempMap ===");
					// Temporary container
					TMap<int, int> TempMap;
					TempMap.Add(1, 10);
					TempMap.Add(2, 20);
					TempMapResult = TempMap.Num();
					Print("Temp map size: " + TempMapResult);
					// Map goes out of scope here
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== Non-UPROPERTY Containers Test ===");
					ProcessLocalArray();
					UseTempMap();
				}
			}
			)AS"),
			TEXT("ACoverageContainerNonUPropActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Non-UPROPERTY container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Non-UPROPERTY container actor should spawn")));

		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LocalArrayResult"), 3, TEXT("Local array should have 3 elements"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TempMapResult"), 2, TEXT("Temp map should have 2 entries"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
