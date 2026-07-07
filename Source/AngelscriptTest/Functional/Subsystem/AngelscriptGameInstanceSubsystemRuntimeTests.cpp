#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_ANGELSCRIPT_UNITTESTS


struct FAngelscriptTickBehaviorTestAccess
{
	static FAngelscriptEngine* TryGetGlobalEngine()
	{
		return FAngelscriptEngine::TryGetGlobalEngine();
	}

	static void ResetToIsolatedState()
	{
		if (!UAngelscriptSubsystem::HasAnyTickOwner() && FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptEngine::DestroyGlobal();
		}
	}

	static double GetNextHotReloadCheck(const FAngelscriptEngine& Engine)
	{
		return Engine.NextHotReloadCheck;
	}

	static void PrepareTickProbe(FAngelscriptEngine& Engine)
	{
		Engine.bScriptDevelopmentMode = true;
		Engine.bUseHotReloadCheckerThread = true;
		Engine.bWaitingForHotReloadResults = false;
		Engine.NextHotReloadCheck = 0.0;
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptGameInstanceSubsystemTests,
	"Angelscript.TestModule.GameInstanceSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FCoreTestContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FCoreTestContextStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FCoreTestContextStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

		void DiscardSavedStack()
		{
			SavedStack.Reset();
		}
	};

	static bool InitializeRuntimeSubsystemTestCase(
		FAutomationTestBase& Test,
		FActorTestSpawner& Spawner,
		UWorld*& OutWorld,
		UGameInstance*& OutGameInstance,
		UAngelscriptSubsystem*& OutSubsystem)
	{
		Spawner.InitializeGameSubsystems();

		OutWorld = &Spawner.GetWorld();
		if (!Test.TestNotNull(TEXT("Subsystem runtime test case should create a test world"), OutWorld))
		{
			return false;
		}

		OutGameInstance = OutWorld->GetGameInstance();
		if (!Test.TestNotNull(TEXT("Subsystem runtime test case should expose a game instance"), OutGameInstance))
		{
			return false;
		}

		OutSubsystem = OutGameInstance->GetSubsystem<UAngelscriptSubsystem>();
		return Test.TestNotNull(TEXT("Subsystem runtime test case should expose the Angelscript game-instance subsystem"), OutSubsystem);
	}

	static bool VerifyTickAdvancesProbe(
		FAutomationTestBase& Test,
		UAngelscriptSubsystem& Subsystem,
		const TCHAR* ContextLabel)
	{
		FAngelscriptEngine* PrimaryEngine = Subsystem.GetEngine();
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("%s should expose a primary engine"), ContextLabel),
			PrimaryEngine))
		{
			return false;
		}

		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*PrimaryEngine);
		const double PreviousNextHotReloadCheck = FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*PrimaryEngine);
		Subsystem.Tick(0.0f);

		return Test.TestTrue(
			*FString::Printf(TEXT("%s should advance the engine tick probe when the primary engine is tickable"), ContextLabel),
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*PrimaryEngine) > PreviousNextHotReloadCheck);
	}

public:
	TEST_METHOD(InitializeAdoptsOrOwnsEngineAndTicksIt)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		{
			FActorTestSpawner AdoptSpawner;
			UWorld* AdoptWorld = nullptr;
			UGameInstance* AdoptGameInstance = nullptr;
			UAngelscriptSubsystem* AdoptSubsystem = nullptr;
			ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, AdoptSpawner, AdoptWorld, AdoptGameInstance, AdoptSubsystem)));

			ASSERT_THAT(IsTrue(AdoptSubsystem->GetEngine() == &Engine, TEXT("Adopt case should reuse the outer engine when one is already active")));
			ASSERT_THAT(IsTrue(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Adopt case should register an active tick owner")));
			ASSERT_THAT(IsTrue(AdoptSubsystem->IsAllowedToTick(), TEXT("Adopt case should allow the subsystem to tick")));

			{
				FScopedTestWorldContextScope WorldContextScope(AdoptWorld);
				ASSERT_THAT(IsTrue(UAngelscriptSubsystem::GetCurrent() == AdoptSubsystem, TEXT("Adopt case should resolve GetCurrent() from the ambient world")));
			}

			ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == &Engine, TEXT("Adopt case should keep the shared test engine as the current engine while the outer scope is active")));
			ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *AdoptSubsystem, TEXT("Adopt case"))));

			AdoptSubsystem->Deinitialize();
			ASSERT_THAT(IsNull(AdoptSubsystem->GetEngine(), TEXT("Adopt case should clear the subsystem primary engine during deinitialize")));
			ASSERT_THAT(IsFalse(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Adopt case should release its active tick owner during deinitialize")));
			ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == &Engine, TEXT("Adopt case should restore the shared outer engine after subsystem deinitialize")));
		}

		{
			FCoreTestContextStackGuard ContextGuard;
			ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Own case should begin without a current engine on the cleared context stack")));

			FActorTestSpawner OwnSpawner;
			UWorld* OwnWorld = nullptr;
			UGameInstance* OwnGameInstance = nullptr;
			UAngelscriptSubsystem* OwnSubsystem = nullptr;
			ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, OwnSpawner, OwnWorld, OwnGameInstance, OwnSubsystem)));

			FAngelscriptEngine* OwnedEngine = OwnSubsystem->GetEngine();
			ASSERT_THAT(IsNotNull(OwnedEngine, TEXT("Own case should create a subsystem-owned primary engine")));
			ASSERT_THAT(IsTrue(OwnedEngine != &Engine, TEXT("Own case should create a different primary engine when no outer engine is active")));
			ASSERT_THAT(IsTrue(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Own case should register an active tick owner")));
			ASSERT_THAT(IsTrue(OwnSubsystem->IsAllowedToTick(), TEXT("Own case should allow the subsystem to tick")));
			ASSERT_THAT(IsTrue(OwnedEngine->ShouldTick(), TEXT("Own case should expose a tickable owned engine")));

			{
				FScopedTestWorldContextScope WorldContextScope(OwnWorld);
				ASSERT_THAT(IsTrue(UAngelscriptSubsystem::GetCurrent() == OwnSubsystem, TEXT("Own case should resolve GetCurrent() from the ambient world")));
				ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == OwnedEngine, TEXT("Own case should resolve the subsystem-owned engine as current when ambient world context is available")));
			}

			ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *OwnSubsystem, TEXT("Own case"))));

			OwnSubsystem->Deinitialize();
			ASSERT_THAT(IsNull(OwnSubsystem->GetEngine(), TEXT("Own case should clear the primary engine during deinitialize")));
			ASSERT_THAT(IsFalse(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Own case should release its active tick owner during deinitialize")));
			ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Own case should no longer resolve a current engine after the subsystem deinitializes")));
		}

		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == &Engine, TEXT("Leaving the cleared-stack own case should restore the shared test engine scope")));
	}

};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptGameInstanceSubsystemMultiOwnerLifecycleTests,
	"Angelscript.TestModule.GameInstanceSubsystem.MultiOwnerLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FCoreTestContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FCoreTestContextStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FCoreTestContextStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

		void DiscardSavedStack()
		{
			SavedStack.Reset();
		}
	};

	static bool InitializeRuntimeSubsystemTestCase(
		FAutomationTestBase& Test,
		FActorTestSpawner& Spawner,
		UWorld*& OutWorld,
		UGameInstance*& OutGameInstance,
		UAngelscriptSubsystem*& OutSubsystem)
	{
		Spawner.InitializeGameSubsystems();

		OutWorld = &Spawner.GetWorld();
		if (!Test.TestNotNull(TEXT("Subsystem runtime test case should create a test world"), OutWorld))
		{
			return false;
		}

		OutGameInstance = OutWorld->GetGameInstance();
		if (!Test.TestNotNull(TEXT("Subsystem runtime test case should expose a game instance"), OutGameInstance))
		{
			return false;
		}

		OutSubsystem = OutGameInstance->GetSubsystem<UAngelscriptSubsystem>();
		return Test.TestNotNull(TEXT("Subsystem runtime test case should expose the Angelscript game-instance subsystem"), OutSubsystem);
	}

	static bool VerifyTickAdvancesProbe(
		FAutomationTestBase& Test,
		UAngelscriptSubsystem& Subsystem,
		const TCHAR* ContextLabel)
	{
		FAngelscriptEngine* PrimaryEngine = Subsystem.GetEngine();
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("%s should expose a primary engine"), ContextLabel),
			PrimaryEngine))
		{
			return false;
		}

		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*PrimaryEngine);
		const double PreviousNextHotReloadCheck = FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*PrimaryEngine);
		Subsystem.Tick(0.0f);

		return Test.TestTrue(
			*FString::Printf(TEXT("%s should advance the engine tick probe when the primary engine is tickable"), ContextLabel),
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*PrimaryEngine) > PreviousNextHotReloadCheck);
	}

public:
	TEST_METHOD(SharedPrimaryEngineKeepsTickOwnershipUntilLastShutdown)
	{
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		UWorld* WorldA = nullptr;
		UWorld* WorldB = nullptr;
		UGameInstance* GameInstanceA = nullptr;
		UGameInstance* GameInstanceB = nullptr;
		UAngelscriptSubsystem* SubsystemA = nullptr;
		UAngelscriptSubsystem* SubsystemB = nullptr;
		ON_SCOPE_EXIT
		{
			if (SubsystemB != nullptr && SubsystemB->GetEngine() != nullptr) { SubsystemB->Deinitialize(); }
			if (SubsystemA != nullptr && SubsystemA->GetEngine() != nullptr) { SubsystemA->Deinitialize(); }
			if (!UAngelscriptSubsystem::HasAnyTickOwner() && FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Multi-owner lifecycle should begin without a current engine on the cleared context stack")));
		ASSERT_THAT(IsFalse(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should begin without active tick owners")));

		FActorTestSpawner SpawnerA;
		ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, SpawnerA, WorldA, GameInstanceA, SubsystemA)));

		FAngelscriptEngine* EngineA = SubsystemA->GetEngine();
		ASSERT_THAT(IsNotNull(EngineA, TEXT("Multi-owner lifecycle should create the first subsystem-owned engine")));
		ASSERT_THAT(IsTrue(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem A initializes")));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == EngineA, TEXT("Multi-owner lifecycle should keep subsystem A's engine current after its initialization")));
		ASSERT_THAT(IsTrue(SubsystemA->IsAllowedToTick(), TEXT("Multi-owner lifecycle should keep subsystem A tickable while it owns the engine")));
		ASSERT_THAT(IsTrue(EngineA->ShouldTick(), TEXT("Multi-owner lifecycle should keep subsystem A's engine tickable while it owns the engine")));

		FActorTestSpawner SpawnerB;
		ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, SpawnerB, WorldB, GameInstanceB, SubsystemB)));
		ASSERT_THAT(IsTrue(WorldA != WorldB, TEXT("Multi-owner lifecycle should create independent worlds for subsystem A and B")));
		ASSERT_THAT(IsTrue(GameInstanceA != GameInstanceB, TEXT("Multi-owner lifecycle should create independent game instances for subsystem A and B")));

		FAngelscriptEngine* EngineB = SubsystemB->GetEngine();
		ASSERT_THAT(IsTrue(EngineB == EngineA, TEXT("Multi-owner lifecycle should reuse subsystem A's engine for subsystem B")));
		ASSERT_THAT(IsTrue(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem B initializes")));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == EngineA, TEXT("Multi-owner lifecycle should keep subsystem A's engine current while both owners are alive")));
		ASSERT_THAT(IsTrue(SubsystemB->IsAllowedToTick(), TEXT("Multi-owner lifecycle should keep subsystem B tickable while it borrows subsystem A's engine")));
		ASSERT_THAT(IsTrue(EngineA->ShouldTick(), TEXT("Multi-owner lifecycle should keep the shared engine tickable while both owners are alive")));
		ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *SubsystemB, TEXT("Multi-owner lifecycle while both subsystems are alive"))));

		SubsystemB->Deinitialize();
		ASSERT_THAT(IsNull(SubsystemB->GetEngine(), TEXT("Multi-owner lifecycle should clear subsystem B's primary engine during deinitialize")));
		ASSERT_THAT(IsTrue(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == EngineA, TEXT("Multi-owner lifecycle should keep subsystem A's engine current after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(SubsystemA->IsAllowedToTick(), TEXT("Multi-owner lifecycle should keep subsystem A tickable after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(EngineA->ShouldTick(), TEXT("Multi-owner lifecycle should keep subsystem A's engine tickable after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *SubsystemA, TEXT("Multi-owner lifecycle after subsystem B deinitializes"))));

		SubsystemA->Deinitialize();
		ASSERT_THAT(IsNull(SubsystemA->GetEngine(), TEXT("Multi-owner lifecycle should clear subsystem A's primary engine during final deinitialize")));
		ASSERT_THAT(IsFalse(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should release the final tick owner during final deinitialize")));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Multi-owner lifecycle should clear the current engine after the last owner shuts down")));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Multi-owner lifecycle should leave no current engine after cleanup")));
		ASSERT_THAT(IsFalse(UAngelscriptSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should leave no active tick owners after cleanup")));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptGameInstanceSubsystemTickPolicyTests,
	"Angelscript.TestModule.GameInstanceSubsystem.TickPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool InitializeRuntimeSubsystemTestCase(
		FAutomationTestBase& Test,
		FActorTestSpawner& Spawner,
		UWorld*& OutWorld,
		UGameInstance*& OutGameInstance,
		UAngelscriptSubsystem*& OutSubsystem)
	{
		Spawner.InitializeGameSubsystems();

		OutWorld = &Spawner.GetWorld();
		if (!Test.TestNotNull(TEXT("Subsystem runtime test case should create a test world"), OutWorld))
		{
			return false;
		}

		OutGameInstance = OutWorld->GetGameInstance();
		if (!Test.TestNotNull(TEXT("Subsystem runtime test case should expose a game instance"), OutGameInstance))
		{
			return false;
		}

		OutSubsystem = OutGameInstance->GetSubsystem<UAngelscriptSubsystem>();
		return Test.TestNotNull(TEXT("Subsystem runtime test case should expose the Angelscript subsystem"), OutSubsystem);
	}

public:
	TEST_METHOD(GatesTemplateAndInitializationState)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UAngelscriptSubsystem* SubsystemCDO = GetMutableDefault<UAngelscriptSubsystem>();
		ASSERT_THAT(IsNotNull(SubsystemCDO, TEXT("TickPolicy test should resolve the subsystem CDO")));
		ASSERT_THAT(AreEqual(ETickableTickType::Never, SubsystemCDO->GetTickableTickType(), TEXT("TickPolicy test should force the subsystem CDO to never tick")));
		ASSERT_THAT(IsFalse(SubsystemCDO->IsAllowedToTick(), TEXT("TickPolicy test should reject the subsystem CDO from ticking")));

		TStrongObjectPtr<UGameInstance> RawGameInstance(NewObject<UGameInstance>());
		UAngelscriptSubsystem* RawSubsystem = NewObject<UAngelscriptSubsystem>(RawGameInstance.Get());
		ASSERT_THAT(IsNotNull(RawSubsystem, TEXT("TickPolicy test should create an uninitialized subsystem instance")));
		ASSERT_THAT(IsNull(RawSubsystem->GetEngine(), TEXT("TickPolicy test should leave a raw subsystem without a primary engine")));
		ASSERT_THAT(IsFalse(RawSubsystem->IsAllowedToTick(), TEXT("TickPolicy test should keep an uninitialized subsystem gated")));

		FActorTestSpawner Spawner;
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;
		UAngelscriptSubsystem* Subsystem = nullptr;
		ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, Spawner, World, GameInstance, Subsystem)));
		ASSERT_THAT(IsTrue(Subsystem->GetEngine() == &Engine, TEXT("TickPolicy test should initialize the subsystem with the active engine")));
		ASSERT_THAT(IsTrue(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should allow a live initialized subsystem to tick")));
		ASSERT_THAT(IsTrue(Subsystem->IsTickableInEditor(), TEXT("TickPolicy test should remain tickable in editor for live subsystem instances")));
		ASSERT_THAT(IsTrue(Subsystem->IsTickableWhenPaused(), TEXT("TickPolicy test should remain tickable while paused for live subsystem instances")));

		Subsystem->Deinitialize();
		ASSERT_THAT(IsNull(Subsystem->GetEngine(), TEXT("TickPolicy test should clear the subsystem engine during deinitialize")));
		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should close the tick gate after deinitialize")));
	}
};

#endif
