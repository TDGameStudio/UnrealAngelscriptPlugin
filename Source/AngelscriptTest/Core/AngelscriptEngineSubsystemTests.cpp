#include "AngelscriptSubsystem.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


struct FAngelscriptEngineSubsystemTestAccess
{
	static void SetStartupEnvironmentOverride(const TOptional<bool>& bIsEditorOverride, const TOptional<bool>& bIsRunningCommandletOverride)
	{
		UAngelscriptSubsystem::SetStartupEnvironmentOverrideForTesting(bIsEditorOverride, bIsRunningCommandletOverride);
	}

	static void ClearStartupEnvironmentOverride()
	{
		UAngelscriptSubsystem::ClearStartupEnvironmentOverrideForTesting();
	}

	static void SetInitializeOverride(TFunction<FAngelscriptEngine*()> InOverride)
	{
		UAngelscriptSubsystem::SetInitializeOverrideForTesting(MoveTemp(InOverride));
	}

	static void SetSubsystemOverride(UAngelscriptSubsystem* InSubsystem)
	{
		UAngelscriptSubsystem::SetSubsystemOverrideForTesting(InSubsystem);
	}

	static void ResetInitializeState()
	{
		UAngelscriptSubsystem::ResetInitializeStateForTesting();
	}

	static bool ShouldCreateSubsystem(const UAngelscriptSubsystem& Subsystem, UObject* Outer)
	{
		return Subsystem.ShouldCreateSubsystem(Outer);
	}

	static void EnsurePrimaryEngineInitialized(UAngelscriptSubsystem& Subsystem)
	{
		Subsystem.EnsurePrimaryEngineInitialized();
	}

	static void ReleasePrimaryEngine(UAngelscriptSubsystem& Subsystem)
	{
		Subsystem.ReleasePrimaryEngine();
	}

	static FAngelscriptEngine* GetPrimaryEngine(const UAngelscriptSubsystem& Subsystem)
	{
		return Subsystem.PrimaryEngine;
	}

	static bool OwnsPrimaryEngine(const UAngelscriptSubsystem& Subsystem)
	{
		return Subsystem.bOwnsPrimaryEngine;
	}

	static bool HasInitializedPrimaryEngine(const UAngelscriptSubsystem& Subsystem)
	{
		return Subsystem.bInitializedPrimaryEngine;
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineSubsystemTests,
	"Angelscript.TestModule.Engine.EngineSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FEngineSubsystemContextStackGuard
{
	TArray<FAngelscriptEngine*> SavedStack;

	FEngineSubsystemContextStackGuard()
	{
		SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	}

	~FEngineSubsystemContextStackGuard()
	{
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	}

	void DiscardSavedStack()
	{
		SavedStack.Reset();
	}
};

public:
	TEST_METHOD(ShouldCreateInEditorCommandletAndRuntime)
	{
		ON_SCOPE_EXIT
		{
			FAngelscriptEngineSubsystemTestAccess::ClearStartupEnvironmentOverride();
		};

		const UAngelscriptSubsystem* SubsystemCdo = GetDefault<UAngelscriptSubsystem>();
		if (!this->Assert.IsNotNull(SubsystemCdo, TEXT("EngineSubsystem should expose a native CDO")))
		{
			return;
		}

		UObject* Outer = GEngine != nullptr ? static_cast<UObject*>(GEngine) : GetTransientPackage();

		struct FStartupTestCase
		{
			const TCHAR* Label;
			bool bIsEditor = false;
			bool bIsRunningCommandlet = false;
			bool bShouldCreate = false;
		};

		const TArray<FStartupTestCase> TestCases = {
			{ TEXT("EditorStartup"), true, false, true },
			{ TEXT("CommandletStartup"), false, true, true },
			{ TEXT("PlainRuntimeStartup"), false, false, true },
		};

		for (const FStartupTestCase& TestCase : TestCases)
		{
			FAngelscriptEngineSubsystemTestAccess::SetStartupEnvironmentOverride(TestCase.bIsEditor, TestCase.bIsRunningCommandlet);
			ASSERT_THAT(AreEqual(
				TestCase.bShouldCreate,
				FAngelscriptEngineSubsystemTestAccess::ShouldCreateSubsystem(*SubsystemCdo, Outer),
				FString::Printf(TEXT("%s should create the Angelscript engine subsystem"), TestCase.Label)));
		}
	}

	TEST_METHOD(InitializeOverrideIsIdempotentAndRestorable)
	{
		FEngineSubsystemContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		TStrongObjectPtr<UAngelscriptSubsystem> Subsystem(NewObject<UAngelscriptSubsystem>(GetTransientPackage()));
		if (!this->Assert.IsNotNull(Subsystem.Get(), TEXT("EngineSubsystem initialize-override test should create a native subsystem object")))
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			FAngelscriptEngineSubsystemTestAccess::ReleasePrimaryEngine(*Subsystem);
			FAngelscriptEngineSubsystemTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		FAngelscriptEngineSubsystemTestAccess::ResetInitializeState();
		FAngelscriptEngineSubsystemTestAccess::SetSubsystemOverride(Subsystem.Get());
		if (!this->Assert.IsNull(FAngelscriptEngine::TryGetCurrentEngine(), TEXT("EngineSubsystem initialize-override test should start without a current engine")))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> OverrideEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(OverrideEngine.Get(), TEXT("EngineSubsystem initialize-override test should create an isolated override engine")))
		{
			return;
		}

		FAngelscriptEngineSubsystemTestAccess::SetInitializeOverride([&OverrideEngine]()
		{
			return OverrideEngine.Get();
		});

		FAngelscriptEngineSubsystemTestAccess::EnsurePrimaryEngineInitialized(*Subsystem);
		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == OverrideEngine.Get(),
			TEXT("EngineSubsystem initialize-override test should resolve the override engine through subsystem fallback after first initialize")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngineSubsystemTestAccess::HasInitializedPrimaryEngine(*Subsystem),
			TEXT("EngineSubsystem initialize-override test should mark the primary engine initialized")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngineSubsystemTestAccess::GetPrimaryEngine(*Subsystem) == OverrideEngine.Get(),
			TEXT("EngineSubsystem initialize-override test should expose the override engine as primary")));
		ASSERT_THAT(IsFalse(
			FAngelscriptEngineSubsystemTestAccess::OwnsPrimaryEngine(*Subsystem),
			TEXT("EngineSubsystem initialize-override test should not take ownership of an override engine")));

		FAngelscriptEngineSubsystemTestAccess::EnsurePrimaryEngineInitialized(*Subsystem);

		TArray<FAngelscriptEngine*> StackAfterSecondInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
		ASSERT_THAT(AreEqual(
			0,
			StackAfterSecondInitialize.Num(),
			TEXT("EngineSubsystem initialize-override test should not push the subsystem-owned engine onto the explicit context stack")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == OverrideEngine.Get(),
			TEXT("EngineSubsystem initialize-override test should still resolve through the subsystem when the explicit stack is empty")));

		FAngelscriptEngineSubsystemTestAccess::ReleasePrimaryEngine(*Subsystem);
		ASSERT_THAT(IsFalse(
			FAngelscriptEngineSubsystemTestAccess::HasInitializedPrimaryEngine(*Subsystem),
			TEXT("EngineSubsystem initialize-override test should clear initialized state after release")));
		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("EngineSubsystem initialize-override test should clear the current engine after release")));

		const TArray<FAngelscriptEngine*> StackAfterRelease = FAngelscriptEngineContextStack::SnapshotAndClear();
		ASSERT_THAT(AreEqual(
			0,
			StackAfterRelease.Num(),
			TEXT("EngineSubsystem initialize-override test should leave the context stack empty after release")));
	}

	TEST_METHOD(SubsystemFallbackYieldsToScopedEngine)
	{
		FEngineSubsystemContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		TStrongObjectPtr<UAngelscriptSubsystem> Subsystem(NewObject<UAngelscriptSubsystem>(GetTransientPackage()));
		if (!this->Assert.IsNotNull(Subsystem.Get(), TEXT("EngineSubsystem fallback test should create a native subsystem object")))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineSubsystemTestAccess::ReleasePrimaryEngine(*Subsystem);
			FAngelscriptEngineSubsystemTestAccess::ResetInitializeState();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			DestroySharedTestEngine();
		};

		FAngelscriptEngineSubsystemTestAccess::ResetInitializeState();
		FAngelscriptEngineSubsystemTestAccess::SetSubsystemOverride(Subsystem.Get());
		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("EngineSubsystem fallback test should start without a current engine before subsystem initialization")));

		TUniquePtr<FAngelscriptEngine> SubsystemEngine = CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> ScopedEngine = CreateFullTestEngine();
		ASSERT_THAT(IsNotNull(SubsystemEngine.Get(), TEXT("EngineSubsystem fallback test should create a subsystem engine")));
		ASSERT_THAT(IsNotNull(ScopedEngine.Get(), TEXT("EngineSubsystem fallback test should create a scoped engine")));

		FAngelscriptEngineSubsystemTestAccess::SetInitializeOverride([&SubsystemEngine]()
		{
			return SubsystemEngine.Get();
		});
		FAngelscriptEngineSubsystemTestAccess::EnsurePrimaryEngineInitialized(*Subsystem);

		ASSERT_THAT(IsTrue(
			FAngelscriptEngineContextStack::IsEmpty(),
			TEXT("EngineSubsystem fallback test should leave the explicit context stack empty after subsystem initialization")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine.Get(),
			TEXT("EngineSubsystem fallback test should resolve the subsystem engine when no scope is active")));

		{
			FAngelscriptEngineScope ScopedOverride(*ScopedEngine);
			ASSERT_THAT(IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == ScopedEngine.Get(),
				TEXT("EngineSubsystem fallback test should prefer the active explicit scope over the subsystem engine")));
		}

		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine.Get(),
			TEXT("EngineSubsystem fallback test should restore subsystem fallback after the explicit scope exits")));

		FAngelscriptEngineSubsystemTestAccess::ReleasePrimaryEngine(*Subsystem);
		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("EngineSubsystem fallback test should stop resolving a current engine after subsystem release")));
	}
};

#endif
