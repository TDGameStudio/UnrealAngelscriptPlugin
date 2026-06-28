#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestWorld.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

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
private:
	static bool ExpectBoolByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, bool Expected, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(VerifyByPath<FBoolProperty, bool>(Test, Object, Path, Expected, Message), Message);
	}

	static bool ExpectIntByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32 Expected, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(VerifyByPath<FIntProperty, int32>(Test, Object, Path, Expected, Message), Message);
	}

	static bool ReadObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject*& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetObjectByPath(Test, Object, Path, OutValue), Message);
	}

	static bool ReadIntByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetByPath<FIntProperty, int32>(Test, Object, Path, OutValue), Message);
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component basic declaration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootIsValid"), true, TEXT("Root component should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildIsValid"), true, TEXT("Child component should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LogicComponentIsValid"), true, TEXT("Logic component should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildIsAttached"), true, TEXT("Child should be attached to Root"))));
	}

	// -------------------------------------------------------------------------
	// Component UPROPERTY specifiers: EditAnywhere, BlueprintReadOnly, Instanced
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentPropertySpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_PropertySpecifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageInstancedLogicObject : UObject
			{
				UPROPERTY()
				int Value = 19;
			}

			UCLASS()
			class ACoverageComponentPropertySpecifierActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, ShowOnActor, EditAnywhere, BlueprintReadOnly)
				USceneComponent VisibleChild;

				UPROPERTY(Instanced)
				UCoverageInstancedLogicObject InlineObject;

				UPROPERTY()
				bool VisibleChildValid = false;

				UPROPERTY()
				bool InlineObjectAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VisibleChildValid = VisibleChild != nullptr;
					InlineObject = NewObject(this, UCoverageInstancedLogicObject::StaticClass());
					InlineObjectAssigned = InlineObject != nullptr && InlineObject.Value == 19;
				}
			}
			)AS"),
			TEXT("ACoverageComponentPropertySpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component property specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* VisibleChildProperty = ScriptClass->FindPropertyByName(TEXT("VisibleChild"));
		ASSERT_THAT(IsNotNull(VisibleChildProperty, TEXT("Default component property should exist")));
		if (VisibleChildProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere/ShowOnActor should make the component property editable")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly should make the component property Blueprint visible")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly should set CPF_BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("DefaultComponent should be an instanced exported reference")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should add EditInline metadata")));

		FProperty* InlineObjectProperty = ScriptClass->FindPropertyByName(TEXT("InlineObject"));
		ASSERT_THAT(IsNotNull(InlineObjectProperty, TEXT("Instanced object property should exist")));
		if (InlineObjectProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(InlineObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance), TEXT("UPROPERTY(Instanced) should set persistent instance flags")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component property specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("VisibleChildValid"), true, TEXT("Default component specifier property should create a component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InlineObjectAssigned"), true, TEXT("Instanced property should accept a runtime inline object"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component lifecycle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UActorComponent* TestComp = Actor->GetComponentByClass(UActorComponent::StaticClass());
		ASSERT_THAT(IsNotNull(TestComp, TEXT("TestComp should exist")));
		if (TestComp == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, TestComp, TEXT("LifecycleStage"), 2, TEXT("Lifecycle should reach BeginPlay stage"))));

		// Tick a few frames
		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 2);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, TestComp, TEXT("TickCount"), TickCount, TEXT("TickCount should be readable"))));
		ASSERT_THAT(IsTrue(TickCount >= 2, TEXT("Component should tick multiple times")));
	}

	// -------------------------------------------------------------------------
	// Component lifecycle ordering: create, initialize, begin play, uninitialize, destroy
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentLifecycleOrdering)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_LifecycleOrdering"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentLifecycleOrdering.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageLifecycleOrderComponent : UActorComponent
			{
				UPROPERTY()
				int NextOrder = 0;

				UPROPERTY()
				int CreatedOrder = 0;

				UPROPERTY()
				int InitializedOrder = 0;

				UPROPERTY()
				int BeginPlayOrder = 0;

				UPROPERTY()
				int EndPlayOrder = 0;

				UPROPERTY()
				int UninitializedOrder = 0;

				UPROPERTY()
				int DestroyedOrder = 0;

				UPROPERTY()
				bool bSawOwnerDuringBeginPlay = false;

				int ClaimOrder()
				{
					NextOrder++;
					return NextOrder;
				}

				UFUNCTION(BlueprintOverride)
				void OnComponentCreated()
				{
					CreatedOrder = ClaimOrder();
				}

				UFUNCTION(BlueprintOverride)
				void InitializeComponent()
				{
					InitializedOrder = ClaimOrder();
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayOrder = ClaimOrder();
					bSawOwnerDuringBeginPlay = GetOwner() != nullptr;
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					EndPlayOrder = ClaimOrder();
				}

				UFUNCTION(BlueprintOverride)
				void UninitializeComponent()
				{
					UninitializedOrder = ClaimOrder();
				}

				UFUNCTION(BlueprintOverride)
				void OnComponentDestroyed(bool bDestroyingHierarchy)
				{
					DestroyedOrder = ClaimOrder();
				}
			}

			UCLASS()
			class ACoverageComponentLifecycleOrderingActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				UCoverageLifecycleOrderComponent Probe;

				UPROPERTY()
				int ActorBeginPlayOrder = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ActorBeginPlayOrder = 1;
					Tags.Add(n"ActorBeginPlayRan");
				}
			}
			)AS"),
			TEXT("ACoverageComponentLifecycleOrderingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component lifecycle ordering actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FAngelscriptTestWorld World(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(World.IsValid(), TEXT("Lifecycle ordering world should be valid")));
		if (!World.IsValid())
		{
			return;
		}
		AActor* Actor = World.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component lifecycle ordering actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		World.BeginPlay(*Actor);

		UObject* ProbeObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("Probe"), ProbeObject, TEXT("Probe component should be readable"))));
		UActorComponent* Probe = Cast<UActorComponent>(ProbeObject);
		ASSERT_THAT(IsNotNull(Probe, TEXT("Probe should be an actor component")));
		if (Probe == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Probe, TEXT("bSawOwnerDuringBeginPlay"), true, TEXT("Component BeginPlay should observe its owner"))));
		ASSERT_THAT(IsTrue(Probe->IsRegistered(), TEXT("Default component should be registered before explicit unregister")));

		World.DestroyAndDrain(*Actor);

		int32 CreatedOrder = 0;
		int32 InitializedOrder = 0;
		int32 BeginPlayOrder = 0;
		int32 EndPlayOrder = 0;
		int32 UninitializedOrder = 0;
		int32 DestroyedOrder = 0;
		ASSERT_THAT(IsTrue(
			ReadIntByPath(*TestRunner, Probe, TEXT("CreatedOrder"), CreatedOrder, TEXT("CreatedOrder should be readable"))
			&& ReadIntByPath(*TestRunner, Probe, TEXT("InitializedOrder"), InitializedOrder, TEXT("InitializedOrder should be readable"))
			&& ReadIntByPath(*TestRunner, Probe, TEXT("BeginPlayOrder"), BeginPlayOrder, TEXT("BeginPlayOrder should be readable"))
			&& ReadIntByPath(*TestRunner, Probe, TEXT("EndPlayOrder"), EndPlayOrder, TEXT("EndPlayOrder should be readable"))
			&& ReadIntByPath(*TestRunner, Probe, TEXT("UninitializedOrder"), UninitializedOrder, TEXT("UninitializedOrder should be readable"))
			&& ReadIntByPath(*TestRunner, Probe, TEXT("DestroyedOrder"), DestroyedOrder, TEXT("DestroyedOrder should be readable")),
			TEXT("Lifecycle order values should be readable after DestroyAndDrain")));

		ASSERT_THAT(IsTrue(CreatedOrder > 0, TEXT("OnComponentCreated should be recorded")));
		ASSERT_THAT(IsTrue(InitializedOrder > 0, TEXT("InitializeComponent should be recorded")));
		ASSERT_THAT(IsTrue(BeginPlayOrder > 0, TEXT("BeginPlay should be recorded")));
		ASSERT_THAT(IsTrue(EndPlayOrder > 0, TEXT("EndPlay should be recorded")));
		ASSERT_THAT(IsTrue(UninitializedOrder > 0, TEXT("UninitializeComponent should be recorded")));
		ASSERT_THAT(IsTrue(DestroyedOrder > 0, TEXT("OnComponentDestroyed should be recorded")));

		ASSERT_THAT(IsTrue(CreatedOrder < InitializedOrder, TEXT("OnComponentCreated should precede InitializeComponent")));
		ASSERT_THAT(IsTrue(InitializedOrder < BeginPlayOrder, TEXT("InitializeComponent should precede BeginPlay")));
		ASSERT_THAT(IsTrue(BeginPlayOrder < EndPlayOrder, TEXT("BeginPlay should precede EndPlay")));
		ASSERT_THAT(IsTrue(EndPlayOrder < UninitializedOrder, TEXT("EndPlay should precede UninitializeComponent")));
		ASSERT_THAT(IsTrue(UninitializedOrder < DestroyedOrder, TEXT("UninitializeComponent should precede OnComponentDestroyed")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tick control actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Tick multiple times
		for (int32 i = 0; i < 10; i++)
		{
			TickWorld(Engine, Spawner.GetWorld(), 0.05f, 1);
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DisableTickCount"), 2, TEXT("Tick should be disabled after reaching count"))));
	}

	// -------------------------------------------------------------------------
	// Component tick configuration: start enabled, interval, tick group and prerequisites
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTickConfigurationAndPrerequisites)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_TickConfiguration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentTickConfiguration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageTickConfigComponent : UActorComponent
			{
				UPROPERTY()
				int TickCount = 0;

				default PrimaryComponentTick.bCanEverTick = true;
				default PrimaryComponentTick.TickInterval = 0.5f;
				default PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;
				default PrimaryComponentTick.bStartWithTickEnabled = true;

				UFUNCTION(BlueprintOverride)
				void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
				{
					TickCount++;
				}
			}

			UCLASS()
			class UCoverageTickDisabledComponent : UActorComponent
			{
				default PrimaryComponentTick.bCanEverTick = false;
			}

			UCLASS()
			class ACoverageComponentTickConfigurationActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageTickConfigComponent TickComp;

				UPROPERTY(DefaultComponent)
				UCoverageTickDisabledComponent DisabledComp;

				UPROPERTY()
				bool CanEverTickEnabled = false;

				UPROPERTY()
				bool StartTickEnabled = false;

				UPROPERTY()
				bool DisabledCanEverTick = true;

				UPROPERTY()
				float TickInterval = 0.0f;

				UPROPERTY()
				bool TickGroupMatched = false;

				UPROPERTY()
				bool ComponentPrereqAdded = false;

				UPROPERTY()
				bool ActorPrereqAdded = false;

				UPROPERTY()
				bool ComponentPrereqRemoved = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					CanEverTickEnabled = TickComp.PrimaryComponentTick.bCanEverTick;
					StartTickEnabled = TickComp.IsComponentTickEnabled();
					DisabledCanEverTick = DisabledComp.PrimaryComponentTick.bCanEverTick;
					TickInterval = TickComp.PrimaryComponentTick.TickInterval;
					TickGroupMatched = TickComp.PrimaryComponentTick.TickGroup == ETickingGroup::TG_PrePhysics;

					TickComp.AddTickPrerequisiteComponent(DisabledComp);
					ComponentPrereqAdded = true;

					TickComp.AddTickPrerequisiteActor(this);
					ActorPrereqAdded = true;

					TickComp.RemoveTickPrerequisiteComponent(DisabledComp);
					ComponentPrereqRemoved = true;
				}
			}
			)AS"),
			TEXT("ACoverageComponentTickConfigurationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component tick configuration actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tick configuration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("CanEverTickEnabled"), true, TEXT("bCanEverTick default should enable ticking"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("StartTickEnabled"), true, TEXT("bStartWithTickEnabled should start enabled"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("DisabledCanEverTick"), false, TEXT("bCanEverTick false default should disable ticking support"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("TickGroupMatched"), true, TEXT("TickGroup default should round-trip"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ComponentPrereqAdded"), true, TEXT("AddTickPrerequisiteComponent should be callable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ActorPrereqAdded"), true, TEXT("AddTickPrerequisiteActor should be callable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ComponentPrereqRemoved"), true, TEXT("RemoveTickPrerequisiteComponent should be callable"))));

		float TickInterval = 0.0f;
		ASSERT_THAT(IsTrue(GetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("TickInterval"), TickInterval), TEXT("TickInterval should be readable")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TickInterval, 0.5f, 0.01f), TEXT("TickInterval default should be 0.5")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component activation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyActive"), true, TEXT("Component should be initially active"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterDeactivate"), false, TEXT("Component should be inactive after Deactivate"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterReactivate"), true, TEXT("Component should be active after Activate"))));
	}

	// -------------------------------------------------------------------------
	// Component registration and activation APIs: Register, Unregister, Activate
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentRegistrationAndActivation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_RegistrationActivation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentRegistrationActivation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageRuntimeLogicComponent : UActorComponent
			{
				UPROPERTY()
				int Value = 17;
			}

			UCLASS()
			class ACoverageComponentRegistrationActivationActor : AActor
			{
				UPROPERTY()
				UCoverageRuntimeLogicComponent RuntimeComp;

				UPROPERTY()
				bool RegisteredAfterCreate = false;

				UPROPERTY()
				bool UnregisteredAfterCall = false;

				UPROPERTY()
				bool RegisteredAgain = false;

				UPROPERTY()
				bool ActiveAfterActivate = false;

				UPROPERTY()
				bool InactiveAfterDeactivate = false;

				UPROPERTY()
				bool OwnerMatched = false;

				UPROPERTY()
				bool WorldMatched = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RuntimeComp = UCoverageRuntimeLogicComponent::Create(this, n"RuntimeComp");
					RegisteredAfterCreate = RuntimeComp != nullptr && RuntimeComp.IsRegistered();

					RuntimeComp.UnregisterComponent();
					UnregisteredAfterCall = !RuntimeComp.IsRegistered();

					RuntimeComp.RegisterComponent();
					RegisteredAgain = RuntimeComp.IsRegistered();

					RuntimeComp.Activate(true);
					ActiveAfterActivate = RuntimeComp.IsActive();

					RuntimeComp.Deactivate();
					InactiveAfterDeactivate = !RuntimeComp.IsActive();

					OwnerMatched = RuntimeComp.GetOwner() == this;
					WorldMatched = RuntimeComp.GetWorld() == GetWorld();
				}
			}
			)AS"),
			TEXT("ACoverageComponentRegistrationActivationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component registration/activation actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component registration/activation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("RegisteredAfterCreate"), true, TEXT("Create should register the component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("UnregisteredAfterCall"), true, TEXT("UnregisterComponent should clear registration"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("RegisteredAgain"), true, TEXT("RegisterComponent should restore registration"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ActiveAfterActivate"), true, TEXT("Activate should set component active"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InactiveAfterDeactivate"), true, TEXT("Deactivate should clear component active"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("OwnerMatched"), true, TEXT("GetOwner should return the owning actor"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("WorldMatched"), true, TEXT("GetWorld should match actor world"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component finding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundSingleComponent"), true, TEXT("GetComponentByClass should find a component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SceneComponentCount"), 3, TEXT("Should find 3 scene components"))));
	}

	// -------------------------------------------------------------------------
	// Component finding APIs: GetComponentByClass, FindComponentByClass and tag filtering
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentFindingByClassAndTag)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_FindingByClassAndTag"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentFindingByClassAndTag.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageFindBaseComponent : UActorComponent
			{
			}

			UCLASS()
			class UCoverageFindDerivedComponent : UCoverageFindBaseComponent
			{
			}

			UCLASS()
			class ACoverageComponentFindingByClassAndTagActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageFindDerivedComponent DerivedA;

				UPROPERTY(DefaultComponent)
				UCoverageFindDerivedComponent DerivedB;

				UPROPERTY()
				bool GetComponentByClassFound = false;

				UPROPERTY()
				bool FindComponentByClassFoundDerived = false;

				UPROPERTY()
				int TaggedComponentCount = 0;

				UPROPERTY()
				int DerivedComponentCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DerivedA.ComponentTags.Add(n"CoverageTag");
					DerivedB.ComponentTags.Add(n"CoverageTag");

					UActorComponent FoundBase = GetComponentByClass(UCoverageFindBaseComponent::StaticClass());
					GetComponentByClassFound = FoundBase != nullptr;

					UActorComponent FoundDerived = FindComponentByClass(UCoverageFindDerivedComponent::StaticClass());
					FindComponentByClassFoundDerived = FoundDerived != nullptr;

					TArray<UActorComponent> AllComponents;
					GetComponentsByClass(UActorComponent::StaticClass(), AllComponents);
					for (UActorComponent Component : AllComponents)
					{
						if (Component.ComponentHasTag(n"CoverageTag"))
						{
							TaggedComponentCount++;
						}
					}

					TArray<UCoverageFindDerivedComponent> DerivedComponents;
					GetComponentsByClass(UCoverageFindDerivedComponent::StaticClass(), DerivedComponents);
					DerivedComponentCount = DerivedComponents.Num();
				}
			}
			)AS"),
			TEXT("ACoverageComponentFindingByClassAndTagActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component class/tag finding actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component class/tag finding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("GetComponentByClassFound"), true, TEXT("GetComponentByClass should find a derived component through base class"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("FindComponentByClassFoundDerived"), true, TEXT("FindComponentByClass should find a derived component"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("TaggedComponentCount"), 2, TEXT("ComponentHasTag should identify both tagged components after class lookup"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("DerivedComponentCount"), 2, TEXT("GetComponentsByClass should find both derived components"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tags actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasTestTag"), true, TEXT("Should have TestTag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasOtherTag"), false, TEXT("Should not have OtherTag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TagCount"), 2, TEXT("Should have 2 tags"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Custom script component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RetrievedValue"), 42, TEXT("Should retrieve custom value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("RetrievedName"), FString(TEXT("TestComponent")), TEXT("Should retrieve custom name"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DoubledValue"), 84, TEXT("Should calculate doubled value"))));
	}

	// -------------------------------------------------------------------------
	// Custom component reuse: multiple actors, inheritance chain, and Create()
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomComponentReuseInheritanceAndInstantiation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_CustomReuseInheritance"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentCustomReuseInheritance.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageReusableBaseComponent : UActorComponent
			{
				UPROPERTY()
				int BaseValue = 10;

				UFUNCTION()
				int ComputeValue()
				{
					return BaseValue;
				}
			}

			UCLASS()
			class UCoverageReusableDerivedComponent : UCoverageReusableBaseComponent
			{
				UPROPERTY()
				int DerivedValue = 5;

				UFUNCTION()
				int ComputeDerivedValue()
				{
					return ComputeValue() + DerivedValue;
				}
			}

			UCLASS()
			class ACoverageComponentReusableActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageReusableDerivedComponent DefaultReusable;

				UPROPERTY()
				UCoverageReusableDerivedComponent DynamicReusable;

				UPROPERTY()
				bool DefaultComponentValid = false;

				UPROPERTY()
				bool DynamicComponentValid = false;

				UPROPERTY()
				bool DynamicRegistered = false;

				UPROPERTY()
				int CombinedValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DynamicReusable = UCoverageReusableDerivedComponent::Create(this, n"DynamicReusable");

					DefaultComponentValid = DefaultReusable != nullptr;
					DynamicComponentValid = DynamicReusable != nullptr && DynamicReusable != DefaultReusable;
					DynamicRegistered = DynamicReusable != nullptr && DynamicReusable.IsRegistered();

					if (DefaultReusable != nullptr && DynamicReusable != nullptr)
					{
						CombinedValue = DefaultReusable.ComputeDerivedValue() + DynamicReusable.ComputeDerivedValue();
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentReusableActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Reusable component actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* FirstActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		AActor* SecondActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(FirstActor, TEXT("First reusable component actor should spawn")));
		ASSERT_THAT(IsNotNull(SecondActor, TEXT("Second reusable component actor should spawn")));
		if (FirstActor == nullptr || SecondActor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *FirstActor);
		BeginPlayActor(Engine, *SecondActor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, FirstActor, TEXT("DefaultComponentValid"), true, TEXT("First actor should receive reusable default component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, FirstActor, TEXT("DynamicComponentValid"), true, TEXT("First actor should create a distinct reusable runtime component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, FirstActor, TEXT("DynamicRegistered"), true, TEXT("First actor runtime component should register"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, FirstActor, TEXT("CombinedValue"), 30, TEXT("Component inheritance methods should work on first actor"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, SecondActor, TEXT("DefaultComponentValid"), true, TEXT("Second actor should receive reusable default component"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, SecondActor, TEXT("CombinedValue"), 30, TEXT("Component class should be reusable across actor instances"))));

		UObject* FirstDefaultObject = nullptr;
		UObject* SecondDefaultObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, FirstActor, TEXT("DefaultReusable"), FirstDefaultObject, TEXT("First default component should be readable"))));
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, SecondActor, TEXT("DefaultReusable"), SecondDefaultObject, TEXT("Second default component should be readable"))));
		ASSERT_THAT(IsTrue(FirstDefaultObject != nullptr && SecondDefaultObject != nullptr && FirstDefaultObject != SecondDefaultObject, TEXT("Reusable component instances should be distinct per actor")));
		if (FirstDefaultObject == nullptr || SecondDefaultObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FirstDefaultObject->GetClass() == SecondDefaultObject->GetClass(), TEXT("Reusable component instances should share the generated component class")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component destruction actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasDestroyed"), true, TEXT("Component should be destroyed"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
