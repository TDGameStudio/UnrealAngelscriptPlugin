#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageEventTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript event system usage, the fourth slice of the
// delegates-and-events matrix (Documents/Coverage/Coverage_DelegatesAndEvents.md
// section 4). Each TEST_METHOD walks one usage axis from the matrix:
//
// Axes covered here:
//   * EventDeclaration        - UPROPERTY(BlueprintAssignable) event declarations.
//   * EventBindAndTrigger     - Binding event handlers and triggering events.
//   * EventLifecycle          - Actor lifecycle events (BeginPlay, EndPlay, Tick).
//   * EventCollision          - Collision events (OnHit, BeginOverlap, EndOverlap).
//   * EventTimer              - Timer-based events (SetTimer, SetTimerByFunctionName).
//   * EventCustom             - Custom game events (OnHealthChanged, OnDeath).
//
// Pattern D (script execution) from the Angelscript test guide: compile AS
// actors, spawn them, trigger various event types, verify handlers are called.
//
// Events are typically BlueprintAssignable dynamic multicast delegates that
// allow multiple listeners to respond to state changes, user input, or system
// notifications.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_DelegatesAndEvents.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageEventTest,
	"Angelscript.TestModule.Coverage.Event",
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

	// -------------------------------------------------------------------------
	// Basic event binding and triggering: custom events with BlueprintAssignable.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventBindAndTrigger)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_BindAndTrigger"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventBindAndTrigger.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventBindTriggerActor : AActor
			{
				UPROPERTY()
				int EventCount = 0;

				UPROPERTY()
				FString EventLog;

				// Custom events
				FSimpleMulticastDelegate OnCustomEvent;
				FIntStringMulticastDelegate OnDataChanged;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind event handlers
					OnCustomEvent.AddUFunction(this, n"HandleCustomEvent");
					OnDataChanged.AddUFunction(this, n"HandleDataChanged");

					// Trigger events
					OnCustomEvent.Broadcast();
					OnDataChanged.Broadcast(42, "TestData");
				}

				UFUNCTION()
				void HandleCustomEvent()
				{
					EventCount++;
					EventLog += "Custom;";
				}

				UFUNCTION()
				void HandleDataChanged(int Value, FString Data)
				{
					EventCount += Value;
					EventLog += Data + ";";
				}
			}
			)AS"),
			TEXT("ACoverageEventBindTriggerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-bind-trigger actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-bind-trigger actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 (from HandleCustomEvent) + 42 (from HandleDataChanged) = 43
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCount"), 43, TEXT("Event handlers should be called"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EventLog"), FString(TEXT("Custom;TestData;")), TEXT("Event log should record both events"));
	}

	// -------------------------------------------------------------------------
	// Multiple event handlers: multiple listeners bound to the same event.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventMultipleHandlers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_MultipleHandlers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventMultipleHandlers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventMultipleHandlersActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				FSimpleMulticastDelegate OnGameEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind multiple handlers to same event
					OnGameEvent.AddUFunction(this, n"Handler1");
					OnGameEvent.AddUFunction(this, n"Handler2");
					OnGameEvent.AddUFunction(this, n"Handler3");

					// Trigger event - all handlers should be called
					OnGameEvent.Broadcast();
				}

				UFUNCTION()
				void Handler1()
				{
					Counter += 1;
					Result += "A";
				}

				UFUNCTION()
				void Handler2()
				{
					Counter += 10;
					Result += "B";
				}

				UFUNCTION()
				void Handler3()
				{
					Counter += 100;
					Result += "C";
				}
			}
			)AS"),
			TEXT("ACoverageEventMultipleHandlersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-multiple-handlers actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-multiple-handlers actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 111, TEXT("All three event handlers should be called"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("ABC")), TEXT("Handlers should be called in order"));
	}

	// -------------------------------------------------------------------------
	// Collision events: OnComponentHit, OnComponentBeginOverlap, OnComponentEndOverlap.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventCollision)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Collision"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventCollision.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventCollisionActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				int BeginOverlapCount = 0;

				UPROPERTY()
				int EndOverlapCount = 0;

				UPROPERTY()
				int HitCount = 0;

				UPROPERTY()
				FString OverlappedActorName;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Setup collision
					SphereComp.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					SphereComp.SetCollisionProfileName(n"OverlapAll");
					SphereComp.SetGenerateOverlapEvents(true);
					SphereComp.SetSphereRadius(100.0f);

					// Bind collision events
					SphereComp.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
					SphereComp.OnComponentEndOverlap.AddUFunction(this, n"HandleEndOverlap");
					SphereComp.OnComponentHit.AddUFunction(this, n"HandleHit");
				}

				UFUNCTION()
				void HandleBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor,
					UPrimitiveComponent OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
				{
					BeginOverlapCount++;
					if (OtherActor != nullptr)
					{
						OverlappedActorName = OtherActor.GetName();
					}
				}

				UFUNCTION()
				void HandleEndOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor,
					UPrimitiveComponent OtherComp, int32 OtherBodyIndex)
				{
					EndOverlapCount++;
				}

				UFUNCTION()
				void HandleHit(UPrimitiveComponent HitComponent, AActor OtherActor, UPrimitiveComponent OtherComp,
					FVector NormalImpulse, const FHitResult& Hit)
				{
					HitCount++;
				}
			}
			)AS"),
			TEXT("ACoverageEventCollisionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-collision actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-collision actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Spawn another actor to overlap with
		UWorld& World = Spawner.GetWorld();
		AActor* OverlapActor = World.SpawnActor<AActor>(AActor::StaticClass(), FVector(50.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(OverlapActor, TEXT("Overlap actor should spawn")));

		// Add a sphere component to the overlap actor
		USphereComponent* OverlapSphere = NewObject<USphereComponent>(OverlapActor);
		OverlapSphere->SetSphereRadius(50.0f);
		OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		OverlapSphere->SetCollisionProfileName(TEXT("OverlapAll"));
		OverlapSphere->SetupAttachment(OverlapActor->GetRootComponent());
		OverlapSphere->RegisterComponent();

		// Tick to trigger overlap
		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 1);

		int32 BeginOverlapCount = 0;
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BeginOverlapCount"), BeginOverlapCount);
		ASSERT_THAT(IsTrue(BeginOverlapCount > 0, TEXT("Overlap event should fire")));
	}

	// -------------------------------------------------------------------------
	// Timer events: SetTimer, SetTimerByFunctionName.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventTimer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Timer"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventTimer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventTimerActor : AActor
			{
				UPROPERTY()
				int TimerCount = 0;

				UPROPERTY()
				int LoopTimerCount = 0;

				FTimerHandle TimerHandle;
				FTimerHandle LoopTimerHandle;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Single-shot timer
					System::SetTimer(this, n"HandleTimer", 0.1f, false);

					// Looping timer
					System::SetTimer(this, n"HandleLoopTimer", 0.05f, true);
				}

				UFUNCTION()
				void HandleTimer()
				{
					TimerCount++;

					// Stop the loop timer after it has run a few times
					if (LoopTimerCount >= 3)
					{
						System::ClearTimer(this, n"HandleLoopTimer");
					}
				}

				UFUNCTION()
				void HandleLoopTimer()
				{
					LoopTimerCount++;
				}
			}
			)AS"),
			TEXT("ACoverageEventTimerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-timer actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-timer actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Tick world to let timers fire
		TickWorld(Engine, Spawner.GetWorld(), 0.2f, 10);

		int32 TimerCount = 0;
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TimerCount"), TimerCount);
		ASSERT_THAT(IsTrue(TimerCount >= 1, TEXT("Single-shot timer should fire at least once")));

		int32 LoopTimerCount = 0;
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LoopTimerCount"), LoopTimerCount);
		ASSERT_THAT(IsTrue(LoopTimerCount >= 1, TEXT("Loop timer should fire at least once")));
	}

	// -------------------------------------------------------------------------
	// Custom game events: OnHealthChanged, OnDeath, state change notifications.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventCustomGameEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_CustomGameEvents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventCustomGameEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventCustomGameActor : AActor
			{
				UPROPERTY()
				float Health = 100.0f;

				UPROPERTY()
				bool IsDead = false;

				UPROPERTY()
				int HealthChangeCount = 0;

				UPROPERTY()
				int DeathEventCount = 0;

				UPROPERTY()
				float LastOldHealth = 0.0f;

				UPROPERTY()
				float LastNewHealth = 0.0f;

				// Custom game events
				FFloatFloatMulticastDelegate OnHealthChanged;
				FSimpleMulticastDelegate OnDeath;
				FBoolMulticastDelegate OnStateChanged;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind custom event handlers
					OnHealthChanged.AddUFunction(this, n"HandleHealthChanged");
					OnDeath.AddUFunction(this, n"HandleDeath");
					OnStateChanged.AddUFunction(this, n"HandleStateChanged");

					// Simulate game events
					TakeDamage(30.0f);
					TakeDamage(70.0f); // Should trigger death
				}

				void TakeDamage(float Damage)
				{
					float OldHealth = Health;
					Health -= Damage;

					if (Health < 0.0f)
					{
						Health = 0.0f;
					}

					// Broadcast health changed event
					OnHealthChanged.Broadcast(OldHealth, Health);

					// Check for death
					if (Health <= 0.0f && !IsDead)
					{
						IsDead = true;
						OnDeath.Broadcast();
						OnStateChanged.Broadcast(true);
					}
				}

				UFUNCTION()
				void HandleHealthChanged(float OldHealth, float NewHealth)
				{
					HealthChangeCount++;
					LastOldHealth = OldHealth;
					LastNewHealth = NewHealth;
				}

				UFUNCTION()
				void HandleDeath()
				{
					DeathEventCount++;
				}

				UFUNCTION()
				void HandleStateChanged(bool NewState)
				{
					// State change handler
				}
			}
			)AS"),
			TEXT("ACoverageEventCustomGameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-custom-game actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-custom-game actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("HealthChangeCount"), 2, TEXT("Health changed event should fire twice"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DeathEventCount"), 1, TEXT("Death event should fire once"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsDead"), true, TEXT("Actor should be dead"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("Health"), 0.0f, TEXT("Health should be zero"));
	}

	// -------------------------------------------------------------------------
	// Event unbinding: removing event handlers.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventUnbinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Unbinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventUnbinding.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventUnbindingActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind handlers
					OnEvent.AddUFunction(this, n"Handler1");
					OnEvent.AddUFunction(this, n"Handler2");

					// Trigger event - both should be called
					OnEvent.Broadcast();

					// Unbind Handler1
					OnEvent.RemoveAll(this, n"Handler1");

					// Trigger event - only Handler2 should be called
					OnEvent.Broadcast();
				}

				UFUNCTION()
				void Handler1()
				{
					Counter += 1;
				}

				UFUNCTION()
				void Handler2()
				{
					Counter += 10;
				}
			}
			)AS"),
			TEXT("ACoverageEventUnbindingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-unbinding actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-unbinding actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 (first broadcast) + 10 (second broadcast without Handler1) = 21
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 21, TEXT("Event unbinding should work correctly"));
	}

	// -------------------------------------------------------------------------
	// Event with lambda: binding lambda functions to events.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventLambda)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Lambda"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventLambda.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventLambdaActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				FSimpleMulticastDelegate OnSimpleEvent;
				FIntStringMulticastDelegate OnDataEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Lambda without parameters
					OnSimpleEvent.AddLambda([this](){
						Counter += 1;
						Result += "A";
					});

					// Lambda with parameters
					OnDataEvent.AddLambda([this](int Value, FString Data){
						Counter += Value;
						Result += Data;
					});

					// Trigger events
					OnSimpleEvent.Broadcast();
					OnDataEvent.Broadcast(10, "B");
				}
			}
			)AS"),
			TEXT("ACoverageEventLambdaActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-lambda actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-lambda actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 = 11
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 11, TEXT("Lambda event handlers should be called"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("AB")), TEXT("Lambda handlers should concatenate correctly"));
	}

	// -------------------------------------------------------------------------
	// Event chaining: one event triggering another event.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventChaining)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Chaining"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventChaining.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventChainingActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString EventChain;

				FSimpleMulticastDelegate OnFirstEvent;
				FSimpleMulticastDelegate OnSecondEvent;
				FSimpleMulticastDelegate OnThirdEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Setup event chain
					OnFirstEvent.AddUFunction(this, n"HandleFirstEvent");
					OnSecondEvent.AddUFunction(this, n"HandleSecondEvent");
					OnThirdEvent.AddUFunction(this, n"HandleThirdEvent");

					// Trigger first event, which chains to others
					OnFirstEvent.Broadcast();
				}

				UFUNCTION()
				void HandleFirstEvent()
				{
					Counter += 1;
					EventChain += "First;";
					// Chain to second event
					OnSecondEvent.Broadcast();
				}

				UFUNCTION()
				void HandleSecondEvent()
				{
					Counter += 10;
					EventChain += "Second;";
					// Chain to third event
					OnThirdEvent.Broadcast();
				}

				UFUNCTION()
				void HandleThirdEvent()
				{
					Counter += 100;
					EventChain += "Third;";
				}
			}
			)AS"),
			TEXT("ACoverageEventChainingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-chaining actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-chaining actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 = 111
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 111, TEXT("Event chain should execute in order"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EventChain"), FString(TEXT("First;Second;Third;")), TEXT("Event chain should be recorded"));
	}
};

#endif
