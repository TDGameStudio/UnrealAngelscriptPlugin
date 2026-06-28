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
public:
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Container parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByValue"), 6, TEXT("Sum by value should be 6"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByRef"), 3, TEXT("Array modified by ref should have 3 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultByOut"), 3, TEXT("Out array should have 3 elements"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Container return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArraySize"), 3, TEXT("Returned array should have 3 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapSize"), 2, TEXT("Returned map should have 2 entries"))));
	}

	// -------------------------------------------------------------------------
	// Container reference return: return a member array by reference
	// -------------------------------------------------------------------------
	TEST_METHOD(ContainerReferenceReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_RefReturn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerReferenceReturn.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerReferenceReturnActor : AActor
			{
				UPROPERTY()
				TArray<int> Values;

				UPROPERTY()
				int RefSize = 0;

				UPROPERTY()
				int FirstValue = 0;

				TArray<int>& GetValuesRef()
				{
					return Values;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Values.Add(10);
					Values.Add(20);

					TArray<int>& Ref = GetValuesRef();
					Ref.Add(30);

					RefSize = Values.Num();
					FirstValue = Ref[0];
				}
			}
			)AS"),
			TEXT("ACoverageContainerReferenceReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Container reference return actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Container reference return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RefSize"), 3, TEXT("Returned array reference should modify the member array"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstValue"), 10, TEXT("Returned array reference should preserve member contents"))));
	}

	// -------------------------------------------------------------------------
	// Array<Struct<Array>>: container inside struct, then struct array
	// -------------------------------------------------------------------------
	TEST_METHOD(ArrayOfStructsContainingArrays)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_ArrayStructArray"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerArrayStructArray.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FArrayPayload
			{
				UPROPERTY()
				TArray<int> Values;
			}

			UCLASS()
			class ACoverageContainerArrayStructArrayActor : AActor
			{
				UPROPERTY()
				TArray<FArrayPayload> Payloads;

				UPROPERTY()
				int PayloadCount = 0;

				UPROPERTY()
				int FirstInnerSize = 0;

				UPROPERTY()
				int SecondInnerFirstValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FArrayPayload First;
					First.Values.Add(1);
					First.Values.Add(2);

					FArrayPayload Second;
					Second.Values.Add(10);
					Second.Values.Add(20);
					Second.Values.Add(30);

					Payloads.Add(First);
					Payloads.Add(Second);

					PayloadCount = Payloads.Num();
					FirstInnerSize = Payloads[0].Values.Num();
					SecondInnerFirstValue = Payloads[1].Values[0];
				}
			}
			)AS"),
			TEXT("ACoverageContainerArrayStructArrayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Array<Struct<Array>> actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Array<Struct<Array>> actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PayloadCount"), 2, TEXT("Outer TArray<FArrayPayload> should have two elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstInnerSize"), 2, TEXT("First struct inner array should have two elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SecondInnerFirstValue"), 10, TEXT("Second struct inner array should preserve first value"))));
	}

	// -------------------------------------------------------------------------
	// Nested container combinations are rejected by this fork.
	// -------------------------------------------------------------------------
	TEST_METHOD(NestedContainerCombinationsUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ExpectedDiagnostic(TEXT("Containers cannot be nested in other containers"));
		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(ExpectedDiagnostic);

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(*TestRunner, Engine, TEXT("ASCoverageContainer_ArrayOfMapsUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerArrayOfMapsActor : AActor
			{
				UPROPERTY()
				TArray<TMap<int, FString>> ArrayOfMaps;
			}
			)AS"),
			TEXT("TArray<TMap<int,FString>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Advanced iterator operations: copy/assignment, mutation, map removal
	// -------------------------------------------------------------------------
	TEST_METHOD(ContainerIteratorAdvancedOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_IteratorAdvanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerIteratorAdvanced.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageContainerIteratorAdvancedActor : AActor
			{
				UPROPERTY()
				int ArrayCopyAssignSum = 0;

				UPROPERTY()
				int ArrayMutableWriteSum = 0;

				UPROPERTY()
				int MapCopyAssignKeySum = 0;

				UPROPERTY()
				int MapCopyAssignValueSum = 0;

				UPROPERTY()
				int MapMutationVisitedCount = 0;

				UPROPERTY()
				int MapMutationRemainingCount = 0;

				UPROPERTY()
				int MapMutationUpdatedValueSum = 0;

				UPROPERTY()
				int MapMutationRemovedKeyCount = 0;

				UPROPERTY()
				int SetCopyAssignSum = 0;

				UPROPERTY()
				int SetCopyAssignVisitCount = 0;

				void ExerciseArrayIterator()
				{
					TArray<int> Values;
					Values.Add(1);
					Values.Add(2);
					Values.Add(3);
					Values.Add(4);

					TArrayIterator<int> Original = Values.Iterator();
					TArrayIterator<int> Copied = Original;
					TArrayIterator<int> Assigned = Values.Iterator();
					Assigned = Copied;

					while (Original.CanProceed)
					{
						ArrayCopyAssignSum += Original.Proceed();
					}

					while (Copied.CanProceed)
					{
						ArrayCopyAssignSum += Copied.Proceed();
					}

					while (Assigned.CanProceed)
					{
						ArrayCopyAssignSum += Assigned.Proceed();
					}

					TArrayIterator<int> Mutating = Values.Iterator();
					if (Mutating.CanProceed)
					{
						Mutating.Proceed() = 10;
					}

					ArrayMutableWriteSum = Values[0] + Values[1] + Values[2] + Values[3];
				}

				void ExerciseMapIterator()
				{
					TMap<int, int> Values;
					Values.Add(1, 10);
					Values.Add(2, 20);
					Values.Add(3, 30);

					TMapIterator<int, int> Original = Values.Iterator();
					TMapIterator<int, int> Copied = Original;
					TMapIterator<int, int> Assigned = Values.Iterator();
					Assigned = Copied;

					while (Original.CanProceed)
					{
						Original.Proceed();
						MapCopyAssignKeySum += Original.GetKey();
						MapCopyAssignValueSum += Original.GetValue();
					}

					while (Copied.CanProceed)
					{
						Copied.Proceed();
						MapCopyAssignKeySum += Copied.GetKey();
						MapCopyAssignValueSum += Copied.GetValue();
					}

					while (Assigned.CanProceed)
					{
						Assigned.Proceed();
						MapCopyAssignKeySum += Assigned.GetKey();
						MapCopyAssignValueSum += Assigned.GetValue();
					}

					TMap<int, int> MutableValues;
					MutableValues.Add(1, 10);
					MutableValues.Add(2, 20);
					MutableValues.Add(3, 30);
					MutableValues.Add(4, 40);

					TMapIterator<int, int> Mutating = MutableValues.Iterator();
					while (Mutating.CanProceed)
					{
						Mutating.Proceed();
						MapMutationVisitedCount++;

						if (Mutating.GetKey() == 2 || Mutating.GetKey() == 4)
						{
							Mutating.RemoveCurrent();
						}
						else
						{
							int NewValue = Mutating.GetValue() + 100;
							Mutating.SetValue(NewValue);
						}
					}

					MapMutationRemainingCount = MutableValues.Num();

					int FoundValue = 0;
					if (MutableValues.Find(1, FoundValue))
					{
						MapMutationUpdatedValueSum += FoundValue;
					}

					if (MutableValues.Find(3, FoundValue))
					{
						MapMutationUpdatedValueSum += FoundValue;
					}

					if (MutableValues.Contains(2))
					{
						MapMutationRemovedKeyCount++;
					}

					if (MutableValues.Contains(4))
					{
						MapMutationRemovedKeyCount++;
					}
				}

				void ExerciseSetIterator()
				{
					TSet<int> Values;
					Values.Add(2);
					Values.Add(4);
					Values.Add(8);

					TSetIterator<int> Original = Values.Iterator();
					TSetIterator<int> Copied = Original;
					TSetIterator<int> Assigned = Values.Iterator();
					Assigned = Copied;

					while (Original.CanProceed)
					{
						SetCopyAssignSum += Original.Proceed();
						SetCopyAssignVisitCount++;
					}

					while (Copied.CanProceed)
					{
						SetCopyAssignSum += Copied.Proceed();
						SetCopyAssignVisitCount++;
					}

					while (Assigned.CanProceed)
					{
						SetCopyAssignSum += Assigned.Proceed();
						SetCopyAssignVisitCount++;
					}
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ExerciseArrayIterator();
					ExerciseMapIterator();
					ExerciseSetIterator();
				}
			}
			)AS"),
			TEXT("ACoverageContainerIteratorAdvancedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Advanced container iterator actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Advanced container iterator actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayCopyAssignSum"), 30, TEXT("TArray iterator copy and assignment should traverse the same source"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayMutableWriteSum"), 19, TEXT("TArray mutable iterator should write through Proceed reference"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapCopyAssignKeySum"), 18, TEXT("TMap iterator copy and assignment should preserve key traversal"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapCopyAssignValueSum"), 180, TEXT("TMap iterator copy and assignment should preserve value traversal"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapMutationVisitedCount"), 4, TEXT("TMap mutating iterator should visit all original entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapMutationRemainingCount"), 2, TEXT("TMap RemoveCurrent should remove the selected entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapMutationUpdatedValueSum"), 240, TEXT("TMap SetValue should update retained entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapMutationRemovedKeyCount"), 0, TEXT("TMap removed keys should no longer be present"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetCopyAssignSum"), 42, TEXT("TSet iterator copy and assignment should traverse the same source"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetCopyAssignVisitCount"), 9, TEXT("TSet copied and assigned iterators should each visit all values"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Non-UPROPERTY container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LocalArrayResult"), 3, TEXT("Local array should have 3 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TempMapResult"), 2, TEXT("Temp map should have 2 entries"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
