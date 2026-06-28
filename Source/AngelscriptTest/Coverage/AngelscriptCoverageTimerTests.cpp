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
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer with parameters actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify lambda setup compiled successfully
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LambdaCapturedValue"), 0,
			TEXT("lambda captured value should be initialized"))));
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
				bool bRemainingIsPositive = false;

				UPROPERTY()
				bool bElapsedIsNonNegative = false;

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

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bQueriesSucceeded"), true,
			TEXT("timer queries should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRemainingIsPositive"), true,
			TEXT("remaining time should be positive after timer set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bElapsedIsNonNegative"), true,
			TEXT("elapsed time query should return a non-negative value after timer set"))));
	}

	TEST_METHOD(TimerFirstDelay)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_FirstDelay"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerFirstDelay.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerFirstDelayActor : AActor
			{
				UPROPERTY()
				float FirstDelayRemaining = 0.0f;

				UPROPERTY()
				bool bFirstDelayTimerActive = false;

				UPROPERTY()
				int CallbackCount = 0;

				FTimerHandle FirstDelayHandle;

				UFUNCTION()
				void FirstDelayCallback()
				{
					CallbackCount++;
					Print("FirstDelayCallback executed, count: " + CallbackCount);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("TimerFirstDelay: Testing looping timer with custom first delay");

					// Set looping timer: 1.0s interval, but first execution after 2.0s
					FirstDelayHandle = System::SetTimer(this, n"FirstDelayCallback", 1.0f, true, 2.0f);

					bFirstDelayTimerActive = System::IsTimerActiveHandle(FirstDelayHandle);
					FirstDelayRemaining = System::GetTimerRemainingHandle(FirstDelayHandle);

					Print("First delay timer set, remaining: " + FirstDelayRemaining + " seconds");
					Print("Timer active: " + bFirstDelayTimerActive);
				}
			}
			)AS"),
			TEXT("ACoverageTimerFirstDelayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer first delay actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer first delay actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFirstDelayTimerActive"), true,
			TEXT("timer with first delay should be active"))));
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
					Print("TimerImmediateExecution: Testing short-delay timer setup");

					// Set timer with a positive delay so this test only verifies setup state.
					ImmediateHandle = System::SetTimer(this, n"ImmediateCallback", 0.1f, false);

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
			TEXT("short-delay timer should be set up successfully"))));
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

	TEST_METHOD(TimerReplaceHandle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_ReplaceHandle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerReplaceHandle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerReplaceHandleActor : AActor
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
				bool bHandleReplacedSuccessfully = false;

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
					Print("TimerReplaceHandle: Testing handle replacement behavior");

					// Set first timer
					SharedHandle = System::SetTimer(this, n"FirstCallback", 2.0f, false);
					FirstRemaining = System::GetTimerRemainingHandle(SharedHandle);
					Print("First timer set, remaining: " + FirstRemaining);

					// Replace with second timer using same handle variable
					SharedHandle = System::SetTimer(this, n"SecondCallback", 1.0f, false);
					SecondRemaining = System::GetTimerRemainingHandle(SharedHandle);
					Print("Second timer set (replaced), remaining: " + SecondRemaining);

					bHandleReplacedSuccessfully = (SecondRemaining > 0.0f && SecondRemaining <= 1.0f);
				}
			}
			)AS"),
			TEXT("ACoverageTimerReplaceHandleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer replace handle actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer replace handle actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bHandleReplacedSuccessfully"), true,
			TEXT("handle should be replaceable with new timer"))));
	}

	TEST_METHOD(TimerLambdaCapture)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTimer_LambdaCapture"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTimerLambdaCapture.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageTimerLambdaCaptureActor : AActor
			{
				UPROPERTY()
				bool bLambdaTimerSetup = false;

				UPROPERTY()
				int CapturedValueSetup = 0;

				UPROPERTY()
				bool bMultipleLambdasSetup = false;

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

					bLambdaTimerSetup = true;
					CapturedValueSetup = LocalValue;

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

					bMultipleLambdasSetup = (System::IsTimerActiveHandle(LambdaHandle1) &&
					System::IsTimerActiveHandle(LambdaHandle2));

					Print("Lambda timers set up successfully");
				}
			}
			)AS"),
			TEXT("ACoverageTimerLambdaCaptureActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("timer lambda capture actor should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("timer lambda capture actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLambdaTimerSetup"), true,
			TEXT("lambda timer should be set up successfully"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CapturedValueSetup"), 42,
			TEXT("captured value should be accessible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMultipleLambdasSetup"), true,
			TEXT("multiple lambda timers should be active"))));
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
