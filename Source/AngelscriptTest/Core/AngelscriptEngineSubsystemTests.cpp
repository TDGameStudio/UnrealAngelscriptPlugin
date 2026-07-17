#include "AngelscriptSubsystem.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


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

template <typename TSubsystem>
static constexpr auto HasAutomationHooks(int) -> decltype(
	&TSubsystem::SetStartupEnvironmentOverrideForTesting,
	&TSubsystem::SetInitializeOverrideForTesting,
	&TSubsystem::SetSubsystemOverrideForTesting,
	bool())
{
	return true;
}

template <typename TSubsystem>
static constexpr bool HasAutomationHooks(...)
{
	return false;
}

public:
	TEST_METHOD(ProductionSurfaceDoesNotExposeAutomationHooks)
	{
		static_assert(
			!HasAutomationHooks<UAngelscriptSubsystem>(0),
			"UAngelscriptSubsystem must not expose automation-only controls.");
	}

	TEST_METHOD(ShouldCreateWhenSuperAllows)
	{
		const UAngelscriptSubsystem* SubsystemCdo = GetDefault<UAngelscriptSubsystem>();
		ASSERT_THAT(IsNotNull(SubsystemCdo, TEXT("EngineSubsystem should expose a native CDO")));

		UObject* Outer = GEngine != nullptr ? static_cast<UObject*>(GEngine) : GetTransientPackage();
		ASSERT_THAT(IsTrue(
			SubsystemCdo->ShouldCreateSubsystem(Outer),
			TEXT("EngineSubsystem should create whenever the base subsystem permits creation")));
	}

	TEST_METHOD(AdoptsAmbientEngineIdempotently)
	{
		FEngineSubsystemContextStackGuard ContextGuard;

		TStrongObjectPtr<UAngelscriptSubsystem> Subsystem(NewObject<UAngelscriptSubsystem>(GetTransientPackage()));
		ASSERT_THAT(IsNotNull(Subsystem.Get(), TEXT("EngineSubsystem ambient-engine test should create a native subsystem object")));

		TUniquePtr<FAngelscriptEngine> AmbientEngine = CreateScriptScanFreeEngineForTesting(
			FAngelscriptEngineConfig(),
			FAngelscriptEngineDependencies::CreateDefault());
		ASSERT_THAT(IsNotNull(AmbientEngine.Get(), TEXT("EngineSubsystem ambient-engine test should create an isolated engine")));

		{
			FAngelscriptEngineScope AmbientScope(*AmbientEngine);
			Subsystem->EnsurePrimaryEngineInitialized();
			ASSERT_THAT(IsTrue(
				Subsystem->GetEngine() == AmbientEngine.Get(),
				TEXT("EngineSubsystem should adopt the ambient engine")));

			Subsystem->EnsurePrimaryEngineInitialized();
			ASSERT_THAT(IsTrue(
				Subsystem->GetEngine() == AmbientEngine.Get(),
				TEXT("EngineSubsystem should keep the same ambient engine after repeated initialization")));
		}

		ASSERT_THAT(IsNotNull(
			AmbientEngine->GetScriptEngine(),
			TEXT("EngineSubsystem must not shut down the test-owned ambient engine")));
	}

	TEST_METHOD(ScopedEngineOverridesRealSubsystemFallback)
	{
		FEngineSubsystemContextStackGuard ContextGuard;
		UAngelscriptSubsystem* Subsystem = UAngelscriptSubsystem::Get();
		ASSERT_THAT(IsNotNull(Subsystem, TEXT("EngineSubsystem fallback test should resolve the real engine subsystem")));
		Subsystem->EnsurePrimaryEngineInitialized();
		FAngelscriptEngine* SubsystemEngine = Subsystem->GetEngine();
		ASSERT_THAT(IsNotNull(SubsystemEngine, TEXT("EngineSubsystem fallback test should expose a primary engine")));
		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine,
			TEXT("EngineSubsystem fallback test should resolve the real subsystem engine without an explicit scope")));

		TUniquePtr<FAngelscriptEngine> ScopedEngine = CreateScriptScanFreeEngineForTesting(
			FAngelscriptEngineConfig(),
			FAngelscriptEngineDependencies::CreateDefault());
		ASSERT_THAT(IsNotNull(ScopedEngine.Get(), TEXT("EngineSubsystem fallback test should create a scoped engine")));

		{
			FAngelscriptEngineScope ScopedOverride(*ScopedEngine);
			ASSERT_THAT(IsTrue(
				FAngelscriptEngine::TryGetCurrentEngine() == ScopedEngine.Get(),
				TEXT("EngineSubsystem fallback test should prefer the active explicit scope over the subsystem engine")));
		}

		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::TryGetCurrentEngine() == SubsystemEngine,
			TEXT("EngineSubsystem fallback test should restore subsystem fallback after the explicit scope exits")));
	}
};

#endif
