#include "AngelscriptEngine.h"
#include "AngelscriptEngineSubsystem.h"
#include "AngelscriptGameInstanceSubsystem.h"
#include "AngelscriptRuntimeModule.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptRuntimeModuleTests_Private
{
	struct FRuntimeModuleContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FRuntimeModuleContextStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FRuntimeModuleContextStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

		void DiscardSavedStack()
		{
			SavedStack.Reset();
		}
	};
}


struct FAngelscriptRuntimeModuleTickTestAccess
{
	static void SetInitializeOverride(TFunction<FAngelscriptEngine*()> InOverride)
	{
		FAngelscriptRuntimeModule::SetInitializeOverrideForTesting(MoveTemp(InOverride));
	}

	static void ResetInitializeState()
	{
		FAngelscriptRuntimeModule::ResetInitializeStateForTesting();
	}

	static bool HasOwnedPrimaryEngine()
	{
		return FAngelscriptRuntimeModule::OwnedPrimaryEngine.IsValid();
	}

	static bool WasInitializeAngelscriptCalled()
	{
		return FAngelscriptRuntimeModule::bInitializeAngelscriptCalled;
	}

	static FAngelscriptEngine* GetOwnedPrimaryEngine()
	{
		return FAngelscriptRuntimeModule::OwnedPrimaryEngine.Get();
	}
};

struct FAngelscriptTickBehaviorTestAccess
{
	static int32 GetActiveTickOwners()
	{
		return UAngelscriptGameInstanceSubsystem::ActiveTickOwners;
	}

	static void SetActiveTickOwners(const int32 InValue)
	{
		UAngelscriptGameInstanceSubsystem::ActiveTickOwners = InValue;
	}

	static double GetNextHotReloadCheck(const FAngelscriptEngine& Engine)
	{
		return Engine.NextHotReloadCheck;
	}

	static void PrepareTickProbe(FAngelscriptEngine& Engine, const double InNextHotReloadCheck)
	{
		Engine.bScriptDevelopmentMode = true;
		Engine.bUseHotReloadCheckerThread = true;
		Engine.bWaitingForHotReloadResults = false;
		Engine.NextHotReloadCheck = InNextHotReloadCheck;
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptRuntimeModuleTests,
	"Angelscript.TestModule.Engine.RuntimeModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InitializeOverrideIsIdempotentAndRestorable)
	{
		using namespace AngelscriptTest_Core_AngelscriptRuntimeModuleTests_Private;
		FRuntimeModuleContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
		if (!TestRunner->TestNull(TEXT("RuntimeModule initialize-override test should start without a current engine"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> OverrideEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("RuntimeModule initialize-override test should create an isolated override engine"), OverrideEngine.Get()))
		{
			return;
		}

		FAngelscriptRuntimeModuleTickTestAccess::SetInitializeOverride([&OverrideEngine]()
		{
			return OverrideEngine.Get();
		});

		FAngelscriptRuntimeModule::InitializeAngelscript();
		if (!TestRunner->TestTrue(
				TEXT("RuntimeModule initialize-override test should make the override engine current after first initialize"),
				FAngelscriptEngine::TryGetCurrentEngine() == OverrideEngine.Get()))
		{
			return;
		}

		FAngelscriptRuntimeModule::InitializeAngelscript();

		TArray<FAngelscriptEngine*> StackAfterSecondInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
		if (!TestRunner->TestEqual(
				TEXT("RuntimeModule initialize-override test should keep exactly one engine on the context stack after repeated initialize"),
				StackAfterSecondInitialize.Num(),
				1)
			|| !TestRunner->TestTrue(
				TEXT("RuntimeModule initialize-override test should keep the override engine as the only stack entry"),
				StackAfterSecondInitialize.Num() == 1 && StackAfterSecondInitialize[0] == OverrideEngine.Get()))
		{
			return;
		}

		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterSecondInitialize));
		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();

		if (!TestRunner->TestNull(
				TEXT("RuntimeModule initialize-override test should clear the current engine after reset"),
				FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		const TArray<FAngelscriptEngine*> StackAfterReset = FAngelscriptEngineContextStack::SnapshotAndClear();
		TestRunner->TestEqual(
			TEXT("RuntimeModule initialize-override test should leave the context stack empty after reset"),
			StackAfterReset.Num(),
			0);
	}

	TEST_METHOD(InitializeRoutesToEngineSubsystem)
	{
		using namespace AngelscriptTest_Core_AngelscriptRuntimeModuleTests_Private;
		FRuntimeModuleContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
		FAngelscriptRuntimeModule RuntimeModule;
		UAngelscriptEngineSubsystem* EngineSubsystem = UAngelscriptEngineSubsystem::Get();
		if (!TestRunner->TestNotNull(TEXT("RuntimeModule subsystem-route test should have an engine subsystem in editor automation"), EngineSubsystem))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			RuntimeModule.ShutdownModule();
			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		if (!TestRunner->TestNull(TEXT("RuntimeModule shutdown test should start without a current engine"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		FAngelscriptRuntimeModule::InitializeAngelscript();
		if (!TestRunner->TestFalse(
				TEXT("RuntimeModule subsystem-route test should not create a module-owned primary engine when the engine subsystem exists"),
				FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine()))
		{
			return;
		}

		FAngelscriptEngine* SubsystemEngine = EngineSubsystem->GetEngine();
		if (!TestRunner->TestNotNull(
				TEXT("RuntimeModule subsystem-route test should expose the subsystem primary engine instance"),
				SubsystemEngine))
		{
			return;
		}
		if (!TestRunner->TestTrue(
				TEXT("RuntimeModule subsystem-route test should make the subsystem primary engine current"),
				FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine))
		{
			return;
		}

		RuntimeModule.ShutdownModule();

		TestRunner->TestFalse(
			TEXT("RuntimeModule subsystem-route test should still not own the primary engine after shutdown"),
			FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine());
		TestRunner->TestTrue(
			TEXT("RuntimeModule subsystem-route test should leave the subsystem primary engine current after module shutdown"),
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine);

		TArray<FAngelscriptEngine*> StackAfterFirstShutdown = FAngelscriptEngineContextStack::SnapshotAndClear();
		TestRunner->TestEqual(
			TEXT("RuntimeModule subsystem-route test should leave exactly one subsystem engine on the context stack after first shutdown"),
			StackAfterFirstShutdown.Num(),
			1);
		TestRunner->TestTrue(
			TEXT("RuntimeModule subsystem-route test should keep the subsystem engine as the only stack entry"),
			StackAfterFirstShutdown.Num() == 1 && StackAfterFirstShutdown[0] == SubsystemEngine);

		RuntimeModule.ShutdownModule();

		TestRunner->TestFalse(
			TEXT("RuntimeModule subsystem-route test should keep module-owned engine absent on repeated shutdown"),
			FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine());
		TestRunner->TestNull(
			TEXT("RuntimeModule subsystem-route test should keep the current engine cleared after the test manually clears the stack"),
			FAngelscriptEngine::TryGetCurrentEngine());

		TArray<FAngelscriptEngine*> StackAfterSecondShutdown = FAngelscriptEngineContextStack::SnapshotAndClear();
		TestRunner->TestEqual(
			TEXT("RuntimeModule subsystem-route test should keep the context stack empty after manual clear and repeated shutdown"),
			StackAfterSecondShutdown.Num(),
			0);
	}

	TEST_METHOD(StartupModuleDoesNotBootstrapPrimaryEngine)
	{
		using namespace AngelscriptTest_Core_AngelscriptRuntimeModuleTests_Private;
		FRuntimeModuleContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		int32 InitializeCalls = 0;
		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
		FAngelscriptRuntimeModuleTickTestAccess::SetInitializeOverride([&InitializeCalls]()
		{
			++InitializeCalls;
			return nullptr;
		});

		if (!TestRunner->TestNull(TEXT("RuntimeModule startup test should start without a current engine"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		FAngelscriptRuntimeModule RuntimeModule;
		RuntimeModule.StartupModule();

		TestRunner->TestEqual(
			TEXT("RuntimeModule startup test should not call compatibility initialization"),
			InitializeCalls,
			0);
		TestRunner->TestFalse(
			TEXT("RuntimeModule startup test should leave InitializeAngelscript uncalled"),
			FAngelscriptRuntimeModuleTickTestAccess::WasInitializeAngelscriptCalled());
		TestRunner->TestNull(
			TEXT("RuntimeModule startup test should leave the context stack empty"),
			FAngelscriptEngine::TryGetCurrentEngine());

		RuntimeModule.ShutdownModule();

		const TArray<FAngelscriptEngine*> StackAfterStartup = FAngelscriptEngineContextStack::SnapshotAndClear();
		TestRunner->TestEqual(
			TEXT("RuntimeModule startup test should leave the context stack empty after shutdown"),
			StackAfterStartup.Num(),
			0);
	}

	TEST_METHOD(InitializeAdoptsAmbientEngineWithoutOwningIt)
	{
		using namespace AngelscriptTest_Core_AngelscriptRuntimeModuleTests_Private;
		FRuntimeModuleContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
		if (!TestRunner->TestNull(TEXT("RuntimeModule ambient-initialize test should start without a current engine"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> AmbientEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("RuntimeModule ambient-initialize test should create an isolated ambient engine"), AmbientEngine.Get()))
		{
			return;
		}

		{
			FAngelscriptEngineScope AmbientScope(*AmbientEngine);
			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should make the isolated engine current inside the scope"),
				FAngelscriptEngine::TryGetCurrentEngine() == AmbientEngine.Get());

			TArray<FAngelscriptEngine*> StackBeforeInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
			TestRunner->TestEqual(
				TEXT("RuntimeModule ambient-initialize test should start with exactly one ambient engine on the context stack"),
				StackBeforeInitialize.Num(),
				1);
			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine as the only pre-initialize stack entry"),
				StackBeforeInitialize.Num() == 1 && StackBeforeInitialize[0] == AmbientEngine.Get());
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackBeforeInitialize));

			FAngelscriptRuntimeModule::InitializeAngelscript();

			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should mark initialize as called after initialization"),
				FAngelscriptRuntimeModuleTickTestAccess::WasInitializeAngelscriptCalled());
			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine current after initialization"),
				FAngelscriptEngine::TryGetCurrentEngine() == AmbientEngine.Get());
			TestRunner->TestFalse(
				TEXT("RuntimeModule ambient-initialize test should not create an owned primary engine when an ambient engine already exists"),
				FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine());

			TArray<FAngelscriptEngine*> StackAfterInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
			TestRunner->TestEqual(
				TEXT("RuntimeModule ambient-initialize test should keep the context stack depth unchanged after initialization"),
				StackAfterInitialize.Num(),
				1);
			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine as the only stack entry after initialization"),
				StackAfterInitialize.Num() == 1 && StackAfterInitialize[0] == AmbientEngine.Get());
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterInitialize));

			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();

			TestRunner->TestFalse(
				TEXT("RuntimeModule ambient-initialize test should clear the initialize-called flag on reset"),
				FAngelscriptRuntimeModuleTickTestAccess::WasInitializeAngelscriptCalled());
			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should preserve the ambient current engine after reset"),
				FAngelscriptEngine::TryGetCurrentEngine() == AmbientEngine.Get());
			TestRunner->TestFalse(
				TEXT("RuntimeModule ambient-initialize test should still avoid owned-engine creation after reset"),
				FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine());

			TArray<FAngelscriptEngine*> StackAfterReset = FAngelscriptEngineContextStack::SnapshotAndClear();
			TestRunner->TestEqual(
				TEXT("RuntimeModule ambient-initialize test should preserve the ambient stack depth after reset"),
				StackAfterReset.Num(),
				1);
			TestRunner->TestTrue(
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine as the only stack entry after reset"),
				StackAfterReset.Num() == 1 && StackAfterReset[0] == AmbientEngine.Get());
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterReset));
		}

		TestRunner->TestNull(
			TEXT("RuntimeModule ambient-initialize test should clear the current engine after the ambient scope exits"),
			FAngelscriptEngine::TryGetCurrentEngine());
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineSubsystemTickTests,
	"Angelscript.TestModule.Engine.EngineSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(TickRespectsGameInstanceOwnership)
	{
		using namespace AngelscriptTest_Core_AngelscriptRuntimeModuleTests_Private;
		FRuntimeModuleContextStackGuard ContextGuard;
		const int32 SavedActiveTickOwners = FAngelscriptTickBehaviorTestAccess::GetActiveTickOwners();
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(SavedActiveTickOwners);
			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> TestEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("EngineSubsystem tick test should create an isolated full engine"), TestEngine.Get()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*TestEngine);
		if (!TestRunner->TestTrue(TEXT("EngineSubsystem tick test should make the isolated engine current"), FAngelscriptEngine::TryGetCurrentEngine() == TestEngine.Get()))
		{
			return;
		}

		TStrongObjectPtr<UAngelscriptEngineSubsystem> EngineSubsystem(NewObject<UAngelscriptEngineSubsystem>(GetTransientPackage()));
		if (!TestRunner->TestNotNull(TEXT("EngineSubsystem tick test should create a native subsystem object"), EngineSubsystem.Get()))
		{
			return;
		}

		EngineSubsystem->EnsurePrimaryEngineInitialized();

		FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(0);
		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*TestEngine, -1.0);

		TestRunner->TestFalse(
			TEXT("EngineSubsystem tick test should start without game instance tick owners"),
			UAngelscriptGameInstanceSubsystem::HasAnyTickOwner());
		EngineSubsystem->Tick(0.016f);
		TestRunner->TestTrue(
			TEXT("EngineSubsystem tick test should advance NextHotReloadCheck when no game instance owner exists"),
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*TestEngine) > 0.0);

		FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(1);
		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*TestEngine, -1.0);

		TestRunner->TestTrue(
			TEXT("EngineSubsystem tick test should report an active game instance tick owner after setup"),
			UAngelscriptGameInstanceSubsystem::HasAnyTickOwner());
		EngineSubsystem->Tick(0.016f);
		TestRunner->TestEqual(
			TEXT("EngineSubsystem tick test should leave NextHotReloadCheck unchanged while a game instance owner exists"),
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*TestEngine),
			-1.0);

		EngineSubsystem->Deinitialize();
	}
};

#endif
