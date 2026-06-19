#include "AngelscriptEngine.h"
#include "AngelscriptEngineSubsystem.h"
#include "AngelscriptGameInstanceSubsystem.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptTestUtilities.h"

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
		if (!this->Assert.IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("RuntimeModule initialize-override test should start without a current engine")))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> OverrideEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(OverrideEngine.Get(), TEXT("RuntimeModule initialize-override test should create an isolated override engine")))
		{
			return;
		}

		FAngelscriptRuntimeModuleTickTestAccess::SetInitializeOverride([&OverrideEngine]()
		{
			return OverrideEngine.Get();
		});

		FAngelscriptRuntimeModule::InitializeAngelscript();
		if (!this->Assert.IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == OverrideEngine.Get(),
				TEXT("RuntimeModule initialize-override test should make the override engine current after first initialize")))
		{
			return;
		}

		FAngelscriptRuntimeModule::InitializeAngelscript();

		TArray<FAngelscriptEngine*> StackAfterSecondInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
		if (!this->Assert.AreEqual(
				1,
				StackAfterSecondInitialize.Num(),
				TEXT("RuntimeModule initialize-override test should keep exactly one engine on the context stack after repeated initialize"))
			|| !this->Assert.IsTrue(
				StackAfterSecondInitialize.Num() == 1 && StackAfterSecondInitialize[0] == OverrideEngine.Get(),
				TEXT("RuntimeModule initialize-override test should keep the override engine as the only stack entry")))
		{
			return;
		}

		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterSecondInitialize));
		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();

		if (!this->Assert.IsNull(
				FAngelscriptEngine::TryGetCurrentEngine(),
				TEXT("RuntimeModule initialize-override test should clear the current engine after reset")))
		{
			return;
		}

		const TArray<FAngelscriptEngine*> StackAfterReset = FAngelscriptEngineContextStack::SnapshotAndClear();
		(void)this->Assert.AreEqual(
			0,
			StackAfterReset.Num(),
			TEXT("RuntimeModule initialize-override test should leave the context stack empty after reset"));
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
		if (!this->Assert.IsNotNull(EngineSubsystem, TEXT("RuntimeModule subsystem-route test should have an engine subsystem in editor automation")))
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

		if (!this->Assert.IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("RuntimeModule shutdown test should start without a current engine")))
		{
			return;
		}

		FAngelscriptRuntimeModule::InitializeAngelscript();
		if (!this->Assert.IsFalse(
				FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
				TEXT("RuntimeModule subsystem-route test should not create a module-owned primary engine when the engine subsystem exists")))
		{
			return;
		}

		FAngelscriptEngine* SubsystemEngine = EngineSubsystem->GetEngine();
		if (!this->Assert.IsNotNull(
				SubsystemEngine,
				TEXT("RuntimeModule subsystem-route test should expose the subsystem primary engine instance")))
		{
			return;
		}
		if (!this->Assert.IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine,
				TEXT("RuntimeModule subsystem-route test should make the subsystem primary engine current")))
		{
			return;
		}

		RuntimeModule.ShutdownModule();

		bool bOk = true;
		bOk &= this->Assert.IsFalse(
			FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
			TEXT("RuntimeModule subsystem-route test should still not own the primary engine after shutdown"));
		bOk &= this->Assert.IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine,
			TEXT("RuntimeModule subsystem-route test should leave the subsystem primary engine current after module shutdown"));

		TArray<FAngelscriptEngine*> StackAfterFirstShutdown = FAngelscriptEngineContextStack::SnapshotAndClear();
		bOk &= this->Assert.AreEqual(
			1,
			StackAfterFirstShutdown.Num(),
			TEXT("RuntimeModule subsystem-route test should leave exactly one subsystem engine on the context stack after first shutdown"));
		bOk &= this->Assert.IsTrue(
			StackAfterFirstShutdown.Num() == 1 && StackAfterFirstShutdown[0] == SubsystemEngine,
			TEXT("RuntimeModule subsystem-route test should keep the subsystem engine as the only stack entry"));

		RuntimeModule.ShutdownModule();

		bOk &= this->Assert.IsFalse(
			FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
			TEXT("RuntimeModule subsystem-route test should keep module-owned engine absent on repeated shutdown"));
		bOk &= this->Assert.IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("RuntimeModule subsystem-route test should keep the current engine cleared after the test manually clears the stack"));

		TArray<FAngelscriptEngine*> StackAfterSecondShutdown = FAngelscriptEngineContextStack::SnapshotAndClear();
		bOk &= this->Assert.AreEqual(
			0,
			StackAfterSecondShutdown.Num(),
			TEXT("RuntimeModule subsystem-route test should keep the context stack empty after manual clear and repeated shutdown"));
		(void)bOk;
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

		if (!this->Assert.IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("RuntimeModule startup test should start without a current engine")))
		{
			return;
		}

		FAngelscriptRuntimeModule RuntimeModule;
		RuntimeModule.StartupModule();

		bool bOk = true;
		bOk &= this->Assert.AreEqual(
			0,
			InitializeCalls,
			TEXT("RuntimeModule startup test should not call compatibility initialization"));
		bOk &= this->Assert.IsFalse(
			FAngelscriptRuntimeModuleTickTestAccess::WasInitializeAngelscriptCalled(),
			TEXT("RuntimeModule startup test should leave InitializeAngelscript uncalled"));
		bOk &= this->Assert.IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("RuntimeModule startup test should leave the context stack empty"));

		RuntimeModule.ShutdownModule();

		const TArray<FAngelscriptEngine*> StackAfterStartup = FAngelscriptEngineContextStack::SnapshotAndClear();
		bOk &= this->Assert.AreEqual(
			0,
			StackAfterStartup.Num(),
			TEXT("RuntimeModule startup test should leave the context stack empty after shutdown"));
		(void)bOk;
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
		if (!this->Assert.IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("RuntimeModule ambient-initialize test should start without a current engine")))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> AmbientEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(AmbientEngine.Get(), TEXT("RuntimeModule ambient-initialize test should create an isolated ambient engine")))
		{
			return;
		}

		{
			FAngelscriptEngineScope AmbientScope(*AmbientEngine);
			bool bOk = true;
			bOk &= this->Assert.IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == AmbientEngine.Get(),
				TEXT("RuntimeModule ambient-initialize test should make the isolated engine current inside the scope"));

			TArray<FAngelscriptEngine*> StackBeforeInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
			bOk &= this->Assert.AreEqual(
				1,
				StackBeforeInitialize.Num(),
				TEXT("RuntimeModule ambient-initialize test should start with exactly one ambient engine on the context stack"));
			bOk &= this->Assert.IsTrue(
				StackBeforeInitialize.Num() == 1 && StackBeforeInitialize[0] == AmbientEngine.Get(),
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine as the only pre-initialize stack entry"));
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackBeforeInitialize));

			FAngelscriptRuntimeModule::InitializeAngelscript();

			bOk &= this->Assert.IsTrue(
				FAngelscriptRuntimeModuleTickTestAccess::WasInitializeAngelscriptCalled(),
				TEXT("RuntimeModule ambient-initialize test should mark initialize as called after initialization"));
			bOk &= this->Assert.IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == AmbientEngine.Get(),
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine current after initialization"));
			bOk &= this->Assert.IsFalse(
				FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
				TEXT("RuntimeModule ambient-initialize test should not create an owned primary engine when an ambient engine already exists"));

			TArray<FAngelscriptEngine*> StackAfterInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
			bOk &= this->Assert.AreEqual(
				1,
				StackAfterInitialize.Num(),
				TEXT("RuntimeModule ambient-initialize test should keep the context stack depth unchanged after initialization"));
			bOk &= this->Assert.IsTrue(
				StackAfterInitialize.Num() == 1 && StackAfterInitialize[0] == AmbientEngine.Get(),
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine as the only stack entry after initialization"));
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterInitialize));

			FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();

			bOk &= this->Assert.IsFalse(
				FAngelscriptRuntimeModuleTickTestAccess::WasInitializeAngelscriptCalled(),
				TEXT("RuntimeModule ambient-initialize test should clear the initialize-called flag on reset"));
			bOk &= this->Assert.IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == AmbientEngine.Get(),
				TEXT("RuntimeModule ambient-initialize test should preserve the ambient current engine after reset"));
			bOk &= this->Assert.IsFalse(
				FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
				TEXT("RuntimeModule ambient-initialize test should still avoid owned-engine creation after reset"));

			TArray<FAngelscriptEngine*> StackAfterReset = FAngelscriptEngineContextStack::SnapshotAndClear();
			bOk &= this->Assert.AreEqual(
				1,
				StackAfterReset.Num(),
				TEXT("RuntimeModule ambient-initialize test should preserve the ambient stack depth after reset"));
			bOk &= this->Assert.IsTrue(
				StackAfterReset.Num() == 1 && StackAfterReset[0] == AmbientEngine.Get(),
				TEXT("RuntimeModule ambient-initialize test should keep the ambient engine as the only stack entry after reset"));
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterReset));
			(void)bOk;
		}

		(void)this->Assert.IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("RuntimeModule ambient-initialize test should clear the current engine after the ambient scope exits"));
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
		if (!this->Assert.IsNotNull(TestEngine.Get(), TEXT("EngineSubsystem tick test should create an isolated full engine")))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*TestEngine);
		if (!this->Assert.IsTrue(FAngelscriptEngine::TryGetCurrentEngine() == TestEngine.Get(), TEXT("EngineSubsystem tick test should make the isolated engine current")))
		{
			return;
		}

		TStrongObjectPtr<UAngelscriptEngineSubsystem> EngineSubsystem(NewObject<UAngelscriptEngineSubsystem>(GetTransientPackage()));
		if (!this->Assert.IsNotNull(EngineSubsystem.Get(), TEXT("EngineSubsystem tick test should create a native subsystem object")))
		{
			return;
		}

		EngineSubsystem->EnsurePrimaryEngineInitialized();

		FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(0);
		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*TestEngine, -1.0);

		bool bOk = this->Assert.IsFalse(
			UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(),
			TEXT("EngineSubsystem tick test should start without game instance tick owners"));
		EngineSubsystem->Tick(0.016f);
		bOk &= this->Assert.IsTrue(
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*TestEngine) > 0.0,
			TEXT("EngineSubsystem tick test should advance NextHotReloadCheck when no game instance owner exists"));

		FAngelscriptTickBehaviorTestAccess::SetActiveTickOwners(1);
		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*TestEngine, -1.0);

		bOk &= this->Assert.IsTrue(
			UAngelscriptGameInstanceSubsystem::HasAnyTickOwner(),
			TEXT("EngineSubsystem tick test should report an active game instance tick owner after setup"));
		EngineSubsystem->Tick(0.016f);
		bOk &= this->Assert.IsNear(
			-1.0,
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*TestEngine),
			0.0,
			TEXT("EngineSubsystem tick test should leave NextHotReloadCheck unchanged while a game instance owner exists"));

		EngineSubsystem->Deinitialize();
		(void)bOk;
	}
};

#endif
