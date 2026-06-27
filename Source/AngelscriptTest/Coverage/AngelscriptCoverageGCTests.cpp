#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageGCTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript Garbage Collection and object lifecycle verification.
// This file covers the "GC (Garbage Collection)" section of the coverage matrix:
//
//   Documents/Coverage/Coverage_HandlesAndReferences.md - Sub-matrix 9
//
// Axes covered here:
//   * GCBasicReclaim           - unreferenced objects are collected by GC
//   * GCUPropertyProtection    - UPROPERTY members protect objects from GC
//   * GCWeakPtrInvalidation    - TWeakObjectPtr becomes invalid after GC
//   * GCContainerProtection    - TArray/TMap containers protect contained objects
//   * GCCrossFrameHold         - objects held across multiple frames remain valid
//
// GC verification strategy:
//   1. Create objects with various reference patterns
//   2. Trigger ForceGarbageCollection() or CollectGarbage()
//   3. Verify object validity using IsValid() and TWeakObjectPtr.IsValid()
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_HandlesAndReferences.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageGCTest,
	"Angelscript.TestModule.Coverage.GC",
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
	// GC Basic Reclaim: unreferenced objects should be collected by GC
	// -------------------------------------------------------------------------
	TEST_METHOD(GCBasicReclaim)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_BasicReclaim"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCBasicReclaim.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCBasicReclaimActor : AActor
			{
				UPROPERTY()
				bool WeakRefInvalidatedAfterGC = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create a local actor that has no strong references
					AActor TempActor = SpawnActor(AActor::StaticClass());

					// Create weak reference to track it
					TWeakObjectPtr<AActor> WeakRef = TempActor;

					// At this point, only the local variable holds it
					// Local variables do not protect from GC in AngelScript

					// Clear the local reference
					TempActor = nullptr;

					// Force garbage collection
					System::ForceGarbageCollection(true);

					// After GC, the weak reference should be invalid
					if (!WeakRef.IsValid())
					{
						WeakRefInvalidatedAfterGC = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCBasicReclaimActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-basic-reclaim actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-basic-reclaim actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakRefInvalidatedAfterGC"), true,
			TEXT("Unreferenced object should be collected and weak reference invalidated"));
	}

	// -------------------------------------------------------------------------
	// GC UPROPERTY Protection: UPROPERTY members protect objects from GC
	// -------------------------------------------------------------------------
	TEST_METHOD(GCUPropertyProtection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_UPropertyProtection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCUPropertyProtection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCUPropertyProtectionActor : AActor
			{
				UPROPERTY()
				AActor StrongRefActor;

				UPROPERTY()
				bool ObjectSurvivedGC = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create an actor and hold it in UPROPERTY member
					StrongRefActor = SpawnActor(AActor::StaticClass());

					// Create weak reference to verify it survives
					TWeakObjectPtr<AActor> WeakRef = StrongRefActor;

					// Force garbage collection
					System::ForceGarbageCollection(true);

					// UPROPERTY should protect the object from GC
					if (WeakRef.IsValid() && IsValid(StrongRefActor))
					{
						ObjectSurvivedGC = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCUPropertyProtectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-UPROPERTY-protection actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-UPROPERTY-protection actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectSurvivedGC"), true,
			TEXT("UPROPERTY member should protect object from GC"));
	}

	// -------------------------------------------------------------------------
	// GC WeakPtr Invalidation: TWeakObjectPtr fails after object is collected
	// -------------------------------------------------------------------------
	TEST_METHOD(GCWeakPtrInvalidation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_WeakPtrInvalidation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCWeakPtrInvalidation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCWeakPtrInvalidationActor : AActor
			{
				UPROPERTY()
				bool WeakPtrValidBeforeGC = false;

				UPROPERTY()
				bool WeakPtrInvalidAfterGC = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create an actor with no strong references
					AActor TempActor = SpawnActor(AActor::StaticClass());

					// Create weak reference
					TWeakObjectPtr<AActor> WeakRef = TempActor;

					// Verify weak ref is initially valid
					if (WeakRef.IsValid())
					{
						WeakPtrValidBeforeGC = true;
					}

					// Clear strong reference
					TempActor = nullptr;

					// Force GC
					System::ForceGarbageCollection(true);

					// Weak reference should now be invalid
					if (!WeakRef.IsValid())
					{
						WeakPtrInvalidAfterGC = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCWeakPtrInvalidationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-weak-ptr-invalidation actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-weak-ptr-invalidation actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakPtrValidBeforeGC"), true,
			TEXT("TWeakObjectPtr should be valid before GC"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakPtrInvalidAfterGC"), true,
			TEXT("TWeakObjectPtr should become invalid after GC collects the object"));
	}

	// -------------------------------------------------------------------------
	// GC Container Protection: TArray and TMap protect contained objects
	// -------------------------------------------------------------------------
	TEST_METHOD(GCContainerProtection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_ContainerProtection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCContainerProtection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCContainerProtectionActor : AActor
			{
				UPROPERTY()
				TArray<AActor> ActorArray;

				UPROPERTY()
				TMap<int32, AActor> ActorMap;

				UPROPERTY()
				bool ArrayObjectSurvivedGC = false;

				UPROPERTY()
				bool MapObjectSurvivedGC = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create actors and store in containers
					AActor ArrayActor = SpawnActor(AActor::StaticClass());
					ActorArray.Add(ArrayActor);

					AActor MapActor = SpawnActor(AActor::StaticClass());
					ActorMap.Add(1, MapActor);

					// Create weak references to verify survival
					TWeakObjectPtr<AActor> WeakArrayRef = ArrayActor;
					TWeakObjectPtr<AActor> WeakMapRef = MapActor;

					// Clear local references
					ArrayActor = nullptr;
					MapActor = nullptr;

					// Force GC
					System::ForceGarbageCollection(true);

					// Container members should protect objects from GC
					if (WeakArrayRef.IsValid() && IsValid(ActorArray[0]))
					{
						ArrayObjectSurvivedGC = true;
					}

					AActor RetrievedMapActor = ActorMap[1];
					if (WeakMapRef.IsValid() && IsValid(RetrievedMapActor))
					{
						MapObjectSurvivedGC = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCContainerProtectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-container-protection actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-container-protection actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayObjectSurvivedGC"), true,
			TEXT("TArray<AActor> should protect contained objects from GC"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapObjectSurvivedGC"), true,
			TEXT("TMap with AActor values should protect contained objects from GC"));
	}

	// -------------------------------------------------------------------------
	// GC Cross-Frame Hold: objects held in UPROPERTY survive across frames
	// -------------------------------------------------------------------------
	TEST_METHOD(GCCrossFrameHold)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_CrossFrameHold"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCCrossFrameHold.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCCrossFrameHoldActor : AActor
			{
				UPROPERTY()
				AActor HeldActor;

				UPROPERTY()
				TWeakObjectPtr<AActor> WeakRef;

				UPROPERTY()
				int32 FrameCount = 0;

				UPROPERTY()
				bool ObjectValidAfterMultipleFrames = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create actor and hold it
					HeldActor = SpawnActor(AActor::StaticClass());
					WeakRef = HeldActor;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					FrameCount++;

					// After 3 frames, force GC and verify object still exists
					if (FrameCount == 3)
					{
						System::ForceGarbageCollection(true);

						if (WeakRef.IsValid() && IsValid(HeldActor))
						{
							ObjectValidAfterMultipleFrames = true;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCCrossFrameHoldActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-cross-frame-hold actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-cross-frame-hold actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Tick multiple frames
		for (int32 i = 0; i < 5; ++i)
		{
			TickWorld(Engine, Spawner.GetWorld(), 0.016f, 1);
		}

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectValidAfterMultipleFrames"), true,
			TEXT("Objects held in UPROPERTY should survive GC across multiple frames"));
	}

	// -------------------------------------------------------------------------
	// GC Local Variable No Protection: local variables don't protect from GC
	// -------------------------------------------------------------------------
	TEST_METHOD(GCLocalVariableNoProtection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_LocalVariableNoProtection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCLocalVariableNoProtection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCLocalVariableNoProtectionActor : AActor
			{
				UPROPERTY()
				bool LocalVariableDidNotProtect = false;

				void TestLocalScope()
				{
					// Create actor in local scope
					AActor LocalActor = SpawnActor(AActor::StaticClass());

					// Create weak reference to track it
					TWeakObjectPtr<AActor> WeakRef = LocalActor;

					// Force GC while local variable still exists
					System::ForceGarbageCollection(true);

					// Local variables in AngelScript do NOT protect from GC
					// (Unlike C++ stack variables which do protect)
					if (!WeakRef.IsValid())
					{
						LocalVariableDidNotProtect = true;
					}
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestLocalScope();
				}
			}
			)AS"),
			TEXT("ACoverageGCLocalVariableNoProtectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-local-variable-no-protection actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-local-variable-no-protection actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LocalVariableDidNotProtect"), true,
			TEXT("Local variables should NOT protect objects from GC in AngelScript"));
	}

	// -------------------------------------------------------------------------
	// GC CollectGarbage vs ForceGarbageCollection: both trigger collection
	// -------------------------------------------------------------------------
	TEST_METHOD(GCCollectionMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_CollectionMethods"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCCollectionMethods.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCCollectionMethodsActor : AActor
			{
				UPROPERTY()
				bool CollectGarbageWorked = false;

				UPROPERTY()
				bool ForceGarbageCollectionWorked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test CollectGarbage()
					{
						AActor TempActor = SpawnActor(AActor::StaticClass());
						TWeakObjectPtr<AActor> WeakRef = TempActor;
						TempActor = nullptr;

						System::CollectGarbage(EObjectFlags::RF_NoFlags, true);

						if (!WeakRef.IsValid())
						{
							CollectGarbageWorked = true;
						}
					}

					// Test ForceGarbageCollection()
					{
						AActor TempActor2 = SpawnActor(AActor::StaticClass());
						TWeakObjectPtr<AActor> WeakRef2 = TempActor2;
						TempActor2 = nullptr;

						System::ForceGarbageCollection(true);

						if (!WeakRef2.IsValid())
						{
							ForceGarbageCollectionWorked = true;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCCollectionMethodsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-collection-methods actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-collection-methods actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollectGarbageWorked"), true,
			TEXT("System::CollectGarbage() should trigger garbage collection"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ForceGarbageCollectionWorked"), true,
			TEXT("System::ForceGarbageCollection() should trigger garbage collection"));
	}

	// -------------------------------------------------------------------------
	// GC IsValid Check: IsValid() returns false for collected objects
	// -------------------------------------------------------------------------
	TEST_METHOD(GCIsValidCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_IsValidCheck"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCIsValidCheck.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCIsValidCheckActor : AActor
			{
				UPROPERTY()
				bool IsValidReturnedTrueBeforeGC = false;

				UPROPERTY()
				bool IsValidDetectedInvalidObject = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create actor
					AActor TempActor = SpawnActor(AActor::StaticClass());

					// IsValid should return true for live object
					if (IsValid(TempActor))
					{
						IsValidReturnedTrueBeforeGC = true;
					}

					// Get weak reference, then clear strong ref
					TWeakObjectPtr<AActor> WeakRef = TempActor;
					TempActor = nullptr;

					// Force GC
					System::ForceGarbageCollection(true);

					// Try to get object from weak ref
					AActor RetrievedActor = WeakRef.Get();

					// IsValid should return false (or RetrievedActor is nullptr)
					if (RetrievedActor == nullptr || !IsValid(RetrievedActor))
					{
						IsValidDetectedInvalidObject = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCIsValidCheckActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-IsValid-check actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-IsValid-check actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidReturnedTrueBeforeGC"), true,
			TEXT("IsValid() should return true for live objects"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidDetectedInvalidObject"), true,
			TEXT("IsValid() should detect invalid/collected objects"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
