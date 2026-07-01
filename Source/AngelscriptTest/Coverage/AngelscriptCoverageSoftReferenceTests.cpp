#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPtr.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageSoftReferenceTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript soft references (TSoftObjectPtr, TSoftClassPtr).
// This file covers the soft reference sections from:
//
//   OpenSpec: test-coverage/coverage-matrix.md - Sub-matrix 4 & 5
//
// Axes covered here:
//   * SoftObjectPtrBasics        - TSoftObjectPtr declaration, assignment, Get
//   * SoftObjectPtrNullChecks    - IsNull, IsValid checks
//   * SoftObjectPtrPath          - ToSoftObjectPath, ToString path operations
//   * SoftObjectPtrAsProperty    - TSoftObjectPtr as UPROPERTY with specifiers
//   * SoftObjectPtrInContainers  - TArray, TMap with soft references
//   * SoftClassPtrBasics         - TSoftClassPtr declaration, Get
//   * SoftClassPtrPath           - ToString, path operations
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: OpenSpec: test-coverage/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageSoftReferenceTest,
	"Angelscript.TestModule.Coverage.SoftReference",
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
	// TSoftObjectPtr basics: declaration, assignment, Get
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefBasicsActor : AActor
			{
				UPROPERTY()
				bool DeclarationWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool GetWorked = false;

				UPROPERTY()
				bool GetBeforeLoadReturnsNull = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test declaration
					TSoftObjectPtr<AActor> SoftActor;
					DeclarationWorked = true;

					// Test assignment from strong reference
					AActor SpawnedActor = SpawnActor(AActor::StaticClass());
					SoftActor = SpawnedActor;
					if (SoftActor.IsValid())
					{
						AssignmentWorked = true;
					}

					// Test Get() on already-loaded object
					AActor Retrieved = SoftActor.Get();
					if (Retrieved == SpawnedActor)
					{
						GetWorked = true;
					}

					// Test Get() before load returns null for unloaded reference
					// (We simulate this by creating a soft reference to an unloaded path)
					TSoftObjectPtr<AActor> UnloadedSoft;
					AActor UnloadedGet = UnloadedSoft.Get();
					if (UnloadedGet == nullptr)
					{
						GetBeforeLoadReturnsNull = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref-basics actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSoftObjectPtr declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSoftObjectPtr assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSoftObjectPtr Get should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetBeforeLoadReturnsNull"), true, TEXT("TSoftObjectPtr Get should return null for unloaded object"))));
	}

	// -------------------------------------------------------------------------
	// TSoftObjectPtr null checks: IsNull, IsValid
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrNullChecks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_NullChecks"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefNullChecks.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefNullChecksActor : AActor
			{
				UPROPERTY()
				bool IsNullWorkedForEmpty = false;

				UPROPERTY()
				bool IsValidWorkedForEmpty = false;

				UPROPERTY()
				bool IsNullWorkedForAssigned = false;

				UPROPERTY()
				bool IsValidWorkedForAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test empty soft reference
					TSoftObjectPtr<AActor> EmptySoft;
					if (EmptySoft.IsNull())
					{
						IsNullWorkedForEmpty = true;
					}

					if (!EmptySoft.IsValid())
					{
						IsValidWorkedForEmpty = true;
					}

					// Test assigned soft reference
					TSoftObjectPtr<AActor> AssignedSoft;
					AActor SpawnedActor = SpawnActor(AActor::StaticClass());
					AssignedSoft = SpawnedActor;

					if (!AssignedSoft.IsNull())
					{
						IsNullWorkedForAssigned = true;
					}

					if (AssignedSoft.IsValid())
					{
						IsValidWorkedForAssigned = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefNullChecksActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref-null-checks actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-null-checks actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsNullWorkedForEmpty"), true, TEXT("IsNull should return true for empty soft reference"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidWorkedForEmpty"), true, TEXT("IsValid should return false for empty soft reference"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsNullWorkedForAssigned"), true, TEXT("IsNull should return false for assigned soft reference"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidWorkedForAssigned"), true, TEXT("IsValid should return true for assigned soft reference"))));
	}

	// -------------------------------------------------------------------------
	// TSoftObjectPtr path operations: ToSoftObjectPath, ToString
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrPath)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_Path"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefPath.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefPathActor : AActor
			{
				UPROPERTY()
				bool ToSoftObjectPathWorked = false;

				UPROPERTY()
				bool ToStringWorked = false;

				UPROPERTY()
				bool ToStringNotEmpty = false;

				UPROPERTY()
				bool PathComparisonWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create soft reference
					TSoftObjectPtr<AActor> SoftActor;
					AActor SpawnedActor = SpawnActor(AActor::StaticClass());
					SoftActor = SpawnedActor;

					// Test ToSoftObjectPath
					FSoftObjectPath Path = SoftActor.ToSoftObjectPath();
					if (Path.IsValid())
					{
						ToSoftObjectPathWorked = true;
					}

					// Test ToString
					FString PathString = SoftActor.ToString();
					ToStringWorked = true;

					// Verify string is not empty
					if (PathString.Len() > 0)
					{
						ToStringNotEmpty = true;
					}

					// Test path comparison via two soft references
					TSoftObjectPtr<AActor> SoftActor2 = SpawnedActor;
					if (SoftActor.ToSoftObjectPath() == SoftActor2.ToSoftObjectPath())
					{
						PathComparisonWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefPathActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref-path actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-path actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToSoftObjectPathWorked"), true, TEXT("ToSoftObjectPath should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringWorked"), true, TEXT("ToString should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringNotEmpty"), true, TEXT("ToString should return non-empty string"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PathComparisonWorked"), true, TEXT("Soft reference path comparison should work"))));
	}

	// -------------------------------------------------------------------------
	// TSoftObjectPtr as UPROPERTY: EditAnywhere, BlueprintReadWrite
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrAsProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_Property"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefProperty.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefPropertyActor : AActor
			{
				UPROPERTY(EditAnywhere)
				TSoftObjectPtr<AActor> SoftActor;

				UPROPERTY(BlueprintReadWrite)
				TSoftObjectPtr<UStaticMesh> SoftMesh;

				UPROPERTY(Category="SoftRefs")
				TSoftObjectPtr<APawn> SoftPawn;

				UPROPERTY()
				bool PropertiesAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SoftActor = SpawnActor(AActor::StaticClass());
					SoftPawn = Cast<APawn>(SpawnActor(APawn::StaticClass()));

					if (SoftActor.IsValid() && SoftPawn.IsValid())
					{
						PropertiesAssigned = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefPropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref-property actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Check that properties exist
		const FProperty* SoftActorProp = ScriptClass->FindPropertyByName(FName(TEXT("SoftActor")));
		ASSERT_THAT(IsNotNull(SoftActorProp, TEXT("SoftActor property should exist")));
		if (SoftActorProp == nullptr)
		{
			return;
		}

		const FProperty* SoftMeshProp = ScriptClass->FindPropertyByName(FName(TEXT("SoftMesh")));
		ASSERT_THAT(IsNotNull(SoftMeshProp, TEXT("SoftMesh property should exist")));
		if (SoftMeshProp == nullptr)
		{
			return;
		}

		const FProperty* SoftPawnProp = ScriptClass->FindPropertyByName(FName(TEXT("SoftPawn")));
		ASSERT_THAT(IsNotNull(SoftPawnProp, TEXT("SoftPawn property should exist")));
		if (SoftPawnProp == nullptr)
		{
			return;
		}

		// Check specifiers are applied
		ASSERT_THAT(IsTrue(SoftActorProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit on soft ref")));
		ASSERT_THAT(IsTrue(SoftMeshProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible on soft ref")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesAssigned"), true, TEXT("Soft reference properties should be assignable"))));
	}

	// -------------------------------------------------------------------------
	// TSoftObjectPtr in containers: TArray, TMap
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrInContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_Containers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefContainers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefContainersActor : AActor
			{
				UPROPERTY()
				TArray<TSoftObjectPtr<AActor>> SoftActorArray;

				UPROPERTY()
				TMap<int, TSoftObjectPtr<AActor>> SoftActorMap;

				UPROPERTY()
				bool ArrayWorked = false;

				UPROPERTY()
				bool MapWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test TArray with soft references
					AActor Actor1 = SpawnActor(AActor::StaticClass());
					AActor Actor2 = SpawnActor(AActor::StaticClass());

					TSoftObjectPtr<AActor> Soft1 = Actor1;
					TSoftObjectPtr<AActor> Soft2 = Actor2;

					SoftActorArray.Add(Soft1);
					SoftActorArray.Add(Soft2);

					if (SoftActorArray.Num() == 2 &&
						SoftActorArray[0].IsValid() &&
						SoftActorArray[1].IsValid())
					{
						ArrayWorked = true;
					}

					// Test TMap with soft references
					SoftActorMap.Add(1, Soft1);
					SoftActorMap.Add(2, Soft2);

					if (SoftActorMap.Num() == 2 &&
						SoftActorMap[1].IsValid() &&
						SoftActorMap[2].IsValid())
					{
						MapWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefContainersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref-containers actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-containers actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayWorked"), true, TEXT("TArray<TSoftObjectPtr> should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapWorked"), true, TEXT("TMap with TSoftObjectPtr values should work"))));
	}

	// -------------------------------------------------------------------------
	// TSoftClassPtr basics: declaration, Get
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftClassPtrBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftClass_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftClassBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftClassBasicsActor : AActor
			{
				UPROPERTY()
				bool DeclarationWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool GetWorked = false;

				UPROPERTY()
				bool SpawnFromSoftClassWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test declaration
					TSoftClassPtr<AActor> SoftClass;
					DeclarationWorked = true;

					// Test assignment
					SoftClass = AActor::StaticClass();
					if (SoftClass.IsValid())
					{
						AssignmentWorked = true;
					}

					// Test Get
					TSubclassOf<AActor> GetClass = SoftClass.Get();
					if (GetClass.IsValid() && GetClass.IsChildOf(AActor::StaticClass()))
					{
						GetWorked = true;
					}

					// Test spawning with loaded class
					if (GetClass.IsValid())
					{
						AActor SpawnedActor = SpawnActor(GetClass);
						if (SpawnedActor != nullptr)
						{
							SpawnFromSoftClassWorked = true;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftClassBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-class-basics actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSoftClassPtr declaration should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSoftClassPtr assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSoftClassPtr Get should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SpawnFromSoftClassWorked"), true, TEXT("Spawning from soft class reference should work"))));
	}

	// -------------------------------------------------------------------------
	// TSoftClassPtr path operations: ToString
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftClassPtrPath)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftClass_Path"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftClassPath.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftClassPathActor : AActor
			{
				UPROPERTY()
				bool ToStringWorked = false;

				UPROPERTY()
				bool ToStringNotEmpty = false;

				UPROPERTY()
				bool ToSoftObjectPathWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create soft class reference
					TSoftClassPtr<AActor> SoftClass = AActor::StaticClass();

					// Test ToString
					FString PathString = SoftClass.ToString();
					ToStringWorked = true;

					// Verify string is not empty
					if (PathString.Len() > 0)
					{
						ToStringNotEmpty = true;
					}

					// Test ToSoftObjectPath
					FSoftObjectPath Path = SoftClass.ToSoftObjectPath();
					if (Path.IsValid())
					{
						ToSoftObjectPathWorked = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftClassPathActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-class-path actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class-path actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringWorked"), true, TEXT("TSoftClassPtr ToString should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringNotEmpty"), true, TEXT("TSoftClassPtr ToString should return non-empty string"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToSoftObjectPathWorked"), true, TEXT("TSoftClassPtr ToSoftObjectPath should work"))));
	}

	// -------------------------------------------------------------------------
	// TSoftClassPtr as UPROPERTY: class selector
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftClassPtrAsProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftClass_Property"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftClassProperty.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftClassPropertyActor : AActor
			{
				UPROPERTY(EditDefaultsOnly)
				TSoftClassPtr<AActor> ActorClassSoft;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				TSoftClassPtr<APawn> PawnClassSoft;

				UPROPERTY()
				bool PropertiesSet = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ActorClassSoft = AActor::StaticClass();
					PawnClassSoft = APawn::StaticClass();

					if (ActorClassSoft.IsValid() && PawnClassSoft.IsValid())
					{
						PropertiesSet = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSoftClassPropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-class-property actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Check that properties exist
		const FProperty* ActorClassSoftProp = ScriptClass->FindPropertyByName(FName(TEXT("ActorClassSoft")));
		ASSERT_THAT(IsNotNull(ActorClassSoftProp, TEXT("ActorClassSoft property should exist")));
		if (ActorClassSoftProp == nullptr)
		{
			return;
		}

		const FProperty* PawnClassSoftProp = ScriptClass->FindPropertyByName(FName(TEXT("PawnClassSoft")));
		ASSERT_THAT(IsNotNull(PawnClassSoftProp, TEXT("PawnClassSoft property should exist")));
		if (PawnClassSoftProp == nullptr)
		{
			return;
		}

		// Check specifiers
		ASSERT_THAT(IsTrue(ActorClassSoftProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditDefaultsOnly should set CPF_Edit")));
		ASSERT_THAT(IsTrue(ActorClassSoftProp->HasAnyPropertyFlags(CPF_DisableEditOnInstance), TEXT("EditDefaultsOnly should disable instance edit")));
		ASSERT_THAT(IsTrue(PawnClassSoftProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class-property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesSet"), true, TEXT("TSoftClassPtr properties should be assignable"))));
	}

	// -------------------------------------------------------------------------
	// TSoftObjectPtr path construction, pending state, and path-only references
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrPathConstructionAndPending)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_PathConstructionPending"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefPathConstructionPending.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefPathConstructionActor : AActor
			{
				UPROPERTY()
				TSoftObjectPtr<UTexture2D> TextureFromPath;

				UPROPERTY()
				TSoftObjectPtr<AActor> CrossLevelActorPath;

				UPROPERTY()
				bool ConstructedFromPath = false;

				UPROPERTY()
				bool PendingStateWorked = false;

				UPROPERTY()
				bool CrossLevelPathStored = false;

				UPROPERTY()
				bool ResourcePathCanResolve = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FSoftObjectPath TexturePath("/Engine/EngineResources/DefaultTexture.DefaultTexture");
					TextureFromPath = TSoftObjectPtr<UTexture2D>(TexturePath);
					ConstructedFromPath = TextureFromPath.ToSoftObjectPath() == TexturePath && TextureFromPath.ToString() == TexturePath.ToString();

					TSoftObjectPtr<UTexture2D> MissingTexture(FSoftObjectPath("/Game/Coverage/MissingTexture.MissingTexture"));
					PendingStateWorked = !MissingTexture.IsNull() && !MissingTexture.IsValid() && MissingTexture.IsPending();

					CrossLevelActorPath = TSoftObjectPtr<AActor>(FSoftObjectPath("/Game/Coverage/OtherMap.OtherMap:PersistentLevel.OtherActor"));
					CrossLevelPathStored = CrossLevelActorPath.IsPending() && CrossLevelActorPath.ToString().Contains("PersistentLevel");

					UObject ResolvedTexture = TextureFromPath.ToSoftObjectPath().TryLoad();
					ResourcePathCanResolve = Cast<UTexture2D>(ResolvedTexture) != nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefPathConstructionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref path construction actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref path construction actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConstructedFromPath"), true, TEXT("TSoftObjectPtr should construct from FSoftObjectPath"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PendingStateWorked"), true, TEXT("TSoftObjectPtr.IsPending should report path-only unresolved references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CrossLevelPathStored"), true, TEXT("TSoftObjectPtr should preserve cross-level actor object paths"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResourcePathCanResolve"), true, TEXT("FSoftObjectPath should support on-demand resource loading"))));
	}

	// -------------------------------------------------------------------------
	// TSoftObjectPtr async load: already-loaded resource callback boundary
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftObjectPtrAsyncLoad)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftRef_AsyncLoad"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftRefAsyncLoad.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSoftRefAsyncLoadActor : AActor
			{
				UPROPERTY()
				int AsyncCallbackCount = 0;

				UPROPERTY()
				bool AsyncLoadReturnedTexture = false;

				UPROPERTY()
				bool AsyncLoadPreservedResourcePath = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FOnSoftObjectLoaded Delegate;
					Delegate.BindUFunction(this, n"HandleTextureLoaded");

					FSoftObjectPath TexturePath("/Engine/EngineResources/DefaultTexture.DefaultTexture");
					TexturePath.TryLoad();

					TSoftObjectPtr<UTexture2D> TextureRef(TexturePath);
					AsyncLoadPreservedResourcePath = TextureRef.ToString().Contains("DefaultTexture");
					TextureRef.LoadAsync(Delegate);
				}

				UFUNCTION()
				void HandleTextureLoaded(UObject LoadedObject)
				{
					AsyncCallbackCount += 1;
					AsyncLoadReturnedTexture = Cast<UTexture2D>(LoadedObject) != nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageSoftRefAsyncLoadActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-ref async actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref async actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);

		int32 AsyncCallbackCount = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, Actor, TEXT("AsyncCallbackCount"), AsyncCallbackCount), TEXT("AsyncCallbackCount should be readable")));

		ASSERT_THAT(AreEqual(1, AsyncCallbackCount, TEXT("TSoftObjectPtr.LoadAsync should invoke the callback exactly once")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AsyncLoadPreservedResourcePath"), true, TEXT("Async soft reference should preserve the resource path before load"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AsyncLoadReturnedTexture"), true, TEXT("TSoftObjectPtr.LoadAsync should return the loaded texture"))));
	}

	// -------------------------------------------------------------------------
	// TSoftClassPtr configured path: class reference from FSoftObjectPath
	// -------------------------------------------------------------------------
	TEST_METHOD(SoftClassPtrConfiguredPath)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSoftClass_ConfiguredPath"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSoftClassConfiguredPath.as"),
			ASTEST_AS(R"AS(
			UCLASS(Config=Game)
			class ACoverageSoftClassConfiguredPathActor : AActor
			{
				UPROPERTY(Config)
				TSoftClassPtr<AActor> ConfiguredActorClass;

				UPROPERTY()
				bool ConstructedFromConfiguredPath = false;

				UPROPERTY()
				bool LoadedConfiguredClass = false;

				UPROPERTY()
				bool ConfiguredClassCanSpawn = false;

				UPROPERTY()
				bool PendingConfiguredPath = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ConfiguredActorClass = TSoftClassPtr<AActor>(FSoftObjectPath("/Script/Engine.Actor"));
					ConstructedFromConfiguredPath = ConfiguredActorClass.ToString().Contains("Actor");

					TSubclassOf<AActor> LoadedClass = ConfiguredActorClass.Get();
					LoadedConfiguredClass = LoadedClass.IsValid() && LoadedClass.IsChildOf(AActor::StaticClass());

					AActor SpawnedActor = SpawnActor(LoadedClass);
					ConfiguredClassCanSpawn = SpawnedActor != nullptr;

					TSoftClassPtr<AActor> MissingConfiguredClass(FSoftObjectPath("/Game/Coverage/MissingActorClass.MissingActorClass_C"));
					PendingConfiguredPath = !MissingConfiguredClass.IsNull() && !MissingConfiguredClass.IsValid() && MissingConfiguredClass.IsPending();
				}
			}
			)AS"),
			TEXT("ACoverageSoftClassConfiguredPathActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Soft-class configured-path actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Config), TEXT("Config=Game should make the soft-class harness a config class")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), ScriptClass->ClassConfigName, TEXT("Config=Game should set the soft-class harness config name")));

		const FProperty* ConfiguredActorClassProp = ScriptClass->FindPropertyByName(TEXT("ConfiguredActorClass"));
		ASSERT_THAT(IsNotNull(ConfiguredActorClassProp, TEXT("ConfiguredActorClass config property should exist")));
		if (ConfiguredActorClassProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConfiguredActorClassProp->HasAnyPropertyFlags(CPF_Config), TEXT("UPROPERTY(Config) should set CPF_Config on TSoftClassPtr")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class configured-path actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConstructedFromConfiguredPath"), true, TEXT("TSoftClassPtr should construct from a configured path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LoadedConfiguredClass"), true, TEXT("Configured TSoftClassPtr should resolve the actor class"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConfiguredClassCanSpawn"), true, TEXT("Configured TSoftClassPtr should be usable as a spawn class"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PendingConfiguredPath"), true, TEXT("TSoftClassPtr should report pending for path-only configured classes"))));
	}
};

#endif
