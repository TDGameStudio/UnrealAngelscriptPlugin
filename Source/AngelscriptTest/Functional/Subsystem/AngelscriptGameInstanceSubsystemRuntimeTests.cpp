#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

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

namespace AngelscriptTest_Subsystem_AngelscriptGameInstanceSubsystemRuntimeTests_Private
{
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

	bool InitializeRuntimeSubsystemTestCase(
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

	bool VerifyTickAdvancesProbe(
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
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGameInstanceSubsystemRuntimeLifecycleTest,
	"Angelscript.TestModule.GameInstanceSubsystem.InitializeAdoptsOrOwnsEngineAndTicksIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGameInstanceSubsystemTickPolicyTest,
	"Angelscript.TestModule.GameInstanceSubsystem.TickPolicy.GatesTemplateAndInitializationState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGameInstanceSubsystemMultiOwnerLifecycleTest,
	"Angelscript.TestModule.GameInstanceSubsystem.MultiOwnerLifecycle.SharedPrimaryEngineKeepsTickOwnershipUntilLastShutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGameInstanceSubsystemRuntimeLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Subsystem_AngelscriptGameInstanceSubsystemRuntimeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	{
		FActorTestSpawner AdoptSpawner;
		UWorld* AdoptWorld = nullptr;
		UGameInstance* AdoptGameInstance = nullptr;
		UAngelscriptGameInstanceSubsystem* AdoptSubsystem = nullptr;
		if (!InitializeRuntimeSubsystemTestCase(*this, AdoptSpawner, AdoptWorld, AdoptGameInstance, AdoptSubsystem))
		{
			return false;
		}

		if (!TestTrue(
			TEXT("Adopt case should reuse the outer engine when one is already active"),
			AdoptSubsystem->GetEngine() == &Engine))
		{
			return false;
		}

		if (!TestTrue(TEXT("Adopt case should register an active tick owner"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner()))
		{
			return false;
		}

		if (!TestTrue(TEXT("Adopt case should allow the subsystem to tick"), AdoptSubsystem->IsAllowedToTick()))
		{
			return false;
		}

		{
			FScopedTestWorldContextScope WorldContextScope(AdoptWorld);
			if (!TestTrue(TEXT("Adopt case should resolve GetCurrent() from the ambient world"), UAngelscriptGameInstanceSubsystem::GetCurrent() == AdoptSubsystem))
			{
				return false;
			}
		}

		if (!TestTrue(TEXT("Adopt case should keep the shared test engine as the current engine while the outer scope is active"), FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			return false;
		}

		if (!VerifyTickAdvancesProbe(*this, *AdoptSubsystem, TEXT("Adopt case")))
		{
			return false;
		}

		AdoptSubsystem->Deinitialize();
		if (!TestNull(TEXT("Adopt case should clear the subsystem primary engine during deinitialize"), AdoptSubsystem->GetEngine()))
		{
			return false;
		}

		if (!TestFalse(TEXT("Adopt case should release its active tick owner during deinitialize"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner()))
		{
			return false;
		}

		if (!TestTrue(TEXT("Adopt case should restore the shared outer engine after subsystem deinitialize"), FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			return false;
		}
	}

	{
		FCoreTestContextStackGuard ContextGuard;
		if (!TestNull(TEXT("Own case should begin without a current engine on the cleared context stack"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return false;
		}

		FActorTestSpawner OwnSpawner;
		UWorld* OwnWorld = nullptr;
		UGameInstance* OwnGameInstance = nullptr;
		UAngelscriptGameInstanceSubsystem* OwnSubsystem = nullptr;
		if (!InitializeRuntimeSubsystemTestCase(*this, OwnSpawner, OwnWorld, OwnGameInstance, OwnSubsystem))
		{
			return false;
		}

		FAngelscriptEngine* OwnedEngine = OwnSubsystem->GetEngine();
		if (!TestNotNull(TEXT("Own case should create a subsystem-owned primary engine"), OwnedEngine))
		{
			return false;
		}

		if (!TestTrue(TEXT("Own case should create a different primary engine when no outer engine is active"), OwnedEngine != &Engine))
		{
			return false;
		}

		if (!TestTrue(TEXT("Own case should register an active tick owner"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner()))
		{
			return false;
		}

		if (!TestTrue(TEXT("Own case should allow the subsystem to tick"), OwnSubsystem->IsAllowedToTick()))
		{
			return false;
		}

		if (!TestTrue(TEXT("Own case should expose a tickable owned engine"), OwnedEngine->ShouldTick()))
		{
			return false;
		}

		{
			FScopedTestWorldContextScope WorldContextScope(OwnWorld);
			if (!TestTrue(TEXT("Own case should resolve GetCurrent() from the ambient world"), UAngelscriptGameInstanceSubsystem::GetCurrent() == OwnSubsystem))
			{
				return false;
			}

			if (!TestTrue(TEXT("Own case should resolve the subsystem-owned engine as current when ambient world context is available"), FAngelscriptEngine::TryGetCurrentEngine() == OwnedEngine))
			{
				return false;
			}
		}

		if (!VerifyTickAdvancesProbe(*this, *OwnSubsystem, TEXT("Own case")))
		{
			return false;
		}

		OwnSubsystem->Deinitialize();
		if (!TestNull(TEXT("Own case should clear the primary engine during deinitialize"), OwnSubsystem->GetEngine()))
		{
			return false;
		}

		if (!TestFalse(TEXT("Own case should release its active tick owner during deinitialize"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner()))
		{
			return false;
		}

		if (!TestNull(TEXT("Own case should no longer resolve a current engine after the subsystem deinitializes"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return false;
		}
	}

	TestTrue(TEXT("Leaving the cleared-stack own case should restore the shared test engine scope"), FAngelscriptEngine::TryGetCurrentEngine() == &Engine);
	}

	return true;
}

bool FAngelscriptGameInstanceSubsystemMultiOwnerLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Subsystem_AngelscriptGameInstanceSubsystemRuntimeTests_Private;
	FCoreTestContextStackGuard ContextGuard;
	DestroySharedTestEngine();
	if (FAngelscriptEngine::IsInitialized())
	{
		FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
	}
	ContextGuard.DiscardSavedStack();
	{
		UWorld* WorldA = nullptr; UWorld* WorldB = nullptr;
		UGameInstance* GameInstanceA = nullptr; UGameInstance* GameInstanceB = nullptr;
		UAngelscriptGameInstanceSubsystem* SubsystemA = nullptr; UAngelscriptGameInstanceSubsystem* SubsystemB = nullptr;
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

		if (!TestNull(TEXT("Multi-owner lifecycle should begin without a current engine on the cleared context stack"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return false;
		}
		if (!TestFalse(TEXT("Multi-owner lifecycle should begin without active tick owners"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner()))
		{
			return false;
		}

		FActorTestSpawner SpawnerA;
		if (!InitializeRuntimeSubsystemTestCase(*this, SpawnerA, WorldA, GameInstanceA, SubsystemA))
		{
			return false;
		}

		FAngelscriptEngine* EngineA = SubsystemA->GetEngine();
		if (!TestNotNull(TEXT("Multi-owner lifecycle should create the first subsystem-owned engine"), EngineA)
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A owning the primary engine"), FAngelscriptTickBehaviorTestAccess::GetSubsystemOwnsPrimaryEngine(*SubsystemA))
			|| !TestTrue(TEXT("Multi-owner lifecycle should mark the first subsystem as initialized"), FAngelscriptTickBehaviorTestAccess::GetSubsystemInitialized(*SubsystemA))
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem A initializes"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner())
			|| !TestEqual(TEXT("Multi-owner lifecycle should register one active tick owner after subsystem A initializes"), FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), 1)
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A's engine current after its initialization"), FAngelscriptEngine::TryGetCurrentEngine() == EngineA))
		{
			return false;
		}
		if (!TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A tickable while it owns the engine"), SubsystemA->IsAllowedToTick())
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A's engine tickable while it owns the engine"), EngineA->ShouldTick()))
		{
			return false;
		}

		FActorTestSpawner SpawnerB;
		if (!InitializeRuntimeSubsystemTestCase(*this, SpawnerB, WorldB, GameInstanceB, SubsystemB))
		{
			return false;
		}

		if (!TestTrue(TEXT("Multi-owner lifecycle should create independent worlds for subsystem A and B"), WorldA != WorldB)
			|| !TestTrue(TEXT("Multi-owner lifecycle should create independent game instances for subsystem A and B"), GameInstanceA != GameInstanceB))
		{
			return false;
		}

		FAngelscriptEngine* EngineB = SubsystemB->GetEngine();
		if (!TestTrue(TEXT("Multi-owner lifecycle should reuse subsystem A's engine for subsystem B"), EngineB == EngineA)
			|| !TestFalse(TEXT("Multi-owner lifecycle should keep subsystem B on the adopt path"), FAngelscriptTickBehaviorTestAccess::GetSubsystemOwnsPrimaryEngine(*SubsystemB))
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem B initializes"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner())
			|| !TestEqual(TEXT("Multi-owner lifecycle should register two active tick owners after subsystem B initializes"), FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), 2)
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A's engine current while both owners are alive"), FAngelscriptEngine::TryGetCurrentEngine() == EngineA))
		{
			return false;
		}
		if (!TestTrue(TEXT("Multi-owner lifecycle should keep subsystem B tickable while it borrows subsystem A's engine"), SubsystemB->IsAllowedToTick())
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep the shared engine tickable while both owners are alive"), EngineA->ShouldTick()))
		{
			return false;
		}
		if (!VerifyTickAdvancesProbe(*this, *SubsystemB, TEXT("Multi-owner lifecycle while both subsystems are alive")))
		{
			return false;
		}

		SubsystemB->Deinitialize();
		if (!TestNull(TEXT("Multi-owner lifecycle should clear subsystem B's primary engine during deinitialize"), SubsystemB->GetEngine())
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep tick ownership active after subsystem B deinitializes"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner())
			|| !TestEqual(TEXT("Multi-owner lifecycle should keep one active tick owner after subsystem B deinitializes"), FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), 1)
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A's engine current after subsystem B deinitializes"), FAngelscriptEngine::TryGetCurrentEngine() == EngineA))
		{
			return false;
		}
		if (!TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A tickable after subsystem B deinitializes"), SubsystemA->IsAllowedToTick())
			|| !TestTrue(TEXT("Multi-owner lifecycle should keep subsystem A's engine tickable after subsystem B deinitializes"), EngineA->ShouldTick()))
		{
			return false;
		}
		if (!VerifyTickAdvancesProbe(*this, *SubsystemA, TEXT("Multi-owner lifecycle after subsystem B deinitializes")))
		{
			return false;
		}

		SubsystemA->Deinitialize();
		if (!TestNull(TEXT("Multi-owner lifecycle should clear subsystem A's primary engine during final deinitialize"), SubsystemA->GetEngine())
			|| !TestFalse(TEXT("Multi-owner lifecycle should release the final tick owner during final deinitialize"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner())
			|| !TestEqual(TEXT("Multi-owner lifecycle should clear the active tick owner count after the last shutdown"), FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners(), 0)
			|| !TestNull(TEXT("Multi-owner lifecycle should clear the current engine after the last owner shuts down"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return false;
		}
	}

	return TestNull(TEXT("Multi-owner lifecycle should leave no current engine after cleanup"), FAngelscriptEngine::TryGetCurrentEngine())
		&& TestFalse(TEXT("Multi-owner lifecycle should leave no active tick owners after cleanup"), UAngelscriptGameInstanceSubsystem::HasAnyTickOwner());
}

bool FAngelscriptGameInstanceSubsystemTickPolicyTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Subsystem_AngelscriptGameInstanceSubsystemRuntimeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	UAngelscriptGameInstanceSubsystem* SubsystemCDO = GetMutableDefault<UAngelscriptGameInstanceSubsystem>();
	if (!TestNotNull(TEXT("TickPolicy test should resolve the subsystem CDO"), SubsystemCDO))
	{
		return false;
	}

	if (!TestEqual(TEXT("TickPolicy test should force the subsystem CDO to never tick"), SubsystemCDO->GetTickableTickType(), ETickableTickType::Never))
	{
		return false;
	}

	if (!TestFalse(TEXT("TickPolicy test should reject the subsystem CDO from ticking"), SubsystemCDO->IsAllowedToTick()))
	{
		return false;
	}

	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	UAngelscriptGameInstanceSubsystem* Subsystem = NewObject<UAngelscriptGameInstanceSubsystem>(GameInstance.Get());
	if (!TestNotNull(TEXT("TickPolicy test should create a live subsystem instance"), Subsystem))
	{
		return false;
	}

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

	if (!TestFalse(TEXT("TickPolicy test should keep an uninitialized subsystem gated"), Subsystem->IsAllowedToTick()))
	{
		return false;
	}

	FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, &Engine);
	if (!TestFalse(TEXT("TickPolicy test should keep a subsystem with an engine but no initialization gated"), Subsystem->IsAllowedToTick()))
	{
		return false;
	}

	FAngelscriptTickBehaviorTestAccess::SetSubsystemInitialized(*Subsystem, true);
	FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(1);

	if (!TestTrue(TEXT("TickPolicy test should allow a live subsystem to tick only after initialization and engine injection"), Subsystem->IsAllowedToTick()))
	{
		return false;
	}

	if (!TestTrue(TEXT("TickPolicy test should remain tickable in editor for live subsystem instances"), Subsystem->IsTickableInEditor()))
	{
		return false;
	}

	if (!TestTrue(TEXT("TickPolicy test should remain tickable while paused for live subsystem instances"), Subsystem->IsTickableWhenPaused()))
	{
		return false;
	}

	FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, nullptr);
	if (!TestFalse(TEXT("TickPolicy test should close the gate immediately when the primary engine is cleared"), Subsystem->IsAllowedToTick()))
	{
		return false;
	}

	FAngelscriptTickBehaviorTestAccess::SetSubsystemPrimaryEngineRaw(*Subsystem, &Engine);
	if (!TestTrue(TEXT("TickPolicy test should reopen the gate when the primary engine is restored and initialization remains true"), Subsystem->IsAllowedToTick()))
	{
		return false;
	}

	FAngelscriptTickBehaviorTestAccess::SetSubsystemInitialized(*Subsystem, false);
	if (!TestFalse(TEXT("TickPolicy test should close the gate immediately when initialization is revoked"), Subsystem->IsAllowedToTick()))
	{
		return false;
	}

	}

	return true;
}

#endif
