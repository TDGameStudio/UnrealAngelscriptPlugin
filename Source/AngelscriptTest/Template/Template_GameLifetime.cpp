#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestWorld.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// -----------------------------------------------------------------------------
// Template_GameLifetime
// -----------------------------------------------------------------------------
// Advanced counterpart of Template_WorldTick.cpp — demonstrates the full game
// lifecycle event chain of an AS-scripted Actor. A single test walks through:
//
//   1. UserConstructionScript - dispatched by UE during Spawn (construction).
//   2. BeginPlay              - triggered via BeginPlayActor; enters Play phase.
//   3. Tick(DeltaTime)        - driven by TickWorld for several frames.
//   4. EndPlay(Reason)        - dispatched synchronously by Destroy(),
//                               carries EEndPlayReason::Destroyed.
//   5. Destroyed              - dispatched right after EndPlay; final teardown
//                               event in the destruction phase.
//
// The AS class records two complementary kinds of information:
//
//   - Counters (BeginPlayCount / TickCount / EndPlayCount / DestroyedCount)
//   - Ordering (each phase increments NextOrder and stores the current value;
//               the relative magnitudes recover the dispatch sequence)
//
// This lets the test verify both "how many times was each event called" and
// "in what order were the events dispatched":
//   ConstructOrder < BeginPlayOrder < FirstTickOrder < EndPlayOrder < DestroyedOrder
//
// The template reuses the shared FAngelscriptTestWorld harness
// (Shared/AngelscriptTestWorld.h) for spawn / BeginPlay / Tick / DestroyAndDrain,
// keeping all driving paths consistent with Template_WorldTick.
// -----------------------------------------------------------------------------

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateGameLifetimeTest,
	"Angelscript.Template.GameLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptActorFullLifecycle)
	{
		FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TemplateGameLifetimeScriptActor"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TemplateGameLifetimeScriptActor.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateGameLifetimeScriptActor : AActor
{
	UPROPERTY()
	int ConstructCount = 0;

	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int TickCount = 0;

	UPROPERTY()
	int EndPlayCount = 0;

	UPROPERTY()
	int DestroyedCount = 0;

	UPROPERTY()
	float TotalDeltaTime = 0.f;

	UPROPERTY()
	EEndPlayReason LastEndPlayReason = EEndPlayReason::Quit;

	// Order tracking: each phase increments NextOrder and stores the current
	// value into its own *Order field, so the relative magnitudes of the
	// *Order properties reconstruct the dispatch sequence.
	UPROPERTY()
	int NextOrder = 0;

	UPROPERTY()
	int ConstructOrder = 0;

	UPROPERTY()
	int BeginPlayOrder = 0;

	UPROPERTY()
	int FirstTickOrder = 0;

	UPROPERTY()
	int EndPlayOrder = 0;

	UPROPERTY()
	int DestroyedOrder = 0;

	UFUNCTION(BlueprintOverride)
	void UserConstructionScript()
	{
		ConstructCount += 1;
		if (ConstructOrder == 0)
		{
			NextOrder += 1;
			ConstructOrder = NextOrder;
		}
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
		NextOrder += 1;
		BeginPlayOrder = NextOrder;
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
		TotalDeltaTime += DeltaTime;
		if (FirstTickOrder == 0)
		{
			NextOrder += 1;
			FirstTickOrder = NextOrder;
		}
	}

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		EndPlayCount += 1;
		LastEndPlayReason = Reason;
		NextOrder += 1;
		EndPlayOrder = NextOrder;
	}

	UFUNCTION(BlueprintOverride)
	void Destroyed()
	{
		DestroyedCount += 1;
		NextOrder += 1;
		DestroyedOrder = NextOrder;
	}
}
)AS"),
			TEXT("ATemplateGameLifetimeScriptActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld WorldTemplate(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(WorldTemplate.IsValid()));

		// 1. Spawn — UserConstructionScript is dispatched here automatically
		//    by UE (one or more times depending on the construction path).
		AActor* Actor = WorldTemplate.SpawnActorOfClass<AActor>(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("GameLifetime template should spawn the generated script actor")));

		// 2. BeginPlay — transition the actor into the Play phase.
		WorldTemplate.BeginPlay(*Actor);

		// 3. Tick — advance several frames at a stable DeltaTime to verify that
		//    ReceiveTick keeps being dispatched.
		constexpr float DeltaTime = 0.016f;
		constexpr int32 NumTicks = 3;
		WorldTemplate.Tick(DeltaTime, NumTicks);

		// Snapshot BeginPlay/Tick counts before destruction so we can confirm
		// later that Destroy() does not bump them any further.
		int32 BeginPlayCountBeforeDestroy = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCountBeforeDestroy)));

		int32 TickCountBeforeDestroy = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCountBeforeDestroy)));

		ASSERT_THAT(IsTrue(BeginPlayCountBeforeDestroy == 1, TEXT("BeginPlay should have run exactly once before destruction")));
		ASSERT_THAT(IsTrue(TickCountBeforeDestroy >= NumTicks, TEXT("Tick should have run at least once per world tick before destruction")));

		// 4 + 5. Destroy — synchronously dispatches EndPlay(Reason=Destroyed)
		//        and Destroyed.
		// After destruction the actor is marked PendingKill (TWeakObjectPtr is
		// considered invalid), but the UObject memory is still alive, and
		// FProperty::GetPropertyValue_InContainer reads the object memory directly,
		// so phase counters remain readable. This matches the read pattern used
		// by AngelscriptActorLifecycleTests.
		WorldTemplate.DestroyAndDrain(*Actor);

		int32 ConstructCount = 0;
		int32 BeginPlayCount = 0;
		int32 TickCount = 0;
		int32 EndPlayCount = 0;
		int32 DestroyedCount = 0;
		// AS-declared `float` UPROPERTY is reflected as FDoubleProperty in UE 5.x
		// (math types were migrated to double), so the C++ side reads it as double.
		double TotalDeltaTime = 0.0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ConstructCount"), ConstructCount)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("EndPlayCount"), EndPlayCount)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("DestroyedCount"), DestroyedCount)
			&& ReadPropertyValue<FDoubleProperty>(*TestRunner, Actor, TEXT("TotalDeltaTime"), TotalDeltaTime)));

		int32 ConstructOrder = 0;
		int32 BeginPlayOrder = 0;
		int32 FirstTickOrder = 0;
		int32 EndPlayOrder = 0;
		int32 DestroyedOrder = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ConstructOrder"), ConstructOrder)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayOrder"), BeginPlayOrder)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("FirstTickOrder"), FirstTickOrder)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("EndPlayOrder"), EndPlayOrder)
			&& ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("DestroyedOrder"), DestroyedOrder)));

		int64 LastEndPlayReason = -1;
		ASSERT_THAT(IsTrue(GetEnumByPath(*TestRunner, Actor, TEXT("LastEndPlayReason"), LastEndPlayReason)));

		// Counter checks: each phase fires at least once; BeginPlay / EndPlay /
		// Destroyed must fire exactly once across the whole lifecycle.
		ASSERT_THAT(IsTrue(ConstructCount >= 1, TEXT("UserConstructionScript should run at least once during spawn")));
		ASSERT_THAT(AreEqual(1, BeginPlayCount, TEXT("BeginPlay should run exactly once across the full lifecycle")));
		ASSERT_THAT(AreEqual(TickCountBeforeDestroy, TickCount, TEXT("Tick count should not change between Destroy() and the property read")));
		ASSERT_THAT(AreEqual(1, EndPlayCount, TEXT("EndPlay should run exactly once when Destroy() is called")));
		ASSERT_THAT(AreEqual(1, DestroyedCount, TEXT("Destroyed should run exactly once when Destroy() is called")));
		ASSERT_THAT(IsTrue(TotalDeltaTime > 0.0, TEXT("TotalDeltaTime should accumulate the per-tick DeltaTime")));

		// Ordering checks: UserConstructionScript -> BeginPlay -> Tick -> EndPlay -> Destroyed.
		ASSERT_THAT(IsTrue(ConstructOrder > 0 && ConstructOrder < BeginPlayOrder,
			TEXT("UserConstructionScript should run before BeginPlay")));
		ASSERT_THAT(IsTrue(BeginPlayOrder < FirstTickOrder, TEXT("BeginPlay should run before the first Tick")));
		ASSERT_THAT(IsTrue(FirstTickOrder < EndPlayOrder, TEXT("First Tick should run before EndPlay")));
		ASSERT_THAT(IsTrue(EndPlayOrder < DestroyedOrder, TEXT("EndPlay should run before Destroyed during destruction")));

		// Reason check: when destruction is triggered through Actor->Destroy(),
		// the EndPlay reason must be EEndPlayReason::Destroyed.
		ASSERT_THAT(AreEqual(static_cast<int64>(EEndPlayReason::Destroyed), LastEndPlayReason,
			TEXT("EndPlay should receive EEndPlayReason::Destroyed when triggered by Destroy()")));
	}
};

#endif
