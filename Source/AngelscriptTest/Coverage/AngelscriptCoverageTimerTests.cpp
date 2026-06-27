#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageTimerTests
// -----------------------------------------------------------------------------
// Coverage landing file for script-side timer handles. Headless automation does
// not reliably prove real wall-clock timer callback counts, so these tests focus
// on deterministic Set/Pause/UnPause/Clear handle state and function-name
// lifecycle behavior.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageTimerTest,
	"Angelscript.TestModule.Coverage.Timer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(TimerHandlePauseUnpauseAndClear)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_HandleState"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerHandleState.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerHandleActor : AActor
{
	UPROPERTY()
	bool bAfterSetIsPaused = true;

	UPROPERTY()
	bool bAfterPauseIsPaused = false;

	UPROPERTY()
	bool bAfterUnPauseIsPaused = true;

	UPROPERTY()
	bool bAfterClearIsPaused = true;

	UPROPERTY()
	bool bCallbackCanCompile = false;

	FTimerHandle LoopingHandle;

	UFUNCTION()
	void NoopTimerCallback()
	{
		bCallbackCanCompile = true;
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
			TEXT("ACoverageTimerHandleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer handle actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer handle actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterSetIsPaused"), false,
			TEXT("newly registered timer should not start paused"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterPauseIsPaused"), true,
			TEXT("PauseTimerHandle should mark the handle paused"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterUnPauseIsPaused"), false,
			TEXT("UnPauseTimerHandle should clear the paused state"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterClearIsPaused"), false,
			TEXT("cleared handle should no longer report paused"));
	}

	TEST_METHOD(SingleShotAndMultipleHandlesCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_MultipleHandles"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerMultipleHandles.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerMultipleHandlesActor : AActor
{
	UPROPERTY()
	bool bSingleShotInitiallyActive = false;

	UPROPERTY()
	bool bLoopInitiallyActive = false;

	UPROPERTY()
	bool bSingleShotClearObserved = false;

	UPROPERTY()
	bool bLoopClearObserved = false;

	FTimerHandle SingleShotHandle;
	FTimerHandle LoopingHandle;

	UFUNCTION()
	void SingleShotCallback()
	{
	}

	UFUNCTION()
	void LoopCallback()
	{
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		SingleShotHandle = System::SetTimer(this, n"SingleShotCallback", 0.25f, false);
		LoopingHandle = System::SetTimer(this, n"LoopCallback", 0.5f, true);

		bSingleShotInitiallyActive = !System::IsTimerPausedHandle(SingleShotHandle);
		bLoopInitiallyActive = !System::IsTimerPausedHandle(LoopingHandle);

		System::ClearAndInvalidateTimerHandle(SingleShotHandle);
		System::ClearAndInvalidateTimerHandle(LoopingHandle);

		bSingleShotClearObserved = !System::IsTimerPausedHandle(SingleShotHandle);
		bLoopClearObserved = !System::IsTimerPausedHandle(LoopingHandle);
	}
}
)AS"),
			TEXT("ACoverageTimerMultipleHandlesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("multiple timer handles actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("multiple timer handles actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSingleShotInitiallyActive"), true,
			TEXT("single-shot timer handle should be observable immediately after set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopInitiallyActive"), true,
			TEXT("looping timer handle should be observable immediately after set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSingleShotClearObserved"), true,
			TEXT("clearing single-shot handle should leave a non-paused invalid handle"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopClearObserved"), true,
			TEXT("clearing looping handle should leave a non-paused invalid handle"));
	}

	TEST_METHOD(TimerBasicUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_BasicUsage"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerBasicUsage.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerBasicUsageActor : AActor
{
	UPROPERTY()
	int SingleShotCallCount = 0;

	UPROPERTY()
	int LoopingCallCount = 0;

	UPROPERTY()
	bool bSingleShotExecuted = false;

	UPROPERTY()
	bool bLoopingSetupComplete = false;

	FTimerHandle SingleShotHandle;
	FTimerHandle LoopingHandle;

	UFUNCTION()
	void SingleShotCallback()
	{
		SingleShotCallCount++;
		bSingleShotExecuted = true;
		Print("SingleShotCallback executed, count: " + SingleShotCallCount);
	}

	UFUNCTION()
	void LoopingCallback()
	{
		LoopingCallCount++;
		Print("LoopingCallback executed, count: " + LoopingCallCount);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Print("TimerBasicUsage: Setting up timers");

		// SetTimer single shot (non-looping)
		SingleShotHandle = System::SetTimer(this, n"SingleShotCallback", 0.1f, false);
		Print("Single shot timer set with 0.1s delay");

		// SetTimer looping
		LoopingHandle = System::SetTimer(this, n"LoopingCallback", 0.1f, true);
		bLoopingSetupComplete = true;
		Print("Looping timer set with 0.1s interval");
	}
}
)AS"),
			TEXT("ACoverageTimerBasicUsageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer basic usage actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer basic usage actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopingSetupComplete"), true,
			TEXT("looping timer should be set up successfully"));
	}

	TEST_METHOD(TimerManagement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_Management"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerManagement.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerManagementActor : AActor
{
	UPROPERTY()
	bool bIsActiveAfterSet = false;

	UPROPERTY()
	bool bIsActiveAfterClear = true;

	UPROPERTY()
	bool bIsPausedAfterPause = false;

	UPROPERTY()
	bool bIsPausedAfterUnPause = true;

	UPROPERTY()
	int CallbackExecutionCount = 0;

	FTimerHandle ManagedHandle;

	UFUNCTION()
	void ManagedCallback()
	{
		CallbackExecutionCount++;
		Print("ManagedCallback executed, count: " + CallbackExecutionCount);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Print("TimerManagement: Testing timer lifecycle operations");

		// Set a looping timer
		ManagedHandle = System::SetTimer(this, n"ManagedCallback", 0.2f, true);
		bIsActiveAfterSet = System::IsTimerActiveHandle(ManagedHandle);
		Print("Timer set, IsActive: " + bIsActiveAfterSet);

		// Pause the timer
		System::PauseTimerHandle(ManagedHandle);
		bIsPausedAfterPause = System::IsTimerPausedHandle(ManagedHandle);
		Print("Timer paused, IsPaused: " + bIsPausedAfterPause);

		// UnPause the timer
		System::UnPauseTimerHandle(ManagedHandle);
		bIsPausedAfterUnPause = System::IsTimerPausedHandle(ManagedHandle);
		Print("Timer unpaused, IsPaused: " + bIsPausedAfterUnPause);

		// Clear the timer
		System::ClearAndInvalidateTimerHandle(ManagedHandle);
		bIsActiveAfterClear = System::IsTimerActiveHandle(ManagedHandle);
		Print("Timer cleared, IsActive: " + bIsActiveAfterClear);
	}
}
)AS"),
			TEXT("ACoverageTimerManagementActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer management actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer management actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsActiveAfterSet"), true,
			TEXT("timer should be active after SetTimer"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsPausedAfterPause"), true,
			TEXT("timer should be paused after PauseTimerHandle"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsPausedAfterUnPause"), false,
			TEXT("timer should not be paused after UnPauseTimerHandle"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsActiveAfterClear"), false,
			TEXT("timer should not be active after ClearAndInvalidateTimerHandle"));
	}

	TEST_METHOD(TimerWithParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_WithParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerWithParameters.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerWithParametersActor : AActor
{
	UPROPERTY()
	int ReceivedValue = 0;

	UPROPERTY()
	FString ReceivedMessage;

	UPROPERTY()
	bool bLambdaExecuted = false;

	UPROPERTY()
	int LambdaCapturedValue = 0;

	FTimerHandle ParameterHandle;

	UFUNCTION()
	void ParameterCallback(int Value, FString Message)
	{
		ReceivedValue = Value;
		ReceivedMessage = Message;
		Print("ParameterCallback executed with Value: " + Value + ", Message: " + Message);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Print("TimerWithParameters: Testing timer with callback parameters");

		// Using a lambda with captured variables
		int CapturedValue = 42;
		System::SetTimer(FTimerDelegate(this, function()
		{
			bLambdaExecuted = true;
			LambdaCapturedValue = CapturedValue;
			Print("Lambda timer callback executed, captured: " + CapturedValue);
		}), 0.1f, false);

		Print("Timer with lambda set up, captured value: " + CapturedValue);
	}
}
)AS"),
			TEXT("ACoverageTimerWithParametersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer with parameters actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer with parameters actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify lambda setup compiled successfully
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LambdaCapturedValue"), 0,
			TEXT("lambda captured value should be initialized"));
	}

	TEST_METHOD(TimerDelayExecution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_DelayExecution"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerDelayExecution.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerDelayExecutionActor : AActor
{
	UPROPERTY()
	bool bShortDelaySetup = false;

	UPROPERTY()
	bool bLongDelaySetup = false;

	UPROPERTY()
	bool bRepeatingDelaySetup = false;

	UPROPERTY()
	int ShortDelayCallCount = 0;

	UPROPERTY()
	int RepeatingCallCount = 0;

	FTimerHandle ShortDelayHandle;
	FTimerHandle LongDelayHandle;
	FTimerHandle RepeatingHandle;

	UFUNCTION()
	void ShortDelayCallback()
	{
		ShortDelayCallCount++;
		Print("ShortDelayCallback executed after 0.5s delay");
	}

	UFUNCTION()
	void LongDelayCallback()
	{
		Print("LongDelayCallback executed after 1.0s delay");
	}

	UFUNCTION()
	void RepeatingCallback()
	{
		RepeatingCallCount++;
		Print("RepeatingCallback executed, count: " + RepeatingCallCount);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Print("TimerDelayExecution: Testing delayed execution patterns");

		// Delay 0.5 seconds, execute once
		ShortDelayHandle = System::SetTimer(this, n"ShortDelayCallback", 0.5f, false);
		bShortDelaySetup = true;
		Print("Short delay timer (0.5s) set for single execution");

		// Delay 1.0 second, execute once
		LongDelayHandle = System::SetTimer(this, n"LongDelayCallback", 1.0f, false);
		bLongDelaySetup = true;
		Print("Long delay timer (1.0s) set for single execution");

		// Delay 0.3 seconds, then repeat every 0.3 seconds
		RepeatingHandle = System::SetTimer(this, n"RepeatingCallback", 0.3f, true);
		bRepeatingDelaySetup = true;
		Print("Repeating timer (0.3s interval) set up");
	}
}
)AS"),
			TEXT("ACoverageTimerDelayExecutionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer delay execution actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer delay execution actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bShortDelaySetup"), true,
			TEXT("short delay timer should be set up"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLongDelaySetup"), true,
			TEXT("long delay timer should be set up"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRepeatingDelaySetup"), true,
			TEXT("repeating delay timer should be set up"));
	}

	TEST_METHOD(MultipleTimers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_MultipleTimers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerMultipleTimers.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageTimerMultipleTimersActor : AActor
{
	UPROPERTY()
	int FastTimerCallCount = 0;

	UPROPERTY()
	int MediumTimerCallCount = 0;

	UPROPERTY()
	int SlowTimerCallCount = 0;

	UPROPERTY()
	bool bAllTimersActive = false;

	UPROPERTY()
	int ActiveTimerCount = 0;

	FTimerHandle FastHandle;
	FTimerHandle MediumHandle;
	FTimerHandle SlowHandle;

	UFUNCTION()
	void FastTimerCallback()
	{
		FastTimerCallCount++;
		Print("FastTimer tick, count: " + FastTimerCallCount);
	}

	UFUNCTION()
	void MediumTimerCallback()
	{
		MediumTimerCallCount++;
		Print("MediumTimer tick, count: " + MediumTimerCallCount);
	}

	UFUNCTION()
	void SlowTimerCallback()
	{
		SlowTimerCallCount++;
		Print("SlowTimer tick, count: " + SlowTimerCallCount);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Print("MultipleTimers: Running multiple independent timers");

		// Fast timer: 0.1s interval
		FastHandle = System::SetTimer(this, n"FastTimerCallback", 0.1f, true);
		Print("Fast timer (0.1s) started");

		// Medium timer: 0.25s interval
		MediumHandle = System::SetTimer(this, n"MediumTimerCallback", 0.25f, true);
		Print("Medium timer (0.25s) started");

		// Slow timer: 0.5s interval
		SlowHandle = System::SetTimer(this, n"SlowTimerCallback", 0.5f, true);
		Print("Slow timer (0.5s) started");

		// Verify all timers are active
		int Count = 0;
		if (System::IsTimerActiveHandle(FastHandle))
			Count++;
		if (System::IsTimerActiveHandle(MediumHandle))
			Count++;
		if (System::IsTimerActiveHandle(SlowHandle))
			Count++;

		ActiveTimerCount = Count;
		bAllTimersActive = (Count == 3);
		Print("Active timer count: " + Count);
	}
}
)AS"),
			TEXT("ACoverageTimerMultipleTimersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("multiple timers actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("multiple timers actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAllTimersActive"), true,
			TEXT("all three timers should be active"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActiveTimerCount"), 3,
			TEXT("should have exactly 3 active timers"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
