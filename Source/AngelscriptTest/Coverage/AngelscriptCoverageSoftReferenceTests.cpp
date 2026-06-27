#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPtr.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageSoftReferenceTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript soft references (TSoftObjectPtr, TSoftClassPtr).
// This file covers the soft reference sections from:
//
//   Documents/Coverage/Coverage_HandlesAndReferences.md - Sub-matrix 4 & 5
//
// Axes covered here:
//   * SoftObjectPtrBasics        - TSoftObjectPtr declaration, assignment, Get, LoadSynchronous
//   * SoftObjectPtrNullChecks    - IsNull, IsValid checks
//   * SoftObjectPtrPath          - ToSoftObjectPath, ToString path operations
//   * SoftObjectPtrAsProperty    - TSoftObjectPtr as UPROPERTY with specifiers
//   * SoftObjectPtrInContainers  - TArray, TMap with soft references
//   * SoftClassPtrBasics         - TSoftClassPtr declaration, LoadSynchronous, Get
//   * SoftClassPtrPath           - ToString, path operations
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_HandlesAndReferences.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

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
	// TSoftObjectPtr basics: declaration, assignment, Get, LoadSynchronous
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

				UPROPERTY()
				bool LoadSynchronousWorked = false;

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

					// Test LoadSynchronous on already-loaded object
					AActor Loaded = SoftActor.LoadSynchronous();
					if (Loaded == SpawnedActor)
					{
						LoadSynchronousWorked = true;
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSoftObjectPtr declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSoftObjectPtr assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSoftObjectPtr Get should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetBeforeLoadReturnsNull"), true, TEXT("TSoftObjectPtr Get should return null for unloaded object"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LoadSynchronousWorked"), true, TEXT("TSoftObjectPtr LoadSynchronous should work"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-null-checks actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsNullWorkedForEmpty"), true, TEXT("IsNull should return true for empty soft reference"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidWorkedForEmpty"), true, TEXT("IsValid should return false for empty soft reference"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsNullWorkedForAssigned"), true, TEXT("IsNull should return false for assigned soft reference"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidWorkedForAssigned"), true, TEXT("IsValid should return true for assigned soft reference"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-path actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToSoftObjectPathWorked"), true, TEXT("ToSoftObjectPath should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringWorked"), true, TEXT("ToString should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringNotEmpty"), true, TEXT("ToString should return non-empty string"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PathComparisonWorked"), true, TEXT("Soft reference path comparison should work"));
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

		// Check that properties exist
		const FProperty* SoftActorProp = ScriptClass->FindPropertyByName(FName(TEXT("SoftActor")));
		ASSERT_THAT(IsNotNull(SoftActorProp, TEXT("SoftActor property should exist")));

		const FProperty* SoftMeshProp = ScriptClass->FindPropertyByName(FName(TEXT("SoftMesh")));
		ASSERT_THAT(IsNotNull(SoftMeshProp, TEXT("SoftMesh property should exist")));

		const FProperty* SoftPawnProp = ScriptClass->FindPropertyByName(FName(TEXT("SoftPawn")));
		ASSERT_THAT(IsNotNull(SoftPawnProp, TEXT("SoftPawn property should exist")));

		// Check specifiers are applied
		ASSERT_THAT(IsTrue(SoftActorProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit on soft ref")));
		ASSERT_THAT(IsTrue(SoftMeshProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible on soft ref")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-property actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesAssigned"), true, TEXT("Soft reference properties should be assignable"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-ref-containers actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayWorked"), true, TEXT("TArray<TSoftObjectPtr> should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapWorked"), true, TEXT("TMap with TSoftObjectPtr values should work"));
	}

	// -------------------------------------------------------------------------
	// TSoftClassPtr basics: declaration, LoadSynchronous, Get
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
				bool LoadSynchronousWorked = false;

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

					// Test LoadSynchronous
					UClass LoadedClass = SoftClass.LoadSynchronous();
					if (LoadedClass != nullptr)
					{
						LoadSynchronousWorked = true;
					}

					// Test Get
					UClass GetClass = SoftClass.Get();
					if (GetClass != nullptr)
					{
						GetWorked = true;
					}

					// Test spawning with loaded class
					if (LoadedClass != nullptr)
					{
						AActor SpawnedActor = SpawnActor(LoadedClass);
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DeclarationWorked"), true, TEXT("TSoftClassPtr declaration should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true, TEXT("TSoftClassPtr assignment should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LoadSynchronousWorked"), true, TEXT("TSoftClassPtr LoadSynchronous should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GetWorked"), true, TEXT("TSoftClassPtr Get should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SpawnFromSoftClassWorked"), true, TEXT("Spawning from soft class reference should work"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class-path actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringWorked"), true, TEXT("TSoftClassPtr ToString should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToStringNotEmpty"), true, TEXT("TSoftClassPtr ToString should return non-empty string"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ToSoftObjectPathWorked"), true, TEXT("TSoftClassPtr ToSoftObjectPath should work"));
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

		// Check that properties exist
		const FProperty* ActorClassSoftProp = ScriptClass->FindPropertyByName(FName(TEXT("ActorClassSoft")));
		ASSERT_THAT(IsNotNull(ActorClassSoftProp, TEXT("ActorClassSoft property should exist")));

		const FProperty* PawnClassSoftProp = ScriptClass->FindPropertyByName(FName(TEXT("PawnClassSoft")));
		ASSERT_THAT(IsNotNull(PawnClassSoftProp, TEXT("PawnClassSoft property should exist")));

		// Check specifiers
		ASSERT_THAT(IsTrue(ActorClassSoftProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditDefaultsOnly should set CPF_Edit")));
		ASSERT_THAT(IsTrue(ActorClassSoftProp->HasAnyPropertyFlags(CPF_DisableEditOnInstance), TEXT("EditDefaultsOnly should disable instance edit")));
		ASSERT_THAT(IsTrue(PawnClassSoftProp->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Soft-class-property actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PropertiesSet"), true, TEXT("TSoftClassPtr properties should be assignable"));
	}
};

#endif
