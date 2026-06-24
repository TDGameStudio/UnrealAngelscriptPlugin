#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS


struct FAngelscriptTickBehaviorTestAccess
{
	static FAngelscriptEngine* TryGetGlobalEngine()
	{
		return FAngelscriptEngine::TryGetGlobalEngine();
	}

	static void ResetToIsolatedState()
	{
		if (!UAngelscriptGameInstanceSubsystem::HasAnyTickOwner() && FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptEngine::DestroyGlobal();
		}
	}

	static double GetNextHotReloadCheck(const FAngelscriptEngine& Engine)
	{
		return Engine.NextHotReloadCheck;
	}

	static FAngelscriptEngine* GetSubsystemPrimaryEngine(const UAngelscriptGameInstanceSubsystem& Subsystem)
	{
		return Subsystem.PrimaryEngine;
	}

	static bool GetSubsystemOwnsPrimaryEngine(const UAngelscriptGameInstanceSubsystem& Subsystem)
	{
		return Subsystem.bOwnsPrimaryEngine;
	}

	static bool GetSubsystemInitialized(const UAngelscriptGameInstanceSubsystem& Subsystem)
	{
		return Subsystem.bInitialized;
	}

	static int32 GetActiveTickOwners()
	{
		return UAngelscriptGameInstanceSubsystem::ActiveTickOwners;
	}

	static void PrepareTickProbe(FAngelscriptEngine& Engine)
	{
		Engine.bScriptDevelopmentMode = true;
		Engine.bUseHotReloadCheckerThread = true;
		Engine.bWaitingForHotReloadResults = false;
		Engine.NextHotReloadCheck = 0.0;
	}

	static void SetSubsystemPrimaryEngine(UAngelscriptGameInstanceSubsystem& Subsystem, FAngelscriptEngine* Engine)
	{
		Subsystem.PrimaryEngine = Engine;
		Subsystem.bOwnsPrimaryEngine = true;
		Subsystem.bInitialized = true;
		Subsystem.ActiveTickOwners = 1;
		FAngelscriptEngineContextStack::Push(Engine);
	}

	static void SetSubsystemPrimaryEngineRaw(UAngelscriptGameInstanceSubsystem& Subsystem, FAngelscriptEngine* Engine)
	{
		Subsystem.PrimaryEngine = Engine;
	}

	static void SetSubsystemOwnsPrimaryEngine(UAngelscriptGameInstanceSubsystem& Subsystem, bool bOwnsPrimaryEngine)
	{
		Subsystem.bOwnsPrimaryEngine = bOwnsPrimaryEngine;
	}

	static void SetSubsystemInitialized(UAngelscriptGameInstanceSubsystem& Subsystem, bool bInitialized)
	{
		Subsystem.bInitialized = bInitialized;
	}

	static void SetActiveTickOwners(int32 ActiveTickOwners)
	{
		UAngelscriptGameInstanceSubsystem::ActiveTickOwners = ActiveTickOwners;
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
		UAngelscriptGameInstanceSubsystem*& OutSubsystem)
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

		OutSubsystem = OutGameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
		return Test.TestNotNull(TEXT("Subsystem runtime test case should expose the Angelscript game-instance subsystem"), OutSubsystem);
	}

	static bool VerifyTickAdvancesProbe(
		FAutomationTestBase& Test,
		UAngelscriptGameInstanceSubsystem& Subsystem,
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
			UAngelscriptGameInstanceSubsystem* AdoptSubsystem = nullptr;
			ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, AdoptSpawner, AdoptWorld, AdoptGameInstance, AdoptSubsystem)));

			ASSERT_THAT(IsTrue(AdoptSubsystem->GetEngine() == &Engine, TEXT("Adopt case should reuse the outer engine when one is already active")));
			ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Adopt case should register an active tick owner")));
			ASSERT_THAT(IsTrue(AdoptSubsystem->IsAllowedToTick(), TEXT("Adopt case should allow the subsystem to tick")));

			{
				FScopedTestWorldContextScope WorldContextScope(AdoptWorld);
				ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::GetCurrent() == AdoptSubsystem, TEXT("Adopt case should resolve GetCurrent() from the ambient world")));
			}

			ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == &Engine, TEXT("Adopt case should keep the shared test engine as the current engine while the outer scope is active")));
			ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *AdoptSubsystem, TEXT("Adopt case"))));

			AdoptSubsystem->Deinitialize();
			ASSERT_THAT(IsNull(AdoptSubsystem->GetEngine(), TEXT("Adopt case should clear the subsystem primary engine during deinitialize")));
			ASSERT_THAT(IsFalse(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Adopt case should release its active tick owner during deinitialize")));
			ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == &Engine, TEXT("Adopt case should restore the shared outer engine after subsystem deinitialize")));
		}

		{
			FCoreTestContextStackGuard ContextGuard;
			ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Own case should begin without a current engine on the cleared context stack")));

			FActorTestSpawner OwnSpawner;
			UWorld* OwnWorld = nullptr;
			UGameInstance* OwnGameInstance = nullptr;
			UAngelscriptGameInstanceSubsystem* OwnSubsystem = nullptr;
			ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, OwnSpawner, OwnWorld, OwnGameInstance, OwnSubsystem)));

			FAngelscriptEngine* OwnedEngine = OwnSubsystem->GetEngine();
			ASSERT_THAT(IsNotNull(OwnedEngine, TEXT("Own case should create a subsystem-owned primary engine")));
			ASSERT_THAT(IsTrue(OwnedEngine != &Engine, TEXT("Own case should create a different primary engine when no outer engine is active")));
			ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Own case should register an active tick owner")));
			ASSERT_THAT(IsTrue(OwnSubsystem->IsAllowedToTick(), TEXT("Own case should allow the subsystem to tick")));
			ASSERT_THAT(IsTrue(OwnedEngine->ShouldTick(), TEXT("Own case should expose a tickable owned engine")));

			{
				FScopedTestWorldContextScope WorldContextScope(OwnWorld);
				ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::GetCurrent() == OwnSubsystem, TEXT("Own case should resolve GetCurrent() from the ambient world")));
				ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == OwnedEngine, TEXT("Own case should resolve the subsystem-owned engine as current when ambient world context is available")));
			}

			ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *OwnSubsystem, TEXT("Own case"))));

			OwnSubsystem->Deinitialize();
			ASSERT_THAT(IsNull(OwnSubsystem->GetEngine(), TEXT("Own case should clear the primary engine during deinitialize")));
			ASSERT_THAT(IsFalse(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Own case should release its active tick owner during deinitialize")));
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
		UAngelscriptGameInstanceSubsystem*& OutSubsystem)
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

		OutSubsystem = OutGameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
		return Test.TestNotNull(TEXT("Subsystem runtime test case should expose the Angelscript game-instance subsystem"), OutSubsystem);
	}

	static bool VerifyTickAdvancesProbe(
		FAutomationTestBase& Test,
		UAngelscriptGameInstanceSubsystem& Subsystem,
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
		UAngelscriptGameInstanceSubsystem* SubsystemA = nullptr;
		UAngelscriptGameInstanceSubsystem* SubsystemB = nullptr;
		ON_SCOPE_EXIT
		{
			if (SubsystemB != nullptr && FAngelscriptTickBehaviorTestAccess::GetSubsystemInitialized(*SubsystemB)) { SubsystemB->Deinitialize(); }
			if (SubsystemA != nullptr && FAngelscriptTickBehaviorTestAccess::GetSubsystemInitialized(*SubsystemA)) { SubsystemA->Deinitialize(); }
			if (!UAngelscriptGameInstanceSubsystem::HasAnyTickOwner() && FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Multi-owner lifecycle should begin without a current engine on the cleared context stack")));
		ASSERT_THAT(IsFalse(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should begin without active tick owners")));

		FActorTestSpawner SpawnerA;
		ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, SpawnerA, WorldA, GameInstanceA, SubsystemA)));

		FAngelscriptEngine* EngineA = SubsystemA->GetEngine();
		ASSERT_THAT(IsNotNull(EngineA, TEXT("Multi-owner lifecycle should create the first subsystem-owned engine")));
		ASSERT_THAT(IsTrue(FAngelscriptTickBehaviorTestAccess::GetSubsystemOwnsPrimaryEngine(*SubsystemA), TEXT("Multi-owner lifecycle should keep subsystem A owning the primary engine")));
		ASSERT_THAT(IsTrue(FAngelscriptTickBehaviorTestAccess::GetSubsystemInitialized(*SubsystemA), TEXT("Multi-owner lifecycle should mark the first subsystem as initialized")));
		ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem A initializes")));
		ASSERT_THAT(AreEqual(1, FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), TEXT("Multi-owner lifecycle should register one active tick owner after subsystem A initializes")));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == EngineA, TEXT("Multi-owner lifecycle should keep subsystem A's engine current after its initialization")));
		ASSERT_THAT(IsTrue(SubsystemA->IsAllowedToTick(), TEXT("Multi-owner lifecycle should keep subsystem A tickable while it owns the engine")));
		ASSERT_THAT(IsTrue(EngineA->ShouldTick(), TEXT("Multi-owner lifecycle should keep subsystem A's engine tickable while it owns the engine")));

		FActorTestSpawner SpawnerB;
		ASSERT_THAT(IsTrue(InitializeRuntimeSubsystemTestCase(*TestRunner, SpawnerB, WorldB, GameInstanceB, SubsystemB)));
		ASSERT_THAT(IsTrue(WorldA != WorldB, TEXT("Multi-owner lifecycle should create independent worlds for subsystem A and B")));
		ASSERT_THAT(IsTrue(GameInstanceA != GameInstanceB, TEXT("Multi-owner lifecycle should create independent game instances for subsystem A and B")));

		FAngelscriptEngine* EngineB = SubsystemB->GetEngine();
		ASSERT_THAT(IsTrue(EngineB == EngineA, TEXT("Multi-owner lifecycle should reuse subsystem A's engine for subsystem B")));
		ASSERT_THAT(IsFalse(FAngelscriptTickBehaviorTestAccess::GetSubsystemOwnsPrimaryEngine(*SubsystemB), TEXT("Multi-owner lifecycle should keep subsystem B on the adopt path")));
		ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem B initializes")));
		ASSERT_THAT(AreEqual(2, FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), TEXT("Multi-owner lifecycle should register two active tick owners after subsystem B initializes")));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == EngineA, TEXT("Multi-owner lifecycle should keep subsystem A's engine current while both owners are alive")));
		ASSERT_THAT(IsTrue(SubsystemB->IsAllowedToTick(), TEXT("Multi-owner lifecycle should keep subsystem B tickable while it borrows subsystem A's engine")));
		ASSERT_THAT(IsTrue(EngineA->ShouldTick(), TEXT("Multi-owner lifecycle should keep the shared engine tickable while both owners are alive")));
		ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *SubsystemB, TEXT("Multi-owner lifecycle while both subsystems are alive"))));

		SubsystemB->Deinitialize();
		ASSERT_THAT(IsNull(SubsystemB->GetEngine(), TEXT("Multi-owner lifecycle should clear subsystem B's primary engine during deinitialize")));
		ASSERT_THAT(IsTrue(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem B deinitializes")));
		ASSERT_THAT(AreEqual(1, FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), TEXT("Multi-owner lifecycle should keep one active tick owner after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == EngineA, TEXT("Multi-owner lifecycle should keep subsystem A's engine current after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(SubsystemA->IsAllowedToTick(), TEXT("Multi-owner lifecycle should keep subsystem A tickable after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(EngineA->ShouldTick(), TEXT("Multi-owner lifecycle should keep subsystem A's engine tickable after subsystem B deinitializes")));
		ASSERT_THAT(IsTrue(VerifyTickAdvancesProbe(*TestRunner, *SubsystemA, TEXT("Multi-owner lifecycle after subsystem B deinitializes"))));

		SubsystemA->Deinitialize();
		ASSERT_THAT(IsNull(SubsystemA->GetEngine(), TEXT("Multi-owner lifecycle should clear subsystem A's primary engine during final deinitialize")));
		ASSERT_THAT(IsFalse(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should release the final tick owner during final deinitialize")));
		ASSERT_THAT(AreEqual(0, FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), TEXT("Multi-owner lifecycle should clear the active tick owner count after the last shutdown")));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Multi-owner lifecycle should clear the current engine after the last owner shuts down")));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("Multi-owner lifecycle should leave no current engine after cleanup")));
		ASSERT_THAT(IsFalse(UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(), TEXT("Multi-owner lifecycle should leave no active tick owners after cleanup")));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptGameInstanceSubsystemTickPolicyTests,
	"Angelscript.TestModule.GameInstanceSubsystem.TickPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GatesTemplateAndInitializationState)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UAngelscriptGameInstanceSubsystem* SubsystemCDO = GetMutableDefault<UAngelscriptGameInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(SubsystemCDO, TEXT("TickPolicy test should resolve the subsystem CDO")));
		ASSERT_THAT(AreEqual(ETickableTickType::Never, SubsystemCDO->GetTickableTickType(), TEXT("TickPolicy test should force the subsystem CDO to never tick")));
		ASSERT_THAT(IsFalse(SubsystemCDO->IsAllowedToTick(), TEXT("TickPolicy test should reject the subsystem CDO from ticking")));

		TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
		UAngelscriptGameInstanceSubsystem* Subsystem = NewObject<UAngelscriptGameInstanceSubsystem>(GameInstance.Get());
		ASSERT_THAT(IsNotNull(Subsystem, TEXT("TickPolicy test should create a live subsystem instance")));

		const FAngelscriptEngine* SavedPrimaryEngine = FAngelscriptTickBehaviorTestAccess::GetSubsystemPrimaryEngine(*Subsystem);
		const bool bSavedOwnsPrimaryEngine = FAngelscriptTickBehaviorTestAccess::GetSubsystemOwnsPrimaryEngine(*Subsystem);
		const bool bSavedInitialized = FAngelscriptTickBehaviorTestAccess::GetSubsystemInitialized(*Subsystem);
		const int32 SavedActiveTickOwners = FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners();
		ON_SCOPE_EXIT
		{
			FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, const_cast<FAngelscriptEngine*>(SavedPrimaryEngine));
			FAngelscriptTickBehaviorTestAccess::SetSubsystemOwnsPrimaryEngine(*Subsystem, bSavedOwnsPrimaryEngine);
			FAngelscriptTickBehaviorTestAccess::SetSubsystemInitialized(*Subsystem, bSavedInitialized);
			FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(SavedActiveTickOwners);
		};

		FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, nullptr);
		FAngelscriptTickBehaviorTestAccess::SetSubsystemOwnsPrimaryEngine(*Subsystem, false);
		FAngelscriptTickBehaviorTestAccess::SetSubsystemInitialized(*Subsystem, false);
		FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(0);

		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should keep an uninitialized subsystem gated")));

		FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, &Engine);
		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should keep a subsystem with an engine but no initialization gated")));

		FAngelscriptTickBehaviorTestAccess::SetSubsystemInitialized(*Subsystem, true);
		FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(1);
		ASSERT_THAT(IsTrue(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should allow a live subsystem to tick only after initialization and engine injection")));
		ASSERT_THAT(IsTrue(Subsystem->IsTickableInEditor(), TEXT("TickPolicy test should remain tickable in editor for live subsystem instances")));
		ASSERT_THAT(IsTrue(Subsystem->IsTickableWhenPaused(), TEXT("TickPolicy test should remain tickable while paused for live subsystem instances")));

		FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, nullptr);
		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should close the gate immediately when the primary engine is cleared")));

		FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, &Engine);
		ASSERT_THAT(IsTrue(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should reopen the gate when the primary engine is restored and initialization remains true")));

		FAngelscriptTickBehaviorTestAccess::SetSubsystemInitialized(*Subsystem, false);
		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick(), TEXT("TickPolicy test should close the gate immediately when initialization is revoked")));
	}
};

#endif
