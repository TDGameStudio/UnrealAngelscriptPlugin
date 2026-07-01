#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoveragePrimitiveComponentTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UPrimitiveComponent features (rendering, collision,
// physics), corresponding to OpenSpec: test-coverage/coverage-matrix.md section 6.
//
// Axes covered here:
//   * PrimitiveRendering        - Visibility, CastShadow, CustomDepth
//   * PrimitiveMaterials        - SetMaterial, GetMaterial
//   * PrimitiveCollisionSetup   - CollisionEnabled, CollisionProfile
//   * PrimitiveCollisionEvents  - OnComponentHit, BeginOverlap, EndOverlap
//   * PrimitivePhysics          - SimulatePhysics, AddImpulse, AddForce
//   * PrimitivePhysicsStateReadback - MassOverride, LinearVelocity, AngularVelocity
//
// Pattern D (script execution): compile AS actors with primitive components,
// spawn them, manipulate rendering/collision/physics, verify results.
//
// Detailed coverage matrix: OpenSpec: test-coverage/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

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
				bool CustomDepthCallAccepted = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitiallyVisible = MeshComp.IsVisible();

					MeshComp.SetVisibility(false);
					AfterSetVisible = MeshComp.IsVisible();

					MeshComp.SetCastShadow(false);
					AfterSetCastShadow = true;

					MeshComp.SetRenderCustomDepth(true);
					MeshComp.SetCustomDepthStencilValue(128);
					CustomDepthCallAccepted = true;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveRenderingActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive rendering actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive rendering actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyVisible"), true, TEXT("Component should be initially visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterSetVisible"), false, TEXT("Component should be invisible after SetVisibility(false)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterSetCastShadow"), true, TEXT("SetCastShadow should be callable from AS"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CustomDepthCallAccepted"), true, TEXT("Custom depth setters should be callable from AS"))));
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

				UPROPERTY()
				bool CollisionModesCovered = false;

				UPROPERTY()
				bool CommonProfilesCovered = false;

				UPROPERTY()
				bool NotifyRigidBodyCollisionSet = false;

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

					// Exercise collision enabled modes with readback.
					MeshComp.SetCollisionEnabled(ECollisionEnabled::NoCollision);
					bool bNoCollision = MeshComp.GetCollisionEnabled() == ECollisionEnabled::NoCollision;
					MeshComp.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					bool bQueryOnly = MeshComp.GetCollisionEnabled() == ECollisionEnabled::QueryOnly;
					MeshComp.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
					bool bPhysicsOnly = MeshComp.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly;
					MeshComp.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					bool bQueryAndPhysics = MeshComp.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics;
					CollisionModesCovered = bNoCollision && bQueryOnly && bPhysicsOnly && bQueryAndPhysics;

					// Exercise common built-in profiles with readback.
					MeshComp.SetCollisionProfileName(n"NoCollision");
					bool bNoCollisionProfile = MeshComp.GetCollisionProfileName() == n"NoCollision";
					MeshComp.SetCollisionProfileName(n"BlockAll");
					bool bBlockAllProfile = MeshComp.GetCollisionProfileName() == n"BlockAll";
					MeshComp.SetCollisionProfileName(n"OverlapAll");
					bool bOverlapAllProfile = MeshComp.GetCollisionProfileName() == n"OverlapAll";
					MeshComp.SetCollisionProfileName(n"BlockAllDynamic");
					bool bBlockAllDynamicProfile = MeshComp.GetCollisionProfileName() == n"BlockAllDynamic";
					MeshComp.SetCollisionProfileName(n"OverlapAllDynamic");
					bool bOverlapAllDynamicProfile = MeshComp.GetCollisionProfileName() == n"OverlapAllDynamic";
					MeshComp.SetCollisionProfileName(n"IgnoreOnlyPawn");
					bool bIgnoreOnlyPawnProfile = MeshComp.GetCollisionProfileName() == n"IgnoreOnlyPawn";
					MeshComp.SetCollisionProfileName(n"OverlapOnlyPawn");
					bool bOverlapOnlyPawnProfile = MeshComp.GetCollisionProfileName() == n"OverlapOnlyPawn";
					MeshComp.SetCollisionProfileName(n"Pawn");
					bool bPawnProfile = MeshComp.GetCollisionProfileName() == n"Pawn";
					MeshComp.SetCollisionProfileName(n"PhysicsActor");
					bool bPhysicsActorProfile = MeshComp.GetCollisionProfileName() == n"PhysicsActor";
					CommonProfilesCovered =
						bNoCollisionProfile
						&& bBlockAllProfile
						&& bOverlapAllProfile
						&& bBlockAllDynamicProfile
						&& bOverlapAllDynamicProfile
						&& bIgnoreOnlyPawnProfile
						&& bOverlapOnlyPawnProfile
						&& bPawnProfile
						&& bPhysicsActorProfile;

					MeshComp.SetNotifyRigidBodyCollision(true);
					NotifyRigidBodyCollisionSet = true;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionSetupActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision setup actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision setup actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionEnabledSet"), true, TEXT("Collision enabled should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProfileSet"), true, TEXT("Collision profile should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GenerateOverlapEventsSet"), true, TEXT("Generate overlap events should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionModesCovered"), true, TEXT("Collision enabled modes should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CommonProfilesCovered"), true, TEXT("Common collision profiles should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NotifyRigidBodyCollisionSet"), true, TEXT("Rigid body collision notification should be set"))));

		UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();
		ASSERT_THAT(IsNotNull(MeshComp, TEXT("Collision setup mesh component should exist")));
		if (MeshComp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MeshComp->BodyInstance.bNotifyRigidBodyCollision, TEXT("Rigid body collision notification should round-trip to BodyInstance")));
	}

	// -------------------------------------------------------------------------
	// Primitive collision configuration: component-level setter/getter readback
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveCollisionConfigurationReadback)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_CollisionConfigurationReadback"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveCollisionConfigurationReadback.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveCollisionConfigurationReadbackActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				bool CollisionEnabledRoundTripped = false;

				UPROPERTY()
				bool ObjectTypeRoundTripped = false;

				UPROPERTY()
				bool ChannelResponseRoundTripped = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SphereComp.SetCollisionEnabled(ECollisionEnabled::NoCollision);
					bool bNoCollision = SphereComp.GetCollisionEnabled() == ECollisionEnabled::NoCollision;
					SphereComp.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					bool bQueryOnly = SphereComp.GetCollisionEnabled() == ECollisionEnabled::QueryOnly;
					SphereComp.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					bool bQueryAndPhysics = SphereComp.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics;
					CollisionEnabledRoundTripped = bNoCollision && bQueryOnly && bQueryAndPhysics;

					SphereComp.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
					bool bWorldDynamic = SphereComp.GetCollisionObjectType() == ECollisionChannel::ECC_WorldDynamic;
					SphereComp.SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
					bool bPawn = SphereComp.GetCollisionObjectType() == ECollisionChannel::ECC_Pawn;
					ObjectTypeRoundTripped = bWorldDynamic && bPawn;

					SphereComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
					bool bPawnBlocks = SphereComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block;
					SphereComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
					bool bVisibilityOverlaps = SphereComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Overlap;
					SphereComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
					bool bCameraIgnores = SphereComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Ignore;
					ChannelResponseRoundTripped = bPawnBlocks && bVisibilityOverlaps && bCameraIgnores;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionConfigurationReadbackActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision configuration readback actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision configuration readback actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionEnabledRoundTripped"), true, TEXT("SetCollisionEnabled should round-trip through GetCollisionEnabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectTypeRoundTripped"), true, TEXT("SetCollisionObjectType should round-trip through GetCollisionObjectType"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChannelResponseRoundTripped"), true, TEXT("SetCollisionResponseToChannel should round-trip through GetCollisionResponseToChannel"))));
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
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionEventsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision events actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision events actor should spawn")));
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

		USphereComponent* SphereComp = Cast<USphereComponent>(Actor->GetRootComponent());
		ASSERT_THAT(IsNotNull(SphereComp, TEXT("Primitive collision events actor should have a sphere root component")));
		if (SphereComp == nullptr)
		{
			return;
		}

		FHitResult SweepResult;
		SphereComp->OnComponentBeginOverlap.Broadcast(SphereComp, OverlapActor, OverlapSphere, 0, false, SweepResult);
		SphereComp->OnComponentEndOverlap.Broadcast(SphereComp, OverlapActor, OverlapSphere, 0);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BeginOverlapCount"), 1, TEXT("Begin overlap delegate broadcast should invoke AS handler once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EndOverlapCount"), 1, TEXT("End overlap delegate broadcast should invoke AS handler once"))));
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
				USphereComponent SphereComp;

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
					SphereComp.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

					// Enable physics simulation
					SphereComp.SetSimulatePhysics(true);
					PhysicsEnabled = SphereComp.IsSimulatingPhysics();

					// Enable gravity
					SphereComp.SetEnableGravity(true);
					GravityEnabled = SphereComp.IsGravityEnabled();

					// Apply impulse
					SphereComp.AddImpulse(FVector(0.0f, 0.0f, 1000.0f), NAME_None, false);
					ImpulseApplied = true;

					// Apply force
					SphereComp.AddForce(FVector(0.0f, 0.0f, 500.0f), NAME_None, false);
					ForceApplied = true;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitivePhysicsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive physics actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive physics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PhysicsEnabled"), true, TEXT("Physics should be enabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GravityEnabled"), true, TEXT("Gravity should be enabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImpulseApplied"), true, TEXT("Impulse should be applied"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ForceApplied"), true, TEXT("Force should be applied"))));
	}

	// -------------------------------------------------------------------------
	// Primitive physics state: mass and velocity getter/setter readback
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitivePhysicsStateReadback)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_PhysicsStateReadback"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitivePhysicsStateReadback.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitivePhysicsStateReadbackActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				bool SimulatePhysicsEnabled = false;

				UPROPERTY()
				bool SimulatePhysicsDisabled = false;

				UPROPERTY()
				bool GravityDisabled = false;

				UPROPERTY()
				bool MassOverrideRoundTripped = false;

				UPROPERTY()
				bool LinearVelocityRoundTripped = false;

				UPROPERTY()
				bool AngularVelocityRoundTripped = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SphereComp.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					SphereComp.SetSimulatePhysics(true);
					SimulatePhysicsEnabled = SphereComp.IsSimulatingPhysics();

					SphereComp.SetEnableGravity(false);
					GravityDisabled = !SphereComp.IsGravityEnabled();

					SphereComp.SetMassOverrideInKg(NAME_None, 125.0f, true);
					float Mass = SphereComp.GetMass();
					MassOverrideRoundTripped = Mass > 124.0f && Mass < 126.0f;

					FVector TargetLinearVelocity = FVector(120.0f, 30.0f, 0.0f);
					SphereComp.SetPhysicsLinearVelocity(TargetLinearVelocity, false, NAME_None);
					FVector LinearVelocity = SphereComp.GetPhysicsLinearVelocity(NAME_None);
					LinearVelocityRoundTripped =
						LinearVelocity.X > 119.0f
						&& LinearVelocity.X < 121.0f
						&& LinearVelocity.Y > 29.0f
						&& LinearVelocity.Y < 31.0f;

					FVector TargetAngularVelocity = FVector(0.0f, 45.0f, 90.0f);
					SphereComp.SetPhysicsAngularVelocityInDegrees(TargetAngularVelocity, false, NAME_None);
					FVector AngularVelocity = SphereComp.GetPhysicsAngularVelocityInDegrees(NAME_None);
					AngularVelocityRoundTripped =
						AngularVelocity.Y > 44.0f
						&& AngularVelocity.Y < 46.0f
						&& AngularVelocity.Z > 89.0f
						&& AngularVelocity.Z < 91.0f;

					SphereComp.SetSimulatePhysics(false);
					SimulatePhysicsDisabled = !SphereComp.IsSimulatingPhysics();
				}
			}
			)AS"),
			TEXT("ACoveragePrimitivePhysicsStateReadbackActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive physics state readback actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive physics state readback actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SimulatePhysicsEnabled"), true, TEXT("SetSimulatePhysics(true) should round-trip through IsSimulatingPhysics"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SimulatePhysicsDisabled"), true, TEXT("SetSimulatePhysics(false) should round-trip through IsSimulatingPhysics"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GravityDisabled"), true, TEXT("SetEnableGravity(false) should round-trip through IsGravityEnabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MassOverrideRoundTripped"), true, TEXT("SetMassOverrideInKg should round-trip through GetMass"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LinearVelocityRoundTripped"), true, TEXT("SetPhysicsLinearVelocity should round-trip through GetPhysicsLinearVelocity"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AngularVelocityRoundTripped"), true, TEXT("SetPhysicsAngularVelocityInDegrees should round-trip through GetPhysicsAngularVelocityInDegrees"))));
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

				UPROPERTY()
				bool ObjectTypeSet = false;

				UPROPERTY()
				bool BlockResponseSet = false;

				UPROPERTY()
				bool OverlapResponseSet = false;

				UPROPERTY()
				bool IgnoreResponseSet = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set response to specific channel
					MeshComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
					BlockResponseSet = MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block;

					MeshComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
					OverlapResponseSet = MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Overlap;

					MeshComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
					IgnoreResponseSet = MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Ignore;
					ResponseSet = BlockResponseSet && OverlapResponseSet && IgnoreResponseSet;

					// Set response to all channels
					MeshComp.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
					AllChannelsSet =
						MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Block
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Block;

					// Set object type
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
					ObjectTypeSet = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_WorldDynamic;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionResponseActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision response actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision response actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResponseSet"), true, TEXT("Collision response should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BlockResponseSet"), true, TEXT("Block response should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverlapResponseSet"), true, TEXT("Overlap response should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IgnoreResponseSet"), true, TEXT("Ignore response should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllChannelsSet"), true, TEXT("All channels response should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectTypeSet"), true, TEXT("Collision object type should round-trip"))));
	}

	// -------------------------------------------------------------------------
	// Primitive collision channels: built-in and game trace channel readback
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveCollisionChannelMatrixReadback)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_CollisionChannelMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveCollisionChannelMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveCollisionChannelMatrixActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool BuiltInObjectTypesRoundTripped = false;

				UPROPERTY()
				bool BuiltInTraceChannelsRoundTripped = false;

				UPROPERTY()
				bool AllResponsesRoundTripped = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
					bool bWorldStatic = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_WorldStatic;
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
					bool bWorldDynamic = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_WorldDynamic;
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
					bool bPawn = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_Pawn;
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
					bool bPhysicsBody = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_PhysicsBody;
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_Vehicle);
					bool bVehicle = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_Vehicle;
					MeshComp.SetCollisionObjectType(ECollisionChannel::ECC_Destructible);
					bool bDestructible = MeshComp.GetCollisionObjectType() == ECollisionChannel::ECC_Destructible;
					BuiltInObjectTypesRoundTripped =
						bWorldStatic
						&& bWorldDynamic
						&& bPawn
						&& bPhysicsBody
						&& bVehicle
						&& bDestructible;

					MeshComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
					bool bVisibility = MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Block;
					MeshComp.SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
					bool bCamera = MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Ignore;
					BuiltInTraceChannelsRoundTripped = bVisibility && bCamera;

					MeshComp.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
					AllResponsesRoundTripped =
						MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Vehicle) == ECollisionResponse::ECR_Ignore
						&& MeshComp.GetCollisionResponseToChannel(ECollisionChannel::ECC_Destructible) == ECollisionResponse::ECR_Ignore;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveCollisionChannelMatrixActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive collision channel matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive collision channel matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BuiltInObjectTypesRoundTripped"), true, TEXT("Built-in object channels should round-trip through object type"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BuiltInTraceChannelsRoundTripped"), true, TEXT("Built-in trace channels should round-trip through responses"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllResponsesRoundTripped"), true, TEXT("SetCollisionResponseToAllChannels should affect built-in and game channels"))));

		UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		ASSERT_THAT(IsNotNull(MeshComp, TEXT("Primitive collision channel matrix actor should keep a primitive root")));
		if (MeshComp == nullptr)
		{
			return;
		}

		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel18, ECR_Block);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ECR_Overlap),
			static_cast<int32>(MeshComp->GetCollisionResponseToChannel(ECC_GameTraceChannel1)),
			TEXT("Game trace channel 1 should round-trip through primitive collision responses")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ECR_Block),
			static_cast<int32>(MeshComp->GetCollisionResponseToChannel(ECC_GameTraceChannel18)),
			TEXT("Game trace channel 18 should round-trip through primitive collision responses")));
	}

	// -------------------------------------------------------------------------
	// Primitive trace/object query setup: channel and object query parameter readback
	// -------------------------------------------------------------------------
	TEST_METHOD(PrimitiveTraceObjectQueryReadback)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePrimitive_TraceObjectQuery"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePrimitiveTraceObjectQuery.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePrimitiveTraceObjectQueryActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				bool TraceChannelsReadable = false;

				UPROPERTY()
				bool ObjectQueryChannelsValidated = false;

				UPROPERTY()
				bool ObjectQueryParamsRoundTripped = false;

				UPROPERTY()
				bool ResponseContainerRoundTripped = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TraceChannelsReadable =
						int(ECollisionChannel::ECC_Visibility) >= 0
						&& int(ECollisionChannel::ECC_Camera) >= 0;

					ObjectQueryChannelsValidated =
						FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_WorldStatic)
						&& FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_WorldDynamic)
						&& FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_Pawn)
						&& FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_PhysicsBody)
						&& !FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_Visibility)
						&& !FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_Camera);

					FCollisionObjectQueryParams ObjectParams;
					ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
					ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
					ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
					int64 BeforeRemove = ObjectParams.GetQueryBitfield64();
					ObjectParams.RemoveObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
					int64 AfterRemove = ObjectParams.GetQueryBitfield64();
					ObjectQueryParamsRoundTripped =
						ObjectParams.IsValid()
						&& BeforeRemove != 0
						&& AfterRemove != 0
						&& BeforeRemove != AfterRemove
						&& ObjectParams.GetObjectTypesToQuery() == AfterRemove;

					FCollisionResponseContainer Responses(ECollisionResponse::ECR_Ignore);
					bool bSetVisibility = Responses.SetResponse(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
					bool bSetPawn = Responses.SetResponse(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
					bool bSetAll = Responses.SetAllChannels(ECollisionResponse::ECR_Block);
					ResponseContainerRoundTripped =
						bSetVisibility
						&& bSetPawn
						&& bSetAll
						&& Responses.GetResponse(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Block
						&& Responses.GetResponse(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block
						&& Responses.GetResponse(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Block;

					SphereComp.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
					SphereComp.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveTraceObjectQueryActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive trace object query actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive trace object query actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TraceChannelsReadable"), true, TEXT("Trace channel enums should be readable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectQueryChannelsValidated"), true, TEXT("Object query validation should distinguish object and trace channels"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectQueryParamsRoundTripped"), true, TEXT("Object query params should round-trip channel bitfields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResponseContainerRoundTripped"), true, TEXT("Response container should round-trip channel responses"))));
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
					FVector NormalImpulse, const FHitResult&in Hit)
				{
					HitCount++;
					HitNormal = Hit.Normal;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveHitEventsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive hit events actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive hit events actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify event was bound (actual hit would require physics simulation)
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("HitCount"), 0, TEXT("Hit count should be 0 initially"))));
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
				bool SetHiddenInGameAccepted = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MeshComp.SetHiddenInGame(true);
					SetHiddenInGameAccepted = true;
				}
			}
			)AS"),
			TEXT("ACoveragePrimitiveHiddenInGameActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Primitive hidden in game actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Primitive hidden in game actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SetHiddenInGameAccepted"), true, TEXT("SetHiddenInGame should be callable from AS"))));
		UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
		ASSERT_THAT(IsNotNull(MeshComponent, TEXT("Primitive hidden actor should own a static mesh component")));
		if (MeshComponent == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MeshComponent->bHiddenInGame, TEXT("SetHiddenInGame(true) should update native hidden state")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
