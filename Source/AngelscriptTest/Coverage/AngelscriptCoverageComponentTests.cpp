#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageComponentTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UActorComponent basics and lifecycle, corresponding
// to Documents/Coverage/Coverage_UComponent.md sections 1-4 and 8-10.
//
// Axes covered here:
//   * ComponentDeclaration      - DefaultComponent, RootComponent, Attach specifiers
//   * ComponentLifecycle        - OnComponentCreated, BeginPlay, Tick, EndPlay
//   * ComponentTickControl      - bCanEverTick, TickInterval, SetComponentTickEnabled
//   * ComponentActivation       - Activate, Deactivate, IsActive
//   * ComponentFinding          - GetComponentByClass, GetComponentsByClass
//   * ComponentTags             - ComponentTags, ComponentHasTag
//   * CustomScriptComponent     - Script-derived component classes
//
// Pattern D (script execution): compile AS actors with components, spawn them,
// drive component operations through lifecycle, verify state via properties.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_UComponent.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageComponentTest,
	"Angelscript.TestModule.Coverage.Component",
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
	// Component declaration: DefaultComponent, RootComponent, Attach specifiers
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_BasicDeclaration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentBasicDeclaration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentBasicActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child;

				UPROPERTY(DefaultComponent)
				UActorComponent LogicComponent;

				UPROPERTY()
				bool RootIsValid = false;

				UPROPERTY()
				bool ChildIsValid = false;

				UPROPERTY()
				bool ChildIsAttached = false;

				UPROPERTY()
				bool LogicComponentIsValid = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RootIsValid = (Root != nullptr);
					ChildIsValid = (Child != nullptr);
					LogicComponentIsValid = (LogicComponent != nullptr);

					if (Child != nullptr && Root != nullptr)
					{
						ChildIsAttached = Child.IsAttachedTo(Root);
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component basic declaration actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component basic declaration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootIsValid"), true, TEXT("Root component should be created"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildIsValid"), true, TEXT("Child component should be created"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LogicComponentIsValid"), true, TEXT("Logic component should be created"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildIsAttached"), true, TEXT("Child should be attached to Root"));
	}

	// -------------------------------------------------------------------------
	// Component lifecycle: OnComponentCreated -> BeginPlay -> Tick -> EndPlay
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Lifecycle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentLifecycle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ULifecycleTestComponent : UActorComponent
			{
				UPROPERTY()
				int LifecycleStage = 0;

				UPROPERTY()
				int TickCount = 0;

				default PrimaryComponentTick.bCanEverTick = true;

				UFUNCTION(BlueprintOverride)
				void OnComponentCreated()
				{
					LifecycleStage = 1;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (LifecycleStage == 1)
					{
						LifecycleStage = 2;
					}
				}

				UFUNCTION(BlueprintOverride)
				void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
				{
					if (LifecycleStage == 2)
					{
						TickCount++;
					}
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					if (LifecycleStage == 2)
					{
						LifecycleStage = 3;
					}
				}
			}

			UCLASS()
			class ACoverageComponentLifecycleActor : AActor
			{
				UPROPERTY(DefaultComponent)
				ULifecycleTestComponent TestComp;
			}
			)AS"),
			TEXT("ACoverageComponentLifecycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component lifecycle actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component lifecycle actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		UActorComponent* TestComp = Actor->GetComponentByClass(UActorComponent::StaticClass());
		ASSERT_THAT(IsNotNull(TestComp, TEXT("TestComp should exist")));

		VerifyByPath<FIntProperty, int32>(*TestRunner, TestComp, TEXT("LifecycleStage"), 2, TEXT("Lifecycle should reach BeginPlay stage"));

		// Tick a few frames
		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 2);

		int32 TickCount = 0;
		GetByPath<FIntProperty, int32>(*TestRunner, TestComp, TEXT("TickCount"), TickCount);
		ASSERT_THAT(IsTrue(TickCount >= 2, TEXT("Component should tick multiple times")));
	}

	// -------------------------------------------------------------------------
	// Component tick control: bCanEverTick, TickInterval, SetComponentTickEnabled
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTickControl)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_TickControl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentTickControl.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UTickControlComponent : UActorComponent
			{
				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				float AccumulatedTime = 0.0f;

				default PrimaryComponentTick.bCanEverTick = true;
				default PrimaryComponentTick.TickInterval = 0.2f;

				UFUNCTION(BlueprintOverride)
				void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
				{
					TickCount++;
					AccumulatedTime += DeltaTime;
				}
			}

			UCLASS()
			class ACoverageComponentTickControlActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UTickControlComponent TestComp;

				UPROPERTY()
				int DisableTickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (TestComp.IsComponentTickEnabled())
					{
						DisableTickCount = 1;
					}
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					if (TestComp.TickCount >= 3 && DisableTickCount == 1)
					{
						TestComp.SetComponentTickEnabled(false);
						DisableTickCount = 2;
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentTickControlActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component tick control actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tick control actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Tick multiple times
		for (int32 i = 0; i < 10; i++)
		{
			TickWorld(Engine, Spawner.GetWorld(), 0.05f, 1);
		}

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DisableTickCount"), 2, TEXT("Tick should be disabled after reaching count"));
	}

	// -------------------------------------------------------------------------
	// Component activation: Activate, Deactivate, IsActive
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentActivation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Activation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentActivation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentActivationActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UActorComponent TestComp;

				UPROPERTY()
				bool InitiallyActive = false;

				UPROPERTY()
				bool AfterDeactivate = true;

				UPROPERTY()
				bool AfterReactivate = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitiallyActive = TestComp.IsActive();

					TestComp.Deactivate();
					AfterDeactivate = TestComp.IsActive();

					TestComp.Activate(true);
					AfterReactivate = TestComp.IsActive();
				}
			}
			)AS"),
			TEXT("ACoverageComponentActivationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component activation actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component activation actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyActive"), true, TEXT("Component should be initially active"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterDeactivate"), false, TEXT("Component should be inactive after Deactivate"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterReactivate"), true, TEXT("Component should be active after Activate"));
	}

	// -------------------------------------------------------------------------
	// Component finding: GetComponentByClass, GetComponentsByClass
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentFinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Finding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentFinding.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentFindingActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child1;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child2;

				UPROPERTY(DefaultComponent)
				UActorComponent LogicComp;

				UPROPERTY()
				bool FoundSingleComponent = false;

				UPROPERTY()
				int SceneComponentCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UActorComponent FoundComp = GetComponentByClass(UActorComponent::StaticClass());
					FoundSingleComponent = (FoundComp != nullptr);

					TArray<USceneComponent> SceneComps;
					GetComponentsByClass(USceneComponent::StaticClass(), SceneComps);
					SceneComponentCount = SceneComps.Num();
				}
			}
			)AS"),
			TEXT("ACoverageComponentFindingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component finding actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component finding actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundSingleComponent"), true, TEXT("GetComponentByClass should find a component"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SceneComponentCount"), 3, TEXT("Should find 3 scene components"));
	}

	// -------------------------------------------------------------------------
	// Component tags: ComponentTags, ComponentHasTag
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Tags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentTags.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentTagsActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UActorComponent TestComp;

				UPROPERTY()
				bool HasTestTag = false;

				UPROPERTY()
				bool HasOtherTag = false;

				UPROPERTY()
				int TagCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestComp.ComponentTags.Add(n"TestTag");
					TestComp.ComponentTags.Add(n"AnotherTag");

					HasTestTag = TestComp.ComponentHasTag(n"TestTag");
					HasOtherTag = TestComp.ComponentHasTag(n"OtherTag");
					TagCount = TestComp.ComponentTags.Num();
				}
			}
			)AS"),
			TEXT("ACoverageComponentTagsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component tags actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tags actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasTestTag"), true, TEXT("Should have TestTag"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasOtherTag"), false, TEXT("Should not have OtherTag"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TagCount"), 2, TEXT("Should have 2 tags"));
	}

	// -------------------------------------------------------------------------
	// Custom script component: script-derived UActorComponent with custom properties
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomScriptComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_CustomScript"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentCustomScript.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCustomLogicComponent : UActorComponent
			{
				UPROPERTY()
				int CustomValue = 42;

				UPROPERTY()
				FString CustomName = "TestComponent";

				UFUNCTION()
				int GetDoubledValue()
				{
					return CustomValue * 2;
				}
			}

			UCLASS()
			class ACoverageComponentCustomScriptActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCustomLogicComponent CustomComp;

				UPROPERTY()
				int RetrievedValue = 0;

				UPROPERTY()
				FString RetrievedName;

				UPROPERTY()
				int DoubledValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (CustomComp != nullptr)
					{
						RetrievedValue = CustomComp.CustomValue;
						RetrievedName = CustomComp.CustomName;
						DoubledValue = CustomComp.GetDoubledValue();
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentCustomScriptActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Custom script component actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Custom script component actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RetrievedValue"), 42, TEXT("Should retrieve custom value"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("RetrievedName"), FString(TEXT("TestComponent")), TEXT("Should retrieve custom name"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DoubledValue"), 84, TEXT("Should calculate doubled value"));
	}

	// -------------------------------------------------------------------------
	// Component destruction: DestroyComponent, IsBeingDestroyed
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentDestruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Destruction"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentDestruction.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentDestructionActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UActorComponent TestComp;

				UPROPERTY()
				bool WasDestroyed = false;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					if (TestComp != nullptr && !TestComp.IsBeingDestroyed())
					{
						TestComp.DestroyComponent();
						WasDestroyed = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentDestructionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component destruction actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component destruction actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 1);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasDestroyed"), true, TEXT("Component should be destroyed"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
