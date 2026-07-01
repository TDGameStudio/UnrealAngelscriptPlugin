#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageHandleTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UObject Handle (basic object references).
// This file covers the "UObject Handle" section of the coverage matrix:
//
//   OpenSpec: test-coverage/coverage-matrix.md - Sub-matrix 1
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
// Detailed coverage matrix: OpenSpec: test-coverage/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TestPassed"), true, TEXT("Handle basics test should pass"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NullCheckResult"), 3, TEXT("Null check should complete all stages"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IsValidResult"), 2, TEXT("IsValid check should complete both stages"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-comparison actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SameRefEqual"), true, TEXT("Same reference handles should be equal"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullComparison"), true, TEXT("Null comparison should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DifferentRefNotEqual"), true, TEXT("Different references should not be equal"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-cast actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToBaseSucceeded"), true, TEXT("Cast to base class should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToDerivedSucceeded"), true, TEXT("Cast back to derived should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToUnrelatedFailed"), true, TEXT("Cast to unrelated type should fail"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Check that properties exist with correct types
		const FProperty* TargetActorProp = ScriptClass->FindPropertyByName(FName(TEXT("TargetActor")));
		ASSERT_THAT(IsNotNull(TargetActorProp, TEXT("TargetActor property should exist")));
		if (TargetActorProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(TargetActorProp->IsA<FObjectProperty>(), TEXT("TargetActor should be FObjectProperty")));

		const FProperty* TargetPawnProp = ScriptClass->FindPropertyByName(FName(TEXT("TargetPawn")));
		ASSERT_THAT(IsNotNull(TargetPawnProp, TEXT("TargetPawn property should exist")));
		if (TargetPawnProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(TargetPawnProp->IsA<FObjectProperty>(), TEXT("TargetPawn should be FObjectProperty")));

		// Check specifiers are applied
		ASSERT_THAT(IsTrue(TargetActorProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit")));
		ASSERT_THAT(IsTrue(TargetPawnProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesAssigned"), true, TEXT("Handle properties should be assignable"))));
	}

	// -------------------------------------------------------------------------
	// Member references: UObject, Actor, and Component UPROPERTY handles
	// -------------------------------------------------------------------------
	TEST_METHOD(MemberObjectActorComponentReferences)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_MemberReferences"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleMemberReferences.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleMemberReferenceActor : AActor
			{
				UPROPERTY()
				UObject MemberObject;

				UPROPERTY()
				AActor MemberActor;

				UPROPERTY()
				UActorComponent MemberComponent;

				UPROPERTY()
				bool ObjectReferenceWorked = false;

				UPROPERTY()
				bool ActorReferenceWorked = false;

				UPROPERTY()
				bool ComponentReferenceWorked = false;

				UPROPERTY()
				bool ComponentOwnerWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MemberObject = NewObject(this, UTexture2D::StaticClass(), n"CoverageMemberObject");
					MemberActor = SpawnActor(AActor::StaticClass());
					MemberComponent = CreateComponent(USceneComponent::StaticClass(), n"CoverageMemberComponent");

					ObjectReferenceWorked = MemberObject != nullptr && MemberObject.GetOuter() == this;
					ActorReferenceWorked = MemberActor != nullptr && MemberActor != this;
					ComponentReferenceWorked = MemberComponent != nullptr && MemberComponent.GetName() == n"CoverageMemberComponent";
					ComponentOwnerWorked = MemberComponent != nullptr && MemberComponent.GetOwner() == this;
				}
			}
			)AS"),
			TEXT("ACoverageHandleMemberReferenceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Member-reference actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* MemberObjectProp = ScriptClass->FindPropertyByName(FName(TEXT("MemberObject")));
		ASSERT_THAT(IsNotNull(MemberObjectProp, TEXT("MemberObject property should exist")));
		if (MemberObjectProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MemberObjectProp->IsA<FObjectProperty>(), TEXT("MemberObject should be an object property")));

		const FProperty* MemberActorProp = ScriptClass->FindPropertyByName(FName(TEXT("MemberActor")));
		ASSERT_THAT(IsNotNull(MemberActorProp, TEXT("MemberActor property should exist")));
		if (MemberActorProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MemberActorProp->IsA<FObjectProperty>(), TEXT("MemberActor should be an object property")));

		const FProperty* MemberComponentProp = ScriptClass->FindPropertyByName(FName(TEXT("MemberComponent")));
		ASSERT_THAT(IsNotNull(MemberComponentProp, TEXT("MemberComponent property should exist")));
		if (MemberComponentProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MemberComponentProp->IsA<FObjectProperty>(), TEXT("MemberComponent should be an object property")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Member-reference actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectReferenceWorked"), true, TEXT("UObject member reference should hold a created object"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorReferenceWorked"), true, TEXT("Actor member reference should hold a spawned actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentReferenceWorked"), true, TEXT("Component member reference should hold a created component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentOwnerWorked"), true, TEXT("Component member reference should keep actor ownership"))));

		UObject* MemberObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("MemberObject"), MemberObject), TEXT("MemberObject should be readable")));
		ASSERT_THAT(IsNotNull(MemberObject, TEXT("MemberObject should hold a UObject")));
		if (MemberObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), MemberObject->GetOuter(), TEXT("MemberObject should be outered to the actor")));

		UObject* MemberComponentObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("MemberComponent"), MemberComponentObject), TEXT("MemberComponent should be readable")));
		UActorComponent* MemberComponent = Cast<UActorComponent>(MemberComponentObject);
		ASSERT_THAT(IsNotNull(MemberComponent, TEXT("MemberComponent should hold a UActorComponent")));
		if (MemberComponent == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(Actor, MemberComponent->GetOwner(), TEXT("MemberComponent should be owned by the actor")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InputParamWorked"), true, TEXT("Handle as input parameter should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReturnValueWorked"), true, TEXT("Handle as return value should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OutParamWorked"), true, TEXT("Handle as out parameter should work"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// --- TArray<AActor> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ActorArray"), Count), TEXT("TArray<AActor> length should resolve")));
			ASSERT_THAT(AreEqual(4, Count, TEXT("TArray<AActor> should have 4 elements")));
		}

		// Verify first element is self (non-null handle)
		{
			const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Actor->GetClass()->FindPropertyByName(FName(TEXT("ActorArray"))));
			ASSERT_THAT(IsNotNull(ArrayProp, TEXT("ActorArray property should exist")));
			if (ArrayProp == nullptr)
			{
				return;
			}

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
					FString Name = GetName().ToString();
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Handle-operations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetClassWorked"), true, TEXT("GetClass should return valid UClass"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetNameWorked"), true, TEXT("GetName should return non-empty string"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsAWorked"), true, TEXT("IsA type check should work"))));

		// Verify ActorName is not empty
		FString ActorName;
		const FStrProperty* NameProp = CastField<FStrProperty>(ScriptClass->FindPropertyByName(FName(TEXT("ActorName"))));
		if (NameProp != nullptr)
		{
			ActorName = NameProp->GetPropertyValue_InContainer(Actor);
			ASSERT_THAT(IsFalse(ActorName.IsEmpty(), TEXT("Actor name should not be empty")));
		}
	}

	// -------------------------------------------------------------------------
	// UObject handle and NewObject: generic UObject creation and identity
	// -------------------------------------------------------------------------
	TEST_METHOD(UObjectHandleAndNewObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_UObjectNewObject"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleUObjectNewObject.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleUObjectNewObjectActor : AActor
			{
				UPROPERTY()
				bool UObjectDeclaredAndAssigned = false;

				UPROPERTY()
				bool NewObjectCreated = false;

				UPROPERTY()
				bool NewObjectOuterWorked = false;

				UPROPERTY()
				bool NewObjectNameWorked = false;

				UPROPERTY()
				bool NewObjectClassWorked = false;

				UPROPERTY()
				UObject GenericObject;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject EmptyObject;
					if (EmptyObject != nullptr)
					{
						return;
					}

					GenericObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"CoverageUObjectHandleTexture");
					if (GenericObject == nullptr)
					{
						return;
					}

					UObjectDeclaredAndAssigned = true;
					NewObjectCreated = IsValid(GenericObject);
					NewObjectOuterWorked = GenericObject.GetOuter() == GetTransientPackage();
					NewObjectNameWorked = GenericObject.GetName() == n"CoverageUObjectHandleTexture";
					NewObjectClassWorked = GenericObject.IsA(UTexture2D::StaticClass());
				}
			}
			)AS"),
			TEXT("ACoverageHandleUObjectNewObjectActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UObject/NewObject actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UObject/NewObject actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("UObjectDeclaredAndAssigned"), true, TEXT("UObject handle declaration and assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectCreated"), true, TEXT("NewObject should create a valid UObject"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectOuterWorked"), true, TEXT("NewObject should honor the supplied Outer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectNameWorked"), true, TEXT("NewObject should honor the supplied name"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectClassWorked"), true, TEXT("NewObject should create the requested class"))));

		UObject* GenericObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("GenericObject"), GenericObject), TEXT("GenericObject should be readable as reflected UObject")));
		ASSERT_THAT(IsNotNull(GenericObject, TEXT("GenericObject should hold the AS-created UObject")));
		if (GenericObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UTexture2D::StaticClass(), GenericObject->GetClass(), TEXT("C++ should observe the AS-created UObject class")));
	}

	// -------------------------------------------------------------------------
	// UObject handle assignment: generic UObject identity with actor Outer
	// -------------------------------------------------------------------------
	TEST_METHOD(UObjectHandleAssignmentAndActorOuter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_UObjectAssignmentActorOuter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleUObjectAssignmentActorOuter.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleUObjectAssignmentActorOuter : AActor
			{
				UPROPERTY()
				UObject OuterOwnedObject;

				UPROPERTY()
				bool NullAssignmentWorked = false;

				UPROPERTY()
				bool AssignmentEqualityWorked = false;

				UPROPERTY()
				bool ReassignmentWorked = false;

				UPROPERTY()
				bool ActorOuterWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject ObjectA = NewObject(this, UTexture2D::StaticClass(), n"CoverageHandleObjectA");
					UObject ObjectB = NewObject(this, UTexture2D::StaticClass(), n"CoverageHandleObjectB");

					UObject GenericObject = nullptr;
					NullAssignmentWorked = GenericObject == nullptr;

					GenericObject = ObjectA;
					AssignmentEqualityWorked = GenericObject == ObjectA && GenericObject != ObjectB;

					OuterOwnedObject = ObjectB;
					ActorOuterWorked = OuterOwnedObject != nullptr && OuterOwnedObject.GetOuter() == this;

					GenericObject = OuterOwnedObject;
					ReassignmentWorked = GenericObject == ObjectB && GenericObject != ObjectA;
				}
			}
			)AS"),
			TEXT("ACoverageHandleUObjectAssignmentActorOuter"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UObject assignment actor-outer class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UObject assignment actor-outer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullAssignmentWorked"), true, TEXT("UObject null assignment should compare as null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentEqualityWorked"), true, TEXT("UObject assignment should preserve identity equality"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReassignmentWorked"), true, TEXT("UObject reassignment should update identity equality"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorOuterWorked"), true, TEXT("NewObject should accept an actor Outer"))));

		UObject* OuterOwnedObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("OuterOwnedObject"), OuterOwnedObject), TEXT("OuterOwnedObject should be readable")));
		ASSERT_THAT(IsNotNull(OuterOwnedObject, TEXT("OuterOwnedObject should hold the actor-owned UObject")));
		if (OuterOwnedObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), OuterOwnedObject->GetOuter(), TEXT("C++ should observe the actor Outer on NewObject result")));
	}

	// -------------------------------------------------------------------------
	// Actor destroy: DestroyActor invalidates IsValid on the next frame
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleDestroyActorInvalidatesReference)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_DestroyActor"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleDestroyActor.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleDestroyActor : AActor
			{
				UPROPERTY()
				AActor Victim;

				UPROPERTY()
				bool DestroyCalled = false;

				UPROPERTY()
				bool InvalidAfterDestroy = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Victim = SpawnActor(AActor::StaticClass());
					if (Victim != nullptr)
					{
						Victim.DestroyActor();
						DestroyCalled = true;
					}
					InvalidAfterDestroy = !IsValid(Victim);
				}
			}
			)AS"),
			TEXT("ACoverageHandleDestroyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DestroyActor actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DestroyActor actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 2);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DestroyCalled"), true, TEXT("DestroyActor should be callable from AS"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InvalidAfterDestroy"), true, TEXT("Destroyed actor handle should become invalid"))));
	}

	// -------------------------------------------------------------------------
	// TObjectPtr: declaration, raw-handle parity, implicit conversion
	// -------------------------------------------------------------------------
	TEST_METHOD(TObjectPtrRouting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_TObjectPtrRouting"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleTObjectPtrRouting.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleTObjectPtrActor : AActor
			{
				UPROPERTY()
				TObjectPtr<AActor> StoredActor;

				UPROPERTY()
				bool DefaultNullWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool GetMatchedRawHandle = false;

				UPROPERTY()
				bool ImplicitConversionWorked = false;

				UPROPERTY()
				bool CopyComparisonWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TObjectPtr<AActor> Empty;
					DefaultNullWorked = Empty.Get() == nullptr;

					AActor SpawnedActor = SpawnActor(AActor::StaticClass());
					StoredActor = SpawnedActor;

					AssignmentWorked = StoredActor == SpawnedActor;
					GetMatchedRawHandle = StoredActor.Get() == SpawnedActor;

					AActor RawActor = StoredActor;
					ImplicitConversionWorked = RawActor == SpawnedActor;

					TObjectPtr<AActor> CopiedActor = StoredActor;
					CopyComparisonWorked = CopiedActor == StoredActor;
				}
			}
			)AS"),
			TEXT("ACoverageHandleTObjectPtrActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TObjectPtr actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* StoredActorProp = ScriptClass->FindPropertyByName(FName(TEXT("StoredActor")));
		ASSERT_THAT(IsNotNull(StoredActorProp, TEXT("StoredActor property should exist")));
		if (StoredActorProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StoredActorProp->IsA<FObjectProperty>(), TEXT("TObjectPtr should be emitted as an object property")));
		ASSERT_THAT(IsTrue(StoredActorProp->HasAnyPropertyFlags(CPF_TObjectPtr), TEXT("TObjectPtr property should keep CPF_TObjectPtr routing flag")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TObjectPtr actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultNullWorked"), true, TEXT("Default TObjectPtr should be null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TObjectPtr assignment should compare with raw handles"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetMatchedRawHandle"), true, TEXT("TObjectPtr.Get should match the assigned raw handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImplicitConversionWorked"), true, TEXT("TObjectPtr should implicitly convert to raw handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CopyComparisonWorked"), true, TEXT("TObjectPtr copy should compare equal"))));
	}

	// -------------------------------------------------------------------------
	// TSubclassOf as function parameter and NewObject class argument
	// -------------------------------------------------------------------------
	TEST_METHOD(TSubclassOfParameterAndNewObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_TSubclassOfParameterNewObject"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleTSubclassOfParameterNewObject.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleSubclassParameterActor : AActor
			{
				UPROPERTY()
				bool ParameterAcceptedClass = false;

				UPROPERTY()
				bool NewObjectFromClassWorked = false;

				UPROPERTY()
				bool NewObjectFromTSubclassOfWorked = false;

				void AcceptActorClass(TSubclassOf<AActor> InClass)
				{
					ParameterAcceptedClass = InClass != nullptr && InClass.IsChildOf(AActor::StaticClass());
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TSubclassOf<AActor> ActorClass = AActor::StaticClass();
					AcceptActorClass(ActorClass);

					TSubclassOf<UObject> TextureClass = UTexture2D::StaticClass();
					UObject FromClass = NewObject(GetTransientPackage(), TextureClass.Get(), n"CoverageSubclassNewObjectClass");
					NewObjectFromClassWorked = FromClass != nullptr && FromClass.IsA(UTexture2D::StaticClass());

					UObject FromSubclassOf = NewObject(GetTransientPackage(), TextureClass, n"CoverageSubclassNewObjectSubclass");
					NewObjectFromTSubclassOfWorked = FromSubclassOf != nullptr && FromSubclassOf.IsA(UTexture2D::StaticClass());
				}
			}
			)AS"),
			TEXT("ACoverageHandleSubclassParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSubclassOf parameter actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSubclassOf parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ParameterAcceptedClass"), true, TEXT("TSubclassOf should pass through function parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectFromClassWorked"), true, TEXT("NewObject should accept the UClass returned by TSubclassOf.Get"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectFromTSubclassOfWorked"), true, TEXT("NewObject should accept TSubclassOf directly as a class argument"))));
	}

	// -------------------------------------------------------------------------
	// Handle containers: TSet<AActor> pointer hashing
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleSetContainer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandle_TSet"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandleTSet.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandleSetActor : AActor
			{
				UPROPERTY()
				TSet<AActor> ActorSet;

				UPROPERTY()
				bool AddDedupWorked = false;

				UPROPERTY()
				bool ContainsWorked = false;

				UPROPERTY()
				bool RemoveWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor OtherActor = SpawnActor(AActor::StaticClass());

					ActorSet.Add(this);
					ActorSet.Add(this);
					ActorSet.Add(OtherActor);

					AddDedupWorked = ActorSet.Num() == 2;
					ContainsWorked = ActorSet.Contains(this) && ActorSet.Contains(OtherActor);

					ActorSet.Remove(this);
					RemoveWorked = !ActorSet.Contains(this) && ActorSet.Num() == 1;
				}
			}
			)AS"),
			TEXT("ACoverageHandleSetActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSet<AActor> actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSet<AActor> actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AddDedupWorked"), true, TEXT("TSet<AActor> should deduplicate pointer handles"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ContainsWorked"), true, TEXT("TSet<AActor> should find pointer handles"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RemoveWorked"), true, TEXT("TSet<AActor> should remove pointer handles"))));
	}
};

#endif
