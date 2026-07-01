#include "AngelscriptEngineSubsystem.h"

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
		UAngelscriptEngineSubsystem::SetStartupEnvironmentOverrideForTesting(bIsEditorOverride, bIsRunningCommandletOverride);
	}

	static void ClearStartupEnvironmentOverride()
	{
		UAngelscriptEngineSubsystem::ClearStartupEnvironmentOverrideForTesting();
	}

	static void SetInitializeOverride(TFunction<FAngelscriptEngine*()> InOverride)
	{
		UAngelscriptEngineSubsystem::SetInitializeOverrideForTesting(MoveTemp(InOverride));
	}

	static void ResetInitializeState()
	{
		UAngelscriptEngineSubsystem::ResetInitializeStateForTesting();
	}

	static bool ShouldCreateSubsystem(const UAngelscriptEngineSubsystem& Subsystem, UObject* Outer)
	{
		return Subsystem.ShouldCreateSubsystem(Outer);
	}

	static void EnsurePrimaryEngineInitialized(UAngelscriptEngineSubsystem& Subsystem)
	{
		Subsystem.EnsurePrimaryEngineInitialized();
	}

	static void ReleasePrimaryEngine(UAngelscriptEngineSubsystem& Subsystem)
	{
		Subsystem.ReleasePrimaryEngine();
	}

	static FAngelscriptEngine* GetPrimaryEngine(const UAngelscriptEngineSubsystem& Subsystem)
	{
		return Subsystem.PrimaryEngine;
	}

	static bool OwnsPrimaryEngine(const UAngelscriptEngineSubsystem& Subsystem)
	{
		return Subsystem.bOwnsPrimaryEngine;
	}

	static bool HasInitializedPrimaryEngine(const UAngelscriptEngineSubsystem& Subsystem)
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
	TEST_METHOD(ShouldCreateHonorsEditorAndCommandletGates)
	{
ON_SCOPE_EXIT
		{
			FAngelscriptEngineSubsystemTestAccess::ClearStartupEnvironmentOverride();
		};

		const UAngelscriptEngineSubsystem* SubsystemCdo = GetDefault<UAngelscriptEngineSubsystem>();
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
			{ TEXT("PlainRuntimeStartup"), false, false, false },
		};

		for (const FStartupTestCase& TestCase : TestCases)
		{
			FAngelscriptEngineSubsystemTestAccess::SetStartupEnvironmentOverride(TestCase.bIsEditor, TestCase.bIsRunningCommandlet);
			ASSERT_THAT(AreEqual(
				TestCase.bShouldCreate,
				FAngelscriptEngineSubsystemTestAccess::ShouldCreateSubsystem(*SubsystemCdo, Outer),
				FString::Printf(TEXT("%s should match the expected EngineSubsystem creation gate"), TestCase.Label)));
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

		TStrongObjectPtr<UAngelscriptEngineSubsystem> Subsystem(NewObject<UAngelscriptEngineSubsystem>(GetTransientPackage()));
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
			TEXT("EngineSubsystem initialize-override test should make the override engine current after first initialize")));
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
			1,
			StackAfterSecondInitialize.Num(),
			TEXT("EngineSubsystem initialize-override test should keep exactly one engine on the context stack after repeated initialize")));
		ASSERT_THAT(IsTrue(
			StackAfterSecondInitialize[0] == OverrideEngine.Get(),
			TEXT("EngineSubsystem initialize-override test should keep the override engine as the only stack entry")));
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterSecondInitialize));

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
};

#endif
