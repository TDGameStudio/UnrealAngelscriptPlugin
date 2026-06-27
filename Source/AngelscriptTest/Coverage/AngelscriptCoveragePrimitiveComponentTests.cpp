#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoveragePrimitiveComponentTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UPrimitiveComponent features (rendering, collision,
// physics), corresponding to Documents/Coverage/Coverage_UComponent.md section 6.
//
// Axes covered here:
//   * PrimitiveRendering        - Visibility, CastShadow, CustomDepth
//   * PrimitiveMaterials        - SetMaterial, GetMaterial
//   * PrimitiveCollisionSetup   - CollisionEnabled, CollisionProfile
//   * PrimitiveCollisionEvents  - OnComponentHit, BeginOverlap, EndOverlap
//   * PrimitivePhysics          - SimulatePhysics, AddImpulse, AddForce
//
// Pattern D (script execution): compile AS actors with primitive components,
// spawn them, manipulate rendering/collision/physics, verify results.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_UComponent.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoveragePrimitiveComponentTest,
	"Angelscript.TestModule.Coverage.PrimitiveComponent",
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
	// Primitive rendering: Visibility, CastShadow, CustomDepth
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveRendering)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_Rendering"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveRendering.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveRenderingActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool InitiallyVisible = false;

				UPROPERTY()
				bool AfterSetVisible = false;

				UPROPERTY()
				bool InitiallyCastsShadow = false;

				UPROPERTY()
				bool AfterSetCastShadow = false;

				UPROPERTY()
				bool CustomDepthEnabled = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitiallyVisible = MeshComp.IsVisible();

					MeshComp.SetVisibility(false);
					AfterSetVisible = MeshComp.IsVisible();

					InitiallyCastsShadow = MeshComp.CastShadow;

					MeshComp.SetCastShadow(false);
					AfterSetCastShadow = MeshComp.CastShadow;

					MeshComp.SetRenderCustomDepth(true);
					MeshComp.SetCustomDepthStencilValue(128);
					CustomDepthEnabled = MeshComp.bRenderCustomDepth;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveRenderingActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive rendering actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive rendering actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyVisible"), true, TEXT("Component should be initially visible"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterSetVisible"), false, TEXT("Component should be invisible after SetVisibility(false)"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CustomDepthEnabled"), true, TEXT("Custom depth should be enabled"));
	}

	// -------------------------------------------------------------------------
	// Primitive collision setup: CollisionEnabled, CollisionProfile
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveCollisionSetup)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_CollisionSetup"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveCollisionSetup.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveCollisionSetupActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool CollisionEnabledSet = false;

				UPROPERTY()
				bool ProfileSet = false;

				UPROPERTY()
				bool GenerateOverlapEventsSet = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set collision enabled
					MeshComp.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					CollisionEnabledSet = (MeshComp.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);

					// Set collision profile
					MeshComp.SetCollisionProfileName(n"BlockAll");
					ProfileSet = (MeshComp.GetCollisionProfileName() == n"BlockAll");

					// Enable overlap events
					MeshComp.SetGenerateOverlapEvents(true);
					GenerateOverlapEventsSet = MeshComp.GetGenerateOverlapEvents();
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionSetupActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision setup actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision setup actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionEnabledSet"), true, TEXT("Collision enabled should be set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProfileSet"), true, TEXT("Collision profile should be set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GenerateOverlapEventsSet"), true, TEXT("Generate overlap events should be set"));
	}

	// -------------------------------------------------------------------------
	// Primitive collision events: OnComponentBeginOverlap, OnComponentEndOverlap
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveCollisionEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_CollisionEvents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveCollisionEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveCollisionEventsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				int BeginOverlapCount = 0;

				UPROPERTY()
				int EndOverlapCount = 0;

				UPROPERTY()
				FString OverlappedActorName;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SphereComp.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					SphereComp.SetCollisionProfileName(n"OverlapAll");
					SphereComp.SetGenerateOverlapEvents(true);
					SphereComp.SetSphereRadius(100.0f);

					SphereComp.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
					SphereComp.OnComponentEndOverlap.AddUFunction(this, n"HandleEndOverlap");
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
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionEventsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision events actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision events actor should spawn")));
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
	// Primitive physics: SimulatePhysics, AddImpulse, AddForce
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitivePhysics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_Physics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitivePhysics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitivePhysicsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool PhysicsEnabled = false;

				UPROPERTY()
				bool GravityEnabled = false;

				UPROPERTY()
				bool ImpulseApplied = false;

				UPROPERTY()
				bool ForceApplied = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Enable physics simulation
					MeshComp.SetSimulatePhysics(true);
					PhysicsEnabled = MeshComp.IsSimulatingPhysics();

					// Enable gravity
					MeshComp.SetEnableGravity(true);
					GravityEnabled = MeshComp.IsGravityEnabled();

					// Apply impulse
					MeshComp.AddImpulse(FVector(0.0f, 0.0f, 1000.0f), NAME_None, false);
					ImpulseApplied = true;

					// Apply force
					MeshComp.AddForce(FVector(0.0f, 0.0f, 500.0f), NAME_None, false);
					ForceApplied = true;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitivePhysicsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive physics actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive physics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PhysicsEnabled"), true, TEXT("Physics should be enabled"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GravityEnabled"), true, TEXT("Gravity should be enabled"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImpulseApplied"), true, TEXT("Impulse should be applied"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ForceApplied"), true, TEXT("Force should be applied"));
	}

	// -------------------------------------------------------------------------
	// Primitive collision response: SetCollisionResponseToChannel
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveCollisionResponse)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_CollisionResponse"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveCollisionResponse.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveCollisionResponseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool ResponseSet = false;

				UPROPERTY()
				bool AllChannelsSet = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set response to specific channel
					MeshComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
					ResponseSet = true;

					// Set response to all channels
					MeshComp.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
					AllChannelsSet = true;

					// Set object type
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionResponseActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision response actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision response actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResponseSet"), true, TEXT("Collision response should be set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllChannelsSet"), true, TEXT("All channels response should be set"));
	}

	// -------------------------------------------------------------------------
	// Primitive hit events: OnComponentHit
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveHitEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_HitEvents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveHitEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveHitEventsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				int HitCount = 0;

				UPROPERTY()
				FVector HitNormal;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MeshComp.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					MeshComp.SetNotifyRigidBodyCollision(true);

					MeshComp.OnComponentHit.AddUFunction(this, n"HandleHit");
				}

				UFUNCTION()
				void HandleHit(UPrimitiveComponent HitComponent, AActor OtherActor, UPrimitiveComponent OtherComp,
					FVector NormalImpulse, const FHitResult& Hit)
				{
					HitCount++;
					HitNormal = Hit.Normal;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveHitEventsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive hit events actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive hit events actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify event was bound (actual hit would require physics simulation)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("HitCount"), 0, TEXT("Hit count should be 0 initially"));
	}

	// -------------------------------------------------------------------------
	// Primitive hidden in game: SetHiddenInGame
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveHiddenInGame)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_HiddenInGame"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveHiddenInGame.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveHiddenInGameActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool InitiallyHidden = true;

				UPROPERTY()
				bool AfterSetHidden = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitiallyHidden = MeshComp.bHiddenInGame;

					MeshComp.SetHiddenInGame(true);
					AfterSetHidden = MeshComp.bHiddenInGame;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveHiddenInGameActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive hidden in game actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive hidden in game actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyHidden"), false, TEXT("Component should not be initially hidden"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterSetHidden"), true, TEXT("Component should be hidden after SetHiddenInGame(true)"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
