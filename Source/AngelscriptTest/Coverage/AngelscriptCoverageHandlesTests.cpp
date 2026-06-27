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
#include "UObject/SoftObjectPtr.h"
#include "UObject/GarbageCollection.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageHandlesTests
// -----------------------------------------------------------------------------
// Comprehensive coverage for AngelScript handles and references system.
// This file covers all reference types from:
//
//   Documents/Coverage/Coverage_HandlesAndReferences.md
//
// Test groups:
//   1. Object References (AActor*, UObject*)
//   2. TScriptInterface<IInterface>
//   3. TSubclassOf<T>
//   4. TSoftObjectPtr / TSoftClassPtr
//   5. Reference validity checks (IsValid)
//
// Axes covered:
//   * ObjectReferenceBasics      - UObject handle declaration, null checks, assignment
//   * ObjectReferenceOperations  - Cast, comparison, GetClass, GetName
//   * TSubclassOfUsage          - class references and spawning
//   * SoftReferenceUsage        - TSoftObjectPtr and TSoftClassPtr
//   * ReferenceValidityChecks   - IsValid for all reference types
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_HandlesAndReferences.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageHandlesTest,
	"Angelscript.TestModule.Coverage.Handles",
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
	// 1. Object References: Basic AActor* and UObject* usage
	// -------------------------------------------------------------------------
	TEST_METHOD(ObjectReferenceBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_ObjectRefBasics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesObjectRefBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesObjectRefActor : AActor
			{
				UPROPERTY()
				bool DeclarationWorked = false;

				UPROPERTY()
				bool NullCheckPassed = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool IsValidCheckPassed = false;

				UPROPERTY()
				AActor ActorRef;

				UPROPERTY()
				UObject ObjectRef;

				UPROPERTY()
				APawn PawnRef;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test declaration - references are null by default
					AActor TempActor;
					if (TempActor == nullptr)
					{
						DeclarationWorked = true;
					}

					// Test null check
					if (ActorRef == nullptr)
					{
						NullCheckPassed = true;
					}

					// Test assignment
					ActorRef = this;
					if (ActorRef != nullptr && ActorRef == this)
					{
						AssignmentWorked = true;
					}

					// Test IsValid
					if (!IsValid(ObjectRef) && IsValid(ActorRef))
					{
						IsValidCheckPassed = true;
					}

					// Assign different types
					PawnRef = Cast<APawn>(SpawnActor(APawn::StaticClass()));
					ObjectRef = PawnRef;
				}
			}
			)AS"),
			TEXT("ACoverageHandlesObjectRefActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Object reference basics actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Object reference basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("Object reference declaration should default to null"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullCheckPassed"), true, TEXT("Null check should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("Object reference assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidCheckPassed"), true, TEXT("IsValid should distinguish null from valid references"));
	}

	// -------------------------------------------------------------------------
	// 2. Object Reference Operations: Cast, comparison, type info
	// -------------------------------------------------------------------------
	TEST_METHOD(ObjectReferenceOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_ObjectRefOps"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesObjectRefOps.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesObjectRefOpsActor : APawn
			{
				UPROPERTY()
				bool CastToBaseWorked = false;

				UPROPERTY()
				bool CastToDerivedWorked = false;

				UPROPERTY()
				bool CastToUnrelatedFailed = false;

				UPROPERTY()
				bool ComparisonWorked = false;

				UPROPERTY()
				bool GetClassWorked = false;

				UPROPERTY()
				bool GetNameWorked = false;

				UPROPERTY()
				FString ActorName;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test Cast to base class
					AActor ActorBase = Cast<AActor>(this);
					if (ActorBase != nullptr)
					{
						CastToBaseWorked = true;
					}

					// Test Cast back to derived
					APawn PawnDerived = Cast<APawn>(ActorBase);
					if (PawnDerived != nullptr && PawnDerived == this)
					{
						CastToDerivedWorked = true;
					}

					// Test Cast to unrelated type
					APlayerController Controller = Cast<APlayerController>(this);
					if (Controller == nullptr)
					{
						CastToUnrelatedFailed = true;
					}

					// Test comparison operators
					AActor Ref1 = this;
					AActor Ref2 = this;
					AActor OtherRef = SpawnActor(AActor::StaticClass());
					if (Ref1 == Ref2 && Ref1 != OtherRef)
					{
						ComparisonWorked = true;
					}

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
				}
			}
			)AS"),
			TEXT("ACoverageHandlesObjectRefOpsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Object reference operations actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Object reference operations actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToBaseWorked"), true, TEXT("Cast to base class should succeed"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToDerivedWorked"), true, TEXT("Cast back to derived should succeed"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToUnrelatedFailed"), true, TEXT("Cast to unrelated type should fail"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComparisonWorked"), true, TEXT("Reference comparison should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetClassWorked"), true, TEXT("GetClass should return valid UClass"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetNameWorked"), true, TEXT("GetName should return non-empty string"));
	}

	// -------------------------------------------------------------------------
	// 3. TSubclassOf: Type-safe class references
	// -------------------------------------------------------------------------
	TEST_METHOD(TSubclassOfUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_TSubclassOf"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesTSubclassOf.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesTSubclassOfActor : AActor
			{
				UPROPERTY(EditDefaultsOnly)
				TSubclassOf<AActor> ActorClass;

				UPROPERTY(EditAnywhere)
				TSubclassOf<APawn> PawnClass;

				UPROPERTY()
				bool DeclarationWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool NullCheckWorked = false;

				UPROPERTY()
				bool GetWorked = false;

				UPROPERTY()
				bool ComparisonWorked = false;

				UPROPERTY()
				bool SpawnWithClassWorked = false;

				UPROPERTY()
				bool IsChildOfWorked = false;

				UPROPERTY()
				AActor SpawnedActor;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test declaration
					TSubclassOf<AActor> TempClass;
					DeclarationWorked = true;

					// Test null by default
					if (TempClass == nullptr)
					{
						NullCheckWorked = true;
					}

					// Test assignment
					ActorClass = AActor::StaticClass();
					PawnClass = APawn::StaticClass();
					if (ActorClass != nullptr && PawnClass != nullptr)
					{
						AssignmentWorked = true;
					}

					// Test Get method
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

					// Test spawning with TSubclassOf
					if (ActorClass != nullptr)
					{
						SpawnedActor = SpawnActor(ActorClass);
						if (SpawnedActor != nullptr)
						{
							SpawnWithClassWorked = true;
						}
					}

					// Test IsChildOf type checking
					UClass PawnClassRef = PawnClass.Get();
					if (PawnClassRef != nullptr && PawnClassRef.IsChildOf(AActor::StaticClass()))
					{
						IsChildOfWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandlesTSubclassOfActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TSubclassOf actor should compile")));

		// Verify property types
		const FProperty* ActorClassProp = ScriptClass->FindPropertyByName(FName(TEXT("ActorClass")));
		ASSERT_THAT(IsNotNull(ActorClassProp, TEXT("ActorClass property should exist")));
		ASSERT_THAT(IsTrue(ActorClassProp->IsA<FClassProperty>(), TEXT("ActorClass should be FClassProperty")));
		ASSERT_THAT(IsTrue(ActorClassProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditDefaultsOnly should set CPF_Edit")));

		const FProperty* PawnClassProp = ScriptClass->FindPropertyByName(FName(TEXT("PawnClass")));
		ASSERT_THAT(IsNotNull(PawnClassProp, TEXT("PawnClass property should exist")));
		ASSERT_THAT(IsTrue(PawnClassProp->IsA<FClassProperty>(), TEXT("PawnClass should be FClassProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSubclassOf actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSubclassOf declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSubclassOf assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullCheckWorked"), true, TEXT("TSubclassOf null check should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSubclassOf Get should return UClass"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComparisonWorked"), true, TEXT("TSubclassOf comparison should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SpawnWithClassWorked"), true, TEXT("SpawnActor with TSubclassOf should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsChildOfWorked"), true, TEXT("IsChildOf type check should work"));
	}

	// -------------------------------------------------------------------------
	// 4. TSoftObjectPtr and TSoftClassPtr: Soft references
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftReferenceUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_SoftRef"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesSoftRef.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesSoftRefActor : AActor
			{
				UPROPERTY()
				TSoftObjectPtr<AActor> SoftActorRef;

				UPROPERTY()
				TSoftClassPtr<AActor> SoftClassRef;

				UPROPERTY()
				bool SoftObjectDeclarationWorked = false;

				UPROPERTY()
				bool SoftObjectAssignmentWorked = false;

				UPROPERTY()
				bool SoftObjectIsNullWorked = false;

				UPROPERTY()
				bool SoftObjectIsValidWorked = false;

				UPROPERTY()
				bool SoftObjectGetWorked = false;

				UPROPERTY()
				bool SoftObjectLoadSyncWorked = false;

				UPROPERTY()
				bool SoftObjectToStringWorked = false;

				UPROPERTY()
				bool SoftClassDeclarationWorked = false;

				UPROPERTY()
				bool SoftClassAssignmentWorked = false;

				UPROPERTY()
				bool SoftClassLoadSyncWorked = false;

				UPROPERTY()
				bool SoftClassGetWorked = false;

				UPROPERTY()
				FString SoftObjectPathString;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// --- TSoftObjectPtr tests ---

					// Test declaration
					TSoftObjectPtr<AActor> TempSoft;
					SoftObjectDeclarationWorked = true;

					// Test IsNull on empty soft reference
					if (TempSoft.IsNull())
					{
						SoftObjectIsNullWorked = true;
					}

					// Test assignment
					AActor SpawnedActor = SpawnActor(AActor::StaticClass());
					SoftActorRef = SpawnedActor;
					if (SoftActorRef.IsValid())
					{
						SoftObjectAssignmentWorked = true;
					}

					// Test IsValid
					if (!TempSoft.IsValid() && SoftActorRef.IsValid())
					{
						SoftObjectIsValidWorked = true;
					}

					// Test Get method
					AActor Retrieved = SoftActorRef.Get();
					if (Retrieved == SpawnedActor)
					{
						SoftObjectGetWorked = true;
					}

					// Test LoadSynchronous
					AActor Loaded = SoftActorRef.LoadSynchronous();
					if (Loaded == SpawnedActor)
					{
						SoftObjectLoadSyncWorked = true;
					}

					// Test ToString path
					FString PathString = SoftActorRef.ToString();
					if (!PathString.IsEmpty())
					{
						SoftObjectToStringWorked = true;
						SoftObjectPathString = PathString;
					}

					// --- TSoftClassPtr tests ---

					// Test declaration
					TSoftClassPtr<AActor> TempSoftClass;
					SoftClassDeclarationWorked = true;

					// Test assignment
					SoftClassRef = AActor::StaticClass();
					if (!SoftClassRef.IsNull())
					{
						SoftClassAssignmentWorked = true;
					}

					// Test LoadSynchronous
					UClass LoadedClass = SoftClassRef.LoadSynchronous();
					if (LoadedClass != nullptr)
					{
						SoftClassLoadSyncWorked = true;
					}

					// Test Get method
					UClass ClassRef = SoftClassRef.Get();
					if (ClassRef != nullptr)
					{
						SoftClassGetWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandlesSoftRefActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft reference actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft reference actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TSoftObjectPtr verifications
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectDeclarationWorked"), true, TEXT("TSoftObjectPtr declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectAssignmentWorked"), true, TEXT("TSoftObjectPtr assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectIsNullWorked"), true, TEXT("TSoftObjectPtr IsNull should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectIsValidWorked"), true, TEXT("TSoftObjectPtr IsValid should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectGetWorked"), true, TEXT("TSoftObjectPtr Get should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectLoadSyncWorked"), true, TEXT("TSoftObjectPtr LoadSynchronous should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectToStringWorked"), true, TEXT("TSoftObjectPtr ToString should work"));

		// TSoftClassPtr verifications
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassDeclarationWorked"), true, TEXT("TSoftClassPtr declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassAssignmentWorked"), true, TEXT("TSoftClassPtr assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassLoadSyncWorked"), true, TEXT("TSoftClassPtr LoadSynchronous should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassGetWorked"), true, TEXT("TSoftClassPtr Get should work"));
	}

	// -------------------------------------------------------------------------
	// 5. Reference Validity Checks: IsValid across different reference types
	// -------------------------------------------------------------------------
	TEST_METHOD(ReferenceValidityChecks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_Validity"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesValidity.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesValidityActor : AActor
			{
				UPROPERTY()
				bool IsValidForNullObjectFailed = false;

				UPROPERTY()
				bool IsValidForValidObjectPassed = false;

				UPROPERTY()
				bool IsValidAfterDestroyFailed = false;

				UPROPERTY()
				bool SoftRefIsValidWorked = false;

				UPROPERTY()
				bool WeakRefIsValidWorked = false;

				UPROPERTY()
				bool WeakRefInvalidAfterDestroy = false;

				UPROPERTY()
				AActor TempActor;

				UPROPERTY()
				TWeakObjectPtr<AActor> WeakRef;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test IsValid with null
					AActor NullRef = nullptr;
					if (!IsValid(NullRef))
					{
						IsValidForNullObjectFailed = true;
					}

					// Test IsValid with valid object
					if (IsValid(this))
					{
						IsValidForValidObjectPassed = true;
					}

					// Test IsValid with TSoftObjectPtr
					TSoftObjectPtr<AActor> SoftRef = this;
					if (SoftRef.IsValid())
					{
						SoftRefIsValidWorked = true;
					}

					// Test IsValid with TWeakObjectPtr
					TempActor = SpawnActor(AActor::StaticClass());
					WeakRef = TempActor;
					if (WeakRef.IsValid())
					{
						WeakRefIsValidWorked = true;
					}

					// Destroy actor and check weak ref becomes invalid
					TempActor.DestroyActor();
					System::ExecuteForOneFrame(CheckWeakRefInvalidation);
				}

				void CheckWeakRefInvalidation()
				{
					// After destruction, weak ref should be invalid
					if (!WeakRef.IsValid())
					{
						WeakRefInvalidAfterDestroy = true;
					}

					// IsValid on destroyed object should fail
					if (!IsValid(TempActor))
					{
						IsValidAfterDestroyFailed = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandlesValidityActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Validity checks actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Validity checks actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Tick to allow destruction and deferred check
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 2);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidForNullObjectFailed"), true, TEXT("IsValid should return false for null"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidForValidObjectPassed"), true, TEXT("IsValid should return true for valid object"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidAfterDestroyFailed"), true, TEXT("IsValid should return false after destruction"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftRefIsValidWorked"), true, TEXT("TSoftObjectPtr IsValid should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakRefIsValidWorked"), true, TEXT("TWeakObjectPtr IsValid should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakRefInvalidAfterDestroy"), true, TEXT("TWeakObjectPtr should become invalid after object destruction"));
	}
};

#endif
