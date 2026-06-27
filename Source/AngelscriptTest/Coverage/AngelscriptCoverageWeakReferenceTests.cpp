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
//   Documents/Coverage/Coverage_HandlesAndReferences.md - Sub-matrix 3 & 6
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
// Detailed coverage matrix: Documents/Coverage/Coverage_HandlesAndReferences.md
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TWeakObjectPtr declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TWeakObjectPtr assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidWorked"), true, TEXT("TWeakObjectPtr IsValid should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TWeakObjectPtr Get should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResetWorked"), true, TEXT("TWeakObjectPtr Reset should work"));
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

					// Tick world to process destroy
					System::ExecuteForOneFrame(CheckInvalidation);
				}

				void CheckInvalidation()
				{
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref-invalidation actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Tick to allow destruction and deferred check
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 2);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyValid"), true, TEXT("Weak pointer should be initially valid"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InvalidAfterDestroy"), true, TEXT("Weak pointer should be invalid after object destruction"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetReturnsNull"), true, TEXT("Get should return null after destruction"));
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

		// Check that properties exist
		const FProperty* WeakTargetProp = ScriptClass->FindPropertyByName(FName(TEXT("WeakTarget")));
		ASSERT_THAT(IsNotNull(WeakTargetProp, TEXT("WeakTarget property should exist")));

		const FProperty* WeakPawnProp = ScriptClass->FindPropertyByName(FName(TEXT("WeakPawn")));
		ASSERT_THAT(IsNotNull(WeakPawnProp, TEXT("WeakPawn property should exist")));

		// Check specifiers are applied
		ASSERT_THAT(IsTrue(WeakTargetProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit on weak ref")));
		ASSERT_THAT(IsTrue(WeakPawnProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible on weak ref")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak-ref-property actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesAssigned"), true, TEXT("Weak pointer properties should be assignable"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSubclassOf declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSubclassOf assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSubclassOf Get should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullCheckWorked"), true, TEXT("TSubclassOf null check should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComparisonWorked"), true, TEXT("TSubclassOf comparison should work"));
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

		// Check that properties exist with correct types
		const FProperty* ActorClassProp = ScriptClass->FindPropertyByName(FName(TEXT("ActorClass")));
		ASSERT_THAT(IsNotNull(ActorClassProp, TEXT("ActorClass property should exist")));
		ASSERT_THAT(IsTrue(ActorClassProp->IsA<FClassProperty>(), TEXT("ActorClass should be FClassProperty")));

		const FProperty* PawnClassProp = ScriptClass->FindPropertyByName(FName(TEXT("PawnClass")));
		ASSERT_THAT(IsNotNull(PawnClassProp, TEXT("PawnClass property should exist")));
		ASSERT_THAT(IsTrue(PawnClassProp->IsA<FClassProperty>(), TEXT("PawnClass should be FClassProperty")));

		// Check specifiers
		ASSERT_THAT(IsTrue(ActorClassProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditDefaultsOnly should set CPF_Edit")));
		ASSERT_THAT(IsTrue(ActorClassProp->HasAnyPropertyFlags(CPF_DisableEditOnInstance), TEXT("EditDefaultsOnly should disable instance edit")));
		ASSERT_THAT(IsTrue(PawnClassProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-property actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesSet"), true, TEXT("TSubclassOf properties should be assignable"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-spawn actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorSpawnWorked"), true, TEXT("Spawn with TSubclassOf<AActor> should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PawnSpawnWorked"), true, TEXT("Spawn with TSubclassOf<APawn> should work"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Subclass-of-typecheck actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsChildOfActorWorked"), true, TEXT("AActor should be child of AActor"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsChildOfPawnWorked"), true, TEXT("APawn should be child of APawn"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PawnIsChildOfActorWorked"), true, TEXT("APawn should be child of AActor"));
	}
};

#endif
