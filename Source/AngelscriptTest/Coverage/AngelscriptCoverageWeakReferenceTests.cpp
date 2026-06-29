#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageWeakReferenceTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript weak references and class references.
// This file covers weak reference and class reference sections from:
//
//   OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md - Sub-matrix 3 & 6
//
// Axes covered here:
//   * WeakObjectPtrBasics          - declaration, assignment, IsValid, Get
//   * WeakObjectPtrInvalidation    - object destruction -> weak ptr becomes invalid
//   * WeakObjectPtrAsProperty      - TWeakObjectPtr as UPROPERTY member
//   * TSubclassOfBasics            - declaration, assignment, Get, null checks
//   * TSubclassOfAsProperty        - TSubclassOf as UPROPERTY (class selector)
//   * TSubclassOfSpawn             - using TSubclassOf with SpawnActor
//   * TSubclassOfTypeCheck         - IsChildOf validation
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageWeakReferenceTest,
	"Angelscript.TestModule.Coverage.WeakReference",
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
	// TWeakObjectPtr basics: declaration, assignment, IsValid, Get
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWeakRef_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWeakRefBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageWeakRefBasicsActor : AActor
			{
				UPROPERTY()
				bool DeclarationWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool IsValidWorked = false;

				UPROPERTY()
				bool GetWorked = false;

				UPROPERTY()
				bool ResetWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test declaration
					TWeakObjectPtr<AActor> WeakActor;
					DeclarationWorked = true;

					// Test null by default
					if (!WeakActor.IsValid())
					{
						IsValidWorked = true;
					}

					// Test assignment from strong reference
					WeakActor = this;
					if (WeakActor.IsValid())
					{
						AssignmentWorked = true;
					}

					// Test Get() method
					AActor Retrieved = WeakActor.Get();
					if (Retrieved == this)
					{
						GetWorked = true;
					}

					// Test Reset()
					WeakActor.Reset();
					if (!WeakActor.IsValid())
					{
						ResetWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageWeakRefBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak-ref-basics actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TWeakObjectPtr declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TWeakObjectPtr assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidWorked"), true, TEXT("TWeakObjectPtr IsValid should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TWeakObjectPtr Get should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResetWorked"), true, TEXT("TWeakObjectPtr Reset should work"))));
	}

	// -------------------------------------------------------------------------
	// TWeakObjectPtr invalidation: object destruction causes weak ref to fail
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrInvalidation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWeakRef_Invalidation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWeakRefInvalidation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageWeakRefInvalidationActor : AActor
			{
				UPROPERTY()
				TWeakObjectPtr<AActor> WeakTarget;

				UPROPERTY()
				bool InitiallyValid = false;

				UPROPERTY()
				bool InvalidAfterDestroy = false;

				UPROPERTY()
				bool GetReturnsNull = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Spawn a temporary actor
					AActor TempActor = SpawnActor(AActor::StaticClass());

					// Assign to weak pointer
					WeakTarget = TempActor;

					// Check initially valid
					if (WeakTarget.IsValid())
					{
						InitiallyValid = true;
					}

					// Destroy the actor
					TempActor.DestroyActor();

					// Check that weak pointer is now invalid
					if (!WeakTarget.IsValid())
					{
						InvalidAfterDestroy = true;
					}

					// Check that Get returns null
					AActor Retrieved = WeakTarget.Get();
					if (Retrieved == nullptr)
					{
						GetReturnsNull = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageWeakRefInvalidationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak-ref-invalidation actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref-invalidation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Tick to allow destruction and deferred check
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 2);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyValid"), true, TEXT("Weak pointer should be initially valid"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InvalidAfterDestroy"), true, TEXT("Weak pointer should be invalid after object destruction"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetReturnsNull"), true, TEXT("Get should return null after destruction"))));
	}

	// -------------------------------------------------------------------------
	// TWeakObjectPtr as UPROPERTY member
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrAsProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWeakRef_Property"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWeakRefProperty.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageWeakRefPropertyActor : AActor
			{
				UPROPERTY(EditAnywhere)
				TWeakObjectPtr<AActor> WeakTarget;

				UPROPERTY(BlueprintReadWrite)
				TWeakObjectPtr<APawn> WeakPawn;

				UPROPERTY()
				bool PropertiesAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					WeakTarget = this;
					WeakPawn = Cast<APawn>(SpawnActor(APawn::StaticClass()));

					if (WeakTarget.IsValid() && WeakPawn.IsValid())
					{
						PropertiesAssigned = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageWeakRefPropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak-ref-property actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Check that properties exist
		const FProperty* WeakTargetProp = ScriptClass->FindPropertyByName(FName(TEXT("WeakTarget")));
		ASSERT_THAT(IsNotNull(WeakTargetProp, TEXT("WeakTarget property should exist")));
		if (WeakTargetProp == nullptr)
		{
			return;
		}

		const FProperty* WeakPawnProp = ScriptClass->FindPropertyByName(FName(TEXT("WeakPawn")));
		ASSERT_THAT(IsNotNull(WeakPawnProp, TEXT("WeakPawn property should exist")));
		if (WeakPawnProp == nullptr)
		{
			return;
		}

		// Check specifiers are applied
		ASSERT_THAT(IsTrue(WeakTargetProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit on weak ref")));
		ASSERT_THAT(IsTrue(WeakPawnProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible on weak ref")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref-property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesAssigned"), true, TEXT("Weak pointer properties should be assignable"))));
	}

	// -------------------------------------------------------------------------
	// TSubclassOf basics: declaration, assignment, Get, null checks
	// -------------------------------------------------------------------------
	TEST_METHOD(TSubclassOfBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSubclassOf_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSubclassOfBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSubclassOfBasicsActor : AActor
			{
				UPROPERTY()
				bool DeclarationWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool GetWorked = false;

				UPROPERTY()
				bool NullCheckWorked = false;

				UPROPERTY()
				bool ComparisonWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test declaration
					TSubclassOf<AActor> ActorClass;
					DeclarationWorked = true;

					// Test null by default
					if (ActorClass == nullptr)
					{
						NullCheckWorked = true;
					}

					// Test assignment
					ActorClass = AActor::StaticClass();
					if (ActorClass != nullptr)
					{
						AssignmentWorked = true;
					}

					// Test Get() method
					UClass ClassRef = ActorClass.Get();
					if (ClassRef != nullptr)
					{
						GetWorked = true;
					}

					// Test comparison
					TSubclassOf<AActor> SameClass = AActor::StaticClass();
					if (ActorClass == SameClass)
					{
						ComparisonWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSubclassOfBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Subclass-of-basics actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSubclassOf declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSubclassOf assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSubclassOf Get should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullCheckWorked"), true, TEXT("TSubclassOf null check should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComparisonWorked"), true, TEXT("TSubclassOf comparison should work"))));
	}

	// -------------------------------------------------------------------------
	// TSubclassOf as UPROPERTY: class selector in editor
	// -------------------------------------------------------------------------
	TEST_METHOD(TSubclassOfAsProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSubclassOf_Property"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSubclassOfProperty.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSubclassOfPropertyActor : AActor
			{
				UPROPERTY(EditDefaultsOnly)
				TSubclassOf<AActor> ActorClass;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				TSubclassOf<APawn> PawnClass;

				UPROPERTY(Category="Classes")
				TSubclassOf<AActor> CategoryClass;

				UPROPERTY()
				bool PropertiesSet = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ActorClass = AActor::StaticClass();
					PawnClass = APawn::StaticClass();
					CategoryClass = AActor::StaticClass();

					if (ActorClass != nullptr && PawnClass != nullptr && CategoryClass != nullptr)
					{
						PropertiesSet = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSubclassOfPropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Subclass-of-property actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Check that properties exist with correct types
		const FProperty* ActorClassProp = ScriptClass->FindPropertyByName(FName(TEXT("ActorClass")));
		ASSERT_THAT(IsNotNull(ActorClassProp, TEXT("ActorClass property should exist")));
		if (ActorClassProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ActorClassProp->IsA<FClassProperty>(), TEXT("ActorClass should be FClassProperty")));

		const FProperty* PawnClassProp = ScriptClass->FindPropertyByName(FName(TEXT("PawnClass")));
		ASSERT_THAT(IsNotNull(PawnClassProp, TEXT("PawnClass property should exist")));
		if (PawnClassProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(PawnClassProp->IsA<FClassProperty>(), TEXT("PawnClass should be FClassProperty")));

		// Check specifiers
		ASSERT_THAT(IsTrue(ActorClassProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditDefaultsOnly should set CPF_Edit")));
		ASSERT_THAT(IsTrue(ActorClassProp->HasAnyPropertyFlags(CPF_DisableEditOnInstance), TEXT("EditDefaultsOnly should disable instance edit")));
		ASSERT_THAT(IsTrue(PawnClassProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesSet"), true, TEXT("TSubclassOf properties should be assignable"))));
	}

	// -------------------------------------------------------------------------
	// TSubclassOf with SpawnActor: using class reference to spawn instances
	// -------------------------------------------------------------------------
	TEST_METHOD(TSubclassOfSpawn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSubclassOf_Spawn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSubclassOfSpawn.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSubclassOfSpawnActor : AActor
			{
				UPROPERTY()
				TSubclassOf<AActor> ActorClassToSpawn;

				UPROPERTY()
				TSubclassOf<APawn> PawnClassToSpawn;

				UPROPERTY()
				bool ActorSpawnWorked = false;

				UPROPERTY()
				bool PawnSpawnWorked = false;

				UPROPERTY()
				AActor SpawnedActorRef;

				UPROPERTY()
				APawn SpawnedPawnRef;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set class references
					ActorClassToSpawn = AActor::StaticClass();
					PawnClassToSpawn = APawn::StaticClass();

					// Spawn using TSubclassOf
					if (ActorClassToSpawn != nullptr)
					{
						SpawnedActorRef = SpawnActor(ActorClassToSpawn);
						if (SpawnedActorRef != nullptr)
						{
							ActorSpawnWorked = true;
						}
					}

					if (PawnClassToSpawn != nullptr)
					{
						SpawnedPawnRef = Cast<APawn>(SpawnActor(PawnClassToSpawn));
						if (SpawnedPawnRef != nullptr)
						{
							PawnSpawnWorked = true;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageSubclassOfSpawnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Subclass-of-spawn actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-spawn actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorSpawnWorked"), true, TEXT("Spawn with TSubclassOf<AActor> should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PawnSpawnWorked"), true, TEXT("Spawn with TSubclassOf<APawn> should work"))));
	}

	// -------------------------------------------------------------------------
	// TSubclassOf type checking: IsChildOf validation
	// -------------------------------------------------------------------------
	TEST_METHOD(TSubclassOfTypeCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSubclassOf_TypeCheck"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSubclassOfTypeCheck.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSubclassOfTypeCheckActor : AActor
			{
				UPROPERTY()
				bool IsChildOfActorWorked = false;

				UPROPERTY()
				bool IsChildOfPawnWorked = false;

				UPROPERTY()
				bool PawnIsChildOfActorWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TSubclassOf<AActor> ActorClass = AActor::StaticClass();
					TSubclassOf<APawn> PawnClass = APawn::StaticClass();

					// Check if AActor is child of AActor (itself)
					UClass ActorClassRef = ActorClass.Get();
					if (ActorClassRef.IsChildOf(AActor::StaticClass()))
					{
						IsChildOfActorWorked = true;
					}

					// Check if APawn is child of APawn (itself)
					UClass PawnClassRef = PawnClass.Get();
					if (PawnClassRef.IsChildOf(APawn::StaticClass()))
					{
						IsChildOfPawnWorked = true;
					}

					// Check if APawn is child of AActor (inheritance)
					if (PawnClassRef.IsChildOf(AActor::StaticClass()))
					{
						PawnIsChildOfActorWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSubclassOfTypeCheckActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Subclass-of-typecheck actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-typecheck actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsChildOfActorWorked"), true, TEXT("AActor should be child of AActor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsChildOfPawnWorked"), true, TEXT("APawn should be child of APawn"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PawnIsChildOfActorWorked"), true, TEXT("APawn should be child of AActor"))));
	}

	// -------------------------------------------------------------------------
	// TWeakObjectPtr null comparison and reassignment
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrNullComparisonAndReassignment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWeakRef_NullReassign"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWeakRefNullReassign.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageWeakRefNullReassignActor : AActor
			{
				UPROPERTY()
				TWeakObjectPtr<AActor> WeakTarget;

				UPROPERTY()
				bool DefaultEqualsNull = false;

				UPROPERTY()
				bool AssignedNotNull = false;

				UPROPERTY()
				bool ResetEqualsNull = false;

				UPROPERTY()
				bool ReassignedToNewObject = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DefaultEqualsNull = WeakTarget == nullptr;

					AActor FirstActor = SpawnActor(AActor::StaticClass());
					WeakTarget = FirstActor;
					AssignedNotNull = WeakTarget != nullptr && WeakTarget.Get() == FirstActor;

					WeakTarget = nullptr;
					ResetEqualsNull = WeakTarget == nullptr && WeakTarget.Get() == nullptr;

					AActor SecondActor = SpawnActor(AActor::StaticClass());
					WeakTarget = SecondActor;
					ReassignedToNewObject = WeakTarget.IsValid() && WeakTarget.Get() == SecondActor && WeakTarget.Get() != FirstActor;
				}
			}
			)AS"),
			TEXT("ACoverageWeakRefNullReassignActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak-ref null/reassign actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref null/reassign actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultEqualsNull"), true, TEXT("Default TWeakObjectPtr should compare equal to nullptr"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignedNotNull"), true, TEXT("Assigned TWeakObjectPtr should compare not equal to nullptr"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResetEqualsNull"), true, TEXT("TWeakObjectPtr assigned nullptr should compare equal to nullptr"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReassignedToNewObject"), true, TEXT("TWeakObjectPtr should reassign to a new object"))));
	}

	// -------------------------------------------------------------------------
	// TWeakObjectPtr usage: break a strong cycle with a weak back-reference
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrBreaksBackReferenceCycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWeakRef_BreakCycle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWeakRefBreakCycle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageWeakRefCycleChild : AActor
			{
				UPROPERTY()
				TWeakObjectPtr<AActor> WeakParent;

				UFUNCTION()
				void SetParent(AActor Parent)
				{
					WeakParent = Parent;
				}

				UFUNCTION()
				bool HasWeakParent(AActor ExpectedParent)
				{
					return WeakParent.IsValid() && WeakParent.Get() == ExpectedParent;
				}
			}

			UCLASS()
			class ACoverageWeakRefBreakCycleActor : AActor
			{
				UPROPERTY()
				ACoverageWeakRefCycleChild StrongChild;

				UPROPERTY()
				TWeakObjectPtr<AActor> WeakParent;

				UPROPERTY()
				bool StrongForwardReferenceAlive = false;

				UPROPERTY()
				bool WeakBackReferenceDoesNotOwn = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StrongChild = Cast<ACoverageWeakRefCycleChild>(SpawnActor(ACoverageWeakRefCycleChild::StaticClass()));
					WeakParent = this;
					StrongChild.SetParent(this);

					TWeakObjectPtr<AActor> WeakChild = StrongChild;
					System::ForceGarbageCollection(true);

					StrongForwardReferenceAlive = WeakChild.IsValid() && IsValid(StrongChild);
					WeakBackReferenceDoesNotOwn = WeakParent.IsValid() && WeakParent.Get() == this && StrongChild.HasWeakParent(this);
				}
			}
			)AS"),
			TEXT("ACoverageWeakRefBreakCycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak-ref back-reference actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref back-reference actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StrongForwardReferenceAlive"), true, TEXT("Strong forward reference should keep child alive"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakBackReferenceDoesNotOwn"), true, TEXT("Weak back-reference should observe parent without owning it"))));
	}

	// -------------------------------------------------------------------------
	// TWeakObjectPtr as container element: TArray<TWeakObjectPtr<AActor>>
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrArrayContainer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWeakRef_ArrayContainer"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWeakRefArrayContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageWeakRefArrayContainerActor : AActor
			{
				UPROPERTY()
				TArray<TWeakObjectPtr<AActor>> WeakActors;

				UPROPERTY()
				bool ArrayStoredWeakRefs = false;

				UPROPERTY()
				bool ArrayNullElementWorked = false;

				UPROPERTY()
				bool ArrayInvalidatedDestroyedElement = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor FirstActor = SpawnActor(AActor::StaticClass());
					AActor SecondActor = SpawnActor(AActor::StaticClass());

					TWeakObjectPtr<AActor> WeakFirst = FirstActor;
					TWeakObjectPtr<AActor> WeakSecond = SecondActor;
					TWeakObjectPtr<AActor> EmptyWeak;

					WeakActors.Add(WeakFirst);
					WeakActors.Add(WeakSecond);
					WeakActors.Add(EmptyWeak);

					ArrayStoredWeakRefs = WeakActors.Num() == 3 && WeakActors[0].Get() == FirstActor && WeakActors[1].Get() == SecondActor;
					ArrayNullElementWorked = WeakActors[2] == nullptr;

					SecondActor.DestroyActor();
					ArrayInvalidatedDestroyedElement = !WeakActors[1].IsValid() && WeakActors[1].Get() == nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageWeakRefArrayContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak-ref array actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref array actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 2);

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("WeakActors"), Count), TEXT("TArray<TWeakObjectPtr<AActor>> length should resolve")));
		ASSERT_THAT(AreEqual(3, Count, TEXT("TArray<TWeakObjectPtr<AActor>> should hold three entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayStoredWeakRefs"), true, TEXT("TArray<TWeakObjectPtr<AActor>> should store weak references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayNullElementWorked"), true, TEXT("TArray<TWeakObjectPtr<AActor>> should store null weak references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayInvalidatedDestroyedElement"), true, TEXT("TArray<TWeakObjectPtr<AActor>> should reflect destroyed element invalidation"))));
	}
};

#endif
