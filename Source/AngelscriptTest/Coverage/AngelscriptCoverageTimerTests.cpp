#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

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
private:
	static void TickTimerManager(FAngelscriptEngine& Engine, UWorld& World, float DeltaTime, int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope WorldScope(Engine, &World);
			World.GetTimerManager().Tick(DeltaTime);
		}
	}

public:
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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer handle actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterSetIsPaused"), false,
			TEXT("newly registered timer should not start paused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterPauseIsPaused"), true,
			TEXT("PauseTimerHandle should mark the handle paused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterUnPauseIsPaused"), false,
			TEXT("UnPauseTimerHandle should clear the paused state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterClearIsPaused"), false,
			TEXT("cleared handle should no longer report paused"))));
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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("multiple timer handles actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSingleShotInitiallyActive"), true,
			TEXT("single-shot timer handle should be observable immediately after set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopInitiallyActive"), true,
			TEXT("looping timer handle should be observable immediately after set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSingleShotClearObserved"), true,
			TEXT("clearing single-shot handle should leave a non-paused invalid handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopClearObserved"), true,
			TEXT("clearing looping handle should leave a non-paused invalid handle"))));
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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer basic usage actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopingSetupComplete"), true,
			TEXT("looping timer should be set up successfully"))));
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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer management actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsActiveAfterSet"), true,
			TEXT("timer should be active after SetTimer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsPausedAfterPause"), true,
			TEXT("timer should be paused after PauseTimerHandle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsPausedAfterUnPause"), false,
			TEXT("timer should not be paused after UnPauseTimerHandle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsActiveAfterClear"), false,
			TEXT("timer should not be active after ClearAndInvalidateTimerHandle"))));
	}

	TEST_METHOD(TimerWithParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerWithParametersActor : AActor
			{
				FTimerHandle ParameterHandle;

				UFUNCTION()
				void ParameterCallback(int Value, FString Message)
				{
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ParameterHandle = System::SetTimer(this, n"ParameterCallback", 0.1f, false, 42, "payload");
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("SetTimer") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageTimer_WithParametersUnsupported"),
			*ScriptSource,
			TEXT("System::SetTimer by function name should reject callbacks that require parameters"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TimerDelegateLambdaSetTimerBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerDelegateLambdaActor : AActor
			{
				UPROPERTY()
				int LambdaCapturedValue = 0;

				FTimerHandle LambdaHandle;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					int CapturedValue = 42;
					LambdaHandle = System::SetTimer(FTimerDelegate(this, function()
					{
						LambdaCapturedValue = CapturedValue;
					}), 0.1f, false);
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("FTimerDelegate") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageTimer_DelegateLambdaUnsupported"),
			*ScriptSource,
			TEXT("System::SetTimer currently exposes function-name timers, not FTimerDelegate lambda overloads"),
			MakeArrayView(ExpectedDiagnostics))));
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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer delay execution actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bShortDelaySetup"), true,
			TEXT("short delay timer should be set up"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLongDelaySetup"), true,
			TEXT("long delay timer should be set up"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRepeatingDelaySetup"), true,
			TEXT("repeating delay timer should be set up"))));
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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("multiple timers actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAllTimersActive"), true,
			TEXT("all three timers should be active"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActiveTimerCount"), 3,
			TEXT("should have exactly 3 active timers"))));
	}

	TEST_METHOD(TimerRemainingAndElapsed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_RemainingElapsed"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerRemainingElapsed.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerRemainingElapsedActor : AActor
			{
				UPROPERTY()
				float InitialRemaining = 0.0f;

				UPROPERTY()
				float InitialElapsed = 0.0f;

				UPROPERTY()
				float ObservedRemaining = 0.0f;

				UPROPERTY()
				float ObservedElapsed = 0.0f;

				UPROPERTY()
				bool bRemainingIsPositive = false;

				UPROPERTY()
				bool bElapsedIsNonNegative = false;

				UPROPERTY()
				bool bRemainingDecreasedAfterTick = false;

				UPROPERTY()
				bool bElapsedIncreasedAfterTick = false;

				UPROPERTY()
				bool bQueriesSucceeded = false;

				FTimerHandle QueryHandle;

				UFUNCTION()
				void QueryCallback()
				{
					Print("QueryCallback executed");
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerRemainingAndElapsed: Testing GetTimerRemaining and GetTimerElapsed");

					// Set a timer with 2.0 second delay
					QueryHandle = System::SetTimer(this, n"QueryCallback", 2.0f, false);

					// Query immediately after setting
					InitialRemaining = System::GetTimerRemainingHandle(QueryHandle);
					InitialElapsed = System::GetTimerElapsedHandle(QueryHandle);

					bRemainingIsPositive = (InitialRemaining > 0.0f);
					bElapsedIsNonNegative = (InitialElapsed >= 0.0f);
					bQueriesSucceeded = true;

					Print("Initial Remaining: " + InitialRemaining + " seconds");
					Print("Initial Elapsed: " + InitialElapsed + " seconds");
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					ObservedRemaining = System::GetTimerRemainingHandle(QueryHandle);
					ObservedElapsed = System::GetTimerElapsedHandle(QueryHandle);
					bRemainingDecreasedAfterTick = (ObservedRemaining < InitialRemaining);
					bElapsedIncreasedAfterTick = (ObservedElapsed > InitialElapsed);
				}
			}
			)AS"),
			TEXT("ACoverageTimerRemainingElapsedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer remaining/elapsed actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer remaining/elapsed actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		TickTimerManager(Engine, Spawner.GetWorld(), 0.5f, 1);
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bQueriesSucceeded"), true,
			TEXT("timer queries should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemainingIsPositive"), true,
			TEXT("remaining time should be positive after timer set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bElapsedIsNonNegative"), true,
			TEXT("elapsed time query should return a non-negative value after timer set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemainingDecreasedAfterTick"), true,
			TEXT("GetTimerRemainingHandle should decrease after TimerManager advances"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bElapsedIncreasedAfterTick"), true,
			TEXT("GetTimerElapsedHandle should increase after TimerManager advances"))));
	}

	TEST_METHOD(TimerFirstDelay)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerFirstDelayActor : AActor
			{
				FTimerHandle FirstDelayHandle;

				UFUNCTION()
				void FirstDelayCallback()
				{
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FirstDelayHandle = System::SetTimer(this, n"FirstDelayCallback", 1.0f, true, 2.0f);
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("SetTimer") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageTimer_FirstDelayUnsupported"),
			*ScriptSource,
			TEXT("System::SetTimer should reject the native first-delay overload until that overload is bound"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TimerImmediateExecution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_ImmediateExecution"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerImmediateExecution.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerImmediateExecutionActor : AActor
			{
				UPROPERTY()
				bool bImmediateTimerSetup = false;

				UPROPERTY()
				float ImmediateRemaining = 0.0f;

				UPROPERTY()
				int CallbackCount = 0;

				FTimerHandle ImmediateHandle;

				UFUNCTION()
				void ImmediateCallback()
				{
					CallbackCount++;
					Print("ImmediateCallback executed on next tick, count: " + CallbackCount);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerImmediateExecution: Testing short-delay single-shot timer execution");

					ImmediateHandle = System::SetTimer(this, n"ImmediateCallback", 0.001f, false);

					bImmediateTimerSetup = System::IsTimerActiveHandle(ImmediateHandle);
					ImmediateRemaining = System::GetTimerRemainingHandle(ImmediateHandle);

					Print("Immediate timer set, active: " + bImmediateTimerSetup);
					Print("Remaining: " + ImmediateRemaining + " seconds");
				}
			}
			)AS"),
			TEXT("ACoverageTimerImmediateExecutionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer immediate execution actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer immediate execution actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bImmediateTimerSetup"), true,
			TEXT("short-delay single-shot timer should be set up successfully"))));
		TickTimerManager(Engine, Spawner.GetWorld(), 0.001f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 1,
			TEXT("short-delay single-shot timer should execute once on the next TimerManager tick"))));
		TickTimerManager(Engine, Spawner.GetWorld(), 0.001f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 1,
			TEXT("short-delay single-shot timer should not execute more than once"))));
	}

	TEST_METHOD(SystemDelay)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_SystemDelay"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerSystemDelay.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerSystemDelayActor : AActor
			{
				UPROPERTY()
				bool bBeforeDelay = false;

				UPROPERTY()
				bool bAfterDelay = false;

				UPROPERTY()
				bool bDelayComplete = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("SystemDelay: Testing System::Delay latent function");
					bBeforeDelay = true;
					TestDelaySequence();
				}

				UFUNCTION()
				void TestDelaySequence()
				{
					Print("Before System::Delay");
					bBeforeDelay = true;

					// Latent delay - code appears synchronous but executes across frames
					System::Delay(0.5f);

					Print("After System::Delay");
					bAfterDelay = true;
					bDelayComplete = true;
				}
			}
			)AS"),
			TEXT("ACoverageTimerSystemDelayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("System::Delay actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("System::Delay actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBeforeDelay"), true,
			TEXT("code before System::Delay should execute"))));
	}

	TEST_METHOD(TimerClearAndInvalidate)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_ClearInvalidate"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerClearInvalidate.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerClearInvalidateActor : AActor
			{
				UPROPERTY()
				bool bActiveBeforeClear = false;

				UPROPERTY()
				bool bActiveAfterClear = false;

				UPROPERTY()
				bool bValidBeforeClear = false;

				UPROPERTY()
				bool bValidAfterClear = false;

				FTimerHandle ClearHandle;

				UFUNCTION()
				void ClearTestCallback()
				{
					Print("ClearTestCallback (should not execute if cleared)");
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerClearAndInvalidate: Testing ClearAndInvalidateTimerHandle");

					// Set a timer
					ClearHandle = System::SetTimer(this, n"ClearTestCallback", 1.0f, false);

					bActiveBeforeClear = System::IsTimerActiveHandle(ClearHandle);
					bValidBeforeClear = ClearHandle.IsValid();

					Print("Before clear - Active: " + bActiveBeforeClear + ", Valid: " + bValidBeforeClear);

					// Clear and invalidate
					System::ClearAndInvalidateTimerHandle(ClearHandle);

					bActiveAfterClear = System::IsTimerActiveHandle(ClearHandle);
					bValidAfterClear = ClearHandle.IsValid();

					Print("After clear - Active: " + bActiveAfterClear + ", Valid: " + bValidAfterClear);
				}
			}
			)AS"),
			TEXT("ACoverageTimerClearInvalidateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer clear and invalidate actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer clear and invalidate actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bActiveBeforeClear"), true,
			TEXT("timer should be active before clear"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bValidBeforeClear"), true,
			TEXT("timer handle should be valid before clear"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bActiveAfterClear"), false,
			TEXT("timer should not be active after ClearAndInvalidateTimerHandle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bValidAfterClear"), false,
			TEXT("timer handle should be invalid after ClearAndInvalidateTimerHandle"))));
	}

	TEST_METHOD(TimerClearThenReuseHandleVariable)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_ClearThenReuseHandle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerClearThenReuseHandle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerClearThenReuseHandleActor : AActor
			{
				UPROPERTY()
				int FirstCallbackCount = 0;

				UPROPERTY()
				int SecondCallbackCount = 0;

				UPROPERTY()
				float FirstRemaining = 0.0f;

				UPROPERTY()
				float SecondRemaining = 0.0f;

				UPROPERTY()
				bool bHandleReusedAfterClear = false;

				FTimerHandle SharedHandle;

				UFUNCTION()
				void FirstCallback()
				{
					FirstCallbackCount++;
					Print("FirstCallback executed");
				}

				UFUNCTION()
				void SecondCallback()
				{
					SecondCallbackCount++;
					Print("SecondCallback executed");
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerClearThenReuseHandleVariable: Testing handle cleanup before reuse");

					// Set first timer
					SharedHandle = System::SetTimer(this, n"FirstCallback", 2.0f, false);
					FirstRemaining = System::GetTimerRemainingHandle(SharedHandle);
					Print("First timer set, remaining: " + FirstRemaining);

					System::ClearAndInvalidateTimerHandle(SharedHandle);

					// Reuse the same script handle variable after invalidating the old timer.
					SharedHandle = System::SetTimer(this, n"SecondCallback", 0.25f, false);
					SecondRemaining = System::GetTimerRemainingHandle(SharedHandle);
					Print("Second timer set after clear, remaining: " + SecondRemaining);

					bHandleReusedAfterClear = (SecondRemaining > 0.0f && SecondRemaining <= 0.25f);
				}
			}
			)AS"),
			TEXT("ACoverageTimerClearThenReuseHandleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer clear/reuse handle actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer clear/reuse handle actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		TickTimerManager(Engine, Spawner.GetWorld(), 0.3f, 1);
		TickTimerManager(Engine, Spawner.GetWorld(), 2.0f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bHandleReusedAfterClear"), true,
			TEXT("handle variable should be reusable after clear/invalidate"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstCallbackCount"), 0,
			TEXT("cleared old timer should not execute after reusing the handle variable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SecondCallbackCount"), 1,
			TEXT("replacement timer should execute exactly once"))));
	}

	TEST_METHOD(TimerActorDestroyStopsCallbacks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_ActorDestroyCleanup"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerActorDestroyCleanup.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerDestroyCleanupActor : AActor
			{
				UPROPERTY()
				int CallbackCount = 0;

				UPROPERTY()
				bool bTimerActiveBeforeDestroy = false;

				FTimerHandle DestroyCleanupHandle;

				UFUNCTION()
				void CleanupCallback()
				{
					CallbackCount++;
					Print("CleanupCallback executed, count: " + CallbackCount);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DestroyCleanupHandle = System::SetTimer(this, n"CleanupCallback", 0.1f, true);
					bTimerActiveBeforeDestroy = System::IsTimerActiveHandle(DestroyCleanupHandle);
				}
			}
			)AS"),
			TEXT("ACoverageTimerDestroyCleanupActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer destroy cleanup actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer destroy cleanup actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bTimerActiveBeforeDestroy"), true,
			TEXT("destroy cleanup timer should be active before actor destruction"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 0,
			TEXT("destroy cleanup timer should not have fired before the first timer tick"))));

		ASSERT_THAT(IsTrue(Actor->Destroy(), TEXT("timer destroy cleanup actor should accept destruction")));
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);
		TickTimerManager(Engine, Spawner.GetWorld(), 0.5f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 0,
			TEXT("timer bound to a destroyed actor should not continue invoking callbacks"))));
	}

	TEST_METHOD(TimerLambdaCapture)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerLambdaCaptureActor : AActor
			{
				FTimerHandle LambdaHandle1;
				FTimerHandle LambdaHandle2;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerLambdaCapture: Testing lambda callbacks with captured variables");

					int LocalValue = 42;
					FString LocalMessage = "Hello from lambda";

					// Lambda timer with captured variables
					System::SetTimer(FTimerDelegate(this, function()
					{
						Print("Lambda timer callback executed");
						Print("Captured LocalValue: " + LocalValue);
						Print("Captured LocalMessage: " + LocalMessage);
					}), 0.1f, false);

					// Multiple lambda timers with different captured values
					int Value1 = 100;
					int Value2 = 200;

					LambdaHandle1 = System::SetTimer(FTimerDelegate(this, function()
					{
						Print("Lambda1 with captured value: " + Value1);
					}), 0.2f, false);

					LambdaHandle2 = System::SetTimer(FTimerDelegate(this, function()
					{
						Print("Lambda2 with captured value: " + Value2);
					}), 0.3f, false);
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("FTimerDelegate") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageTimer_LambdaCaptureUnsupported"),
			*ScriptSource,
			TEXT("captured lambda timer delegates should remain an explicit unsupported System::SetTimer boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TimerUseCaseSkillCooldown)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_SkillCooldown"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerSkillCooldown.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerSkillCooldownActor : AActor
			{
				UPROPERTY()
				bool bSkillOnCooldown = false;

				UPROPERTY()
				float CooldownRemaining = 0.0f;

				UPROPERTY()
				int SkillUseCount = 0;

				FTimerHandle CooldownHandle;

				UFUNCTION()
				void UseSkill()
				{
					if (bSkillOnCooldown)
					{
						Print("Skill is on cooldown, remaining: " + CooldownRemaining);
						return;
					}

					SkillUseCount++;
					Print("Skill used! Count: " + SkillUseCount);

					// Start cooldown
					bSkillOnCooldown = true;
					CooldownHandle = System::SetTimer(this, n"OnCooldownComplete", 3.0f, false);
					CooldownRemaining = System::GetTimerRemainingHandle(CooldownHandle);
					Print("Cooldown started, " + CooldownRemaining + " seconds remaining");
				}

				UFUNCTION()
				void OnCooldownComplete()
				{
					bSkillOnCooldown = false;
					Print("Cooldown complete, skill ready!");
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerUseCaseSkillCooldown: Simulating skill cooldown pattern");
					UseSkill();  // First use should succeed
				}
			}
			)AS"),
			TEXT("ACoverageTimerSkillCooldownActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer skill cooldown actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer skill cooldown actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSkillOnCooldown"), true,
			TEXT("skill should be on cooldown after use"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SkillUseCount"), 1,
			TEXT("skill should have been used once"))));
	}

	TEST_METHOD(TimerUseCasePeriodicCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_PeriodicCheck"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerPeriodicCheck.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerPeriodicCheckActor : AActor
			{
				UPROPERTY()
				int CheckCount = 0;

				UPROPERTY()
				float CurrentHealth = 100.0f;

				UPROPERTY()
				bool bHealthCheckActive = false;

				FTimerHandle HealthCheckHandle;

				UFUNCTION()
				void CheckHealth()
				{
					CheckCount++;
					Print("Health check #" + CheckCount + ", current health: " + CurrentHealth);

					if (CurrentHealth <= 0.0f)
					{
						Print("Health depleted, stopping health check");
						System::ClearAndInvalidateTimerHandle(HealthCheckHandle);
						bHealthCheckActive = false;
					}
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerUseCasePeriodicCheck: Periodic health monitoring");

					// Check health every second
					HealthCheckHandle = System::SetTimer(this, n"CheckHealth", 1.0f, true);
					bHealthCheckActive = System::IsTimerActiveHandle(HealthCheckHandle);

					Print("Health check timer started (1.0s interval)");
				}
			}
			)AS"),
			TEXT("ACoverageTimerPeriodicCheckActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer periodic check actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer periodic check actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bHealthCheckActive"), true,
			TEXT("health check timer should be active"))));
	}

	TEST_METHOD(TimerUseCaseBuffDuration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_BuffDuration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerBuffDuration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerBuffDurationActor : AActor
			{
				UPROPERTY()
				bool bHasSpeedBuff = false;

				UPROPERTY()
				float BuffRemainingTime = 0.0f;

				UPROPERTY()
				float SpeedMultiplier = 1.0f;

				FTimerHandle BuffHandle;

				UFUNCTION()
				void ApplySpeedBuff(float Duration)
				{
					Print("Applying speed buff for " + Duration + " seconds");
					bHasSpeedBuff = true;
					SpeedMultiplier = 2.0f;

					// Set timer to remove buff after duration
					BuffHandle = System::SetTimer(this, n"RemoveSpeedBuff", Duration, false);
					BuffRemainingTime = System::GetTimerRemainingHandle(BuffHandle);

					Print("Speed buff active, remaining: " + BuffRemainingTime + " seconds");
				}

				UFUNCTION()
				void RemoveSpeedBuff()
				{
					Print("Speed buff expired");
					bHasSpeedBuff = false;
					SpeedMultiplier = 1.0f;
					BuffRemainingTime = 0.0f;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerUseCaseBuffDuration: Buff duration management");
					ApplySpeedBuff(10.0f);  // 10 second buff
				}
			}
			)AS"),
			TEXT("ACoverageTimerBuffDurationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer buff duration actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer buff duration actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bHasSpeedBuff"), true,
			TEXT("speed buff should be active after application"))));
	}

	TEST_METHOD(TimerHandleInvalidationAndSupportedQueries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_HandleInvalidationSupportedQueries"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerHandleInvalidationSupportedQueries.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerInvalidationActor : AActor
			{
				UPROPERTY()
				bool bValidAfterSet = false;

				UPROPERTY()
				bool bPausedAfterSet = true;

				UPROPERTY()
				bool bPausedAfterPause = false;

				UPROPERTY()
				bool bPausedAfterUnpause = true;

				UPROPERTY()
				bool bInvalidAfterClear = false;

				UPROPERTY()
				bool bPausedQueryAfterClearIsFalse = true;

				FTimerHandle Handle;

				UFUNCTION()
				void Callback()
				{
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Handle = System::SetTimer(this, n"Callback", 30.0f, true);
					bValidAfterSet = Handle.IsValid();
					bPausedAfterSet = System::IsTimerPausedHandle(Handle);

					System::PauseTimerHandle(Handle);
					bPausedAfterPause = System::IsTimerPausedHandle(Handle);

					System::UnPauseTimerHandle(Handle);
					bPausedAfterUnpause = System::IsTimerPausedHandle(Handle);

					System::ClearAndInvalidateTimerHandle(Handle);
					bInvalidAfterClear = !Handle.IsValid();
					bPausedQueryAfterClearIsFalse = !System::IsTimerPausedHandle(Handle);
				}
			}
			)AS"),
			TEXT("ACoverageTimerInvalidationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer handle invalidation actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer handle invalidation actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bValidAfterSet"), true,
			TEXT("System::SetTimer should return a valid FTimerHandle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPausedAfterSet"), false,
			TEXT("new timer should not report paused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPausedAfterPause"), true,
			TEXT("PauseTimerHandle should mark supported handle query paused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPausedAfterUnpause"), false,
			TEXT("UnPauseTimerHandle should mark supported handle query unpaused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInvalidAfterClear"), true,
			TEXT("ClearAndInvalidateTimerHandle should invalidate the AS handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPausedQueryAfterClearIsFalse"), true,
			TEXT("cleared invalid timer handle should not report paused"))));
	}

	TEST_METHOD(TimerCompileStableActorAndComponentCallSites)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_CompileStableCallSites"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerCompileStableCallSites.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageTimerComponentCallSite : UActorComponent
			{
				FTimerHandle ComponentHandle;

				UFUNCTION()
				void ComponentCallback()
				{
				}

				UFUNCTION()
				bool ConfigureComponentTimer()
				{
					ComponentHandle = System::SetTimer(this, n"ComponentCallback", 1.0f, true);
					bool bActive = System::IsTimerActiveHandle(ComponentHandle);
					float Remaining = System::GetTimerRemainingHandle(ComponentHandle);
					float Elapsed = System::GetTimerElapsedHandle(ComponentHandle);

					System::PauseTimerHandle(ComponentHandle);
					bool bPaused = System::IsTimerPausedHandle(ComponentHandle);
					System::UnPauseTimerHandle(ComponentHandle);
					System::ClearAndInvalidateTimerHandle(ComponentHandle);

					return bActive && bPaused && Remaining >= 0.0f && Elapsed >= 0.0f && !ComponentHandle.IsValid();
				}
			}

			UCLASS()
			class ACoverageTimerCompileStableActor : AActor
			{
				FTimerHandle NextTickHandle;
				FTimerHandle FunctionNameHandle;

				UPROPERTY()
				bool bNextTickCallSiteCompiled = false;

				UPROPERTY()
				bool bFunctionNameCallSiteCompiled = false;

				UFUNCTION()
				void NextTickCallback()
				{
				}

				UFUNCTION()
				void FunctionNameCallback()
				{
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					NextTickHandle = System::SetTimer(this, n"NextTickCallback", 0.0f, false);
					bNextTickCallSiteCompiled = NextTickHandle.IsValid() || !System::IsTimerPausedHandle(NextTickHandle);
					System::ClearAndInvalidateTimerHandle(NextTickHandle);

					FunctionNameHandle = System::SetTimer(this, n"FunctionNameCallback", 0.25f, true);
					bFunctionNameCallSiteCompiled = System::IsTimerActiveHandle(FunctionNameHandle)
						&& System::GetTimerRemainingHandle(FunctionNameHandle) >= 0.0f
						&& System::GetTimerElapsedHandle(FunctionNameHandle) >= 0.0f;
					System::ClearAndInvalidateTimerHandle(FunctionNameHandle);
				}
			}
			)AS"),
			TEXT("ACoverageTimerCompileStableActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("stable timer actor/component call sites should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageTimerComponentCallSite"));
		ASSERT_THAT(IsNotNull(ComponentClass, TEXT("component timer call-site class should be generated")));
		if (ComponentClass != nullptr)
		{
			UFunction* ConfigureFunction = FindGeneratedFunction(ComponentClass, TEXT("ConfigureComponentTimer"));
			ASSERT_THAT(IsNotNull(ConfigureFunction, TEXT("component timer setup function should be generated")));
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("stable timer call-site actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNextTickCallSiteCompiled"), true,
			TEXT("0.0f single-shot timer call site should compile and be safe to clear"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFunctionNameCallSiteCompiled"), true,
			TEXT("function-name looping timer call site should expose active/remaining/elapsed queries"))));
	}

	TEST_METHOD(TimerRepeatedFunctionNameReplacesExistingTimer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_RepeatedFunctionNameReplaces"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerRepeatedFunctionNameReplaces.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerRepeatedFunctionNameActor : AActor
			{
				UPROPERTY()
				int CallbackCount = 0;

				UPROPERTY()
				bool bFirstHandleActiveBeforeReplace = false;

				UPROPERTY()
				bool bFirstHandleInactiveAfterReplace = false;

				UPROPERTY()
				bool bReplacementHandleActive = false;

				UPROPERTY()
				float ReplacementRemaining = 0.0f;

				FTimerHandle FirstHandle;
				FTimerHandle ReplacementHandle;

				UFUNCTION()
				void SharedCallback()
				{
					CallbackCount++;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FirstHandle = System::SetTimer(this, n"SharedCallback", 2.0f, false);
					bFirstHandleActiveBeforeReplace = System::IsTimerActiveHandle(FirstHandle);

					ReplacementHandle = System::SetTimer(this, n"SharedCallback", 0.25f, false);
					bFirstHandleInactiveAfterReplace = !System::IsTimerActiveHandle(FirstHandle);
					bReplacementHandleActive = System::IsTimerActiveHandle(ReplacementHandle);
					ReplacementRemaining = System::GetTimerRemainingHandle(ReplacementHandle);
				}
			}
			)AS"),
			TEXT("ACoverageTimerRepeatedFunctionNameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("repeated function-name timer actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("repeated function-name timer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFirstHandleActiveBeforeReplace"), true,
			TEXT("first dynamic timer should be active before replacement"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFirstHandleInactiveAfterReplace"), true,
			TEXT("repeating SetTimer with the same object/function should clear the old handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bReplacementHandleActive"), true,
			TEXT("replacement dynamic timer handle should be active"))));

		double ReplacementRemaining = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReplacementRemaining"), ReplacementRemaining),
			TEXT("replacement remaining time should be readable")));
		ASSERT_THAT(IsTrue(ReplacementRemaining > 0.0 && ReplacementRemaining <= 0.25,
			TEXT("replacement timer should expose the shorter replacement delay")));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.3f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 1,
			TEXT("replacement timer should execute once at the shorter delay"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 2.0f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 1,
			TEXT("old replaced timer should not execute later"))));
	}

	TEST_METHOD(TimerDelegateLambdaSingleShotBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerDelegateLambdaActor : AActor
			{
				UPROPERTY()
				int LambdaCallCount = 0;

				UPROPERTY()
				int LambdaObservedValue = 0;

				UPROPERTY()
				bool bLambdaHandleActiveAfterSet = false;

				UPROPERTY()
				int SeedValue = 41;

				FTimerHandle LambdaHandle;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					LambdaHandle = System::SetTimer(FTimerDelegate(this, function()
					{
						LambdaCallCount++;
						LambdaObservedValue = SeedValue + 1;
					}), 0.05f, false);

					bLambdaHandleActiveAfterSet = System::IsTimerActiveHandle(LambdaHandle);
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("FTimerDelegate") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageTimer_DelegateLambdaSingleShotUnsupported"),
			*ScriptSource,
			TEXT("single-shot FTimerDelegate lambda timers should remain unsupported until the delegate overload is bound"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TimerDynamicFunctionNameReflectionLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_DynamicFunctionNameReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerDynamicFunctionNameReflection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerDynamicFunctionNameActor : AActor
			{
				UPROPERTY()
				int CallbackCount = 0;

				UPROPERTY()
				int SetupCount = 0;

				UPROPERTY()
				float RemainingAfterSetup = 0.0f;

				UPROPERTY()
				bool bConfiguredViaName = false;

				UPROPERTY()
				bool bTimerActiveAfterSetup = false;

				UPROPERTY()
				bool bPausedAfterReflectionPause = false;

				UPROPERTY()
				bool bPausedAfterReflectionResume = true;

				UPROPERTY()
				bool bInvalidAfterReflectionClear = false;

				UPROPERTY()
				bool bActiveAfterReflectionClear = true;

				FTimerHandle DynamicHandle;

				UFUNCTION()
				void DynamicCallback()
				{
					CallbackCount++;
				}

				UFUNCTION()
				bool ConfigureDynamicTimer(FName CallbackName, float DelaySeconds, bool bLooping)
				{
					SetupCount++;
					DynamicHandle = System::SetTimer(this, CallbackName, DelaySeconds, bLooping);
					bConfiguredViaName = DynamicHandle.IsValid();
					bTimerActiveAfterSetup = System::IsTimerActiveHandle(DynamicHandle);
					RemainingAfterSetup = System::GetTimerRemainingHandle(DynamicHandle);
					return bConfiguredViaName && bTimerActiveAfterSetup;
				}

				UFUNCTION()
				bool PauseDynamicTimer()
				{
					System::PauseTimerHandle(DynamicHandle);
					bPausedAfterReflectionPause = System::IsTimerPausedHandle(DynamicHandle);
					return bPausedAfterReflectionPause;
				}

				UFUNCTION()
				bool ResumeDynamicTimer()
				{
					System::UnPauseTimerHandle(DynamicHandle);
					bPausedAfterReflectionResume = System::IsTimerPausedHandle(DynamicHandle);
					return !bPausedAfterReflectionResume;
				}

				UFUNCTION()
				bool ClearDynamicTimer()
				{
					System::ClearAndInvalidateTimerHandle(DynamicHandle);
					bInvalidAfterReflectionClear = !DynamicHandle.IsValid();
					bActiveAfterReflectionClear = System::IsTimerActiveHandle(DynamicHandle);
					return bInvalidAfterReflectionClear && !bActiveAfterReflectionClear;
				}
			}
			)AS"),
			TEXT("ACoverageTimerDynamicFunctionNameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("dynamic function-name timer actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("dynamic function-name timer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		{
			FFunctionInvoker ConfigureInvoker(*TestRunner, Actor, TEXT("ConfigureDynamicTimer"));
			ASSERT_THAT(IsTrue(ConfigureInvoker.IsValid(), TEXT("ConfigureDynamicTimer should be invokable")));
			if (!ConfigureInvoker.IsValid())
			{
				return;
			}
			ConfigureInvoker.AddParam<FName>(FName(TEXT("DynamicCallback"))).AddParam<double>(0.5).AddParam<bool>(true);
			const bool bConfigured = ConfigureInvoker.CallAndReturn<bool>(false);
			ASSERT_THAT(IsTrue(bConfigured, TEXT("dynamic FName timer setup should return an active handle")));
			if (!bConfigured)
			{
				return;
			}
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetupCount"), 1,
			TEXT("reflection timer setup should execute exactly once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bConfiguredViaName"), true,
			TEXT("System::SetTimer should accept an FName supplied through reflection"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bTimerActiveAfterSetup"), true,
			TEXT("dynamic function-name timer should be active after setup"))));

		double RemainingAfterSetup = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RemainingAfterSetup"), RemainingAfterSetup),
			TEXT("dynamic function-name remaining time should be readable")));
		ASSERT_THAT(IsTrue(RemainingAfterSetup > 0.0 && RemainingAfterSetup <= 0.5,
			TEXT("dynamic function-name timer should expose its configured delay")));

		{
			FFunctionInvoker PauseInvoker(*TestRunner, Actor, TEXT("PauseDynamicTimer"));
			ASSERT_THAT(IsTrue(PauseInvoker.IsValid(), TEXT("PauseDynamicTimer should be invokable")));
			if (!PauseInvoker.IsValid())
			{
				return;
			}
			const bool bPaused = PauseInvoker.CallAndReturn<bool>(false);
			ASSERT_THAT(IsTrue(bPaused, TEXT("reflection pause should mark the dynamic timer paused")));
			if (!bPaused)
			{
				return;
			}
		}

		TickTimerManager(Engine, Spawner.GetWorld(), 1.0f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 0,
			TEXT("paused dynamic timer should not execute while TimerManager advances"))));

		{
			FFunctionInvoker ResumeInvoker(*TestRunner, Actor, TEXT("ResumeDynamicTimer"));
			ASSERT_THAT(IsTrue(ResumeInvoker.IsValid(), TEXT("ResumeDynamicTimer should be invokable")));
			if (!ResumeInvoker.IsValid())
			{
				return;
			}
			const bool bResumed = ResumeInvoker.CallAndReturn<bool>(false);
			ASSERT_THAT(IsTrue(bResumed, TEXT("reflection resume should unpause the dynamic timer")));
			if (!bResumed)
			{
				return;
			}
		}

		TickTimerManager(Engine, Spawner.GetWorld(), 0.5f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 1,
			TEXT("resumed dynamic timer should invoke its reflected FName callback"))));

		{
			FFunctionInvoker ClearInvoker(*TestRunner, Actor, TEXT("ClearDynamicTimer"));
			ASSERT_THAT(IsTrue(ClearInvoker.IsValid(), TEXT("ClearDynamicTimer should be invokable")));
			if (!ClearInvoker.IsValid())
			{
				return;
			}
			const bool bCleared = ClearInvoker.CallAndReturn<bool>(false);
			ASSERT_THAT(IsTrue(bCleared, TEXT("reflection clear should invalidate the dynamic timer handle")));
			if (!bCleared)
			{
				return;
			}
		}

		TickTimerManager(Engine, Spawner.GetWorld(), 1.0f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackCount"), 1,
			TEXT("cleared dynamic timer should not execute again"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInvalidAfterReflectionClear"), true,
			TEXT("ClearAndInvalidateTimerHandle should invalidate a reflected dynamic timer handle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bActiveAfterReflectionClear"), false,
			TEXT("cleared reflected dynamic timer should no longer report active"))));
	}

	TEST_METHOD(TimerInvalidHandleQueriesStayDeterministic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_InvalidHandleQueries"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerInvalidHandleQueries.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerInvalidHandleQueryActor : AActor
			{
				UPROPERTY()
				float InvalidRemaining = 1.0f;

				UPROPERTY()
				float InvalidElapsed = 1.0f;

				UPROPERTY()
				bool bDefaultHandleInvalid = false;

				UPROPERTY()
				bool bInactiveBeforeSet = false;

				UPROPERTY()
				bool bNotPausedBeforeSet = false;

				UPROPERTY()
				bool bRemainingNonPositiveForInvalid = false;

				UPROPERTY()
				bool bElapsedNonPositiveForInvalid = false;

				UPROPERTY()
				bool bStillInvalidAfterNoopLifecycle = false;

				FTimerHandle InvalidHandle;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					bDefaultHandleInvalid = !InvalidHandle.IsValid();
					bInactiveBeforeSet = !System::IsTimerActiveHandle(InvalidHandle);
					bNotPausedBeforeSet = !System::IsTimerPausedHandle(InvalidHandle);

					InvalidRemaining = System::GetTimerRemainingHandle(InvalidHandle);
					InvalidElapsed = System::GetTimerElapsedHandle(InvalidHandle);
					bRemainingNonPositiveForInvalid = (InvalidRemaining <= 0.0f);
					bElapsedNonPositiveForInvalid = (InvalidElapsed <= 0.0f);

					System::PauseTimerHandle(InvalidHandle);
					System::UnPauseTimerHandle(InvalidHandle);
					System::ClearAndInvalidateTimerHandle(InvalidHandle);

					bStillInvalidAfterNoopLifecycle = !InvalidHandle.IsValid()
						&& !System::IsTimerActiveHandle(InvalidHandle)
						&& !System::IsTimerPausedHandle(InvalidHandle);
				}
			}
			)AS"),
			TEXT("ACoverageTimerInvalidHandleQueryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("invalid handle query actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("invalid handle query actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDefaultHandleInvalid"), true,
			TEXT("default FTimerHandle should start invalid"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInactiveBeforeSet"), true,
			TEXT("invalid FTimerHandle should not report active"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNotPausedBeforeSet"), true,
			TEXT("invalid FTimerHandle should not report paused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemainingNonPositiveForInvalid"), true,
			TEXT("invalid FTimerHandle remaining query should be non-positive"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bElapsedNonPositiveForInvalid"), true,
			TEXT("invalid FTimerHandle elapsed query should be non-positive"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStillInvalidAfterNoopLifecycle"), true,
			TEXT("pause, unpause, and clear should remain deterministic for an invalid FTimerHandle"))));
	}

	TEST_METHOD(TimerUiCountdownAndAiStatePatterns)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_UiCountdownAiState"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerUiCountdownAiState.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerUiCountdownAiStateActor : AActor
			{
				UPROPERTY()
				int CountdownValue = 0;

				UPROPERTY()
				bool bPromptVisible = false;

				UPROPERTY()
				bool bPromptHiddenByTimer = false;

				UPROPERTY()
				bool bCanAttack = false;

				UPROPERTY()
				bool bAlertState = false;

				UPROPERTY()
				bool bCountdownTimerCleared = false;

				FTimerHandle PromptHandle;
				FTimerHandle CountdownHandle;
				FTimerHandle AttackGateHandle;
				FTimerHandle AlertStateHandle;

				UFUNCTION()
				void HidePrompt()
				{
					bPromptVisible = false;
					bPromptHiddenByTimer = true;
				}

				UFUNCTION()
				void AdvanceCountdown()
				{
					CountdownValue -= 1;
					if (CountdownValue <= 0)
					{
						CountdownValue = 0;
						System::ClearAndInvalidateTimerHandle(CountdownHandle);
						bCountdownTimerCleared = !CountdownHandle.IsValid();
					}
				}

				UFUNCTION()
				void UnlockAttack()
				{
					bCanAttack = true;
				}

				UFUNCTION()
				void EnterAlertState()
				{
					bAlertState = true;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					bPromptVisible = true;
					CountdownValue = 3;
					bCanAttack = false;
					bAlertState = false;

					PromptHandle = System::SetTimer(this, n"HidePrompt", 0.1f, false);
					CountdownHandle = System::SetTimer(this, n"AdvanceCountdown", 0.1f, true);
					AttackGateHandle = System::SetTimer(this, n"UnlockAttack", 0.1f, false);
					AlertStateHandle = System::SetTimer(this, n"EnterAlertState", 0.2f, false);
				}
			}
			)AS"),
			TEXT("ACoverageTimerUiCountdownAiStateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UI countdown and AI state timer actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UI countdown and AI state timer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPromptVisible"), true,
			TEXT("prompt should be visible before its hide timer fires"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountdownValue"), 3,
			TEXT("countdown should start at its configured value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bCanAttack"), false,
			TEXT("attack gate should start locked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAlertState"), false,
			TEXT("AI alert state should start inactive"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.1f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPromptHiddenByTimer"), true,
			TEXT("prompt message should hide from a single-shot timer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bCanAttack"), true,
			TEXT("attack interval timer should unlock attack after its delay"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountdownValue"), 2,
			TEXT("countdown timer should decrement after one timer tick"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAlertState"), false,
			TEXT("state switch timer should not fire before its delay"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.1f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountdownValue"), 1,
			TEXT("countdown timer should continue looping"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAlertState"), true,
			TEXT("state switch timer should enter alert state after its delay"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.1f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountdownValue"), 0,
			TEXT("countdown timer should reach zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bCountdownTimerCleared"), true,
			TEXT("countdown timer should clear itself when complete"))));
	}

	TEST_METHOD(TimerComponentCallbacksRunOnOwnerWorld)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_ComponentCallbacks"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerComponentCallbacks.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageTimerRuntimeComponent : UActorComponent
			{
				UPROPERTY()
				int ComponentCallCount = 0;

				UPROPERTY()
				bool bActiveAfterSetup = false;

				UPROPERTY()
				bool bPausedStoppedCallbacks = false;

				FTimerHandle ComponentHandle;

				UFUNCTION()
				void ComponentCallback()
				{
					ComponentCallCount++;
				}

				UFUNCTION()
				void ConfigureComponentTimer()
				{
					ComponentHandle = System::SetTimer(this, n"ComponentCallback", 0.1f, true);
					bActiveAfterSetup = System::IsTimerActiveHandle(ComponentHandle);
				}

				UFUNCTION()
				void PauseComponentTimer()
				{
					System::PauseTimerHandle(ComponentHandle);
				}

				UFUNCTION()
				void MarkPausedResult()
				{
					bPausedStoppedCallbacks = (ComponentCallCount == 1)
						&& System::IsTimerPausedHandle(ComponentHandle);
				}

				UFUNCTION()
				void ClearComponentTimer()
				{
					System::ClearAndInvalidateTimerHandle(ComponentHandle);
				}
			}

			UCLASS()
			class ACoverageTimerComponentOwnerActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageTimerRuntimeComponent TimerComponent;

				UPROPERTY()
				bool bComponentSetupComplete = false;

				UPROPERTY()
				bool bComponentPausedStoppedCallbacks = false;

				UPROPERTY()
				int ObservedComponentCallCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TimerComponent.ConfigureComponentTimer();
					bComponentSetupComplete = TimerComponent.bActiveAfterSetup;
				}

				UFUNCTION()
				void PauseComponentTimerFromOwner()
				{
					TimerComponent.PauseComponentTimer();
				}

				UFUNCTION()
				void SnapshotComponentTimerState()
				{
					TimerComponent.MarkPausedResult();
					bComponentPausedStoppedCallbacks = TimerComponent.bPausedStoppedCallbacks;
					ObservedComponentCallCount = TimerComponent.ComponentCallCount;
				}

				UFUNCTION()
				void ClearComponentTimerFromOwner()
				{
					TimerComponent.ClearComponentTimer();
				}
			}
			)AS"),
			TEXT("ACoverageTimerComponentOwnerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("component timer owner actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("component timer owner actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComponentSetupComplete"), true,
			TEXT("component should register an active timer through its owner world"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.1f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TimerComponent.ComponentCallCount"), 1,
			TEXT("component timer callback should run when the owner world timer manager advances"))));

		{
			FFunctionInvoker PauseInvoker(*TestRunner, Actor, TEXT("PauseComponentTimerFromOwner"));
			ASSERT_THAT(IsTrue(PauseInvoker.IsValid(), TEXT("PauseComponentTimerFromOwner should be invokable")));
			if (!PauseInvoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(PauseInvoker.Call(), TEXT("PauseComponentTimerFromOwner should execute")));
		}

		TickTimerManager(Engine, Spawner.GetWorld(), 0.3f, 1);

		{
			FFunctionInvoker SnapshotInvoker(*TestRunner, Actor, TEXT("SnapshotComponentTimerState"));
			ASSERT_THAT(IsTrue(SnapshotInvoker.IsValid(), TEXT("SnapshotComponentTimerState should be invokable")));
			if (!SnapshotInvoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(SnapshotInvoker.Call(), TEXT("SnapshotComponentTimerState should execute")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComponentPausedStoppedCallbacks"), true,
			TEXT("paused component timer should not execute additional callbacks"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObservedComponentCallCount"), 1,
			TEXT("component callback count should remain stable while paused"))));

		{
			FFunctionInvoker ClearInvoker(*TestRunner, Actor, TEXT("ClearComponentTimerFromOwner"));
			ASSERT_THAT(IsTrue(ClearInvoker.IsValid(), TEXT("ClearComponentTimerFromOwner should be invokable")));
			if (!ClearInvoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(ClearInvoker.Call(), TEXT("ClearComponentTimerFromOwner should execute")));
		}
	}

	TEST_METHOD(TimerDelayedSpawnUseCaseRunsFromWorldTimer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_DelayedSpawnUseCase"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerDelayedSpawnUseCase.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerDelayedSpawnActor : AActor
			{
				UPROPERTY()
				AActor SpawnedActor;

				UPROPERTY()
				bool bSpawnTimerActiveAfterSetup = false;

				UPROPERTY()
				bool bSpawnedAfterDelay = false;

				UPROPERTY()
				bool bSpawnedActorValid = false;

				UPROPERTY()
				int SpawnCount = 0;

				FTimerHandle SpawnHandle;

				UFUNCTION()
				void SpawnDelayedActor()
				{
					SpawnedActor = SpawnActor(AActor::StaticClass());
					SpawnCount++;
					bSpawnedAfterDelay = true;
					bSpawnedActorValid = (SpawnedActor != nullptr);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SpawnHandle = System::SetTimer(this, n"SpawnDelayedActor", 0.2f, false);
					bSpawnTimerActiveAfterSetup = System::IsTimerActiveHandle(SpawnHandle);
				}
			}
			)AS"),
			TEXT("ACoverageTimerDelayedSpawnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delayed spawn timer actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delayed spawn timer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSpawnTimerActiveAfterSetup"), true,
			TEXT("delayed spawn timer should be active after setup"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSpawnedAfterDelay"), false,
			TEXT("delayed spawn should not run synchronously during BeginPlay"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.1f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSpawnedAfterDelay"), false,
			TEXT("delayed spawn should wait for its configured delay"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.1f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSpawnedAfterDelay"), true,
			TEXT("delayed spawn timer should execute after the configured delay"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSpawnedActorValid"), true,
			TEXT("delayed spawn timer callback should create a valid actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SpawnCount"), 1,
			TEXT("single-shot delayed spawn timer should execute once"))));

		TickTimerManager(Engine, Spawner.GetWorld(), 0.3f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SpawnCount"), 1,
			TEXT("single-shot delayed spawn timer should not repeat"))));
	}

	TEST_METHOD(TimerDestroyedComponentStopsCallbacks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_DestroyedComponentCleanup"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerDestroyedComponentCleanup.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageTimerDestroyableComponent : UActorComponent
			{
				UPROPERTY()
				int CallbackCount = 0;

				UPROPERTY()
				bool bTimerActiveBeforeDestroy = false;

				FTimerHandle DestroyedComponentHandle;

				UFUNCTION()
				void CallbackAfterDestroy()
				{
					CallbackCount++;
				}

				UFUNCTION()
				void ConfigureTimer()
				{
					DestroyedComponentHandle = System::SetTimer(this, n"CallbackAfterDestroy", 0.1f, true);
					bTimerActiveBeforeDestroy = System::IsTimerActiveHandle(DestroyedComponentHandle);
				}
			}

			UCLASS()
			class ACoverageTimerDestroyedComponentOwner : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageTimerDestroyableComponent DestroyableComponent;

				UPROPERTY()
				bool bComponentTimerActiveBeforeDestroy = false;

				UPROPERTY()
				bool bComponentDestroyed = false;

				UPROPERTY()
				int ObservedCallbackCount = -1;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DestroyableComponent.ConfigureTimer();
					bComponentTimerActiveBeforeDestroy = DestroyableComponent.bTimerActiveBeforeDestroy;
				}

				UFUNCTION()
				void DestroyTimerComponent()
				{
					DestroyableComponent.K2_DestroyComponent(this);
					bComponentDestroyed = DestroyableComponent.IsBeingDestroyed();
				}

				UFUNCTION()
				void SnapshotDestroyedComponent()
				{
					ObservedCallbackCount = DestroyableComponent.CallbackCount;
				}
			}
			)AS"),
			TEXT("ACoverageTimerDestroyedComponentOwner"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("destroyed component timer owner should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("destroyed component timer owner should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComponentTimerActiveBeforeDestroy"), true,
			TEXT("component timer should be active before component destruction"))));

		{
			FFunctionInvoker DestroyInvoker(*TestRunner, Actor, TEXT("DestroyTimerComponent"));
			ASSERT_THAT(IsTrue(DestroyInvoker.IsValid(), TEXT("DestroyTimerComponent should be invokable")));
			if (!DestroyInvoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(DestroyInvoker.Call(), TEXT("DestroyTimerComponent should execute")));
		}

		TickTimerManager(Engine, Spawner.GetWorld(), 0.3f, 1);

		{
			FFunctionInvoker SnapshotInvoker(*TestRunner, Actor, TEXT("SnapshotDestroyedComponent"));
			ASSERT_THAT(IsTrue(SnapshotInvoker.IsValid(), TEXT("SnapshotDestroyedComponent should be invokable")));
			if (!SnapshotInvoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(SnapshotInvoker.Call(), TEXT("SnapshotDestroyedComponent should execute")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComponentDestroyed"), true,
			TEXT("timer component should enter destruction state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObservedCallbackCount"), 0,
			TEXT("destroyed component timer callback should not run after TimerManager advances"))));
	}

	TEST_METHOD(LatentMovementFunctionsRemainCompileBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerLatentMovementBoundaryActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MoveComponentTo(nullptr, FVector::ZeroVector, FRotator::ZeroRotator, false, false, 0.25f, false, EMoveComponentAction::Move);
					RotatorTo(FRotator::ZeroRotator, FRotator(0.0f, 90.0f, 0.0f), 0.25f, 0.0f);
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("MoveComponentTo"), TEXT("RotatorTo") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageTimer_LatentMovementUnsupported"),
			*ScriptSource,
			TEXT("latent movement helpers should remain compile-boundary coverage until deterministic latent advance is available"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(SystemDelayCompilesWithoutDeterministicLatentAdvance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_SystemDelayBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerSystemDelayBoundary.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerSystemDelayBoundaryActor : AActor
			{
				UPROPERTY()
				bool bStartedDelaySequence = false;

				UPROPERTY()
				bool bAfterDelayReachedSynchronously = true;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RunDelaySequence();
				}

				UFUNCTION()
				void RunDelaySequence()
				{
					bStartedDelaySequence = true;
					System::Delay(0.25f);
					bAfterDelayReachedSynchronously = false;
				}
			}
			)AS"),
			TEXT("ACoverageTimerSystemDelayBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("System::Delay boundary actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("System::Delay boundary actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStartedDelaySequence"), true,
			TEXT("System::Delay call site should be reachable before latent suspension"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAfterDelayReachedSynchronously"), true,
			TEXT("headless coverage should not fake latent completion without deterministic async advance"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
