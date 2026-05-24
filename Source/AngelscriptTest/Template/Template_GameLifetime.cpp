#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestWorld.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTemplateGameLifetimeScriptActorTest,
	"Angelscript.Template.GameLifetime.ScriptActorFullLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTemplateGameLifetimeScriptActorTest::RunTest(const FString& Parameters)
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
		*this,
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
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FAngelscriptTestWorld WorldTemplate(*this, Engine);
	if (!WorldTemplate.IsValid())
	{
		return false;
	}

	// 1. Spawn — UserConstructionScript is dispatched here automatically
	//    by UE (one or more times depending on the construction path).
	AActor* Actor = WorldTemplate.SpawnActorOfClass<AActor>(ScriptClass);
	if (!TestNotNull(TEXT("GameLifetime template should spawn the generated script actor"), Actor))
	{
		return false;
	}

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
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("BeginPlayCount"), BeginPlayCountBeforeDestroy))
	{
		return false;
	}

	int32 TickCountBeforeDestroy = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("TickCount"), TickCountBeforeDestroy))
	{
		return false;
	}

	TestTrue(TEXT("BeginPlay should have run exactly once before destruction"), BeginPlayCountBeforeDestroy == 1);
	TestTrue(TEXT("Tick should have run at least once per world tick before destruction"), TickCountBeforeDestroy >= NumTicks);

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
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ConstructCount"), ConstructCount)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("BeginPlayCount"), BeginPlayCount)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("TickCount"), TickCount)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("EndPlayCount"), EndPlayCount)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("DestroyedCount"), DestroyedCount)
		|| !ReadPropertyValue<FDoubleProperty>(*this, Actor, TEXT("TotalDeltaTime"), TotalDeltaTime))
	{
		return false;
	}

	int32 ConstructOrder = 0;
	int32 BeginPlayOrder = 0;
	int32 FirstTickOrder = 0;
	int32 EndPlayOrder = 0;
	int32 DestroyedOrder = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ConstructOrder"), ConstructOrder)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("BeginPlayOrder"), BeginPlayOrder)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("FirstTickOrder"), FirstTickOrder)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("EndPlayOrder"), EndPlayOrder)
		|| !ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("DestroyedOrder"), DestroyedOrder))
	{
		return false;
	}

	int64 LastEndPlayReason = -1;
	if (!GetEnumByPath(*this, Actor, TEXT("LastEndPlayReason"), LastEndPlayReason))
	{
		return false;
	}

	// Counter checks: each phase fires at least once; BeginPlay / EndPlay /
	// Destroyed must fire exactly once across the whole lifecycle.
	TestTrue(TEXT("UserConstructionScript should run at least once during spawn"), ConstructCount >= 1);
	TestEqual(TEXT("BeginPlay should run exactly once across the full lifecycle"), BeginPlayCount, 1);
	TestEqual(TEXT("Tick count should not change between Destroy() and the property read"), TickCount, TickCountBeforeDestroy);
	TestEqual(TEXT("EndPlay should run exactly once when Destroy() is called"), EndPlayCount, 1);
	TestEqual(TEXT("Destroyed should run exactly once when Destroy() is called"), DestroyedCount, 1);
	TestTrue(TEXT("TotalDeltaTime should accumulate the per-tick DeltaTime"), TotalDeltaTime > 0.0);

	// Ordering checks: UserConstructionScript -> BeginPlay -> Tick -> EndPlay -> Destroyed.
	TestTrue(TEXT("UserConstructionScript should run before BeginPlay"), ConstructOrder > 0 && ConstructOrder < BeginPlayOrder);
	TestTrue(TEXT("BeginPlay should run before the first Tick"), BeginPlayOrder < FirstTickOrder);
	TestTrue(TEXT("First Tick should run before EndPlay"), FirstTickOrder < EndPlayOrder);
	TestTrue(TEXT("EndPlay should run before Destroyed during destruction"), EndPlayOrder < DestroyedOrder);

	// Reason check: when destruction is triggered through Actor->Destroy(),
	// the EndPlay reason must be EEndPlayReason::Destroyed.
	TestEqual(TEXT("EndPlay should receive EEndPlayReason::Destroyed when triggered by Destroy()"),
		LastEndPlayReason, static_cast<int64>(EEndPlayReason::Destroyed));

	return true;
}

#endif
