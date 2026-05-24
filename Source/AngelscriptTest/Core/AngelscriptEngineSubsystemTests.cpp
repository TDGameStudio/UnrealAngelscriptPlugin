#include "AngelscriptEngineSubsystem.h"

#include "AngelscriptEngine.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptEngineSubsystemTests_Private
{
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
}


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
	TEST_METHOD(ShouldCreateHonorsEditorAndCommandletGates)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineSubsystemTests_Private;
		ON_SCOPE_EXIT
		{
			FAngelscriptEngineSubsystemTestAccess::ClearStartupEnvironmentOverride();
		};

		const UAngelscriptEngineSubsystem* SubsystemCdo = GetDefault<UAngelscriptEngineSubsystem>();
		if (!TestRunner->TestNotNull(TEXT("EngineSubsystem should expose a native CDO"), SubsystemCdo))
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
			TestRunner->TestEqual(
				FString::Printf(TEXT("%s should match the expected EngineSubsystem creation gate"), TestCase.Label),
				FAngelscriptEngineSubsystemTestAccess::ShouldCreateSubsystem(*SubsystemCdo, Outer),
				TestCase.bShouldCreate);
		}
	}

	TEST_METHOD(InitializeOverrideIsIdempotentAndRestorable)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineSubsystemTests_Private;
		FEngineSubsystemContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		TStrongObjectPtr<UAngelscriptEngineSubsystem> Subsystem(NewObject<UAngelscriptEngineSubsystem>(GetTransientPackage()));
		if (!TestRunner->TestNotNull(TEXT("EngineSubsystem initialize-override test should create a native subsystem object"), Subsystem.Get()))
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
		if (!TestRunner->TestNull(TEXT("EngineSubsystem initialize-override test should start without a current engine"), FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> OverrideEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("EngineSubsystem initialize-override test should create an isolated override engine"), OverrideEngine.Get()))
		{
			return;
		}

		FAngelscriptEngineSubsystemTestAccess::SetInitializeOverride([&OverrideEngine]()
		{
			return OverrideEngine.Get();
		});

		FAngelscriptEngineSubsystemTestAccess::EnsurePrimaryEngineInitialized(*Subsystem);
		TestRunner->TestTrue(
			TEXT("EngineSubsystem initialize-override test should make the override engine current after first initialize"),
			FAngelscriptEngine::TryGetCurrentEngine() == OverrideEngine.Get());
		TestRunner->TestTrue(
			TEXT("EngineSubsystem initialize-override test should mark the primary engine initialized"),
			FAngelscriptEngineSubsystemTestAccess::HasInitializedPrimaryEngine(*Subsystem));
		TestRunner->TestTrue(
			TEXT("EngineSubsystem initialize-override test should expose the override engine as primary"),
			FAngelscriptEngineSubsystemTestAccess::GetPrimaryEngine(*Subsystem) == OverrideEngine.Get());
		TestRunner->TestFalse(
			TEXT("EngineSubsystem initialize-override test should not take ownership of an override engine"),
			FAngelscriptEngineSubsystemTestAccess::OwnsPrimaryEngine(*Subsystem));

		FAngelscriptEngineSubsystemTestAccess::EnsurePrimaryEngineInitialized(*Subsystem);

		TArray<FAngelscriptEngine*> StackAfterSecondInitialize = FAngelscriptEngineContextStack::SnapshotAndClear();
		TestRunner->TestEqual(
			TEXT("EngineSubsystem initialize-override test should keep exactly one engine on the context stack after repeated initialize"),
			StackAfterSecondInitialize.Num(),
			1);
		TestRunner->TestTrue(
			TEXT("EngineSubsystem initialize-override test should keep the override engine as the only stack entry"),
			StackAfterSecondInitialize.Num() == 1 && StackAfterSecondInitialize[0] == OverrideEngine.Get());
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(StackAfterSecondInitialize));

		FAngelscriptEngineSubsystemTestAccess::ReleasePrimaryEngine(*Subsystem);
		TestRunner->TestFalse(
			TEXT("EngineSubsystem initialize-override test should clear initialized state after release"),
			FAngelscriptEngineSubsystemTestAccess::HasInitializedPrimaryEngine(*Subsystem));
		TestRunner->TestNull(
			TEXT("EngineSubsystem initialize-override test should clear the current engine after release"),
			FAngelscriptEngine::TryGetCurrentEngine());

		const TArray<FAngelscriptEngine*> StackAfterRelease = FAngelscriptEngineContextStack::SnapshotAndClear();
		TestRunner->TestEqual(
			TEXT("EngineSubsystem initialize-override test should leave the context stack empty after release"),
			StackAfterRelease.Num(),
			0);
	}
};

#endif
