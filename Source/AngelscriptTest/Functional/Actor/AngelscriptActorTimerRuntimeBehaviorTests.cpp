#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (timer runtime: Pause / UnPause / Clear state machine)
// Note: in headless automation, World->Tick does not advance TimerManager, so this suite exercises
// the Pause/UnPause/Clear state machine via IsTimerPausedHandle snapshots taken inside the script
// rather than asserting on actual timer fire counts. Real fire-count behavior is tracked by
// `Angelscript.TestModule.Learning.Runtime.TimerAndLatent` (a non-asserting trace-style test).
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptActorTimerRuntimeBehaviorTests,
	"Angelscript.TestModule.Functional.Actor.TimerRuntimeBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PauseUnpauseAndClearTransitionsAreObservable)
	{
		using namespace AngelscriptFunctionalTestUtils;
		// Timer system queries IAngelscriptEngine::TryGetCurrentWorldContextObject(), which expects the
		// production-shared engine driving FActorTestSpawner's world.
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalActorTimerRuntimeBehavior"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalActorTimerRuntimeBehavior.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalTimerActor : AActor
{
	UPROPERTY()
	FTimerHandle LoopingHandle;

	UPROPERTY()
	bool bAfterSetIsPaused = false;

	UPROPERTY()
	bool bAfterPauseIsPaused = false;

	UPROPERTY()
	bool bAfterUnPauseIsPaused = false;

	UPROPERTY()
	bool bAfterClearIsPaused = false;

	UFUNCTION()
	void NoopTimerCallback()
	{
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		LoopingHandle = System::SetTimer(this, n"NoopTimerCallback", 0.5f, true);
		bAfterSetIsPaused = System::IsTimerPausedHandle(LoopingHandle);

		System::PauseTimerHandle(LoopingHandle);
		bAfterPauseIsPaused = System::IsTimerPausedHandle(LoopingHandle);

		System::UnPauseTimerHandle(LoopingHandle);
		bAfterUnPauseIsPaused = System::IsTimerPausedHandle(LoopingHandle);

		System::ClearAndInvalidateTimerHandle(LoopingHandle);
		bAfterClearIsPaused = System::IsTimerPausedHandle(LoopingHandle);
	}
}
)AS"),
			TEXT("AFunctionalTimerActor"));
		if (ActorClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		if (Actor == nullptr) { return; }
		BeginPlayActor(Engine, *Actor);

		auto ReadBool = [&](const TCHAR* PropertyName) -> bool
		{
			bool Value = false;
			ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, PropertyName, Value);
			return Value;
		};

		ASSERT_THAT(IsFalse(
			ReadBool(TEXT("bAfterSetIsPaused")),
			TEXT("Freshly registered looping timer should report IsTimerPausedHandle == false")));

		ASSERT_THAT(IsTrue(
			ReadBool(TEXT("bAfterPauseIsPaused")),
			TEXT("After PauseTimerHandle, IsTimerPausedHandle should report true")));

		ASSERT_THAT(IsFalse(
			ReadBool(TEXT("bAfterUnPauseIsPaused")),
			TEXT("After UnPauseTimerHandle, IsTimerPausedHandle should report false")));

		ASSERT_THAT(IsFalse(
			ReadBool(TEXT("bAfterClearIsPaused")),
			TEXT("After ClearAndInvalidateTimerHandle, IsTimerPausedHandle should report false (handle is gone)")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
