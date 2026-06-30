#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageGCTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript Garbage Collection and object lifecycle verification.
// This file covers the "GC (Garbage Collection)" section of the coverage matrix:
//
//   OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md - Sub-matrix 9
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
//   2. Trigger synchronous GC through CoverageGC test helpers
//   3. Verify object validity using IsValid() and TWeakObjectPtr.IsValid()
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
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
					// Create a local object that has no strong references
					UObject TempObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());

					// Create weak reference to track it
					TWeakObjectPtr<UObject> WeakRef = TempObject;

					// At this point, only the local variable holds it
					// Local variables do not protect from GC in AngelScript

					// Clear the local reference
					TempObject = nullptr;

					// Force garbage collection
					CoverageGC::ForceGarbageCollectionNow();

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-basic-reclaim actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakRefInvalidatedAfterGC"), true,
			TEXT("Unreferenced object should be collected and weak reference invalidated"))));
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
				UObject StrongRefObject;

				UPROPERTY()
				bool ObjectSurvivedGC = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create an object and hold it in UPROPERTY member
					StrongRefObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());

					// Create weak reference to verify it survives
					TWeakObjectPtr<UObject> WeakRef = StrongRefObject;

					// Force garbage collection
					CoverageGC::ForceGarbageCollectionNow();

					// UPROPERTY should protect the object from GC
					if (WeakRef.IsValid() && IsValid(StrongRefObject))
					{
						ObjectSurvivedGC = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCUPropertyProtectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-UPROPERTY-protection actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-UPROPERTY-protection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectSurvivedGC"), true,
			TEXT("UPROPERTY member should protect object from GC"))));
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
					// Create an object with no strong references
					UObject TempObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());

					// Create weak reference
					TWeakObjectPtr<UObject> WeakRef = TempObject;

					// Verify weak ref is initially valid
					if (WeakRef.IsValid())
					{
						WeakPtrValidBeforeGC = true;
					}

					// Clear strong reference
					TempObject = nullptr;

					// Force GC
					CoverageGC::ForceGarbageCollectionNow();

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-weak-ptr-invalidation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakPtrValidBeforeGC"), true,
			TEXT("TWeakObjectPtr should be valid before GC"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WeakPtrInvalidAfterGC"), true,
			TEXT("TWeakObjectPtr should become invalid after GC collects the object"))));
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
				TArray<UObject> ObjectArray;

				UPROPERTY()
				TMap<int32, UObject> ObjectMap;

				UPROPERTY()
				bool ArrayObjectSurvivedGC = false;

				UPROPERTY()
				bool MapObjectSurvivedGC = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create objects and store in containers
					UObject ArrayObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
					ObjectArray.Add(ArrayObject);

					UObject MapObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
					ObjectMap.Add(1, MapObject);

					// Create weak references to verify survival
					TWeakObjectPtr<UObject> WeakArrayRef = ArrayObject;
					TWeakObjectPtr<UObject> WeakMapRef = MapObject;

					// Clear local references
					ArrayObject = nullptr;
					MapObject = nullptr;

					// Force GC
					CoverageGC::ForceGarbageCollectionNow();

					// Container members should protect objects from GC
					if (WeakArrayRef.IsValid() && IsValid(ObjectArray[0]))
					{
						ArrayObjectSurvivedGC = true;
					}

					UObject RetrievedMapObject = ObjectMap[1];
					if (WeakMapRef.IsValid() && IsValid(RetrievedMapObject))
					{
						MapObjectSurvivedGC = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCContainerProtectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-container-protection actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-container-protection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrayObjectSurvivedGC"), true,
			TEXT("TArray<UObject> should protect contained objects from GC"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapObjectSurvivedGC"), true,
			TEXT("TMap with UObject values should protect contained objects from GC"))));
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
				UObject HeldObject;

				UPROPERTY()
				TWeakObjectPtr<UObject> WeakRef;

				UPROPERTY()
				int32 FrameCount = 0;

				UPROPERTY()
				bool ObjectValidAfterMultipleFrames = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create object and hold it
					HeldObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
					WeakRef = HeldObject;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					FrameCount++;

					// After 3 frames, force GC and verify object still exists
					if (FrameCount == 3)
					{
						CoverageGC::ForceGarbageCollectionNow();

						if (WeakRef.IsValid() && IsValid(HeldObject))
						{
							ObjectValidAfterMultipleFrames = true;
						}
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCCrossFrameHoldActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-cross-frame-hold actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-cross-frame-hold actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Tick multiple frames
		for (int32 i = 0; i < 5; ++i)
		{
			TickWorld(Engine, Spawner.GetWorld(), 0.016f, 1);
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectValidAfterMultipleFrames"), true,
			TEXT("Objects held in UPROPERTY should survive GC across multiple frames"))));
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
					// Create object in local scope
					UObject LocalObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());

					// Create weak reference to track it
					TWeakObjectPtr<UObject> WeakRef = LocalObject;

					// Force GC while local variable still exists
					CoverageGC::ForceGarbageCollectionNow();

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-local-variable-no-protection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LocalVariableDidNotProtect"), true,
			TEXT("Local variables should NOT protect objects from GC in AngelScript"))));
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
						UObject TempObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
						TWeakObjectPtr<UObject> WeakRef = TempObject;
						TempObject = nullptr;

						CoverageGC::CollectGarbageNow();

						if (!WeakRef.IsValid())
						{
							CollectGarbageWorked = true;
						}
					}

					// Test ForceGarbageCollection()
					{
						UObject TempObject2 = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
						TWeakObjectPtr<UObject> WeakRef2 = TempObject2;
						TempObject2 = nullptr;

						CoverageGC::ForceGarbageCollectionNow();

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-collection-methods actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollectGarbageWorked"), true,
			TEXT("CoverageGC::CollectGarbageNow() should trigger garbage collection"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ForceGarbageCollectionWorked"), true,
			TEXT("CoverageGC::ForceGarbageCollectionNow() should trigger garbage collection"))));
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
					// Create object
					UObject TempObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());

					// IsValid should return true for live object
					if (IsValid(TempObject))
					{
						IsValidReturnedTrueBeforeGC = true;
					}

					// Get weak reference, then clear strong ref
					TWeakObjectPtr<UObject> WeakRef = TempObject;
					TempObject = nullptr;

					// Force GC
					CoverageGC::ForceGarbageCollectionNow();

					// Try to get object from weak ref
					UObject RetrievedObject = WeakRef.Get();

					// IsValid should return false (or RetrievedObject is nullptr)
					if (RetrievedObject == nullptr || !IsValid(RetrievedObject))
					{
						IsValidDetectedInvalidObject = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageGCIsValidCheckActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-IsValid-check actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-IsValid-check actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidReturnedTrueBeforeGC"), true,
			TEXT("IsValid() should return true for live objects"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidDetectedInvalidObject"), true,
			TEXT("IsValid() should detect invalid/collected objects"))));
	}

	// -------------------------------------------------------------------------
	// GC NewObject with explicit Outer: NewObject participates in GC normally
	// -------------------------------------------------------------------------
	TEST_METHOD(GCNewObjectOuterAndCollection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_NewObjectOuter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCNewObjectOuter.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCNewObjectOuterActor : AActor
			{
				UPROPERTY()
				bool NewObjectCreatedWithOuter = false;

				UPROPERTY()
				bool UnreferencedNewObjectCollected = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject CreatedObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"CoverageGCNewObjectOuter");
					TWeakObjectPtr<UObject> WeakCreatedObject = CreatedObject;

					NewObjectCreatedWithOuter = CreatedObject != nullptr &&
						CreatedObject.GetOuter() == GetTransientPackage() &&
						CreatedObject.IsA(UTexture2D::StaticClass());

					CreatedObject = nullptr;
					CoverageGC::ForceGarbageCollectionNow();

					UnreferencedNewObjectCollected = !WeakCreatedObject.IsValid() && WeakCreatedObject.Get() == nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageGCNewObjectOuterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-NewObject-outer actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-NewObject-outer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NewObjectCreatedWithOuter"), true,
			TEXT("NewObject should create an object with the supplied Outer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("UnreferencedNewObjectCollected"), true,
			TEXT("Unreferenced NewObject result should be collectible"))));
	}

	// -------------------------------------------------------------------------
	// GC root reachability: AddToRoot protects, RemoveFromRoot releases
	// -------------------------------------------------------------------------
	TEST_METHOD(GCRootReachability)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_RootReachability"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCRootReachability.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageGCRootReachabilityActor : AActor
			{
				UPROPERTY()
				bool RootedObjectSurvivedGC = false;

				UPROPERTY()
				bool RemovedRootAllowedCollection = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject RootedObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"CoverageGCRootedObject");
					RootedObject.AddToRoot();

					TWeakObjectPtr<UObject> WeakRootedObject = RootedObject;
					RootedObject = nullptr;

					CoverageGC::ForceGarbageCollectionNow();
					RootedObjectSurvivedGC = WeakRootedObject.IsValid();

					UObject UnrootedObject = WeakRootedObject.Get();
					if (UnrootedObject != nullptr)
					{
						UnrootedObject.RemoveFromRoot();
					}
					UnrootedObject = nullptr;

					CoverageGC::ForceGarbageCollectionNow();
					RemovedRootAllowedCollection = !WeakRootedObject.IsValid() && WeakRootedObject.Get() == nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageGCRootReachabilityActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-root-reachability actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-root-reachability actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootedObjectSurvivedGC"), true,
			TEXT("Rooted object should remain reachable during GC"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RemovedRootAllowedCollection"), true,
			TEXT("Removing root protection should allow later collection"))));
	}

	// -------------------------------------------------------------------------
	// GC UPROPERTY reachability chain: roots traverse nested referenced objects
	// -------------------------------------------------------------------------
	TEST_METHOD(GCUPropertyReachabilityChain)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_UPropertyReachabilityChain"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCUPropertyReachabilityChain.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageGCReachabilityNode : UObject
			{
				UPROPERTY()
				UObject Child;
			}

			UCLASS()
			class ACoverageGCReachabilityChainActor : AActor
			{
				UPROPERTY()
				UCoverageGCReachabilityNode RootNode;

				UPROPERTY()
				bool ReachabilityChainSurvivedGC = false;

				UPROPERTY()
				bool ReleasedChainWasCollected = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RootNode = Cast<UCoverageGCReachabilityNode>(
						NewObject(this, UCoverageGCReachabilityNode::StaticClass(), n"CoverageGCReachabilityRoot"));

					UObject LeafObject = NewObject(RootNode, UTexture2D::StaticClass(), n"CoverageGCReachabilityLeaf");
					RootNode.Child = LeafObject;

					TWeakObjectPtr<UObject> WeakRoot = RootNode;
					TWeakObjectPtr<UObject> WeakLeaf = LeafObject;
					LeafObject = nullptr;

					CoverageGC::ForceGarbageCollectionNow();
					ReachabilityChainSurvivedGC = WeakRoot.IsValid() && WeakLeaf.IsValid() && RootNode.Child != nullptr;

					RootNode.Child = nullptr;
					RootNode = nullptr;

					CoverageGC::ForceGarbageCollectionNow();
					ReleasedChainWasCollected = !WeakRoot.IsValid() && !WeakLeaf.IsValid();
				}
			}
			)AS"),
			TEXT("ACoverageGCReachabilityChainActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-UPROPERTY-reachability-chain actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-UPROPERTY-reachability-chain actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReachabilityChainSurvivedGC"), true,
			TEXT("UPROPERTY reachability should traverse nested object references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReleasedChainWasCollected"), true,
			TEXT("Released UPROPERTY chain should become collectible"))));
	}

	// -------------------------------------------------------------------------
	// GC mark-sweep: unreachable strong reference cycles are collectible
	// -------------------------------------------------------------------------
	TEST_METHOD(GCStrongCycleReclaim)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageGC_StrongCycleReclaim"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageGCStrongCycleReclaim.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageGCCycleNode : UObject
			{
				UPROPERTY()
				UObject Other;
			}

			UCLASS()
			class ACoverageGCStrongCycleActor : AActor
			{
				UPROPERTY()
				bool StrongCycleCreated = false;

				UPROPERTY()
				bool StrongCycleCollected = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UCoverageGCCycleNode NodeA = Cast<UCoverageGCCycleNode>(
						NewObject(GetTransientPackage(), UCoverageGCCycleNode::StaticClass(), n"CoverageGCCycleA"));
					UCoverageGCCycleNode NodeB = Cast<UCoverageGCCycleNode>(
						NewObject(GetTransientPackage(), UCoverageGCCycleNode::StaticClass(), n"CoverageGCCycleB"));

					NodeA.Other = NodeB;
					NodeB.Other = NodeA;

					TWeakObjectPtr<UObject> WeakA = NodeA;
					TWeakObjectPtr<UObject> WeakB = NodeB;
					StrongCycleCreated = WeakA.IsValid() && WeakB.IsValid();

					NodeA = nullptr;
					NodeB = nullptr;

					CoverageGC::ForceGarbageCollectionNow();
					StrongCycleCollected = !WeakA.IsValid() && !WeakB.IsValid();
				}
			}
			)AS"),
			TEXT("ACoverageGCStrongCycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GC-strong-cycle actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GC-strong-cycle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StrongCycleCreated"), true,
			TEXT("Strong reference cycle should be created before GC"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StrongCycleCollected"), true,
			TEXT("Unreachable strong reference cycle should be collected by mark-sweep GC"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
