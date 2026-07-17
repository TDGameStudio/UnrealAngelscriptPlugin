#include "AngelscriptEngine.h"
#include "AngelscriptSubsystem.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
private:
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

public:
	TEST_METHOD(InitializeOverrideIsIdempotentAndRestorable)
	{
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

		UAngelscriptSubsystem* ProductionSubsystem = UAngelscriptSubsystem::Get();
		ASSERT_THAT(IsNotNull(ProductionSubsystem, TEXT("RuntimeModule initialize-override test should resolve the production subsystem after reset")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == ProductionSubsystem->GetEngine(),
			TEXT("RuntimeModule initialize-override test should restore the production subsystem engine after reset")));

		const TArray<FAngelscriptEngine*> StackAfterReset = FAngelscriptEngineContextStack::SnapshotAndClear();
		(void)this->Assert.AreEqual(
			0,
			StackAfterReset.Num(),
			TEXT("RuntimeModule initialize-override test should leave the context stack empty after reset"));
	}

	TEST_METHOD(InitializeRoutesToEngineSubsystem)
	{
		FRuntimeModuleContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		FAngelscriptRuntimeModuleTickTestAccess::ResetInitializeState();
		FAngelscriptRuntimeModule RuntimeModule;
		UAngelscriptSubsystem* EngineSubsystem = UAngelscriptSubsystem::Get();
		ASSERT_THAT(IsNotNull(EngineSubsystem, TEXT("RuntimeModule subsystem-route test should resolve the production engine subsystem")));
		EngineSubsystem->EnsurePrimaryEngineInitialized();

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

		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == EngineSubsystem->GetEngine(),
			TEXT("RuntimeModule subsystem-route test should start from the production subsystem engine")));

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
			0,
			StackAfterFirstShutdown.Num(),
			TEXT("RuntimeModule subsystem-route test should not push the subsystem engine onto the explicit context stack"));
		bOk &= this->Assert.IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine,
			TEXT("RuntimeModule subsystem-route test should still resolve the subsystem engine after clearing an empty explicit stack"));

		RuntimeModule.ShutdownModule();

		bOk &= this->Assert.IsFalse(
			FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
			TEXT("RuntimeModule subsystem-route test should keep module-owned engine absent on repeated shutdown"));
		bOk &= this->Assert.IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine,
			TEXT("RuntimeModule subsystem-route test should keep resolving the subsystem engine after repeated module shutdown"));

		TArray<FAngelscriptEngine*> StackAfterSecondShutdown = FAngelscriptEngineContextStack::SnapshotAndClear();
		bOk &= this->Assert.AreEqual(
			0,
			StackAfterSecondShutdown.Num(),
			TEXT("RuntimeModule subsystem-route test should keep the context stack empty after manual clear and repeated shutdown"));
		(void)bOk;
	}

	TEST_METHOD(StartupModuleDoesNotBootstrapPrimaryEngine)
	{
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
		bOk &= this->Assert.IsFalse(
			FAngelscriptRuntimeModuleTickTestAccess::HasOwnedPrimaryEngine(),
			TEXT("RuntimeModule startup test should not create a module-owned primary engine"));

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

		UAngelscriptSubsystem* ProductionSubsystem = UAngelscriptSubsystem::Get();
		ASSERT_THAT(IsNotNull(ProductionSubsystem, TEXT("RuntimeModule ambient-initialize test should resolve the production subsystem after the scope exits")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == ProductionSubsystem->GetEngine(),
			TEXT("RuntimeModule ambient-initialize test should restore the production subsystem engine after the ambient scope exits")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineSubsystemTickTests,
	"Angelscript.TestModule.Engine.EngineSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

public:
	TEST_METHOD(TickAdvancesSubsystemPrimaryEngine)
	{
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

		TUniquePtr<FAngelscriptEngine> TestEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(TestEngine.Get(), TEXT("EngineSubsystem tick test should create an isolated full engine")))
		{
			return;
		}

		TStrongObjectPtr<UAngelscriptSubsystem> EngineSubsystem(NewObject<UAngelscriptSubsystem>(GetTransientPackage()));
		if (!this->Assert.IsNotNull(EngineSubsystem.Get(), TEXT("EngineSubsystem tick test should create a native subsystem object")))
		{
			return;
		}

		FAngelscriptEngineScope TestEngineScope(*TestEngine);
		EngineSubsystem->EnsurePrimaryEngineInitialized();
		ASSERT_THAT(IsTrue(
			EngineSubsystem->GetEngine() == TestEngine.Get(),
			TEXT("EngineSubsystem tick test should adopt the scoped test engine")));

		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*TestEngine, -1.0);

		EngineSubsystem->Tick(0.016f);
		bool bOk = this->Assert.IsTrue(
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*TestEngine) > 0.0,
			TEXT("EngineSubsystem tick test should advance NextHotReloadCheck through the engine subsystem tick path"));

		FAngelscriptTickBehaviorTestAccess::PrepareTickProbe(*TestEngine, -1.0);
		EngineSubsystem->Tick(0.016f);
		bOk &= this->Assert.IsTrue(
			FAngelscriptTickBehaviorTestAccess::GetNextHotReloadCheck(*TestEngine) > 0.0,
			TEXT("EngineSubsystem tick test should keep ticking on repeated subsystem ticks without a game-instance suppressor"));

		EngineSubsystem->Deinitialize();
		(void)bOk;
	}
};

#endif
