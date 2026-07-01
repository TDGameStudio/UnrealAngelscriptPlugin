#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Texture2D.h"
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
//   OpenSpec: test-coverage/coverage-matrix.md
//
// Test groups:
//   1. Object References (AActor*, UObject*)
//   2. Native interface references
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
// Detailed coverage matrix: OpenSpec: test-coverage/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Object reference basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("Object reference declaration should default to null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullCheckPassed"), true, TEXT("Null check should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("Object reference assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidCheckPassed"), true, TEXT("IsValid should distinguish null from valid references"))));
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
					FString Name = GetName().ToString();
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Object reference operations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToBaseWorked"), true, TEXT("Cast to base class should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToDerivedWorked"), true, TEXT("Cast back to derived should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastToUnrelatedFailed"), true, TEXT("Cast to unrelated type should fail"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComparisonWorked"), true, TEXT("Reference comparison should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetClassWorked"), true, TEXT("GetClass should return valid UClass"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetNameWorked"), true, TEXT("GetName should return non-empty string"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Verify property types
		const FProperty* ActorClassProp = ScriptClass->FindPropertyByName(FName(TEXT("ActorClass")));
		ASSERT_THAT(IsNotNull(ActorClassProp, TEXT("ActorClass property should exist")));
		if (ActorClassProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ActorClassProp->IsA<FClassProperty>(), TEXT("ActorClass should be FClassProperty")));
		ASSERT_THAT(IsTrue(ActorClassProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditDefaultsOnly should set CPF_Edit")));

		const FProperty* PawnClassProp = ScriptClass->FindPropertyByName(FName(TEXT("PawnClass")));
		ASSERT_THAT(IsNotNull(PawnClassProp, TEXT("PawnClass property should exist")));
		if (PawnClassProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(PawnClassProp->IsA<FClassProperty>(), TEXT("PawnClass should be FClassProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TSubclassOf actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSubclassOf declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSubclassOf assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullCheckWorked"), true, TEXT("TSubclassOf null check should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSubclassOf Get should return UClass"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComparisonWorked"), true, TEXT("TSubclassOf comparison should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SpawnWithClassWorked"), true, TEXT("SpawnActor with TSubclassOf should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsChildOfWorked"), true, TEXT("IsChildOf type check should work"))));
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
				bool SoftObjectToStringWorked = false;

				UPROPERTY()
				bool SoftClassDeclarationWorked = false;

				UPROPERTY()
				bool SoftClassAssignmentWorked = false;

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

					// Test Get method
					TSubclassOf<AActor> ClassRef = SoftClassRef.Get();
					if (ClassRef.IsValid() && ClassRef.IsChildOf(AActor::StaticClass()))
					{
						SoftClassGetWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageHandlesSoftRefActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft reference actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft reference actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// TSoftObjectPtr verifications
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectDeclarationWorked"), true, TEXT("TSoftObjectPtr declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectAssignmentWorked"), true, TEXT("TSoftObjectPtr assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectIsNullWorked"), true, TEXT("TSoftObjectPtr IsNull should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectIsValidWorked"), true, TEXT("TSoftObjectPtr IsValid should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectGetWorked"), true, TEXT("TSoftObjectPtr Get should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftObjectToStringWorked"), true, TEXT("TSoftObjectPtr ToString should work"))));

		// TSoftClassPtr verifications
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassDeclarationWorked"), true, TEXT("TSoftClassPtr declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassAssignmentWorked"), true, TEXT("TSoftClassPtr assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftClassGetWorked"), true, TEXT("TSoftClassPtr Get should work"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Validity checks actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Tick to allow destruction and deferred check
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 2);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidForNullObjectFailed"), true, TEXT("IsValid should return false for null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidForValidObjectPassed"), true, TEXT("IsValid should return true for valid object"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidAfterDestroyFailed"), true, TEXT("IsValid should return false after destruction"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SoftRefIsValidWorked"), true, TEXT("TSoftObjectPtr IsValid should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakRefIsValidWorked"), true, TEXT("TWeakObjectPtr IsValid should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakRefInvalidAfterDestroy"), true, TEXT("TWeakObjectPtr should become invalid after object destruction"))));
	}

	// -------------------------------------------------------------------------
	// 6. Native interface references: script member, parameter, null
	// -------------------------------------------------------------------------
	TEST_METHOD(NativeInterfaceReferenceHandles)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		static const FName ModuleName(TEXT("ASCoverageHandles_NativeInterfaceRefs"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesNativeInterfaceRefs.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesNativeInterfaceRefsActor : AActor, UAngelscriptNativeParentInterface
			{
				UAngelscriptNativeParentInterface InterfaceRef;

				UAngelscriptNativeParentInterface ClearedInterfaceRef;

				UPROPERTY()
				int NativeValue = 37;

				UPROPERTY()
				int ParameterValue = 0;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UPROPERTY()
				bool DefaultNullWorked = false;

				UPROPERTY()
				bool CastAssignmentWorked = false;

				UPROPERTY()
				bool InterfaceDispatchWorked = false;

				UPROPERTY()
				bool InterfaceParameterWorked = false;

				UPROPERTY()
				bool NullResetWorked = false;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta;
				}

				void AcceptInterface(UAngelscriptNativeParentInterface InInterface)
				{
					if (InInterface != nullptr)
					{
						int Adjusted = 5;
						InInterface.AdjustNativeValue(4, Adjusted);
						ParameterValue = InInterface.GetNativeValue() + Adjusted;
						InterfaceParameterWorked = ParameterValue == 46;
					}
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UAngelscriptNativeParentInterface EmptyRef;
					DefaultNullWorked = EmptyRef == nullptr;

					UObject SelfObject = this;
					InterfaceRef = Cast<UAngelscriptNativeParentInterface>(SelfObject);
					CastAssignmentWorked = InterfaceRef != nullptr;

					if (InterfaceRef != nullptr)
					{
						InterfaceDispatchWorked = InterfaceRef.GetNativeValue() == 37;
						InterfaceRef.SetNativeMarker(n"FromNativeInterfaceHandle");
						AcceptInterface(InterfaceRef);
					}

					ClearedInterfaceRef = InterfaceRef;
					ClearedInterfaceRef = nullptr;
					NullResetWorked = ClearedInterfaceRef == nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageHandlesNativeInterfaceRefsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Native interface handle actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Script actor should implement the native parent interface")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native interface handle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultNullWorked"), true, TEXT("Native interface references should default to null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastAssignmentWorked"), true, TEXT("Native interface references should assign from Cast<I>"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InterfaceDispatchWorked"), true, TEXT("Native interface references should dispatch methods"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InterfaceParameterWorked"), true, TEXT("Native interface references should pass through AS parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullResetWorked"), true, TEXT("Native interface references should reset to null"))));

		int32 ParameterValue = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, Actor, TEXT("ParameterValue"), ParameterValue), TEXT("ParameterValue should be readable")));
		ASSERT_THAT(AreEqual(46, ParameterValue, TEXT("Interface parameter should preserve dispatch and ref argument mutation")));

		FName NativeMarker = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker), TEXT("NativeMarker should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("FromNativeInterfaceHandle")), NativeMarker, TEXT("Interface setter should mutate actor state")));
	}

	// -------------------------------------------------------------------------
	// 7. Advanced handle coverage: UObject, TObjectPtr, NewObject, TSubclassOf
	// -------------------------------------------------------------------------
	TEST_METHOD(UObjectNewObjectTObjectPtrAndSubclassReferences)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_AdvancedRefs"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesAdvancedRefs.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesAdvancedRefsActor : AActor
			{
				UPROPERTY()
				UObject GenericObject;

				UPROPERTY()
				TObjectPtr<UObject> SmartObject;

				UPROPERTY()
				TSubclassOf<UObject> ObjectClass;

				UPROPERTY()
				bool UObjectNullAssignmentEqualityWorked = false;

				UPROPERTY()
				bool NewObjectOuterWorked = false;

				UPROPERTY()
				bool TObjectPtrRoutedToObjectProperty = false;

				UPROPERTY()
				bool SubclassParameterWorked = false;

				UPROPERTY()
				bool ReflectedSubclassParameterWorked = false;

				UPROPERTY()
				bool SubclassCreatedInstance = false;

				void AcceptObjectClass(TSubclassOf<UObject> InClass)
				{
					SubclassParameterWorked = InClass != nullptr && InClass.IsChildOf(UTexture2D::StaticClass());
					ObjectClass = InClass;
				}

				UFUNCTION()
				void AcceptClassFromCpp(TSubclassOf<UObject> InClass)
				{
					ReflectedSubclassParameterWorked = InClass != nullptr && InClass.IsChildOf(UTexture2D::StaticClass());
					AcceptObjectClass(InClass);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject EmptyObject = nullptr;
					UObjectNullAssignmentEqualityWorked = EmptyObject == nullptr;

					GenericObject = NewObject(this, UTexture2D::StaticClass(), n"CoverageHandlesAdvancedRefsTexture");
					NewObjectOuterWorked = GenericObject != nullptr &&
						GenericObject.GetOuter() == this &&
						GenericObject.IsA(UTexture2D::StaticClass());

					SmartObject = GenericObject;
					UObject RawObject = SmartObject;
					TObjectPtrRoutedToObjectProperty = RawObject == GenericObject &&
						SmartObject.Get() == GenericObject;

					AcceptObjectClass(UTexture2D::StaticClass());
					UObject CreatedFromSubclass = NewObject(this, ObjectClass, n"CoverageHandlesAdvancedRefsSubclassTexture");
					SubclassCreatedInstance = CreatedFromSubclass != nullptr &&
						CreatedFromSubclass.GetOuter() == this &&
						CreatedFromSubclass.IsA(UTexture2D::StaticClass());
				}
			}
			)AS"),
			TEXT("ACoverageHandlesAdvancedRefsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Advanced handles actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* SmartObjectProp = ScriptClass->FindPropertyByName(FName(TEXT("SmartObject")));
		ASSERT_THAT(IsNotNull(SmartObjectProp, TEXT("SmartObject property should exist")));
		if (SmartObjectProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SmartObjectProp->IsA<FObjectProperty>(), TEXT("TObjectPtr<UObject> should emit an object property")));
		ASSERT_THAT(IsTrue(SmartObjectProp->HasAnyPropertyFlags(CPF_TObjectPtr), TEXT("TObjectPtr<UObject> should preserve routing flag")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Advanced handles actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("UObjectNullAssignmentEqualityWorked"), true, TEXT("UObject handle null assignment and equality should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectOuterWorked"), true, TEXT("NewObject should honor an actor Outer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TObjectPtrRoutedToObjectProperty"), true, TEXT("TObjectPtr<UObject> should route like a UObject handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SubclassParameterWorked"), true, TEXT("TSubclassOf<UObject> should pass through AS function parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SubclassCreatedInstance"), true, TEXT("NewObject should create from a stored TSubclassOf class"))));

		UObject* GenericObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("GenericObject"), GenericObject), TEXT("GenericObject should be readable")));
		ASSERT_THAT(IsNotNull(GenericObject, TEXT("GenericObject should hold the created UObject")));
		if (GenericObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), GenericObject->GetOuter(), TEXT("C++ should observe the actor as NewObject Outer")));

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("AcceptClassFromCpp")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptClassFromCpp should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddParam<TSubclassOf<UObject>>(TSubclassOf<UObject>(UTexture2D::StaticClass()));
		ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("Reflected TSubclassOf<UObject> parameter invocation should succeed")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReflectedSubclassParameterWorked"), true, TEXT("Reflected TSubclassOf<UObject> parameter should be consumed"))));
	}

	// -------------------------------------------------------------------------
	// 8. Weak reference containers: array storage, null element, reassignment
	// -------------------------------------------------------------------------
	TEST_METHOD(WeakObjectPtrArrayContainerAndReassignment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_WeakArrayReassign"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesWeakArrayReassign.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesWeakArrayReassignActor : AActor
			{
				UPROPERTY()
				TArray<TWeakObjectPtr<AActor>> WeakActors;

				UPROPERTY()
				TWeakObjectPtr<AActor> ReassignedWeakActor;

				UPROPERTY()
				bool ArrayStoredWeakReferences = false;

				UPROPERTY()
				bool ArrayNullElementComparedToNull = false;

				UPROPERTY()
				bool DestroyedElementInvalidated = false;

				UPROPERTY()
				bool ReassignedToNewObject = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor FirstActor = SpawnActor(AActor::StaticClass());
					AActor SecondActor = SpawnActor(AActor::StaticClass());
					AActor ReplacementActor = SpawnActor(AActor::StaticClass());

					TWeakObjectPtr<AActor> WeakFirst = FirstActor;
					TWeakObjectPtr<AActor> WeakSecond = SecondActor;
					TWeakObjectPtr<AActor> EmptyWeak;

					WeakActors.Add(WeakFirst);
					WeakActors.Add(WeakSecond);
					WeakActors.Add(EmptyWeak);

					ArrayStoredWeakReferences = WeakActors.Num() == 3 &&
						WeakActors[0].Get() == FirstActor &&
						WeakActors[1].Get() == SecondActor;
					ArrayNullElementComparedToNull = WeakActors[2] == nullptr;

					SecondActor.DestroyActor();
					DestroyedElementInvalidated = WeakActors[1] == nullptr &&
						WeakActors[1].Get() == nullptr &&
						!WeakActors[1].IsValid();

					ReassignedWeakActor = FirstActor;
					ReassignedWeakActor = ReplacementActor;
					ReassignedToNewObject = ReassignedWeakActor.IsValid() &&
						ReassignedWeakActor.Get() == ReplacementActor &&
						ReassignedWeakActor.Get() != FirstActor;
				}
			}
			)AS"),
			TEXT("ACoverageHandlesWeakArrayReassignActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Weak array/reassignment actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FArrayProperty* WeakActorsProp = CastField<FArrayProperty>(ScriptClass->FindPropertyByName(FName(TEXT("WeakActors"))));
		ASSERT_THAT(IsNotNull(WeakActorsProp, TEXT("WeakActors property should be an array")));
		if (WeakActorsProp == nullptr)
		{
			return;
		}

		const FWeakObjectProperty* WeakActorsInnerProp = CastField<FWeakObjectProperty>(WeakActorsProp->Inner);
		ASSERT_THAT(IsNotNull(WeakActorsInnerProp, TEXT("WeakActors array should store weak object pointers")));
		if (WeakActorsInnerProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(AActor::StaticClass(), WeakActorsInnerProp->PropertyClass, TEXT("WeakActors inner property should target AActor")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Weak array/reassignment actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		int32 WeakActorCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("WeakActors"), WeakActorCount), TEXT("WeakActors array length should be readable")));
		ASSERT_THAT(AreEqual(3, WeakActorCount, TEXT("WeakActors should retain the expected number of entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayStoredWeakReferences"), true, TEXT("TArray<TWeakObjectPtr<AActor>> should store live weak references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayNullElementComparedToNull"), true, TEXT("TArray<TWeakObjectPtr<AActor>> should store and compare null weak elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DestroyedElementInvalidated"), true, TEXT("Destroyed actor weak array element should invalidate deterministically"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReassignedToNewObject"), true, TEXT("TWeakObjectPtr should reassign from one valid object to another"))));
	}

	// -------------------------------------------------------------------------
	// 9. Soft reference path boundary: FSoftObjectPath, pending, configured class
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftReferencePathConstructionAndPendingBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_SoftPathBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesSoftPathBoundary.as"),
			ASTEST_AS(R"AS(
			UCLASS(Config=Game)
			class ACoverageHandlesSoftPathBoundaryActor : AActor
			{
				UPROPERTY()
				TSoftObjectPtr<UTexture2D> TextureFromPath;

				UPROPERTY()
				TSoftObjectPtr<AActor> CrossLevelActorPath;

				UPROPERTY(Config)
				TSoftClassPtr<AActor> ConfiguredActorClass;

				UPROPERTY()
				bool ConstructedObjectPath = false;

				UPROPERTY()
				bool MissingObjectIsPending = false;

				UPROPERTY()
				bool CrossLevelPathStored = false;

				UPROPERTY()
				bool ResourcePathCanResolve = false;

				UPROPERTY()
				bool ConfiguredClassPathWorked = false;

				UPROPERTY()
				bool MissingClassIsPending = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FSoftObjectPath TexturePath("/Engine/EngineResources/DefaultTexture.DefaultTexture");
					TextureFromPath = TSoftObjectPtr<UTexture2D>(TexturePath);
					ConstructedObjectPath = TextureFromPath.ToSoftObjectPath() == TexturePath &&
						TextureFromPath.ToString() == TexturePath.ToString();

					TSoftObjectPtr<UTexture2D> MissingTexture(FSoftObjectPath("/Game/Coverage/MissingTexture.MissingTexture"));
					MissingObjectIsPending = !MissingTexture.IsNull() &&
						!MissingTexture.IsValid() &&
						MissingTexture.IsPending();

					CrossLevelActorPath = TSoftObjectPtr<AActor>(FSoftObjectPath("/Game/Coverage/OtherMap.OtherMap:PersistentLevel.OtherActor"));
					CrossLevelPathStored = CrossLevelActorPath.IsPending() &&
						CrossLevelActorPath.ToString().Contains("PersistentLevel");

					UObject ResolvedTexture = TextureFromPath.ToSoftObjectPath().TryLoad();
					ResourcePathCanResolve = Cast<UTexture2D>(ResolvedTexture) != nullptr;

					ConfiguredActorClass = TSoftClassPtr<AActor>(FSoftObjectPath("/Script/Engine.Actor"));
					TSubclassOf<AActor> LoadedClass = ConfiguredActorClass.Get();
					ConfiguredClassPathWorked = LoadedClass.IsValid() &&
						LoadedClass.IsChildOf(AActor::StaticClass()) &&
						ConfiguredActorClass.ToString().Contains("Actor");

					TSoftClassPtr<AActor> MissingActorClass(FSoftObjectPath("/Game/Coverage/MissingActorClass.MissingActorClass_C"));
					MissingClassIsPending = !MissingActorClass.IsNull() &&
						!MissingActorClass.IsValid() &&
						MissingActorClass.IsPending();
				}
			}
			)AS"),
			TEXT("ACoverageHandlesSoftPathBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft path boundary actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FSoftObjectProperty* TextureFromPathProp = CastField<FSoftObjectProperty>(ScriptClass->FindPropertyByName(FName(TEXT("TextureFromPath"))));
		ASSERT_THAT(IsNotNull(TextureFromPathProp, TEXT("TextureFromPath should be emitted as FSoftObjectProperty")));
		if (TextureFromPathProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UTexture2D::StaticClass(), TextureFromPathProp->PropertyClass, TEXT("TextureFromPath should target UTexture2D")));

		const FSoftClassProperty* ConfiguredActorClassProp = CastField<FSoftClassProperty>(ScriptClass->FindPropertyByName(FName(TEXT("ConfiguredActorClass"))));
		ASSERT_THAT(IsNotNull(ConfiguredActorClassProp, TEXT("ConfiguredActorClass should be emitted as FSoftClassProperty")));
		if (ConfiguredActorClassProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConfiguredActorClassProp->HasAnyPropertyFlags(CPF_Config), TEXT("UPROPERTY(Config) should set CPF_Config on TSoftClassPtr")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft path boundary actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConstructedObjectPath"), true, TEXT("TSoftObjectPtr should construct from FSoftObjectPath"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MissingObjectIsPending"), true, TEXT("TSoftObjectPtr.IsPending should report unresolved path-only object references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CrossLevelPathStored"), true, TEXT("TSoftObjectPtr should preserve cross-level actor object paths"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResourcePathCanResolve"), true, TEXT("FSoftObjectPath should support on-demand resource loading"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConfiguredClassPathWorked"), true, TEXT("TSoftClassPtr should construct from a configured class path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MissingClassIsPending"), true, TEXT("TSoftClassPtr.IsPending should report unresolved class paths"))));
	}

	// -------------------------------------------------------------------------
	// 10. GC reachability boundary: strong properties, containers, weak invalidation
	// -------------------------------------------------------------------------
	TEST_METHOD(GCReachabilityAndWeakInvalidationBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageHandles_GCReachability"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageHandlesGCReachability.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageHandlesGCReachabilityActor : AActor
			{
				UPROPERTY()
				UObject StrongObject;

				UPROPERTY()
				TArray<UObject> StrongObjects;

				UPROPERTY()
				TWeakObjectPtr<UObject> WeakStrongObject;

				UPROPERTY()
				TWeakObjectPtr<UObject> WeakContainerObject;

				UPROPERTY()
				TWeakObjectPtr<AActor> WeakDestroyedActor;

				UPROPERTY()
				bool StrongPropertySurvivedGC = false;

				UPROPERTY()
				bool StrongContainerSurvivedGC = false;

				UPROPERTY()
				bool WeakDestroyedActorInvalidated = false;

				UPROPERTY()
				bool WeakPointersObserveStrongReachability = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StrongObject = NewObject(this, UTexture2D::StaticClass(), n"CoverageHandlesGCStrongTexture");
					UObject ContainerObject = NewObject(this, UTexture2D::StaticClass(), n"CoverageHandlesGCContainerTexture");
					StrongObjects.Add(ContainerObject);

					WeakStrongObject = StrongObject;
					WeakContainerObject = ContainerObject;

					AActor DestroyedActor = SpawnActor(AActor::StaticClass());
					WeakDestroyedActor = DestroyedActor;
					DestroyedActor.DestroyActor();

					CoverageGC::ForceGarbageCollectionNow();

					StrongPropertySurvivedGC = StrongObject != nullptr &&
						WeakStrongObject.IsValid() &&
						WeakStrongObject.Get() == StrongObject;
					StrongContainerSurvivedGC = StrongObjects.Num() == 1 &&
						StrongObjects[0] != nullptr &&
						WeakContainerObject.IsValid() &&
						WeakContainerObject.Get() == StrongObjects[0];
					WeakDestroyedActorInvalidated = WeakDestroyedActor == nullptr &&
						WeakDestroyedActor.Get() == nullptr &&
						!WeakDestroyedActor.IsValid();
					WeakPointersObserveStrongReachability = StrongPropertySurvivedGC &&
						StrongContainerSurvivedGC &&
						WeakStrongObject.Get() != WeakContainerObject.Get();
				}
			}
			)AS"),
			TEXT("ACoverageHandlesGCReachabilityActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC reachability actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FArrayProperty* StrongObjectsProp = CastField<FArrayProperty>(ScriptClass->FindPropertyByName(FName(TEXT("StrongObjects"))));
		ASSERT_THAT(IsNotNull(StrongObjectsProp, TEXT("StrongObjects property should be an array")));
		if (StrongObjectsProp == nullptr)
		{
			return;
		}

		const FObjectProperty* StrongObjectsInnerProp = CastField<FObjectProperty>(StrongObjectsProp->Inner);
		ASSERT_THAT(IsNotNull(StrongObjectsInnerProp, TEXT("StrongObjects array should store UObject handles")));
		if (StrongObjectsInnerProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), StrongObjectsInnerProp->PropertyClass, TEXT("StrongObjects inner property should target UObject")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC reachability actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StrongPropertySurvivedGC"), true, TEXT("UPROPERTY UObject handle should keep NewObject reachable across forced GC"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StrongContainerSurvivedGC"), true, TEXT("TArray<UObject> UPROPERTY should keep contained NewObject reachable across forced GC"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakDestroyedActorInvalidated"), true, TEXT("TWeakObjectPtr should invalidate after observed actor destruction"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakPointersObserveStrongReachability"), true, TEXT("Weak pointers should observe but not replace strong UPROPERTY reachability"))));
	}
};

#endif
