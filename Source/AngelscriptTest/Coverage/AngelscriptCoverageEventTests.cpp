#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/Slider.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageEventTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript event system usage, the fourth slice of the
// delegates-and-events matrix (OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// section 4). Each TEST_METHOD walks one usage axis from the matrix:
//
// Axes covered here:
//   * EventDeclaration        - UPROPERTY(BlueprintAssignable) event declarations.
//   * EventBindAndTrigger     - Binding event handlers and triggering events.
//   * EventLifecycle          - Actor lifecycle events (BeginPlay, EndPlay, Tick).
//   * EventCollision          - Collision events (OnHit, BeginOverlap, EndOverlap).
//   * EventTimer              - Timer function-name callbacks and handle-state events.
//   * EventCustom             - Custom game events (OnHealthChanged, OnDeath).
//
// Pattern D (script execution) from the Angelscript test guide: compile AS
// actors, spawn them, trigger various event types, verify handlers are called.
//
// Events are typically BlueprintAssignable dynamic multicast delegates that
// allow multiple listeners to respond to state changes, user input, or system
// notifications.
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
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
			event void FCoverageCustomEvent();
			event void FCoverageDataChangedEvent(int Value, FString Data);

			UCLASS()
			class ACoverageEventBindTriggerActor : AActor
			{
				UPROPERTY()
				int EventCount = 0;

				UPROPERTY()
				FString EventLog;

				// Custom events
				FCoverageCustomEvent OnCustomEvent;
				FCoverageDataChangedEvent OnDataChanged;

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-bind-trigger actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 (from HandleCustomEvent) + 42 (from HandleDataChanged) = 43
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCount"), 43, TEXT("Event handlers should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EventLog"), FString(TEXT("Custom;TestData;")), TEXT("Event log should record both events"))));
	}

	// -------------------------------------------------------------------------
	// Event property metadata: UPROPERTY event, BlueprintAssignable, and
	// BlueprintCallable flags on AS event members.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventDeclarationMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Metadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventMetadata.as"),
			ASTEST_AS(R"AS(
			event void FCoverageEventPlain();
			event void FCoverageEventAssignable();
			event void FCoverageEventCallable(int Value);

			UCLASS()
			class ACoverageEventMetadataActor : AActor
			{
				UPROPERTY()
				FCoverageEventPlain OnPlainEvent;

				UPROPERTY()
				FCoverageEventAssignable OnAssignableEvent;

				UPROPERTY()
				FCoverageEventCallable OnCallableEvent;
			}
			)AS"),
			TEXT("ACoverageEventMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event metadata actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FMulticastDelegateProperty* PlainProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnPlainEvent"));
		const FMulticastDelegateProperty* AssignableProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnAssignableEvent"));
		const FMulticastDelegateProperty* CallableProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnCallableEvent"));
		ASSERT_THAT(IsNotNull(PlainProperty, TEXT("Plain UPROPERTY event should generate a multicast delegate property")));
		ASSERT_THAT(IsNotNull(AssignableProperty, TEXT("BlueprintAssignable event should generate a multicast delegate property")));
		ASSERT_THAT(IsNotNull(CallableProperty, TEXT("BlueprintCallable event should generate a multicast delegate property")));
		if (PlainProperty == nullptr || AssignableProperty == nullptr || CallableProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(PlainProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("Plain AS event UPROPERTY should be Blueprint visible")));
		ASSERT_THAT(IsTrue(PlainProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("Plain AS event should carry default CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(PlainProperty->HasAnyPropertyFlags(CPF_BlueprintCallable), TEXT("Plain AS event should carry default CPF_BlueprintCallable")));
		ASSERT_THAT(IsTrue(AssignableProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("Second plain AS event should carry default CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(CallableProperty->HasAnyPropertyFlags(CPF_BlueprintCallable), TEXT("Parameterized plain AS event should carry default CPF_BlueprintCallable")));
		ASSERT_THAT(IsNotNull(CallableProperty->SignatureFunction, TEXT("BlueprintCallable event signature function should be generated")));
		if (CallableProperty->SignatureFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(CallableProperty->SignatureFunction, TEXT("Value")), TEXT("BlueprintCallable event signature should expose its parameter")));
	}

	// -------------------------------------------------------------------------
	// BlueprintEvent UFUNCTION metadata and wrapper execution: AS event
	// functions surface as BlueprintEvent UFunctions and route calls through the
	// generated implementation wrapper.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventBlueprintEventMetadataAndExecution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_BlueprintEventMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventBlueprintEventMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageBlueprintEventObject : UObject
			{
				UPROPERTY()
				int LastInput = 0;

				UPROPERTY()
				FString LastLabel;

				UFUNCTION(BlueprintEvent)
				int ComputeEvent(int Value, const FString& Label)
				{
					LastInput = Value;
					LastLabel = Label;
					return Value + 5;
				}

				UFUNCTION(BlueprintCallable, BlueprintEvent)
				int CallableEvent(int Value)
				{
					return Value * 3;
				}

				UFUNCTION()
				int RunEvents()
				{
					return ComputeEvent(37, "Event") + CallableEvent(2);
				}
			}
			)AS"),
			TEXT("UCoverageBlueprintEventObject"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintEvent coverage object class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ComputeEventFunction = ScriptClass->FindFunctionByName(TEXT("ComputeEvent"));
		UFunction* CallableEventFunction = ScriptClass->FindFunctionByName(TEXT("CallableEvent"));
		UFunction* RunEventsFunction = ScriptClass->FindFunctionByName(TEXT("RunEvents"));
		ASSERT_THAT(IsNotNull(ComputeEventFunction, TEXT("BlueprintEvent wrapper UFUNCTION should exist")));
		ASSERT_THAT(IsNotNull(CallableEventFunction, TEXT("BlueprintCallable BlueprintEvent wrapper UFUNCTION should exist")));
		ASSERT_THAT(IsNotNull(RunEventsFunction, TEXT("BlueprintEvent entry UFUNCTION should exist")));
		if (ComputeEventFunction == nullptr || CallableEventFunction == nullptr || RunEventsFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ComputeEventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent), TEXT("ComputeEvent should carry FUNC_BlueprintEvent")));
		ASSERT_THAT(IsFalse(ComputeEventFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable), TEXT("Plain BlueprintEvent should not imply FUNC_BlueprintCallable")));
		ASSERT_THAT(IsTrue(CallableEventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent), TEXT("CallableEvent should carry FUNC_BlueprintEvent")));
		ASSERT_THAT(IsTrue(CallableEventFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable), TEXT("Explicit BlueprintCallable BlueprintEvent should carry FUNC_BlueprintCallable")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ComputeEventFunction, TEXT("Value")), TEXT("BlueprintEvent should expose int input parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(ComputeEventFunction, TEXT("Label")), TEXT("BlueprintEvent should expose FString input parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ComputeEventFunction, TEXT("ReturnValue")), TEXT("BlueprintEvent should expose return parameter")));

		UObject* ScriptObject = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptObject, TEXT("BlueprintEvent coverage object should be creatable")));
		if (ScriptObject == nullptr)
		{
			return;
		}

		FFunctionInvoker RunEventsInvoker(*TestRunner, ScriptObject, TEXT("RunEvents"));
		ASSERT_THAT(IsTrue(RunEventsInvoker.IsValid(), TEXT("RunEvents should be invokable")));
		if (!RunEventsInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(48, RunEventsInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("BlueprintEvent wrapper should execute the AS implementation path")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ScriptObject, TEXT("LastInput"), 37, TEXT("BlueprintEvent implementation should receive the int parameter"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, ScriptObject, TEXT("LastLabel"), FString(TEXT("Event")), TEXT("BlueprintEvent implementation should receive the FString parameter"))));
	}

	// -------------------------------------------------------------------------
	// Actor lifecycle events: BeginPlay, Tick, EndPlay are AS-facing override
	// events and can themselves drive custom event broadcasts.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_Lifecycle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventLifecycle.as"),
			ASTEST_AS(R"AS(
			event void FCoverageLifecycleEvent();

			UCLASS()
			class ACoverageEventLifecycleActor : AActor
			{
				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				int EndPlayCount = 0;

				UPROPERTY()
				int LifecycleEventCount = 0;

				UPROPERTY()
				FCoverageLifecycleEvent OnLifecycle;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnLifecycle.AddUFunction(this, n"HandleLifecycle");
					BeginPlayCount += 1;
					OnLifecycle.Broadcast();
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					TickCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason Reason)
				{
					EndPlayCount += 1;
					OnLifecycle.Broadcast();
				}

				UFUNCTION()
				void HandleLifecycle()
				{
					LifecycleEventCount += 1;
				}
			}
			)AS"),
			TEXT("ACoverageEventLifecycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-lifecycle actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-lifecycle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		TickWorld(Engine, Spawner.GetWorld(), 0.016f, 2);
		Actor->Destroy();
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BeginPlayCount"), 1, TEXT("BeginPlay lifecycle event should run once"))));
		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TickCount"), TickCount), TEXT("TickCount should be readable")));
		ASSERT_THAT(IsTrue(TickCount >= 1, TEXT("Tick lifecycle event should run during world ticks")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EndPlayCount"), 1, TEXT("EndPlay lifecycle event should run on Destroy"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LifecycleEventCount"), 2, TEXT("Lifecycle custom event should broadcast from BeginPlay and EndPlay"))));
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
			event void FCoverageGameEvent();

			UCLASS()
			class ACoverageEventMultipleHandlersActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				FCoverageGameEvent OnGameEvent;

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-multiple-handlers actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 111, TEXT("All three event handlers should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("ABC")), TEXT("Handlers should be called in order"))));
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
					UPrimitiveComponent OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult&in SweepResult)
				{
					BeginOverlapCount++;
					if (OtherActor != nullptr)
					{
						OverlappedActorName = OtherActor.GetName().ToString();
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
					FVector NormalImpulse, const FHitResult&in Hit)
				{
					HitCount++;
				}
			}
			)AS"),
			TEXT("ACoverageEventCollisionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-collision actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-collision actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Spawn another actor to overlap with
		UWorld& World = Spawner.GetWorld();
		AActor* OverlapActor = World.SpawnActor<AActor>(AActor::StaticClass(), FVector(50.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(OverlapActor, TEXT("Overlap actor should spawn")));
		if (OverlapActor == nullptr)
		{
			return;
		}

		// Add a sphere component to the overlap actor
		USphereComponent* OverlapSphere = NewObject<USphereComponent>(OverlapActor);
		ASSERT_THAT(IsNotNull(OverlapSphere, TEXT("Overlap sphere component should be created")));
		if (OverlapSphere == nullptr)
		{
			return;
		}
		OverlapSphere->SetSphereRadius(50.0f);
		OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		OverlapSphere->SetCollisionProfileName(TEXT("OverlapAll"));
		OverlapSphere->SetupAttachment(OverlapActor->GetRootComponent());
		OverlapSphere->RegisterComponent();

		FHitResult SweepResult;
		USphereComponent* SphereComp = Cast<USphereComponent>(Actor->GetRootComponent());
		ASSERT_THAT(IsNotNull(SphereComp, TEXT("Event-collision actor should have a sphere root component")));
		if (SphereComp == nullptr)
		{
			return;
		}
		SphereComp->OnComponentBeginOverlap.Broadcast(SphereComp, OverlapActor, OverlapSphere, 0, false, SweepResult);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BeginOverlapCount"), 1, TEXT("Overlap event should fire"))));
	}

	// -------------------------------------------------------------------------
	// Actor and component built-in event instances: sparse delegates exposed by
	// UE can be bound from AS through AddUFunction/Unbind.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventBuiltInActorAndComponentInstances)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_BuiltInActorComponent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventBuiltInActorComponent.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventBuiltInActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				int ActorBeginOverlapCount = 0;

				UPROPERTY()
				int ActorHitCount = 0;

				UPROPERTY()
				int ActorClickCount = 0;

				UPROPERTY()
				int ActorReleaseCount = 0;

				UPROPERTY()
				int ComponentHitCount = 0;

				UPROPERTY()
				int ComponentBeginOverlapCount = 0;

				UPROPERTY()
				int ComponentClickCount = 0;

				UPROPERTY()
				int ComponentReleaseCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnActorBeginOverlap.AddUFunction(this, n"HandleActorBeginOverlap");
					OnActorHit.AddUFunction(this, n"HandleActorHit");
					OnClicked.AddUFunction(this, n"HandleActorClicked");
					OnReleased.AddUFunction(this, n"HandleActorReleased");

					SphereComp.OnComponentHit.AddUFunction(this, n"HandleComponentHit");
					SphereComp.OnComponentBeginOverlap.AddUFunction(this, n"HandleComponentBeginOverlap");
					SphereComp.OnClicked.AddUFunction(this, n"HandleComponentClicked");
					SphereComp.OnReleased.AddUFunction(this, n"HandleComponentReleased");
				}

				UFUNCTION()
				void HandleActorBeginOverlap(AActor OverlappedActor, AActor OtherActor)
				{
					ActorBeginOverlapCount += 1;
				}

				UFUNCTION()
				void HandleActorHit(AActor SelfActor, AActor OtherActor, FVector NormalImpulse, const FHitResult&in Hit)
				{
					ActorHitCount += 1;
				}

				UFUNCTION()
				void HandleActorClicked(AActor TouchedActor, FKey ButtonPressed)
				{
					ActorClickCount += 1;
				}

				UFUNCTION()
				void HandleActorReleased(AActor TouchedActor, FKey ButtonReleased)
				{
					ActorReleaseCount += 1;
				}

				UFUNCTION()
				void HandleComponentHit(UPrimitiveComponent HitComponent, AActor OtherActor, UPrimitiveComponent OtherComp, FVector NormalImpulse, const FHitResult&in Hit)
				{
					ComponentHitCount += 1;
				}

				UFUNCTION()
				void HandleComponentBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor, UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in SweepResult)
				{
					ComponentBeginOverlapCount += 1;
				}

				UFUNCTION()
				void HandleComponentClicked(UPrimitiveComponent TouchedComponent, FKey ButtonPressed)
				{
					ComponentClickCount += 1;
				}

				UFUNCTION()
				void HandleComponentReleased(UPrimitiveComponent TouchedComponent, FKey ButtonReleased)
				{
					ComponentReleaseCount += 1;
				}
			}
			)AS"),
			TEXT("ACoverageEventBuiltInActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Built-in event actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Built-in event actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		USphereComponent* SphereComp = Cast<USphereComponent>(Actor->GetRootComponent());
		ASSERT_THAT(IsNotNull(SphereComp, TEXT("Built-in event actor should have a sphere root component")));
		if (SphereComp == nullptr)
		{
			return;
		}

		AActor* OtherActor = Spawner.GetWorld().SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(OtherActor, TEXT("Built-in event test should spawn an other actor")));
		if (OtherActor == nullptr)
		{
			return;
		}
		USphereComponent* OtherComponent = NewObject<USphereComponent>(OtherActor);
		ASSERT_THAT(IsNotNull(OtherComponent, TEXT("Built-in event test should create an other component")));
		if (OtherComponent == nullptr)
		{
			return;
		}
		OtherComponent->RegisterComponent();

		FHitResult Hit;
		Actor->OnActorBeginOverlap.Broadcast(Actor, OtherActor);
		Actor->OnActorHit.Broadcast(Actor, OtherActor, FVector(1.0f, 0.0f, 0.0f), Hit);
		Actor->OnClicked.Broadcast(Actor, FKey());
		Actor->OnReleased.Broadcast(Actor, FKey());
		SphereComp->OnComponentHit.Broadcast(SphereComp, OtherActor, OtherComponent, FVector(0.0f, 1.0f, 0.0f), Hit);
		SphereComp->OnComponentBeginOverlap.Broadcast(SphereComp, OtherActor, OtherComponent, 0, false, Hit);
		SphereComp->OnClicked.Broadcast(SphereComp, FKey());
		SphereComp->OnReleased.Broadcast(SphereComp, FKey());

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorBeginOverlapCount"), 1, TEXT("OnActorBeginOverlap should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorHitCount"), 1, TEXT("OnActorHit should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorClickCount"), 1, TEXT("AActor OnClicked should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorReleaseCount"), 1, TEXT("AActor OnReleased should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentHitCount"), 1, TEXT("OnComponentHit should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentBeginOverlapCount"), 1, TEXT("OnComponentBeginOverlap should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentClickCount"), 1, TEXT("UPrimitiveComponent OnClicked should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentReleaseCount"), 1, TEXT("UPrimitiveComponent OnReleased should invoke the AS handler"))));
	}

	// -------------------------------------------------------------------------
	// UMG event instances: UButton/USlider/UEditableText delegates are
	// BlueprintAssignable dynamic multicast events that AS can bind to.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventWidgetEventInstances)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_WidgetEvents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventWidgetEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageEventWidgetHarness : UObject
			{
				UPROPERTY()
				int ClickCount = 0;

				UPROPERTY()
				int PressCount = 0;

				UPROPERTY()
				int ReleaseCount = 0;

				UPROPERTY()
				float SliderValue = 0.0f;

				UPROPERTY()
				FString LastText;

				UFUNCTION()
				void Bind(UButton Button, USlider Slider, UEditableText Text)
				{
					Button.OnClicked.AddUFunction(this, n"HandleClicked");
					Button.OnPressed.AddUFunction(this, n"HandlePressed");
					Button.OnReleased.AddUFunction(this, n"HandleReleased");
					Slider.OnValueChanged.AddUFunction(this, n"HandleSliderChanged");
					Text.OnTextChanged.AddUFunction(this, n"HandleTextChanged");
				}

				UFUNCTION()
				void HandleClicked()
				{
					ClickCount += 1;
				}

				UFUNCTION()
				void HandlePressed()
				{
					PressCount += 1;
				}

				UFUNCTION()
				void HandleReleased()
				{
					ReleaseCount += 1;
				}

				UFUNCTION()
				void HandleSliderChanged(float32 Value)
				{
					SliderValue = Value;
				}

				UFUNCTION()
				void HandleTextChanged(const FText&in Value)
				{
					LastText = Value.ToString();
				}
			}
			)AS"),
			TEXT("UCoverageEventWidgetHarness"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Widget event harness class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		UButton* Button = NewObject<UButton>(GetTransientPackage());
		USlider* Slider = NewObject<USlider>(GetTransientPackage());
		UEditableText* EditableText = NewObject<UEditableText>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(Harness, TEXT("Widget event harness should be created")));
		ASSERT_THAT(IsNotNull(Button, TEXT("Button fixture should be created")));
		ASSERT_THAT(IsNotNull(Slider, TEXT("Slider fixture should be created")));
		ASSERT_THAT(IsNotNull(EditableText, TEXT("EditableText fixture should be created")));
		if (Harness == nullptr || Button == nullptr || Slider == nullptr || EditableText == nullptr)
		{
			return;
		}

		FFunctionInvoker BindInvoker(*TestRunner, Harness, TEXT("Bind"));
		ASSERT_THAT(IsTrue(BindInvoker.IsValid(), TEXT("Widget event bind function should resolve")));
		if (!BindInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BindInvoker.AddParam<UButton*>(Button).AddParam<USlider*>(Slider).AddParam<UEditableText*>(EditableText).Call(), TEXT("Widget event bind function should execute")));

		Button->OnClicked.Broadcast();
		Button->OnPressed.Broadcast();
		Button->OnReleased.Broadcast();
		Slider->OnValueChanged.Broadcast(0.75f);
		EditableText->OnTextChanged.Broadcast(FText::FromString(TEXT("Changed")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("ClickCount"), 1, TEXT("UButton OnClicked should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("PressCount"), 1, TEXT("UButton OnPressed should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("ReleaseCount"), 1, TEXT("UButton OnReleased should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Harness, TEXT("SliderValue"), 0.75, TEXT("USlider OnValueChanged should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Harness, TEXT("LastText"), FString(TEXT("Changed")), TEXT("UEditableText OnTextChanged should invoke the AS handler"))));
	}

	// -------------------------------------------------------------------------
	// Timer events: function-name SetTimer setup and handle state.
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

				UPROPERTY()
				bool bSingleShotNotPausedAfterSet = false;

				UPROPERTY()
				bool bLoopNotPausedAfterSet = false;

				UPROPERTY()
				bool bLoopPausedAfterPause = false;

				UPROPERTY()
				bool bLoopNotPausedAfterUnPause = false;

				UPROPERTY()
				bool bLoopNotPausedAfterClear = false;

				UPROPERTY()
				bool bSingleShotNotPausedAfterClear = false;

				FTimerHandle TimerHandle;
				FTimerHandle LoopTimerHandle;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TimerHandle = System::SetTimer(this, n"HandleTimer", 30.0f, false);
					bSingleShotNotPausedAfterSet = !System::IsTimerPausedHandle(TimerHandle);

					LoopTimerHandle = System::SetTimer(this, n"HandleLoopTimer", 30.0f, true);
					bLoopNotPausedAfterSet = !System::IsTimerPausedHandle(LoopTimerHandle);

					System::PauseTimerHandle(LoopTimerHandle);
					bLoopPausedAfterPause = System::IsTimerPausedHandle(LoopTimerHandle);

					System::UnPauseTimerHandle(LoopTimerHandle);
					bLoopNotPausedAfterUnPause = !System::IsTimerPausedHandle(LoopTimerHandle);

					System::ClearAndInvalidateTimerHandle(LoopTimerHandle);
					bLoopNotPausedAfterClear = !System::IsTimerPausedHandle(LoopTimerHandle);
				}

				UFUNCTION()
				void HandleTimer()
				{
					TimerCount++;
					if (LoopTimerCount >= 3)
					{
						System::ClearAndInvalidateTimerHandle(LoopTimerHandle);
					}
				}

				UFUNCTION()
				void HandleLoopTimer()
				{
					LoopTimerCount++;
				}

				UFUNCTION()
				void ClearSingleShotTimer()
				{
					System::ClearAndInvalidateTimerHandle(TimerHandle);
					bSingleShotNotPausedAfterClear = !System::IsTimerPausedHandle(TimerHandle);
				}
			}
			)AS"),
			TEXT("ACoverageEventTimerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event-timer actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-timer actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSingleShotNotPausedAfterSet"), true,
			TEXT("single-shot function-name timer should be observable through IsTimerPausedHandle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopNotPausedAfterSet"), true,
			TEXT("looping function-name timer should be observable through IsTimerPausedHandle"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopPausedAfterPause"), true,
			TEXT("PauseTimerHandle should mark the looping handle paused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopNotPausedAfterUnPause"), true,
			TEXT("UnPauseTimerHandle should mark the looping handle unpaused"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoopNotPausedAfterClear"), true,
			TEXT("ClearAndInvalidateTimerHandle should leave the cleared looping handle unpaused"))));

		{
			FFunctionInvoker ClearInvoker(*TestRunner, Actor, TEXT("ClearSingleShotTimer"));
			ASSERT_THAT(IsTrue(ClearInvoker.IsValid(), TEXT("ClearSingleShotTimer should be invokable")));
			if (!ClearInvoker.IsValid())
			{
				return;
			}
			FAngelscriptEngineScope ClearScope(Engine, Actor);
			ASSERT_THAT(IsTrue(ClearInvoker.Call(), TEXT("ClearSingleShotTimer should clear the single-shot timer")));
		}
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSingleShotNotPausedAfterClear"), true,
			TEXT("ClearAndInvalidateTimerHandle should leave the cleared single-shot handle unpaused"))));
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
			event void FCoverageHealthChangedEvent(float OldHealth, float NewHealth);
			event void FCoverageDeathEvent();
			event void FCoverageStateChangedEvent(bool NewState);

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
				FCoverageHealthChangedEvent OnHealthChanged;
				FCoverageDeathEvent OnDeath;
				FCoverageStateChangedEvent OnStateChanged;

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-custom-game actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("HealthChangeCount"), 2, TEXT("Health changed event should fire twice"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DeathEventCount"), 1, TEXT("Death event should fire once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsDead"), true, TEXT("Actor should be dead"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("Health"), 0.0, TEXT("Health should be zero"))));
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
			event void FCoverageUnbindEvent();

			UCLASS()
			class ACoverageEventUnbindingActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FCoverageUnbindEvent OnEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind handlers
					OnEvent.AddUFunction(this, n"Handler1");
					OnEvent.AddUFunction(this, n"Handler2");

					// Trigger event - both should be called
					OnEvent.Broadcast();

					// Unbind Handler1
					OnEvent.Unbind(this, n"Handler1");

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-unbinding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 (first broadcast) + 10 (second broadcast without Handler1) = 21
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 21, TEXT("Event unbinding should work correctly"))));
	}

	// -------------------------------------------------------------------------
	// Event lambda boundary: AS exposes AddUFunction/Unbind for events, not C++
	// AddLambda binding.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventLambdaSyntaxIsUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			event void FCoverageEventLambdaSignal();

			UCLASS()
			class ACoverageEventLambdaUnsupportedActor : AActor
			{
				UPROPERTY()
				FCoverageEventLambdaSignal OnSignal;

				UFUNCTION()
				void Handler()
				{
				}

				void TryLambda()
				{
					OnSignal.AddLambda(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'FCoverageEventLambdaSignal::AddLambda"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageEvent_LambdaUnsupported"),
			*ScriptSource,
			TEXT("Event AddLambda should remain an explicit unsupported AS-facing boundary"),
			MakeArrayView(ExpectedDiagnostics))));
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
			event void FCoverageChainEvent();

			UCLASS()
			class ACoverageEventChainingActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString EventChain;

				FCoverageChainEvent OnFirstEvent;
				FCoverageChainEvent OnSecondEvent;
				FCoverageChainEvent OnThirdEvent;

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event-chaining actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 = 111
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 111, TEXT("Event chain should execute in order"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EventChain"), FString(TEXT("First;Second;Third;")), TEXT("Event chain should be recorded"))));
	}

	// -------------------------------------------------------------------------
	// Non-AS-facing coverage boundaries: Blueprint graph drag/drop binding,
	// HTTP request objects, and direct lambda timer syntax are editor/C++
	// surfaces, not currently supported AS syntax.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventNonScriptFacingBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString AddDynamicSource = ASTEST_AS(R"AS(
			event void FCoverageBoundaryEvent();

			UCLASS()
			class ACoverageEventAddDynamicBoundaryActor : AActor
			{
				UPROPERTY()
				FCoverageBoundaryEvent OnBoundary;

				UFUNCTION()
				void Handler()
				{
				}

				UFUNCTION()
				void TryAddDynamic()
				{
					OnBoundary.AddDynamic(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> AddDynamicDiagnostics;
		AddDynamicDiagnostics.Add(TEXT("No matching signatures to 'FCoverageBoundaryEvent::AddDynamic"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageEvent_AddDynamicUnsupported"),
			*AddDynamicSource,
			TEXT("Blueprint AddDynamic macro name should stay outside current AS event coverage"),
			MakeArrayView(AddDynamicDiagnostics))));

		const FString TimerLambdaSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventTimerLambdaBoundaryActor : AActor
			{
				UFUNCTION()
				void TryTimerLambda()
				{
					System::SetTimer(this, [](){}, 1.0f, false);
				}
			}
			)AS");

		TArray<FString> TimerLambdaDiagnostics;
		TimerLambdaDiagnostics.Add(TEXT("Expected expression value"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageEvent_TimerLambdaUnsupported"),
			*TimerLambdaSource,
			TEXT("C++ lambda timer callback syntax should stay outside current AS event coverage"),
			MakeArrayView(TimerLambdaDiagnostics))));

		const FString HttpRequestSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventHttpBoundaryActor : AActor
			{
				UFUNCTION()
				void TryHttpRequestCallback()
				{
					HttpRequest.OnProcessRequestComplete.BindLambda(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> HttpRequestDiagnostics;
		HttpRequestDiagnostics.Add(TEXT("HttpRequest"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageEvent_HttpRequestCallbackUnsupported"),
			*HttpRequestSource,
			TEXT("HTTP BindLambda callback surface should stay outside current AS event coverage"),
			MakeArrayView(HttpRequestDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Network-style RepNotify event: ReplicatedUsing metadata should preserve
	// the notify callback, and the callback should be executable as a real
	// state-change event path.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventRepNotifyExecutesStateChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_RepNotifyExecutes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventRepNotifyExecutes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageEventRepNotifyActor : AActor
			{
				UPROPERTY(ReplicatedUsing=OnRep_TrackedHealth)
				int TrackedHealth = 0;

				UPROPERTY()
				int RepNotifyCount = 0;

				UPROPERTY()
				int LastReplicatedHealth = 0;

				UPROPERTY()
				bool bRepNotifyExecuted = false;

				UFUNCTION()
				void ApplyReplicatedHealth(int NewHealth)
				{
					TrackedHealth = NewHealth;
					OnRep_TrackedHealth();
				}

				UFUNCTION()
				void OnRep_TrackedHealth()
				{
					RepNotifyCount += 1;
					LastReplicatedHealth = TrackedHealth;
					bRepNotifyExecuted = true;
				}
			}
			)AS"),
			TEXT("ACoverageEventRepNotifyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("RepNotify event actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FIntProperty* TrackedHealthProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("TrackedHealth"));
		ASSERT_THAT(IsNotNull(TrackedHealthProperty, TEXT("RepNotify event property should be generated")));
		if (TrackedHealthProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(TrackedHealthProperty->HasAnyPropertyFlags(CPF_Net), TEXT("RepNotify event property should carry CPF_Net")));
		ASSERT_THAT(IsTrue(TrackedHealthProperty->HasAnyPropertyFlags(CPF_RepNotify), TEXT("RepNotify event property should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_TrackedHealth")), TrackedHealthProperty->RepNotifyFunc, TEXT("RepNotify event property should preserve callback name")));

		UFunction* RepNotifyFunction = ScriptClass->FindFunctionByName(TEXT("OnRep_TrackedHealth"));
		UFunction* ApplyFunction = ScriptClass->FindFunctionByName(TEXT("ApplyReplicatedHealth"));
		ASSERT_THAT(IsNotNull(RepNotifyFunction, TEXT("RepNotify callback UFUNCTION should exist")));
		ASSERT_THAT(IsNotNull(ApplyFunction, TEXT("RepNotify state-change entry UFUNCTION should exist")));
		if (RepNotifyFunction == nullptr || ApplyFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(RepNotifyFunction->HasAnyFunctionFlags(FUNC_Net), TEXT("RepNotify callback should be a local notify, not an RPC")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("RepNotify event actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ApplyInvoker(*TestRunner, Actor, TEXT("ApplyReplicatedHealth"));
		ASSERT_THAT(IsTrue(ApplyInvoker.IsValid(), TEXT("ApplyReplicatedHealth should be invokable")));
		if (!ApplyInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ApplyInvoker.AddParam<int32>(87).Call(), TEXT("RepNotify state-change entry should execute")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TrackedHealth"), 87, TEXT("Replicated value should be assigned before notify"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RepNotifyCount"), 1, TEXT("RepNotify event should execute exactly once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastReplicatedHealth"), 87, TEXT("RepNotify event should observe the replicated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRepNotifyExecuted"), true, TEXT("RepNotify event should mark its runtime path"))));
	}

	// -------------------------------------------------------------------------
	// Event bus style decoupling: separate publisher and receiver objects bind
	// through a shared event property, then targeted Unbind stops delivery.
	// -------------------------------------------------------------------------
	TEST_METHOD(EventBusDecouplesPublisherAndReceiver)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageEvent_EventBus"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageEventBus.as"),
			ASTEST_AS(R"AS(
			event void FCoverageEventBusMessage(int Value, const FString& Label);

			UCLASS()
			class UCoverageEventBusReceiver : UObject
			{
				UPROPERTY()
				int Total = 0;

				UPROPERTY()
				FString Log;

				UFUNCTION()
				void HandleMessage(int Value, const FString& Label)
				{
					Total += Value;
					Log += Label;
				}
			}

			UCLASS()
			class ACoverageEventBusActor : AActor
			{
				UPROPERTY()
				FCoverageEventBusMessage OnMessage;

				UPROPERTY()
				UCoverageEventBusReceiver Receiver;

				UPROPERTY()
				int ReceiverTotal = 0;

				UPROPERTY()
				FString ReceiverLog;

				UPROPERTY()
				bool WasBoundBeforeUnbind = false;

				UPROPERTY()
				bool WasBoundAfterUnbind = true;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Receiver = Cast<UCoverageEventBusReceiver>(NewObject(this, UCoverageEventBusReceiver::StaticClass(), n"CoverageEventBusReceiver"));
					OnMessage.AddUFunction(Receiver, n"HandleMessage");
					WasBoundBeforeUnbind = OnMessage.IsBound();

					OnMessage.Broadcast(7, "A");
					OnMessage.Unbind(Receiver, n"HandleMessage");
					WasBoundAfterUnbind = OnMessage.IsBound();
					OnMessage.Broadcast(11, "B");

					ReceiverTotal = Receiver.Total;
					ReceiverLog = Receiver.Log;
				}
			}
			)AS"),
			TEXT("ACoverageEventBusActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Event bus actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Event bus actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasBoundBeforeUnbind"), true, TEXT("Event bus should be bound before targeted Unbind"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasBoundAfterUnbind"), false, TEXT("Targeted Unbind should remove the receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceiverTotal"), 7, TEXT("Receiver should only observe the pre-unbind event"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceiverLog"), FString(TEXT("A")), TEXT("Receiver log should exclude the post-unbind broadcast"))));
	}
};

#endif
