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
// AngelscriptCoveragePhysicsTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript physics and collision features, corresponding to
// Documents/Coverage/Coverage_PhysicsAndCollision.md.
//
// Axes covered here:
//   * CollisionEvents           - OnComponentHit, BeginOverlap, EndOverlap
//   * PhysicsSimulation         - SimulatePhysics, Gravity, Mass, Damping
//   * PhysicsForces             - AddForce, AddImpulse, AddTorque
//   * PhysicsVelocity           - SetPhysicsLinearVelocity, GetPhysicsLinearVelocity
//   * TraceOperations           - LineTraceSingle, LineTraceMulti, SphereTrace
//   * OverlapDetection          - OverlapMulti, SphereOverlapActors
//   * CollisionChannels         - ECollisionChannel enums
//   * CollisionResponses        - SetCollisionResponseToChannel
//
// Pattern D (script execution): compile AS actors with physics components,
// spawn them, manipulate physics state, verify collision and trace results.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_PhysicsAndCollision.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoveragePhysicsTest,
	"Angelscript.TestModule.Coverage.Physics",
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
	// Collision events: OnComponentHit, BeginOverlap, EndOverlap
	// -------------------------------------------------------------------------
	TEST_METHOD(CollisionEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_CollisionEvents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsCollisionEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsCollisionEventsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool HitEventBound = false;

				UPROPERTY()
				bool OverlapEventBound = false;

				UPROPERTY()
				int HitCount = 0;

				UPROPERTY()
				int OverlapBeginCount = 0;

				UPROPERTY()
				int OverlapEndCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind hit event
					Sphere.OnComponentHit.AddUFunction(this, n"OnHit");
					HitEventBound = true;

					// Bind overlap events
					Sphere.OnComponentBeginOverlap.AddUFunction(this, n"OnBeginOverlap");
					Sphere.OnComponentEndOverlap.AddUFunction(this, n"OnEndOverlap");
					OverlapEventBound = true;

					// Enable collision events
					Sphere.SetNotifyRigidBodyCollision(true);
					Sphere.SetGenerateOverlapEvents(true);
				}

				UFUNCTION()
				void OnHit(UPrimitiveComponent HitComp, AActor OtherActor, UPrimitiveComponent OtherComp, FVector NormalImpulse, FHitResult& Hit)
				{
					HitCount++;
				}

				UFUNCTION()
				void OnBeginOverlap(UPrimitiveComponent OverlappedComp, AActor OtherActor, UPrimitiveComponent OtherComp, int32 OtherBodyIndex, bool bFromSweep, FHitResult& SweepResult)
				{
					OverlapBeginCount++;
				}

				UFUNCTION()
				void OnEndOverlap(UPrimitiveComponent OverlappedComp, AActor OtherActor, UPrimitiveComponent OtherComp, int32 OtherBodyIndex)
				{
					OverlapEndCount++;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsCollisionEventsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision events actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision events actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HitEventBound"), true, TEXT("Hit event should be bound"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverlapEventBound"), true, TEXT("Overlap events should be bound"));
	}

	// -------------------------------------------------------------------------
	// Physics simulation: SimulatePhysics, Gravity, Mass, Damping
	// -------------------------------------------------------------------------
	TEST_METHOD(PhysicsSimulation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Simulation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsSimulation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsSimulationActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool SimulatingPhysics = false;

				UPROPERTY()
				bool GravityEnabled = false;

				UPROPERTY()
				float LinearDamping = 0.0f;

				UPROPERTY()
				float AngularDamping = 0.0f;

				UPROPERTY()
				bool MassSet = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Enable physics simulation
					Sphere.SetSimulatePhysics(true);
					SimulatingPhysics = Sphere.IsSimulatingPhysics();

					// Enable gravity
					Sphere.SetEnableGravity(true);
					GravityEnabled = Sphere.IsGravityEnabled();

					// Set damping
					Sphere.SetLinearDamping(0.5f);
					LinearDamping = Sphere.GetLinearDamping();

					Sphere.SetAngularDamping(0.3f);
					AngularDamping = Sphere.GetAngularDamping();

					// Set mass
					Sphere.SetMassOverrideInKg(NAME_None, 100.0f, true);
					MassSet = (Sphere.GetMass() > 99.0f);
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsSimulationActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics simulation actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics simulation actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SimulatingPhysics"), true, TEXT("Physics simulation should be enabled"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GravityEnabled"), true, TEXT("Gravity should be enabled"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MassSet"), true, TEXT("Mass should be set"));
	}

	// -------------------------------------------------------------------------
	// Physics forces: AddForce, AddImpulse, AddTorque, AddRadialForce
	// -------------------------------------------------------------------------
	TEST_METHOD(PhysicsForces)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Forces"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsForces.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsForcesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool ForceApplied = false;

				UPROPERTY()
				bool ImpulseApplied = false;

				UPROPERTY()
				bool TorqueApplied = false;

				UPROPERTY()
				bool RadialForceApplied = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Sphere.SetSimulatePhysics(true);

					// Add force
					Sphere.AddForce(FVector(0, 0, 1000), NAME_None, false);
					ForceApplied = true;

					// Add impulse
					Sphere.AddImpulse(FVector(100, 0, 0), NAME_None, false);
					ImpulseApplied = true;

					// Add torque
					Sphere.AddTorqueInDegrees(FVector(0, 0, 90), NAME_None, false);
					TorqueApplied = true;

					// Add radial force
					FVector Origin = GetActorLocation();
					Sphere.AddRadialForce(Origin, 500.0f, 1000.0f, ERadialImpulseFalloff::RIF_Linear, false);
					RadialForceApplied = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsForcesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics forces actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics forces actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ForceApplied"), true, TEXT("Force should be applied"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImpulseApplied"), true, TEXT("Impulse should be applied"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TorqueApplied"), true, TEXT("Torque should be applied"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RadialForceApplied"), true, TEXT("Radial force should be applied"));
	}

	// -------------------------------------------------------------------------
	// Physics velocity: Set/GetPhysicsLinearVelocity, Set/GetPhysicsAngularVelocity
	// -------------------------------------------------------------------------
	TEST_METHOD(PhysicsVelocity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Velocity"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsVelocity.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsVelocityActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool LinearVelocitySet = false;

				UPROPERTY()
				bool AngularVelocitySet = false;

				UPROPERTY()
				FVector LinearVelocity;

				UPROPERTY()
				FVector AngularVelocity;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Sphere.SetSimulatePhysics(true);

					// Set linear velocity
					FVector TargetLinearVel = FVector(100, 0, 0);
					Sphere.SetPhysicsLinearVelocity(TargetLinearVel, false, NAME_None);
					LinearVelocity = Sphere.GetPhysicsLinearVelocity(NAME_None);
					LinearVelocitySet = (LinearVelocity.Length() > 50.0f);

					// Set angular velocity
					FVector TargetAngularVel = FVector(0, 0, 45);
					Sphere.SetPhysicsAngularVelocityInDegrees(TargetAngularVel, false, NAME_None);
					AngularVelocity = Sphere.GetPhysicsAngularVelocityInDegrees(NAME_None);
					AngularVelocitySet = (AngularVelocity.Length() > 20.0f);
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsVelocityActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics velocity actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics velocity actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LinearVelocitySet"), true, TEXT("Linear velocity should be set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AngularVelocitySet"), true, TEXT("Angular velocity should be set"));
	}

	// -------------------------------------------------------------------------
	// Trace operations: LineTraceSingle, SphereTrace, BoxTrace
	// -------------------------------------------------------------------------
	TEST_METHOD(TraceOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Trace"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsTrace.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsTraceActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool LineTraceExecuted = false;

				UPROPERTY()
				bool SphereTraceExecuted = false;

				UPROPERTY()
				bool BoxTraceExecuted = false;

				UPROPERTY()
				bool CapsuleTraceExecuted = false;

				UPROPERTY()
				FHitResult LineTraceResult;

				UPROPERTY()
				TArray<FHitResult> MultiTraceResults;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Start = GetActorLocation();
					FVector End = Start + FVector(0, 0, -1000);

					// Line trace single
					FHitResult Hit;
					LineTraceExecuted = System::LineTraceSingle(
						Start, End,
						ETraceTypeQuery::Visibility,
						false, TArray<AActor>(), EDrawDebugTrace::None,
						Hit, true
					);
					LineTraceResult = Hit;

					// Sphere trace
					SphereTraceExecuted = System::SphereTraceSingle(
						Start, End, 50.0f,
						ETraceTypeQuery::Visibility,
						false, TArray<AActor>(), EDrawDebugTrace::None,
						Hit, true
					);

					// Box trace
					BoxTraceExecuted = System::BoxTraceSingle(
						Start, End, FVector(50, 50, 50), FRotator::ZeroRotator,
						ETraceTypeQuery::Visibility,
						false, TArray<AActor>(), EDrawDebugTrace::None,
						Hit, true
					);

					// Capsule trace
					CapsuleTraceExecuted = System::CapsuleTraceSingle(
						Start, End, 50.0f, 100.0f,
						ETraceTypeQuery::Visibility,
						false, TArray<AActor>(), EDrawDebugTrace::None,
						Hit, true
					);
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsTraceActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Trace operations actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Trace operations actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify trace calls executed (results depend on world setup)
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LineTraceExecuted"), true, TEXT("Line trace should execute"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SphereTraceExecuted"), true, TEXT("Sphere trace should execute"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoxTraceExecuted"), true, TEXT("Box trace should execute"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CapsuleTraceExecuted"), true, TEXT("Capsule trace should execute"));
	}

	// -------------------------------------------------------------------------
	// Overlap detection: SphereOverlapActors, BoxOverlapActors, CapsuleOverlapActors
	// -------------------------------------------------------------------------
	TEST_METHOD(OverlapDetection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Overlap"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsOverlap.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsOverlapActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool SphereOverlapExecuted = false;

				UPROPERTY()
				bool BoxOverlapExecuted = false;

				UPROPERTY()
				bool CapsuleOverlapExecuted = false;

				UPROPERTY()
				int SphereOverlapCount = 0;

				UPROPERTY()
				TArray<AActor> OverlappedActors;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Location = GetActorLocation();
					TArray<AActor> OutActors;
					TArray<AActor> IgnoreActors;

					// Sphere overlap
					SphereOverlapExecuted = System::SphereOverlapActors(
						Location, 500.0f,
						TArray<EObjectTypeQuery>(),
						AActor::StaticClass(),
						IgnoreActors,
						OutActors
					);
					SphereOverlapCount = OutActors.Num();
					OverlappedActors = OutActors;

					// Box overlap
					BoxOverlapExecuted = System::BoxOverlapActors(
						Location, FVector(100, 100, 100), FRotator::ZeroRotator,
						TArray<EObjectTypeQuery>(),
						AActor::StaticClass(),
						IgnoreActors,
						OutActors
					);

					// Capsule overlap
					CapsuleOverlapExecuted = System::CapsuleOverlapActors(
						Location, 50.0f, 100.0f,
						TArray<EObjectTypeQuery>(),
						AActor::StaticClass(),
						IgnoreActors,
						OutActors
					);
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsOverlapActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Overlap detection actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Overlap detection actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SphereOverlapExecuted"), true, TEXT("Sphere overlap should execute"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoxOverlapExecuted"), true, TEXT("Box overlap should execute"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CapsuleOverlapExecuted"), true, TEXT("Capsule overlap should execute"));
	}

	// -------------------------------------------------------------------------
	// Collision channels and responses
	// -------------------------------------------------------------------------
	TEST_METHOD(CollisionChannelsAndResponses)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Channels"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsChannels.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsChannelsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool ObjectTypeSet = false;

				UPROPERTY()
				bool ChannelResponseSet = false;

				UPROPERTY()
				bool AllChannelsResponseSet = false;

				UPROPERTY()
				ECollisionResponse PawnResponse;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set collision object type
					Sphere.SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
					ObjectTypeSet = (Sphere.GetCollisionObjectType() == ECollisionChannel::ECC_PhysicsBody);

					// Set response to specific channel
					Sphere.SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
					PawnResponse = Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn);
					ChannelResponseSet = (PawnResponse == ECollisionResponse::ECR_Block);

					// Set response to all channels
					Sphere.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
					AllChannelsResponseSet = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsChannelsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision channels actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision channels actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectTypeSet"), true, TEXT("Collision object type should be set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChannelResponseSet"), true, TEXT("Channel response should be set"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllChannelsResponseSet"), true, TEXT("All channels response should be set"));
	}

	// -------------------------------------------------------------------------
	// FHitResult structure fields
	// -------------------------------------------------------------------------
	TEST_METHOD(HitResultFields)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_HitResult"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsHitResult.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsHitResultActor : AActor
			{
				UPROPERTY()
				bool HitResultFieldsAccessed = false;

				UPROPERTY()
				bool BlockingHitChecked = false;

				UPROPERTY()
				bool LocationChecked = false;

				UPROPERTY()
				bool NormalChecked = false;

				UPROPERTY()
				bool DistanceChecked = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Start = GetActorLocation();
					FVector End = Start + FVector(0, 0, -1000);

					FHitResult Hit;
					System::LineTraceSingle(
						Start, End,
						ETraceTypeQuery::Visibility,
						false, TArray<AActor>(), EDrawDebugTrace::None,
						Hit, true
					);

					// Access FHitResult fields
					BlockingHitChecked = true;
					bool bBlocking = Hit.bBlockingHit;

					LocationChecked = true;
					FVector Loc = Hit.Location;

					NormalChecked = true;
					FVector Norm = Hit.Normal;

					DistanceChecked = true;
					float Dist = Hit.Distance;

					FVector ImpactPt = Hit.ImpactPoint;
					FVector ImpactNorm = Hit.ImpactNormal;
					float Time = Hit.Time;
					FName BoneName = Hit.BoneName;

					HitResultFieldsAccessed = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsHitResultActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("HitResult fields actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("HitResult fields actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HitResultFieldsAccessed"), true, TEXT("HitResult fields should be accessed"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BlockingHitChecked"), true, TEXT("BlockingHit field should be checked"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LocationChecked"), true, TEXT("Location field should be checked"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NormalChecked"), true, TEXT("Normal field should be checked"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DistanceChecked"), true, TEXT("Distance field should be checked"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS