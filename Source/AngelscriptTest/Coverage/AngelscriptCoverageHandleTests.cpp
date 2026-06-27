#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageHandleTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UObject Handle (basic object references).
// This file covers the "UObject Handle" section of the coverage matrix:
//
//   Documents/Coverage/Coverage_HandlesAndReferences.md - Sub-matrix 1
//
// Axes covered here:
//   * HandleBasics           - declaration, null checks, IsValid, assignment
//   * HandleComparison       - == and != operator tests
//   * HandleCast             - Cast<T> downcast operations
//   * HandleAsProperty       - UObject handles as UPROPERTY members
//   * HandleAsParameter      - handles as function parameters and return values
//   * HandleInContainers     - TArray<AActor>, TMap with handle values
//   * HandleOperations       - GetClass, GetName, type checks
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_HandlesAndReferences.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageHandleTest,
	"Angelscript.TestModule.Coverage.Handle",
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
	// Handle basics: declaration, null assignment, null checks, IsValid checks
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleBasicsActor : AActor
			{
				UPROPERTY()
				bool TestPassed = false;

				UPROPERTY()
				AActor TargetActor;

				UPROPERTY()
				int NullCheckResult = 0;

				UPROPERTY()
				int IsValidResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test null by default
					if (TargetActor == nullptr)
					{
						NullCheckResult = 1;
					}

					// Test IsValid with nullptr
					if (!IsValid(TargetActor))
					{
						IsValidResult = 1;
					}

					// Assign self
					TargetActor = this;

					// Test non-null after assignment
					if (TargetActor != nullptr)
					{
						NullCheckResult = 2;
					}

					// Test IsValid with valid object
					if (IsValid(TargetActor))
					{
						IsValidResult = 2;
					}

					// Assign back to null
					TargetActor = nullptr;

					// Verify null again
					if (TargetActor == nullptr)
					{
						NullCheckResult = 3;
					}

					TestPassed = (NullCheckResult == 3 && IsValidResult == 2);
				}
			}
			)AS"),
			TEXT("ACoverageHandleBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-basics actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TestPassed"), true, TEXT("Handle basics test should pass"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NullCheckResult"), 3, TEXT("Null check should complete all stages"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IsValidResult"), 2, TEXT("IsValid check should complete both stages"));
	}

	// -------------------------------------------------------------------------
	// Handle comparison: == and != operators
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleComparison)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Comparison"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleComparison.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleComparisonActor : AActor
			{
				UPROPERTY()
				bool SameRefEqual = false;

				UPROPERTY()
				bool NullComparison = false;

				UPROPERTY()
				bool DifferentRefNotEqual = false;

				UPROPERTY()
				AActor OtherActor;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor Handle1 = this;
					AActor Handle2 = this;

					// Same object should be equal
					if (Handle1 == Handle2)
					{
						SameRefEqual = true;
					}

					// Null comparisons
					AActor NullHandle = nullptr;
					if (NullHandle == nullptr && this != nullptr)
					{
						NullComparison = true;
					}

					// Different objects should not be equal
					OtherActor = SpawnActor(AActor::StaticClass());
					if (this != OtherActor && OtherActor != nullptr)
					{
						DifferentRefNotEqual = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandleComparisonActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-comparison actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-comparison actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SameRefEqual"), true, TEXT("Same reference handles should be equal"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullComparison"), true, TEXT("Null comparison should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DifferentRefNotEqual"), true, TEXT("Different references should not be equal"));
	}

	// -------------------------------------------------------------------------
	// Handle Cast: Cast<T> downcast operations
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleCast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Cast"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleCast.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleCastActor : APawn
			{
				UPROPERTY()
				bool CastToBaseSucceeded = false;

				UPROPERTY()
				bool CastToDerivedSucceeded = false;

				UPROPERTY()
				bool CastToUnrelatedFailed = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Cast derived to base (always succeeds)
					AActor ActorRef = Cast<AActor>(this);
					if (ActorRef != nullptr)
					{
						CastToBaseSucceeded = true;
					}

					// Cast base back to derived (should succeed since it's actually a Pawn)
					APawn PawnRef = Cast<APawn>(ActorRef);
					if (PawnRef != nullptr)
					{
						CastToDerivedSucceeded = true;
					}

					// Cast to unrelated type (should fail)
					APlayerController ControllerRef = Cast<APlayerController>(this);
					if (ControllerRef == nullptr)
					{
						CastToUnrelatedFailed = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandleCastActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-cast actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-cast actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToBaseSucceeded"), true, TEXT("Cast to base class should succeed"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToDerivedSucceeded"), true, TEXT("Cast back to derived should succeed"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToUnrelatedFailed"), true, TEXT("Cast to unrelated type should fail"));
	}

	// -------------------------------------------------------------------------
	// Handle as UPROPERTY: strong reference as property member
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleAsProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Property"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleProperty.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlePropertyActor : AActor
			{
				UPROPERTY(EditAnywhere)
				AActor TargetActor;

				UPROPERTY(BlueprintReadWrite)
				APawn TargetPawn;

				UPROPERTY(Category="Refs")
				UActorComponent TargetComponent;

				UPROPERTY()
				bool PropertiesAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TargetActor = this;
					TargetPawn = Cast<APawn>(SpawnActor(APawn::StaticClass()));

					if (TargetActor != nullptr && TargetPawn != nullptr)
					{
						PropertiesAssigned = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandlePropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-property actor class should compile")));

		// Check that properties exist with correct types
		const FProperty* TargetActorProp = ScriptClass->FindPropertyByName(FName(TEXT("TargetActor")));
		ASSERT_THAT(IsNotNull(TargetActorProp, TEXT("TargetActor property should exist")));
		ASSERT_THAT(IsTrue(TargetActorProp->IsA<FObjectProperty>(), TEXT("TargetActor should be FObjectProperty")));

		const FProperty* TargetPawnProp = ScriptClass->FindPropertyByName(FName(TEXT("TargetPawn")));
		ASSERT_THAT(IsNotNull(TargetPawnProp, TEXT("TargetPawn property should exist")));
		ASSERT_THAT(IsTrue(TargetPawnProp->IsA<FObjectProperty>(), TEXT("TargetPawn should be FObjectProperty")));

		// Check specifiers are applied
		ASSERT_THAT(IsTrue(TargetActorProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit")));
		ASSERT_THAT(IsTrue(TargetPawnProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-property actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesAssigned"), true, TEXT("Handle properties should be assignable"));
	}

	// -------------------------------------------------------------------------
	// Handle as function parameter and return value
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Parameter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleParameter.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleParameterActor : AActor
			{
				UPROPERTY()
				bool InputParamWorked = false;

				UPROPERTY()
				bool ReturnValueWorked = false;

				UPROPERTY()
				bool OutParamWorked = false;

				// Function taking handle as input parameter
				void ProcessActor(AActor InActor)
				{
					if (InActor != nullptr && InActor == this)
					{
						InputParamWorked = true;
					}
				}

				// Function returning handle
				AActor GetSelf()
				{
					return this;
				}

				// Function with out parameter
				void GetActorOut(AActor&out OutActor)
				{
					OutActor = this;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test input parameter
					ProcessActor(this);

					// Test return value
					AActor Returned = GetSelf();
					if (Returned == this)
					{
						ReturnValueWorked = true;
					}

					// Test out parameter
					AActor OutResult;
					GetActorOut(OutResult);
					if (OutResult == this)
					{
						OutParamWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandleParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-parameter actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-parameter actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InputParamWorked"), true, TEXT("Handle as input parameter should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReturnValueWorked"), true, TEXT("Handle as return value should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OutParamWorked"), true, TEXT("Handle as out parameter should work"));
	}

	// -------------------------------------------------------------------------
	// Handles in containers: TArray<AActor>, TMap with handle values
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleInContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Containers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleContainers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleContainerActor : AActor
			{
				UPROPERTY()
				TArray<AActor> ActorArray;

				UPROPERTY()
				TMap<int, AActor> IntToActorMap;

				UPROPERTY()
				TMap<FString, AActor> StringToActorMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Populate TArray with handles
					ActorArray.Add(this);
					ActorArray.Add(SpawnActor(AActor::StaticClass()));
					ActorArray.Add(nullptr);
					ActorArray.Add(SpawnActor(AActor::StaticClass()));

					// Populate TMap<int, AActor>
					IntToActorMap.Add(1, this);
					IntToActorMap.Add(2, ActorArray[1]);
					IntToActorMap.Add(3, nullptr);

					// Populate TMap<FString, AActor>
					StringToActorMap.Add("Self", this);
					StringToActorMap.Add("Other", ActorArray[1]);
				}
			}
			)AS"),
			TEXT("ACoverageHandleContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// --- TArray<AActor> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ActorArray"), Count), TEXT("TArray<AActor> length should resolve")));
			ASSERT_THAT(AreEqual(4, Count, TEXT("TArray<AActor> should have 4 elements")));
		}

		// Verify first element is self (non-null handle)
		{
			const FObjectProperty* Prop = CastField<FObjectProperty>(Actor->GetClass()->FindPropertyByName(FName(TEXT("ActorArray"))));
			ASSERT_THAT(IsNotNull(Prop, TEXT("ActorArray property should exist")));

			const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop->Owner.ToField());
			if (ArrayProp != nullptr)
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Actor));
				if (ArrayHelper.Num() > 0)
				{
					const FObjectProperty* InnerProp = CastField<FObjectProperty>(ArrayProp->Inner);
					if (InnerProp != nullptr)
					{
						UObject* Element0 = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(0));
						ASSERT_THAT(AreEqual(Actor, Element0, TEXT("TArray<AActor>[0] should be self")));
					}
				}
			}
		}

		// --- TMap<int, AActor> ---
		{
			int32 MapCount = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToActorMap"), MapCount), TEXT("TMap<int,AActor> length should resolve")));
			ASSERT_THAT(AreEqual(3, MapCount, TEXT("TMap<int,AActor> should have 3 entries")));
		}

		// --- TMap<FString, AActor> ---
		{
			int32 MapCount = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToActorMap"), MapCount), TEXT("TMap<FString,AActor> length should resolve")));
			ASSERT_THAT(AreEqual(2, MapCount, TEXT("TMap<FString,AActor> should have 2 entries")));
		}
	}

	// -------------------------------------------------------------------------
	// Handle operations: GetClass, GetName, type information
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_Operations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleOperations.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleOperationsActor : AActor
			{
				UPROPERTY()
				bool GetClassWorked = false;

				UPROPERTY()
				bool GetNameWorked = false;

				UPROPERTY()
				bool IsAWorked = false;

				UPROPERTY()
				FString ActorName;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test GetClass
					UClass MyClass = GetClass();
					if (MyClass != nullptr)
					{
						GetClassWorked = true;
					}

					// Test GetName
					FString Name = GetName();
					if (!Name.IsEmpty())
					{
						GetNameWorked = true;
						ActorName = Name;
					}

					// Test IsA check
					AActor ActorRef = this;
					if (ActorRef.IsA(AActor::StaticClass()))
					{
						IsAWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandleOperationsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Handle-operations actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-operations actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetClassWorked"), true, TEXT("GetClass should return valid UClass"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetNameWorked"), true, TEXT("GetName should return non-empty string"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsAWorked"), true, TEXT("IsA type check should work"));

		// Verify ActorName is not empty
		FString ActorName;
		const FStrProperty* NameProp = CastField<FStrProperty>(ScriptClass->FindPropertyByName(FName(TEXT("ActorName"))));
		if (NameProp != nullptr)
		{
			ActorName = NameProp->GetPropertyValue_InContainer(Actor);
			ASSERT_THAT(IsFalse(ActorName.IsEmpty(), TEXT("Actor name should not be empty")));
		}
	}
};

#endif
