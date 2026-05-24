#include "CQTest.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestWorld.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// =============================================================================
// Template_WorldTick.cpp
//
// Teaching template — demonstrates how to write "World.Tick + AS Actor /
// Component" style functional tests under the CQTest framework. This template
// focuses purely on the *runtime* tick driving mechanism and intentionally
// does not cover EndPlay / Destroy (that ground is fully covered by
// Template_GameLifetime).
//
// Why CQTest over the classic IMPLEMENT_SIMPLE_AUTOMATION_TEST:
//   1. A single TEST_CLASS_WITH_FLAGS hosts many TEST_METHODs; each one is
//      registered as an independent automation test, avoiding one giant
//      RunTest function with a forest of scoped helpers.
//   2. BEFORE_ALL / AFTER_ALL run once per class, allowing the AS engine to
//      be reused across TEST_METHODs and avoiding the per-test cost of
//      rebuilding a shared clone engine.
//   3. CQTest's ASSERT_THAT short-circuits on failure; combined with RAII
//      scopes (FAngelscriptEngineScope) a setup failure terminates the
//      current method cleanly without contaminating later assertions.
//
// Six TEST_METHODs cover the main World.Tick scenarios from simple to deep:
//
//   1. BasicTickFlow              — minimal flow: spawn -> BeginPlay -> Tick.
//   2. ExplicitTickCount          — strict assertion that TickCount == NumTicks.
//   3. AccumulatedDeltaTime       — verifies sum(DeltaTime) ~= NumTicks * Dt.
//   4. VariableDeltaTime          — multiple frames with different DeltaTimes.
//   5. MultipleActorsTickedTogether — one World drives many actors at once.
//   6. ComponentTickAlongsideActor — Tick is also dispatched to UAngelscriptComponent.
//
// ---- Boundary with Template_GameLifetime ----
//
//   Template_WorldTick covers the "Tick driving" axis: DeltaTime, count,
//   multi-actor, components.
//   Template_GameLifetime covers the "full lifecycle" axis: construction ->
//   BeginPlay -> Tick -> EndPlay -> Destroyed event chain and ordering.
//   The two templates are complementary.
//
// ---- Helpers reused from existing infrastructure ----
//
//   FAngelscriptTestWorld       (Shared/AngelscriptTestWorld.h)
//   EnableActorTick             (Functional/Actor/AngelscriptActorTestHelpers.h)
//   CompileScriptModule         (Shared/AngelscriptFunctionalTestUtils.h)
//   ReadPropertyValue           (Shared/AngelscriptFunctionalTestUtils.h)
//
// ---- Trade-offs of the three tick driving approaches ----
//
//   1. W.Tick(Dt, N)                — calls World.Tick + manual TActorIterator
//                                     dispatch. In a test world the actual
//                                     dispatch depends on whether the actor's
//                                     tick function was registered, so
//                                     TickCount is not strictly controllable.
//                                     Suitable for "weak (>= N)" assertions.
//   2. W.TickViaManager(Dt, N)      — calls World.Tick only; the test world
//                                     default tick group dispatches
//                                     ReceiveTick once for newly-registered
//                                     actors and is a no-op afterwards. Good
//                                     for showing the "UE world scheduling"
//                                     path with weak (>= 1) assertions.
//   3. W.DispatchActorTick(Actor, Dt, N) / W.DispatchComponentTick(Comp, Dt, N)
//                                   — directly loop Actor->Tick /
//                                     Component->TickComponent, bypassing
//                                     the world scheduler. TickCount equals
//                                     NumTicks exactly; preferred for any
//                                     "precise driving" demonstration.
//
//   This template covers all three: BasicTickFlow uses W.TickViaManager with
//   weak assertions to demonstrate the UE scheduling path; the remaining
//   strict-assertion TEST_METHODs use W.DispatchActorTick / W.DispatchComponentTick
//   for precise driving.
// =============================================================================

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

namespace TemplateWorldTickTest
{
	// Default per-frame DeltaTime (~60 Hz). All TEST_METHODs use this value
	// unless they specifically need to demonstrate a different frame rate.
	constexpr float DefaultDeltaTime = 0.016f;

	// Floating-point tolerance for accumulated DeltaTime comparisons; guards
	// against micro-drift introduced by the float -> double reflection path.
	constexpr double DeltaTimeAccumulationTolerance = 1e-4;
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateWorldTickTest,
	"Angelscript.Template.WorldTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// -----------------------------------------------------------------
	// BEFORE_ALL — acquire a clean shared clone engine once before any
	// TEST_METHOD in this class runs.
	// -----------------------------------------------------------------
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	// -----------------------------------------------------------------
	// AFTER_ALL — reset the shared engine after all TEST_METHODs finish so
	// later TEST_CLASSes can rely on a stable baseline.
	// -----------------------------------------------------------------
	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// =================================================================
	// 1. BasicTickFlow
	//
	// The simplest end-to-end World.Tick flow:
	//   - Compile an AS Actor class with BeginPlay / Tick overrides.
	//   - Spawn an instance and dispatch BeginPlay manually.
	//   - Advance N frames via W.TickViaManager (the UE scheduling path).
	//
	// In a test world World.Tick does not guarantee that ReceiveTick is
	// dispatched every frame, so this method uses ">= 1" weak assertions to
	// demonstrate that the actor was driven at least once. Later TEST_METHODs
	// switch to W.DispatchActorTick for strict assertions.
	// =================================================================
	TEST_METHOD(BasicTickFlow)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TemplateWorldTickBasic"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TemplateWorldTickBasic.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateWorldTickBasicActor : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("ATemplateWorldTickBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		EnableActorTick(*Actor);
		W.BeginPlay(*Actor);
		W.TickViaManager(TemplateWorldTickTest::DefaultDeltaTime, 3);

		int32 BeginPlayCount = 0;
		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)));
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)));

		TestRunner->TestTrue(TEXT("[BasicTickFlow] BeginPlay should be dispatched at least once"), BeginPlayCount >= 1);
		TestRunner->TestTrue(TEXT("[BasicTickFlow] Tick should be dispatched at least once"), TickCount >= 1);
	}

	// =================================================================
	// 2. ExplicitTickCount
	//
	// Strict assertion that TickCount == NumTicks (tighter than the ">="
	// used in BasicTickFlow). W.DispatchActorTick loops Actor->Tick(DeltaTime)
	// directly, sidestepping the test world's non-deterministic scheduler
	// dispatch.
	// =================================================================
	TEST_METHOD(ExplicitTickCount)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TemplateWorldTickExplicit"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TemplateWorldTickExplicit.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateWorldTickExplicitActor : AActor
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("ATemplateWorldTickExplicitActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		W.BeginPlay(*Actor);

		constexpr int32 NumTicks = 7;
		W.DispatchActorTick(*Actor, TemplateWorldTickTest::DefaultDeltaTime, NumTicks);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)));
		TestRunner->TestEqual(TEXT("[ExplicitTickCount] TickCount should equal NumTicks exactly"), TickCount, NumTicks);
	}

	// =================================================================
	// 3. AccumulatedDeltaTime
	//
	// Verifies that the AS-side accumulator sums DeltaTime correctly so
	// that Total ~= NumTicks * DeltaTime. AS-declared `float UPROPERTY`
	// reflects as FDoubleProperty in UE 5.x, so the C++ side must read it
	// as FDoubleProperty + double. The tolerance guards against floating
	// micro-drift across the float / double boundary.
	// =================================================================
	TEST_METHOD(AccumulatedDeltaTime)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TemplateWorldTickAccumDt"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TemplateWorldTickAccumDt.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateWorldTickAccumDtActor : AActor
{
	UPROPERTY()
	float TotalDeltaTime = 0.f;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TotalDeltaTime += DeltaTime;
	}
}
)AS"),
			TEXT("ATemplateWorldTickAccumDtActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		W.BeginPlay(*Actor);

		constexpr float DeltaTime = 0.025f;
		constexpr int32 NumTicks = 4;
		W.DispatchActorTick(*Actor, DeltaTime, NumTicks);

		double TotalDeltaTime = 0.0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FDoubleProperty>(*TestRunner, Actor, TEXT("TotalDeltaTime"), TotalDeltaTime)));

		const double Expected = static_cast<double>(DeltaTime) * NumTicks;
		const double Diff = FMath::Abs(TotalDeltaTime - Expected);
		TestRunner->TestTrue(
			*FString::Printf(TEXT("[AccumulatedDeltaTime] TotalDeltaTime=%.6f, Expected=%.6f, Diff=%.6f"),
				TotalDeltaTime, Expected, Diff),
			Diff < TemplateWorldTickTest::DeltaTimeAccumulationTolerance);
	}

	// =================================================================
	// 4. VariableDeltaTime
	//
	// Simulates a variable-rate scenario: drive the actor with a sequence
	// of different DeltaTimes and verify the accumulated total is still
	// correct. This is the most common shape in real games — DeltaTime
	// fluctuates whenever frame rate is not stable.
	// =================================================================
	TEST_METHOD(VariableDeltaTime)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TemplateWorldTickVarDt"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TemplateWorldTickVarDt.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateWorldTickVarDtActor : AActor
{
	UPROPERTY()
	int TickCount = 0;

	UPROPERTY()
	float TotalDeltaTime = 0.f;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
		TotalDeltaTime += DeltaTime;
	}
}
)AS"),
			TEXT("ATemplateWorldTickVarDtActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		W.BeginPlay(*Actor);

		// Mixed 30 Hz / 60 Hz / 120 Hz frame sequence.
		const float DeltaSeries[] = { 0.033f, 0.016f, 0.008f, 0.020f, 0.010f };
		double ExpectedTotal = 0.0;
		for (float Dt : DeltaSeries)
		{
			W.DispatchActorTick(*Actor, Dt, 1);
			ExpectedTotal += Dt;
		}

		int32 TickCount = 0;
		double TotalDeltaTime = 0.0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)));
		ASSERT_THAT(IsTrue(ReadPropertyValue<FDoubleProperty>(*TestRunner, Actor, TEXT("TotalDeltaTime"), TotalDeltaTime)));

		TestRunner->TestEqual(TEXT("[VariableDeltaTime] TickCount should equal the dispatched frame count"),
			TickCount, static_cast<int32>(UE_ARRAY_COUNT(DeltaSeries)));

		const double Diff = FMath::Abs(TotalDeltaTime - ExpectedTotal);
		TestRunner->TestTrue(
			*FString::Printf(TEXT("[VariableDeltaTime] Accumulated DeltaTime should equal the series sum: actual=%.6f expected=%.6f"),
				TotalDeltaTime, ExpectedTotal),
			Diff < TemplateWorldTickTest::DeltaTimeAccumulationTolerance);
	}

	// =================================================================
	// 5. MultipleActorsTickedTogether
	//
	// Spawn several instances of the same class in one world and drive
	// each of them with W.DispatchActorTick for the same number of frames.
	// Each per-actor TickCount must equal NumTicks exactly — no actor
	// influences another's counter, and none is missed.
	// =================================================================
	TEST_METHOD(MultipleActorsTickedTogether)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TemplateWorldTickMultiActor"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TemplateWorldTickMultiActor.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateWorldTickMultiActor : AActor
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("ATemplateWorldTickMultiActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		// Spawn three actors at distinct positions so all of them are
		// registered into the TActorIterator list of the world.
		constexpr int32 NumActors = 3;
		TArray<AActor*> Actors;
		Actors.Reserve(NumActors);
		for (int32 i = 0; i < NumActors; ++i)
		{
			AActor* A = W.SpawnActorOfClass(ScriptClass, FActorSpawnParameters(),
				FVector(static_cast<double>(i) * 100.0, 0.0, 0.0));
			ASSERT_THAT(IsNotNull(A));
			W.BeginPlay(*A);
			Actors.Add(A);
		}

		// Drive each actor with exactly NumTicks frames.
		constexpr int32 NumTicks = 5;
		for (AActor* A : Actors)
		{
			W.DispatchActorTick(*A, TemplateWorldTickTest::DefaultDeltaTime, NumTicks);
		}

		// Every actor should have been ticked NumTicks times exactly.
		for (int32 i = 0; i < NumActors; ++i)
		{
			int32 TickCount = 0;
			ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actors[i], TEXT("TickCount"), TickCount)));
			TestRunner->TestEqual(
				*FString::Printf(TEXT("[MultipleActors] Actor[%d] TickCount should equal NumTicks"), i),
				TickCount, NumTicks);
		}
	}

	// =================================================================
	// 6. ComponentTickAlongsideActor
	//
	// Demonstrates how to push the script logic down into a
	// UAngelscriptComponent subclass and drive its BlueprintOverride Tick
	// precisely via W.DispatchComponentTick.
	//
	// A component must satisfy two conditions to be ticked:
	//   1. PrimaryComponentTick.bCanEverTick = true
	//   2. SetComponentTickEnabled(true)
	// Both flags are toggled on the C++ side here. The AS-side
	// BlueprintOverride method name is `Tick(float DeltaSeconds)`,
	// matching ActorComponent's ReceiveTick signature.
	// =================================================================
	TEST_METHOD(ComponentTickAlongsideActor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TemplateWorldTickComponent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TemplateWorldTickComponent.as"),
			TEXT(R"AS(
UCLASS()
class UTemplateWorldTickComponent : UAngelscriptComponent
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("UTemplateWorldTickComponent"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		// Use a vanilla AActor as the host and attach the AS Component manually.
		AActor* Host = W.SpawnActorOfClass<AActor>(AActor::StaticClass());
		ASSERT_THAT(IsNotNull(Host));

		UActorComponent* Component = NewObject<UActorComponent>(Host, ScriptClass);
		ASSERT_THAT(IsNotNull(Component));

		Host->AddInstanceComponent(Component);
		Component->OnComponentCreated();
		Component->RegisterComponent();
		Component->Activate(true);

		// Enable component tick — AS does not flip these flags automatically.
		Component->PrimaryComponentTick.bCanEverTick = true;
		Component->SetComponentTickEnabled(true);

		W.BeginPlay(*Host);

		constexpr int32 NumTicks = 4;
		W.DispatchComponentTick(*Component, TemplateWorldTickTest::DefaultDeltaTime, NumTicks);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Component, TEXT("TickCount"), TickCount)));
		TestRunner->TestEqual(TEXT("[ComponentTick] Component TickCount should equal NumTicks"), TickCount, NumTicks);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
