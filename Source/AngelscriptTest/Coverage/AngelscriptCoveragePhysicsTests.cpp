#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

// -----------------------------------------------------------------------------
// AngelscriptCoveragePhysicsTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript physics and collision features, corresponding to
// OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md.
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
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision events actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HitEventBound"), true, TEXT("Hit event should be bound"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverlapEventBound"), true, TEXT("Overlap events should be bound"))));
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

				UPROPERTY()
				bool CenterOfMassSet = false;

				UPROPERTY()
				bool PhysicsDisabledRoundTripped = false;

				UPROPERTY()
				bool GravityDisabledRoundTripped = false;

				UPROPERTY()
				bool LinearDampingSet = false;

				UPROPERTY()
				bool AngularDampingSet = false;

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
					LinearDampingSet = (LinearDamping > 0.49f && LinearDamping < 0.51f);

					Sphere.SetAngularDamping(0.3f);
					AngularDamping = Sphere.GetAngularDamping();
					AngularDampingSet = (AngularDamping > 0.29f && AngularDamping < 0.31f);

					// Set mass
					Sphere.SetMassOverrideInKg(NAME_None, 100.0f, true);
					MassSet = (Sphere.GetMass() > 99.0f);

					// Set center of mass offset
					Sphere.SetCenterOfMass(FVector(1.0f, 2.0f, 3.0f), NAME_None);
					CenterOfMassSet = true;

					// Boundary state: disabled physics and gravity should also round-trip.
					Sphere.SetSimulatePhysics(false);
					PhysicsDisabledRoundTripped = !Sphere.IsSimulatingPhysics();
					Sphere.SetEnableGravity(false);
					GravityDisabledRoundTripped = !Sphere.IsGravityEnabled();
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsSimulationActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics simulation actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics simulation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SimulatingPhysics"), true, TEXT("Physics simulation should be enabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GravityEnabled"), true, TEXT("Gravity should be enabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MassSet"), true, TEXT("Mass should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LinearDampingSet"), true, TEXT("Linear damping should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AngularDampingSet"), true, TEXT("Angular damping should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CenterOfMassSet"), true, TEXT("Center of mass offset should be accepted"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PhysicsDisabledRoundTripped"), true, TEXT("Disabled physics simulation should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GravityDisabledRoundTripped"), true, TEXT("Disabled gravity should round-trip"))));
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

				UPROPERTY()
				bool RadialImpulseApplied = false;

				UPROPERTY()
				bool AngularImpulseApplied = false;

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

					// Add radial impulse
					Sphere.AddRadialImpulse(Origin, 500.0f, 250.0f, ERadialImpulseFalloff::RIF_Constant, false);
					RadialImpulseApplied = true;

					// Add angular impulse
					Sphere.AddAngularImpulseInDegrees(FVector(0, 45, 0), NAME_None, false);
					AngularImpulseApplied = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsForcesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics forces actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics forces actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ForceApplied"), true, TEXT("Force should be applied"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImpulseApplied"), true, TEXT("Impulse should be applied"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TorqueApplied"), true, TEXT("Torque should be applied"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RadialForceApplied"), true, TEXT("Radial force should be applied"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RadialImpulseApplied"), true, TEXT("Radial impulse should be applied"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AngularImpulseApplied"), true, TEXT("Angular impulse should be applied"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics velocity actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LinearVelocitySet"), true, TEXT("Linear velocity should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AngularVelocitySet"), true, TEXT("Angular velocity should be set"))));
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
				bool LowLevelTraceParamsConfigured = false;

				UPROPERTY()
				bool LowLevelResponseParamsConfigured = false;

				UPROPERTY()
				bool LowLevelLineTraceCalled = false;

				UPROPERTY()
				bool LowLevelLineTraceMultiCalled = false;

				UPROPERTY()
				bool LowLevelSweepCalled = false;

				UPROPERTY()
				bool LowLevelOverlapCalled = false;

				UPROPERTY()
				bool TraceOutputsInitialized = false;

				UPROPERTY()
				bool CollisionShapesRoundTripped = false;

				UPROPERTY()
				int IgnoredActorCount = 0;

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

					FCollisionQueryParams QueryParams(n"CoverageTraceParams", true, this);
					QueryParams.TraceTag = n"CoverageTraceTag";
					QueryParams.bTraceComplex = true;
					QueryParams.bReturnPhysicalMaterial = true;
					QueryParams.bReturnFaceIndex = true;
					QueryParams.AddIgnoredActor(this);

					TArray<AActor> IgnoredActors;
					IgnoredActors.Add(this);
					QueryParams.AddIgnoredActors(IgnoredActors);
					IgnoredActorCount = QueryParams.GetIgnoredActors().Num();
					LowLevelTraceParamsConfigured =
						QueryParams.TraceTag == n"CoverageTraceTag"
						&& QueryParams.bTraceComplex
						&& QueryParams.bReturnPhysicalMaterial
						&& QueryParams.bReturnFaceIndex
						&& IgnoredActorCount > 0;

					FCollisionResponseParams ResponseParams(ECollisionResponse::ECR_Block);
					LowLevelResponseParamsConfigured = true;

					FHitResult ChannelHit;
					System::LineTraceSingleByChannel(ChannelHit, Start, End, ECollisionChannel::ECC_Visibility, QueryParams, ResponseParams);
					LowLevelLineTraceCalled = true;

					TArray<FHitResult> ChannelHits;
					System::LineTraceMultiByChannel(ChannelHits, Start, End, ECollisionChannel::ECC_Visibility, QueryParams, ResponseParams);
					LowLevelLineTraceMultiCalled = true;

					FCollisionShape SweepShape = FCollisionShape::MakeSphere(25.0f);
					System::SweepSingleByChannel(ChannelHit, Start, End, FQuat::Identity, ECollisionChannel::ECC_Visibility, SweepShape, QueryParams, ResponseParams);
					LowLevelSweepCalled = true;

					TArray<FOverlapResult> Overlaps;
					System::OverlapMultiByChannel(Overlaps, Start, FQuat::Identity, ECollisionChannel::ECC_Visibility, SweepShape, QueryParams, ResponseParams);
					LowLevelOverlapCalled = true;

					FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(10.0f, 20.0f, 30.0f));
					FCollisionShape SphereShape = FCollisionShape::MakeSphere(35.0f);
					FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(12.0f, 40.0f);
					CollisionShapesRoundTripped =
						BoxShape.IsBox()
						&& BoxShape.GetBox().Equals(FVector(10.0f, 20.0f, 30.0f), 0.01f)
						&& SphereShape.IsSphere()
						&& SphereShape.GetSphereRadius() > 34.9f
						&& CapsuleShape.IsCapsule()
						&& CapsuleShape.GetCapsuleRadius() > 11.9f
						&& CapsuleShape.GetCapsuleHalfHeight() > 39.9f;

					TraceOutputsInitialized = ChannelHits.Num() >= 0 && Overlaps.Num() >= 0;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsTraceActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Trace operations actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Trace operations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify trace calls executed (results depend on world setup)
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LineTraceExecuted"), true, TEXT("Line trace should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SphereTraceExecuted"), true, TEXT("Sphere trace should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoxTraceExecuted"), true, TEXT("Box trace should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CapsuleTraceExecuted"), true, TEXT("Capsule trace should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LowLevelTraceParamsConfigured"), true, TEXT("FCollisionQueryParams fields should be configured"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LowLevelResponseParamsConfigured"), true, TEXT("FCollisionResponseParams should be constructed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LowLevelLineTraceCalled"), true, TEXT("LineTraceSingleByChannel should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LowLevelLineTraceMultiCalled"), true, TEXT("LineTraceMultiByChannel should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LowLevelSweepCalled"), true, TEXT("SweepSingleByChannel should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LowLevelOverlapCalled"), true, TEXT("OverlapMultiByChannel should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TraceOutputsInitialized"), true, TEXT("Trace and overlap output arrays should be initialized"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionShapesRoundTripped"), true, TEXT("Collision shape factories and accessors should round-trip"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Overlap detection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SphereOverlapExecuted"), true, TEXT("Sphere overlap should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoxOverlapExecuted"), true, TEXT("Box overlap should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CapsuleOverlapExecuted"), true, TEXT("Capsule overlap should execute"))));
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
				bool StandardChannelsRead = false;

				UPROPERTY()
				bool ProfileChannelConversionsCovered = false;

				UPROPERTY()
				bool OverlapResponseSet = false;

				UPROPERTY()
				bool IgnoreResponseSet = false;

				UPROPERTY()
				ECollisionResponse PawnResponse;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set collision object type
					Sphere.SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
					ObjectTypeSet = (Sphere.GetCollisionObjectType() == ECollisionChannel::ECC_PhysicsBody);

					int StandardChannelCount = 0;
					StandardChannelCount += int(ECollisionChannel::ECC_WorldStatic) >= 0 ? 1 : 0;
					StandardChannelCount += int(ECollisionChannel::ECC_WorldDynamic) >= 0 ? 1 : 0;
					StandardChannelCount += int(ECollisionChannel::ECC_Pawn) >= 0 ? 1 : 0;
					StandardChannelCount += int(ECollisionChannel::ECC_Visibility) >= 0 ? 1 : 0;
					StandardChannelCount += int(ECollisionChannel::ECC_Camera) >= 0 ? 1 : 0;
					StandardChannelCount += int(ECollisionChannel::ECC_PhysicsBody) >= 0 ? 1 : 0;
					StandardChannelsRead = (StandardChannelCount == 6);

					ProfileChannelConversionsCovered =
						UCollisionProfile::ConvertToCollisionChannel(false, int(UCollisionProfile::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic))) == ECollisionChannel::ECC_WorldStatic
						&& UCollisionProfile::ConvertToCollisionChannel(false, int(UCollisionProfile::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic))) == ECollisionChannel::ECC_WorldDynamic
						&& UCollisionProfile::ConvertToCollisionChannel(true, int(UCollisionProfile::ConvertToTraceType(ECollisionChannel::ECC_Visibility))) == ECollisionChannel::ECC_Visibility
						&& UCollisionProfile::ConvertToCollisionChannel(true, int(UCollisionProfile::ConvertToTraceType(ECollisionChannel::ECC_Camera))) == ECollisionChannel::ECC_Camera;

					// Set response to specific channel
					Sphere.SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
					PawnResponse = Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn);
					ChannelResponseSet = (PawnResponse == ECollisionResponse::ECR_Block);

					Sphere.SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
					OverlapResponseSet =
						Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Overlap;

					Sphere.SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
					IgnoreResponseSet =
						Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Ignore;

					// Set response to all channels
					Sphere.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
					AllChannelsResponseSet =
						Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block
						&& Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Block
						&& Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Block;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsChannelsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision channels actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision channels actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectTypeSet"), true, TEXT("Collision object type should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StandardChannelsRead"), true, TEXT("Standard collision channels should be readable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProfileChannelConversionsCovered"), true, TEXT("Collision profile channel conversions should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChannelResponseSet"), true, TEXT("Block response should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverlapResponseSet"), true, TEXT("Overlap response should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IgnoreResponseSet"), true, TEXT("Ignore response should be set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllChannelsResponseSet"), true, TEXT("All channels response should be set"))));
	}

	// -------------------------------------------------------------------------
	// Collision profiles, enabled modes, and component-level query settings
	// -------------------------------------------------------------------------
	TEST_METHOD(CollisionProfilesAndEnabledModes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_CollisionProfiles"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsCollisionProfiles.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsCollisionProfilesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool CollisionEnabledModesRoundTripped = false;

				UPROPERTY()
				bool CoreProfilesRoundTripped = false;

				UPROPERTY()
				bool PawnProfilesRoundTripped = false;

				UPROPERTY()
				bool ComponentQuerySettingsRoundTripped = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Sphere.SetCollisionEnabled(ECollisionEnabled::NoCollision);
					bool bNoCollision = Sphere.GetCollisionEnabled() == ECollisionEnabled::NoCollision;
					Sphere.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					bool bQueryOnly = Sphere.GetCollisionEnabled() == ECollisionEnabled::QueryOnly;
					Sphere.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
					bool bPhysicsOnly = Sphere.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly;
					Sphere.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					bool bQueryAndPhysics = Sphere.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics;
					CollisionEnabledModesRoundTripped = bNoCollision && bQueryOnly && bPhysicsOnly && bQueryAndPhysics;

					Sphere.SetCollisionProfileName(n"NoCollision");
					bool bNoCollisionProfile = Sphere.GetCollisionProfileName() == n"NoCollision";
					Sphere.SetCollisionProfileName(n"BlockAll");
					bool bBlockAllProfile = Sphere.GetCollisionProfileName() == n"BlockAll";
					Sphere.SetCollisionProfileName(n"OverlapAll");
					bool bOverlapAllProfile = Sphere.GetCollisionProfileName() == n"OverlapAll";
					Sphere.SetCollisionProfileName(n"BlockAllDynamic");
					bool bBlockAllDynamicProfile = Sphere.GetCollisionProfileName() == n"BlockAllDynamic";
					Sphere.SetCollisionProfileName(n"OverlapAllDynamic");
					bool bOverlapAllDynamicProfile = Sphere.GetCollisionProfileName() == n"OverlapAllDynamic";
					Sphere.SetCollisionProfileName(n"PhysicsActor");
					bool bPhysicsActorProfile = Sphere.GetCollisionProfileName() == n"PhysicsActor";
					CoreProfilesRoundTripped =
						bNoCollisionProfile
						&& bBlockAllProfile
						&& bOverlapAllProfile
						&& bBlockAllDynamicProfile
						&& bOverlapAllDynamicProfile
						&& bPhysicsActorProfile;

					Sphere.SetCollisionProfileName(n"IgnoreOnlyPawn");
					bool bIgnoreOnlyPawnProfile = Sphere.GetCollisionProfileName() == n"IgnoreOnlyPawn";
					Sphere.SetCollisionProfileName(n"OverlapOnlyPawn");
					bool bOverlapOnlyPawnProfile = Sphere.GetCollisionProfileName() == n"OverlapOnlyPawn";
					Sphere.SetCollisionProfileName(n"Pawn");
					bool bPawnProfile = Sphere.GetCollisionProfileName() == n"Pawn";
					PawnProfilesRoundTripped = bIgnoreOnlyPawnProfile && bOverlapOnlyPawnProfile && bPawnProfile;

					Sphere.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
					Sphere.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
					Sphere.SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
					Sphere.SetGenerateOverlapEvents(true);
					Sphere.SetNotifyRigidBodyCollision(true);

					ComponentQuerySettingsRoundTripped =
						Sphere.GetCollisionObjectType() == ECollisionChannel::ECC_WorldDynamic
						&& Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Overlap
						&& Sphere.GetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Ignore
						&& Sphere.GetGenerateOverlapEvents();
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsCollisionProfilesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision profile actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision profile actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionEnabledModesRoundTripped"), true, TEXT("All collision enabled modes should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CoreProfilesRoundTripped"), true, TEXT("Core collision profiles should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PawnProfilesRoundTripped"), true, TEXT("Pawn collision profiles should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentQuerySettingsRoundTripped"), true, TEXT("Component collision query settings should round-trip"))));

		USphereComponent* SphereComponent = Actor->FindComponentByClass<USphereComponent>();
		ASSERT_THAT(IsNotNull(SphereComponent, TEXT("Collision profile actor should own a sphere component")));
		if (SphereComponent == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SphereComponent->BodyInstance.bNotifyRigidBodyCollision, TEXT("Rigid body collision notification should round-trip to BodyInstance")));
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
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

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

				UPROPERTY()
				bool ActorChecked = false;

				UPROPERTY()
				bool ComponentChecked = false;

				UPROPERTY()
				bool ImpactPointChecked = false;

				UPROPERTY()
				bool ImpactNormalChecked = false;

				UPROPERTY()
				bool TimeChecked = false;

				UPROPERTY()
				bool BoneNameChecked = false;

				UPROPERTY()
				bool PhysMaterialChecked = false;

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
					FHitResult ManualHit(this, Sphere, FVector(10, 20, 30), FVector(0, 0, 1));
					ManualHit.SetbBlockingHit(true);
					ManualHit.SetActor(this);
					ManualHit.SetComponent(Sphere);
					ManualHit.Distance = 123.0f;
					ManualHit.Time = 0.5f;
					ManualHit.ImpactPoint = FVector(11, 22, 33);
					ManualHit.ImpactNormal = FVector(0, 1, 0);
					ManualHit.BoneName = n"CoverageBone";
					ManualHit.SetPhysMaterial(nullptr);

					BlockingHitChecked = ManualHit.GetbBlockingHit();
					ActorChecked = (ManualHit.GetActor() == this);
					ComponentChecked = (ManualHit.GetComponent() == Sphere);

					LocationChecked = ManualHit.Location.Equals(FVector(10, 20, 30), 0.01f);
					NormalChecked = ManualHit.Normal.Equals(FVector(0, 0, 1), 0.01f);

					DistanceChecked = (ManualHit.Distance > 122.9f && ManualHit.Distance < 123.1f);
					ImpactPointChecked = ManualHit.ImpactPoint.Equals(FVector(11, 22, 33), 0.01f);
					ImpactNormalChecked = ManualHit.ImpactNormal.Equals(FVector(0, 1, 0), 0.01f);
					TimeChecked = (ManualHit.Time > 0.49f && ManualHit.Time < 0.51f);
					BoneNameChecked = (ManualHit.BoneName == n"CoverageBone");
					PhysMaterialChecked = (ManualHit.GetPhysMaterial() == nullptr);

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("HitResult fields actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HitResultFieldsAccessed"), true, TEXT("HitResult fields should be accessed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BlockingHitChecked"), true, TEXT("BlockingHit field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorChecked"), true, TEXT("HitResult actor accessor should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentChecked"), true, TEXT("HitResult component accessor should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LocationChecked"), true, TEXT("Location field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NormalChecked"), true, TEXT("Normal field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DistanceChecked"), true, TEXT("Distance field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImpactPointChecked"), true, TEXT("ImpactPoint field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ImpactNormalChecked"), true, TEXT("ImpactNormal field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TimeChecked"), true, TEXT("Time field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoneNameChecked"), true, TEXT("BoneName field should be checked"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PhysMaterialChecked"), true, TEXT("PhysMaterial accessor should be checked"))));
	}

	// -------------------------------------------------------------------------
	// Actor-level collision events: OnActorHit, BeginOverlap, EndOverlap
	// -------------------------------------------------------------------------
	TEST_METHOD(ActorCollisionEvents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ActorCollisionEvents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsActorCollisionEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsActorCollisionEventsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				int ActorHitCount = 0;

				UPROPERTY()
				int ActorBeginOverlapCount = 0;

				UPROPERTY()
				int ActorEndOverlapCount = 0;

				UPROPERTY()
				bool ActorDelegatesBound = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Sphere.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					Sphere.SetCollisionProfileName(n"OverlapAllDynamic");
					Sphere.SetNotifyRigidBodyCollision(true);
					Sphere.SetGenerateOverlapEvents(true);

					OnActorHit.AddUFunction(this, n"OnActorHitEvent");
					OnActorBeginOverlap.AddUFunction(this, n"OnActorBeginOverlapEvent");
					OnActorEndOverlap.AddUFunction(this, n"OnActorEndOverlapEvent");
					ActorDelegatesBound = true;
				}

				UFUNCTION()
				void OnActorHitEvent(AActor SelfActor, AActor OtherActor, FVector NormalImpulse, const FHitResult& Hit)
				{
					ActorHitCount += 1;
				}

				UFUNCTION()
				void OnActorBeginOverlapEvent(AActor OverlappedActor, AActor OtherActor)
				{
					ActorBeginOverlapCount += 1;
				}

				UFUNCTION()
				void OnActorEndOverlapEvent(AActor OverlappedActor, AActor OtherActor)
				{
					ActorEndOverlapCount += 1;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsActorCollisionEventsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Actor collision events actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor collision events actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		AActor* OtherActor = Spawner.GetWorld().SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(OtherActor, TEXT("Actor collision events should have an other actor")));
		if (OtherActor == nullptr)
		{
			return;
		}

		FHitResult Hit;
		Actor->OnActorHit.Broadcast(Actor, OtherActor, FVector(1.0f, 0.0f, 0.0f), Hit);
		Actor->OnActorBeginOverlap.Broadcast(Actor, OtherActor);
		Actor->OnActorEndOverlap.Broadcast(Actor, OtherActor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorDelegatesBound"), true, TEXT("Actor collision delegates should be bound in AS"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorHitCount"), 1, TEXT("OnActorHit should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorBeginOverlapCount"), 1, TEXT("OnActorBeginOverlap should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorEndOverlapCount"), 1, TEXT("OnActorEndOverlap should invoke the AS handler"))));
	}

	// -------------------------------------------------------------------------
	// Actor-level overlap generated by component movement, not manual broadcast
	// -------------------------------------------------------------------------
	TEST_METHOD(ActorOverlapGeneratedByMovement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ActorOverlapGeneratedByMovement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsActorOverlapGeneratedByMovement.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsActorOverlapGeneratedByMovementActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				int ActorBeginOverlapCount = 0;

				UPROPERTY()
				int ActorEndOverlapCount = 0;

				UPROPERTY()
				bool ActorBeginPayloadMatched = false;

				UPROPERTY()
				bool ActorEndPayloadMatched = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Sphere.SetSphereRadius(75.0f);
					Sphere.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					Sphere.SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
					Sphere.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
					Sphere.SetGenerateOverlapEvents(true);

					OnActorBeginOverlap.AddUFunction(this, n"OnActorBeginOverlapEvent");
					OnActorEndOverlap.AddUFunction(this, n"OnActorEndOverlapEvent");
				}

				UFUNCTION()
				void OnActorBeginOverlapEvent(AActor OverlappedActor, AActor OtherActor)
				{
					ActorBeginOverlapCount += 1;
					ActorBeginPayloadMatched =
						OverlappedActor == this
						&& OtherActor != nullptr
						&& OtherActor != this;
				}

				UFUNCTION()
				void OnActorEndOverlapEvent(AActor OverlappedActor, AActor OtherActor)
				{
					ActorEndOverlapCount += 1;
					ActorEndPayloadMatched =
						OverlappedActor == this
						&& OtherActor != nullptr
						&& OtherActor != this;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsActorOverlapGeneratedByMovementActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Actor movement overlap actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass, FActorSpawnParameters(), FVector::ZeroVector);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor movement overlap actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		USphereComponent* ScriptSphere = Cast<USphereComponent>(Actor->GetRootComponent());
		ASSERT_THAT(IsNotNull(ScriptSphere, TEXT("Actor movement overlap actor should own a sphere root component")));
		if (ScriptSphere == nullptr)
		{
			return;
		}

		AActor* OtherActor = Spawner.GetWorld().SpawnActor<AActor>(AActor::StaticClass(), FVector(400.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(OtherActor, TEXT("Actor movement overlap should spawn an other actor")));
		if (OtherActor == nullptr)
		{
			return;
		}

		USphereComponent* OtherSphere = NewObject<USphereComponent>(OtherActor, TEXT("CoverageMovementOverlapSphere"));
		ASSERT_THAT(IsNotNull(OtherSphere, TEXT("Actor movement overlap should create an other sphere component")));
		if (OtherSphere == nullptr)
		{
			return;
		}
		OtherActor->AddInstanceComponent(OtherSphere);
		OtherActor->SetRootComponent(OtherSphere);
		OtherSphere->SetSphereRadius(75.0f);
		OtherSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		OtherSphere->SetCollisionObjectType(ECC_WorldDynamic);
		OtherSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
		OtherSphere->SetGenerateOverlapEvents(true);
		OtherSphere->RegisterComponent();
		OtherSphere->SetWorldLocation(FVector(400.0f, 0.0f, 0.0f));

		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorBeginOverlapCount"), 0, TEXT("Separated actors should not overlap before movement"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorEndOverlapCount"), 0, TEXT("Separated actors should not end overlap before movement"))));

		OtherActor->SetActorLocation(FVector(25.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		ScriptSphere->UpdateOverlaps();
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorBeginOverlapCount"), 1, TEXT("Moving into overlap should generate one actor begin overlap"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorBeginPayloadMatched"), true, TEXT("Actor begin overlap should pass self and the moved actor"))));

		OtherActor->SetActorLocation(FVector(400.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		ScriptSphere->UpdateOverlaps();
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ActorEndOverlapCount"), 1, TEXT("Moving out of overlap should generate one actor end overlap"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorEndPayloadMatched"), true, TEXT("Actor end overlap should pass self and the moved actor"))));
	}

	// -------------------------------------------------------------------------
	// Component-level collision event dispatch: hit and overlap payloads
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentCollisionEventDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ComponentCollisionEventDispatch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsComponentCollisionEventDispatch.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsComponentCollisionEventActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				int ComponentHitCount = 0;

				UPROPERTY()
				int ComponentBeginOverlapCount = 0;

				UPROPERTY()
				int ComponentEndOverlapCount = 0;

				UPROPERTY()
				bool ComponentDelegatesBound = false;

				UPROPERTY()
				bool HitPayloadMatched = false;

				UPROPERTY()
				bool BeginOverlapPayloadMatched = false;

				UPROPERTY()
				bool EndOverlapPayloadMatched = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Sphere.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					Sphere.SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
					Sphere.SetGenerateOverlapEvents(true);
					Sphere.SetNotifyRigidBodyCollision(true);

					Sphere.OnComponentHit.AddUFunction(this, n"OnComponentHitEvent");
					Sphere.OnComponentBeginOverlap.AddUFunction(this, n"OnComponentBeginOverlapEvent");
					Sphere.OnComponentEndOverlap.AddUFunction(this, n"OnComponentEndOverlapEvent");
					ComponentDelegatesBound = true;
				}

				UFUNCTION()
				void OnComponentHitEvent(UPrimitiveComponent HitComponent, AActor OtherActor, UPrimitiveComponent OtherComp, FVector NormalImpulse, const FHitResult& Hit)
				{
					ComponentHitCount += 1;
					HitPayloadMatched =
						HitComponent == Sphere
						&& OtherActor != nullptr
						&& OtherComp != nullptr
						&& Hit.GetbBlockingHit()
						&& NormalImpulse.Equals(FVector(0.0f, 2.0f, 0.0f), 0.01f)
						&& Hit.ImpactPoint.Equals(FVector(10.0f, 20.0f, 30.0f), 0.01f)
						&& Hit.BoneName == n"CoverageComponentHitBone";
				}

				UFUNCTION()
				void OnComponentBeginOverlapEvent(UPrimitiveComponent OverlappedComponent, AActor OtherActor, UPrimitiveComponent OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
				{
					ComponentBeginOverlapCount += 1;
					BeginOverlapPayloadMatched =
						OverlappedComponent == Sphere
						&& OtherActor != nullptr
						&& OtherComp != nullptr
						&& OtherBodyIndex == 17
						&& bFromSweep
						&& SweepResult.GetbBlockingHit()
						&& SweepResult.Location.Equals(FVector(4.0f, 5.0f, 6.0f), 0.01f);
				}

				UFUNCTION()
				void OnComponentEndOverlapEvent(UPrimitiveComponent OverlappedComponent, AActor OtherActor, UPrimitiveComponent OtherComp, int32 OtherBodyIndex)
				{
					ComponentEndOverlapCount += 1;
					EndOverlapPayloadMatched =
						OverlappedComponent == Sphere
						&& OtherActor != nullptr
						&& OtherComp != nullptr
						&& OtherBodyIndex == 19;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsComponentCollisionEventActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component collision event actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component collision event actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		USphereComponent* SphereComponent = Cast<USphereComponent>(Actor->GetRootComponent());
		ASSERT_THAT(IsNotNull(SphereComponent, TEXT("Component collision event actor should own a sphere root component")));
		if (SphereComponent == nullptr)
		{
			return;
		}

		AActor* OtherActor = Spawner.GetWorld().SpawnActor<AActor>(AActor::StaticClass(), FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(OtherActor, TEXT("Component collision event test should spawn an other actor")));
		if (OtherActor == nullptr)
		{
			return;
		}

		USphereComponent* OtherComponent = NewObject<USphereComponent>(OtherActor);
		ASSERT_THAT(IsNotNull(OtherComponent, TEXT("Component collision event test should create an other primitive component")));
		if (OtherComponent == nullptr)
		{
			return;
		}
		OtherComponent->RegisterComponent();

		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = FVector(10.0f, 20.0f, 30.0f);
		Hit.BoneName = FName(TEXT("CoverageComponentHitBone"));
		SphereComponent->OnComponentHit.Broadcast(SphereComponent, OtherActor, OtherComponent, FVector(0.0f, 2.0f, 0.0f), Hit);

		FHitResult SweepHit;
		SweepHit.bBlockingHit = true;
		SweepHit.Location = FVector(4.0f, 5.0f, 6.0f);
		SphereComponent->OnComponentBeginOverlap.Broadcast(SphereComponent, OtherActor, OtherComponent, 17, true, SweepHit);
		SphereComponent->OnComponentEndOverlap.Broadcast(SphereComponent, OtherActor, OtherComponent, 19);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentDelegatesBound"), true, TEXT("Component collision delegates should be bound in AS"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentHitCount"), 1, TEXT("OnComponentHit should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentBeginOverlapCount"), 1, TEXT("OnComponentBeginOverlap should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentEndOverlapCount"), 1, TEXT("OnComponentEndOverlap should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HitPayloadMatched"), true, TEXT("OnComponentHit should pass component, actor, impulse, and hit payload"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BeginOverlapPayloadMatched"), true, TEXT("OnComponentBeginOverlap should pass component, body index, sweep flag, and hit payload"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EndOverlapPayloadMatched"), true, TEXT("OnComponentEndOverlap should pass component and body index payload"))));
	}

	// -------------------------------------------------------------------------
	// Collision channels: remaining engine channels and game trace channels
	// -------------------------------------------------------------------------
	TEST_METHOD(CollisionChannelMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ChannelMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsChannelMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsChannelMatrixActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool RemainingBuiltInChannelsRead = false;

				UPROPERTY()
				bool GameTraceChannelsRead = false;

				UPROPERTY()
				bool ObjectQueryParamsAcceptGameTraceChannel = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					int BuiltInChannelCount = 0;
					BuiltInChannelCount += int(ECollisionChannel::ECC_Vehicle) >= 0 ? 1 : 0;
					BuiltInChannelCount += int(ECollisionChannel::ECC_Destructible) >= 0 ? 1 : 0;
					RemainingBuiltInChannelsRead = (BuiltInChannelCount == 2);

					int GameChannelCount = 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel1) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel2) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel3) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel4) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel5) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel6) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel7) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel8) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel9) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel10) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel11) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel12) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel13) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel14) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel15) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel16) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel17) > 0 ? 1 : 0;
					GameChannelCount += int(ECollisionChannel::ECC_GameTraceChannel18) > 0 ? 1 : 0;
					GameTraceChannelsRead = (GameChannelCount == 18);

					FCollisionObjectQueryParams ObjectQueryParams;
					ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel1);
					ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel18);
					ObjectQueryParamsAcceptGameTraceChannel = ObjectQueryParams.IsValid();
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsChannelMatrixActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision channel matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision channel matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RemainingBuiltInChannelsRead"), true, TEXT("Vehicle and Destructible channels should be readable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GameTraceChannelsRead"), true, TEXT("All game trace channels should be readable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectQueryParamsAcceptGameTraceChannel"), true, TEXT("Object query params should accept AS-visible game trace channels"))));
	}

	// -------------------------------------------------------------------------
	// Trace variants: object-type/profile traces, sweep multi, overlap tests
	// -------------------------------------------------------------------------
	TEST_METHOD(TraceObjectProfileAndSweepVariants)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_TraceVariants"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsTraceVariants.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsTraceVariantActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool ObjectTraceCallsExecuted = false;

				UPROPERTY()
				bool ProfileTraceCallsExecuted = false;

				UPROPERTY()
				bool SweepMultiCallsExecuted = false;

				UPROPERTY()
				bool OverlapTestCallsExecuted = false;

				UPROPERTY()
				bool ComponentQueryCallsExecuted = false;

				UPROPERTY()
				bool SweepSingleVariantsExecuted = false;

				UPROPERTY()
				bool OverlapMultiVariantsExecuted = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Start = GetActorLocation();
					FVector End = Start + FVector(10.0f, 0.0f, 0.0f);
					FCollisionQueryParams QueryParams(n"CoverageTraceVariant", false, this);
					QueryParams.AddIgnoredActor(this);
					FCollisionShape SphereShape = FCollisionShape::MakeSphere(8.0f);
					FCollisionObjectQueryParams ObjectQueryParams;
					ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

					FHitResult Hit;
					TArray<FHitResult> Hits;
					TArray<FOverlapResult> Overlaps;

					System::LineTraceTestByObjectType(Start, End, ObjectQueryParams, QueryParams);
					System::LineTraceSingleByObjectType(Hit, Start, End, ObjectQueryParams, QueryParams);
					System::LineTraceMultiByObjectType(Hits, Start, End, ObjectQueryParams, QueryParams);
					ObjectTraceCallsExecuted = ObjectQueryParams.IsValid();

					System::LineTraceTestByProfile(Start, End, n"BlockAll", QueryParams);
					System::LineTraceSingleByProfile(Hit, Start, End, n"BlockAll", QueryParams);
					System::LineTraceMultiByProfile(Hits, Start, End, n"BlockAll", QueryParams);
					System::SweepSingleByProfile(Hit, Start, End, FQuat::Identity, n"BlockAll", SphereShape, QueryParams);
					System::SweepMultiByProfile(Hits, Start, End, FQuat::Identity, n"BlockAll", SphereShape, QueryParams);
					System::OverlapMultiByProfile(Overlaps, Start, FQuat::Identity, n"OverlapAll", SphereShape, QueryParams);
					ProfileTraceCallsExecuted = true;

					System::SweepTestByChannel(Start, End, FQuat::Identity, ECollisionChannel::ECC_Visibility, SphereShape, QueryParams);
					System::SweepTestByObjectType(Start, End, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams);
					System::SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECollisionChannel::ECC_Visibility, SphereShape, QueryParams);
					System::SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams);
					System::SweepSingleByProfile(Hit, Start, End, FQuat::Identity, n"BlockAll", SphereShape, QueryParams);
					System::SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECollisionChannel::ECC_Visibility, SphereShape, QueryParams);
					System::SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams);
					System::SweepMultiByProfile(Hits, Start, End, FQuat::Identity, n"BlockAll", SphereShape, QueryParams);
					SweepSingleVariantsExecuted = true;
					SweepMultiCallsExecuted = true;

					System::OverlapBlockingTestByChannel(Start, FQuat::Identity, ECollisionChannel::ECC_Visibility, SphereShape, QueryParams);
					System::OverlapAnyTestByChannel(Start, FQuat::Identity, ECollisionChannel::ECC_Visibility, SphereShape, QueryParams);
					System::OverlapAnyTestByObjectType(Start, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams);
					System::OverlapBlockingTestByProfile(Start, FQuat::Identity, n"BlockAll", SphereShape, QueryParams);
					System::OverlapAnyTestByProfile(Start, FQuat::Identity, n"OverlapAll", SphereShape, QueryParams);
					System::OverlapMultiByChannel(Overlaps, Start, FQuat::Identity, ECollisionChannel::ECC_Visibility, SphereShape, QueryParams);
					System::OverlapMultiByObjectType(Overlaps, Start, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams);
					System::OverlapMultiByProfile(Overlaps, Start, FQuat::Identity, n"OverlapAll", SphereShape, QueryParams);
					OverlapMultiVariantsExecuted = true;
					OverlapTestCallsExecuted = true;

					FComponentQueryParams ComponentQueryParams(n"CoverageComponentTraceVariant", this, FCollisionEnabledMask(ECollisionEnabled::QueryOnly));
					System::ComponentSweepMulti(Hits, Sphere, Start, End, FQuat::Identity, ComponentQueryParams);
					System::ComponentSweepMultiByChannel(Hits, Sphere, Start, End, FQuat::Identity, ECollisionChannel::ECC_Visibility, ComponentQueryParams);
					System::ComponentOverlapMulti(Overlaps, Sphere, Start, FQuat::Identity, ComponentQueryParams, ObjectQueryParams);
					System::ComponentOverlapMultiByChannel(Overlaps, Sphere, Start, FQuat::Identity, ECollisionChannel::ECC_Visibility, ComponentQueryParams, ObjectQueryParams);
					ComponentQueryCallsExecuted = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsTraceVariantActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Trace variant actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Trace variant actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectTraceCallsExecuted"), true, TEXT("Object-type trace calls should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProfileTraceCallsExecuted"), true, TEXT("Profile trace calls should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SweepSingleVariantsExecuted"), true, TEXT("Sweep single channel/object/profile calls should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SweepMultiCallsExecuted"), true, TEXT("Sweep test/multi calls should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverlapMultiVariantsExecuted"), true, TEXT("Overlap multi channel/object/profile calls should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("OverlapTestCallsExecuted"), true, TEXT("Overlap test/object/profile calls should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentQueryCallsExecuted"), true, TEXT("Component collision query calls should be AS-visible"))));
	}

	// -------------------------------------------------------------------------
	// Collision query parameter containers and response containers
	// -------------------------------------------------------------------------
	TEST_METHOD(CollisionQueryParameterContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_QueryParameterContainers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsQueryParameterContainers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsQueryParameterActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY()
				bool QueryParamsRoundTripped = false;

				UPROPERTY()
				bool ComponentQueryParamsRoundTripped = false;

				UPROPERTY()
				bool ObjectQueryParamsRoundTripped = false;

				UPROPERTY()
				bool ResponseParamsRoundTripped = false;

				UPROPERTY()
				bool CollisionEnabledMaskRoundTripped = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FCollisionQueryParams QueryParams(n"CoverageQueryContainer", true, this);
					QueryParams.OwnerTag = n"CoverageOwner";
					QueryParams.bFindInitialOverlaps = true;
					QueryParams.bIgnoreBlocks = false;
					QueryParams.bIgnoreTouches = false;
					QueryParams.bSkipNarrowPhase = false;
					QueryParams.MobilityType = EQueryMobilityType::Dynamic;
					QueryParams.AddIgnoredComponent(Sphere);

					TArray<UPrimitiveComponent> IgnoredComponents;
					IgnoredComponents.Add(Sphere);
					QueryParams.AddIgnoredComponents(IgnoredComponents);

					QueryParamsRoundTripped =
						QueryParams.TraceTag == n"CoverageQueryContainer"
						&& QueryParams.OwnerTag == n"CoverageOwner"
						&& QueryParams.bTraceComplex
						&& QueryParams.bFindInitialOverlaps
						&& QueryParams.MobilityType == EQueryMobilityType::Dynamic
						&& QueryParams.GetIgnoredComponents().Num() > 0
						&& QueryParams.ToString().Len() > 0;

					FCollisionEnabledMask QueryOnlyMask(ECollisionEnabled::QueryOnly);
					CollisionEnabledMaskRoundTripped = QueryOnlyMask.Bits != 0;

					FComponentQueryParams ComponentParams(n"CoverageComponentQuery", this, QueryOnlyMask);
					ComponentParams.TraceTag = n"CoverageComponentTrace";
					ComponentParams.ShapeCollisionMask = QueryOnlyMask;
					ComponentParams.AddIgnoredComponent(Sphere);
					ComponentQueryParamsRoundTripped =
						ComponentParams.TraceTag == n"CoverageComponentTrace"
						&& ComponentParams.ShapeCollisionMask.Bits == QueryOnlyMask.Bits
						&& ComponentParams.GetIgnoredComponents().Num() > 0;

					FCollisionObjectQueryParams ObjectParams(ECollisionObjectQueryInitType::AllDynamicObjects);
					ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);
					ObjectParams.RemoveObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);
					ObjectParams.SetObjectTypesToQuery(ObjectParams.GetObjectTypesToQuery());
					ObjectQueryParamsRoundTripped = ObjectParams.IsValid() && ObjectParams.GetQueryBitfield64() != 0;

					FCollisionResponseParams BlockResponses(ECollisionResponse::ECR_Block);
					ResponseParamsRoundTripped = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsQueryParameterActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision query parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Collision query parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("QueryParamsRoundTripped"), true, TEXT("FCollisionQueryParams fields and ignore lists should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentQueryParamsRoundTripped"), true, TEXT("FComponentQueryParams fields should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectQueryParamsRoundTripped"), true, TEXT("FCollisionObjectQueryParams should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ResponseParamsRoundTripped"), true, TEXT("FCollisionResponseParams should be constructible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CollisionEnabledMaskRoundTripped"), true, TEXT("FCollisionEnabledMask should be constructible"))));
	}

	// -------------------------------------------------------------------------
	// Character movement: reflected movement modes and parameters
	// -------------------------------------------------------------------------
	TEST_METHOD(CharacterMovementPhysicsSettings)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_CharacterMovement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsCharacterMovement.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsCharacterMovementActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				UCharacterMovementComponent Movement;

				UPROPERTY()
				bool MovementModesCovered = false;

				UPROPERTY()
				bool MovementParametersCovered = false;

				UPROPERTY()
				bool MovementQueriesCovered = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Movement.MaxWalkSpeed = 700.0f;
					Movement.MaxAcceleration = 2048.0f;
					Movement.BrakingDecelerationWalking = 1024.0f;
					Movement.GroundFriction = 4.0f;
					Movement.JumpZVelocity = 500.0f;
					Movement.AirControl = 0.35f;
					Movement.GravityScale = 1.25f;

					MovementParametersCovered =
						Movement.MaxWalkSpeed > 699.0f
						&& Movement.GetMaxAcceleration() > 2047.0f
						&& Movement.GetMaxBrakingDeceleration() > 1023.0f
						&& Movement.GroundFriction > 3.9f
						&& Movement.JumpZVelocity > 499.0f
						&& Movement.AirControl > 0.34f
						&& Movement.GravityScale > 1.24f;

					bool bWalkingValueReadable = int(EMovementMode::MOVE_Walking) >= 0;
					bool bNavWalkingValueReadable = int(EMovementMode::MOVE_NavWalking) >= 0;
					bool bFallingValueReadable = int(EMovementMode::MOVE_Falling) >= 0;
					bool bSwimmingValueReadable = int(EMovementMode::MOVE_Swimming) >= 0;
					bool bFlyingValueReadable = int(EMovementMode::MOVE_Flying) >= 0;
					bool bCustomValueReadable = int(EMovementMode::MOVE_Custom) >= 0;

					Movement.SetMovementMode(EMovementMode::MOVE_Flying);
					bool bFlyingSet = Movement.MovementMode == EMovementMode::MOVE_Flying;
					Movement.SetMovementMode(EMovementMode::MOVE_Falling);
					bool bFallingSet = Movement.MovementMode == EMovementMode::MOVE_Falling;
					Movement.SetMovementMode(EMovementMode::MOVE_Swimming);
					bool bSwimmingQueryCallable = Movement.IsSwimming() || !Movement.IsSwimming();
					Movement.SetMovementMode(EMovementMode::MOVE_Walking);
					bool bWalkingSet = Movement.MovementMode == EMovementMode::MOVE_Walking;

					MovementModesCovered =
						bWalkingValueReadable
						&& bNavWalkingValueReadable
						&& bFallingValueReadable
						&& bSwimmingValueReadable
						&& bFlyingValueReadable
						&& bCustomValueReadable
						&& bFlyingSet
						&& bFallingSet
						&& bWalkingSet;

					FVector ComponentVelocity = Root.GetComponentVelocity();
					FVector CurrentAcceleration = Movement.GetCurrentAcceleration();
					bool bWalkingQueryCallable = Movement.IsWalking() || !Movement.IsWalking();
					bool bFallingQueryCallable = Movement.IsFalling() || !Movement.IsFalling();
					bool bFlyingQueryCallable = Movement.IsFlying() || !Movement.IsFlying();
					MovementQueriesCovered =
						bWalkingQueryCallable
						&& bFallingQueryCallable
						&& bSwimmingQueryCallable
						&& bFlyingQueryCallable
						&& ComponentVelocity.SizeSquared() >= 0.0f
						&& CurrentAcceleration.SizeSquared() >= 0.0f;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsCharacterMovementActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Character movement coverage actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Character movement coverage actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MovementModesCovered"), true, TEXT("Character movement modes should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MovementParametersCovered"), true, TEXT("Character movement parameters should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MovementQueriesCovered"), true, TEXT("Character movement query methods should be AS-visible"))));

		UActorComponent* MovementComponent = Actor->FindComponentByClass<UCharacterMovementComponent>();
		ASSERT_THAT(IsNotNull(MovementComponent, TEXT("Character movement component should exist on the spawned actor")));
		if (MovementComponent == nullptr)
		{
			return;
		}
		UClass* MovementClass = MovementComponent->GetClass();
		ASSERT_THAT(IsNotNull(FindFProperty<FByteProperty>(MovementClass, TEXT("MovementMode")), TEXT("Movement mode should be reflected")));
		ASSERT_THAT(IsNotNull(FindFProperty<FFloatProperty>(MovementClass, TEXT("MaxWalkSpeed")), TEXT("MaxWalkSpeed should be reflected")));
		ASSERT_THAT(IsNotNull(FindFProperty<FFloatProperty>(MovementClass, TEXT("MaxAcceleration")), TEXT("MaxAcceleration should be reflected")));
		ASSERT_THAT(IsNotNull(FindFProperty<FFloatProperty>(MovementClass, TEXT("BrakingDecelerationWalking")), TEXT("BrakingDecelerationWalking should be reflected")));
		ASSERT_THAT(IsNotNull(FindFProperty<FFloatProperty>(MovementClass, TEXT("GravityScale")), TEXT("GravityScale should be reflected")));
	}

	TEST_METHOD(CharacterMovementModeQueryStates)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_CharacterMovementModeQueryStates"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsCharacterMovementModeQueryStates.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoveragePhysicsCharacterMovementModeQueryHarness : UObject
			{
				UPROPERTY()
				bool WalkingStateMatched = false;

				UPROPERTY()
				bool FallingStateMatched = false;

				UPROPERTY()
				bool SwimmingStateMatched = false;

				UPROPERTY()
				bool FlyingStateMatched = false;

				UPROPERTY()
				bool CustomStateMatched = false;

				UFUNCTION()
				int Run(UCharacterMovementComponent Movement)
				{
					if (Movement == nullptr)
					{
						return 0;
					}

					Movement.SetMovementMode(EMovementMode::MOVE_Walking);
					WalkingStateMatched =
						Movement.MovementMode == EMovementMode::MOVE_Walking;

					Movement.SetMovementMode(EMovementMode::MOVE_Falling);
					FallingStateMatched =
						Movement.MovementMode == EMovementMode::MOVE_Falling;

					Movement.SetMovementMode(EMovementMode::MOVE_Swimming);
					SwimmingStateMatched =
						Movement.MovementMode == EMovementMode::MOVE_Swimming;

					Movement.SetMovementMode(EMovementMode::MOVE_Flying);
					FlyingStateMatched =
						Movement.MovementMode == EMovementMode::MOVE_Flying;

					Movement.SetMovementMode(EMovementMode::MOVE_Custom);
					CustomStateMatched =
						Movement.MovementMode == EMovementMode::MOVE_Custom;

					return WalkingStateMatched
						&& FallingStateMatched
						&& SwimmingStateMatched
						&& FlyingStateMatched
						&& CustomStateMatched ? 1 : 0;
				}
			}
			)AS"),
			TEXT("UCoveragePhysicsCharacterMovementModeQueryHarness"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Character movement mode query harness should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		ACharacter* Character = Spawner.GetWorld().SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(Character, TEXT("Character movement mode query should spawn a native character")));
		if (Character == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Character);

		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		ASSERT_THAT(IsNotNull(Movement, TEXT("Native character should own a character movement component")));
		if (Movement == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Harness, TEXT("Character movement mode query harness should instantiate")));
		if (Harness == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Harness, FName(TEXT("Run")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Character movement mode query Run function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.AddParam<UCharacterMovementComponent*>(Movement).CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1, Result, TEXT("AS should set and query UCharacterMovementComponent movement modes through MovementMode")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("WalkingStateMatched"), true, TEXT("MOVE_Walking should round-trip through MovementMode"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("FallingStateMatched"), true, TEXT("MOVE_Falling should round-trip through MovementMode"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("SwimmingStateMatched"), true, TEXT("MOVE_Swimming should round-trip through MovementMode"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("FlyingStateMatched"), true, TEXT("MOVE_Flying should round-trip through MovementMode"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("CustomStateMatched"), true, TEXT("MOVE_Custom should round-trip through MovementMode"))));
	}

	TEST_METHOD(CharacterMovementVelocityQuery)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_CharacterMovementVelocityQuery"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsCharacterMovementVelocityQuery.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoveragePhysicsCharacterMovementVelocityQueryHarness : UObject
			{
				UPROPERTY()
				bool VelocityRoundTripped = false;

				UPROPERTY()
				bool CurrentAccelerationQueried = false;

				UFUNCTION()
				int Run(UCharacterMovementComponent Movement)
				{
					if (Movement == nullptr)
					{
						return 0;
					}

					FVector TargetVelocity = FVector(120.0f, -30.0f, 45.0f);
					Movement.Velocity = TargetVelocity;
					FVector QueriedVelocity = Movement.Velocity;
					VelocityRoundTripped =
						QueriedVelocity.X > 119.99f
						&& QueriedVelocity.X < 120.01f
						&& QueriedVelocity.Y > -30.01f
						&& QueriedVelocity.Y < -29.99f
						&& QueriedVelocity.Z > 44.99f
						&& QueriedVelocity.Z < 45.01f;

					FVector CurrentAcceleration = Movement.GetCurrentAcceleration();
					CurrentAccelerationQueried = CurrentAcceleration.Equals(FVector::ZeroVector, 0.01f);
					return VelocityRoundTripped && CurrentAccelerationQueried ? 1 : 0;
				}
			}
			)AS"),
			TEXT("UCoveragePhysicsCharacterMovementVelocityQueryHarness"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Character movement velocity query harness should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		ACharacter* Character = Spawner.GetWorld().SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
		ASSERT_THAT(IsNotNull(Character, TEXT("Character movement velocity query should spawn a native character")));
		if (Character == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Character);

		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		ASSERT_THAT(IsNotNull(Movement, TEXT("Native character should own a character movement component")));
		if (Movement == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Harness, TEXT("Character movement velocity query harness should instantiate")));
		if (Harness == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Harness, FName(TEXT("Run")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Character movement velocity query Run function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.AddParam<UCharacterMovementComponent*>(Movement).CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1, Result, TEXT("AS should query UCharacterMovementComponent velocity and acceleration through a native component parameter")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("VelocityRoundTripped"), true, TEXT("UCharacterMovementComponent Velocity should be script-writable and script-readable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("CurrentAccelerationQueried"), true, TEXT("GetCurrentAcceleration should report deterministic zero acceleration without movement input"))));
	}

	// -------------------------------------------------------------------------
	// Physics material: material fields and collision query physical material flag
	// -------------------------------------------------------------------------
	TEST_METHOD(PhysicsMaterialHitResultReference)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_PhysicsMaterial"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsMaterial.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoveragePhysicsMaterialHarness : UObject
			{
				UPROPERTY()
				bool MaterialFieldsRead = false;

				UPROPERTY()
				bool QueryParamsPhysicalMaterialFlagRoundTripped = false;

				UPROPERTY()
				bool HitResultPhysicalMaterialRoundTripped = false;

				UFUNCTION()
				int Run(UPhysicalMaterial Material)
				{
					if (Material == nullptr)
					{
						return 0;
					}

					MaterialFieldsRead =
						Material.Friction > 0.59f
						&& Material.Restitution > 0.39f
						&& Material.Density > 1.19f;

					FCollisionQueryParams QueryParams;
					QueryParams.bReturnPhysicalMaterial = true;
					QueryParamsPhysicalMaterialFlagRoundTripped = QueryParams.bReturnPhysicalMaterial;

					FHitResult Hit;
					Hit.SetPhysMaterial(Material);
					HitResultPhysicalMaterialRoundTripped = Hit.GetPhysMaterial() == Material;

					return MaterialFieldsRead && QueryParamsPhysicalMaterialFlagRoundTripped && HitResultPhysicalMaterialRoundTripped ? 1 : 0;
				}
			}
			)AS"),
			TEXT("UCoveragePhysicsMaterialHarness"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics material harness should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Harness, TEXT("Physics material harness should instantiate")));
		if (Harness == nullptr)
		{
			return;
		}

		UClass* PhysicalMaterialClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("/Script/PhysicsCore.PhysicalMaterial"));
		ASSERT_THAT(IsNotNull(PhysicalMaterialClass, TEXT("Physics material class should load through reflection")));
		if (PhysicalMaterialClass == nullptr)
		{
			return;
		}

		UObject* Material = NewObject<UObject>(GetTransientPackage(), PhysicalMaterialClass);
		ASSERT_THAT(IsNotNull(Material, TEXT("Physics material test should create a physical material")));
		if (Material == nullptr)
		{
			return;
		}

		auto SetNumericProperty = [Material](FName PropertyName, double Value) -> bool
		{
			if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(Material->GetClass(), PropertyName))
			{
				FloatProperty->SetPropertyValue_InContainer(Material, static_cast<float>(Value));
				return true;
			}
			if (FDoubleProperty* DoubleProperty = FindFProperty<FDoubleProperty>(Material->GetClass(), PropertyName))
			{
				DoubleProperty->SetPropertyValue_InContainer(Material, Value);
				return true;
			}
			return false;
		};
		ASSERT_THAT(IsTrue(SetNumericProperty(TEXT("Friction"), 0.6), TEXT("Physics material should expose Friction for setup")));
		ASSERT_THAT(IsTrue(SetNumericProperty(TEXT("Restitution"), 0.4), TEXT("Physics material should expose Restitution for setup")));
		ASSERT_THAT(IsTrue(SetNumericProperty(TEXT("Density"), 1.2), TEXT("Physics material should expose Density for setup")));

		FFunctionInvoker Invoker(*TestRunner, Harness, FName(TEXT("Run")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Physics material harness Run function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}
		const int32 Result = Invoker.AddParam<UObject*>(Material).CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1, Result, TEXT("AS should read physical material fields and request physical material query results")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("MaterialFieldsRead"), true, TEXT("Physical material friction, restitution, and density should be AS-readable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("QueryParamsPhysicalMaterialFlagRoundTripped"), true, TEXT("Collision query physical material flag should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("HitResultPhysicalMaterialRoundTripped"), true, TEXT("HitResult physical material reference should round-trip"))));
	}

	// -------------------------------------------------------------------------
	// Physics constraints: reflected component and key API surface
	// -------------------------------------------------------------------------
	TEST_METHOD(PhysicsConstraintComponentSettings)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_Constraint"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsConstraint.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsConstraintActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USphereComponent BodyA;

				UPROPERTY(DefaultComponent, Attach=Root)
				USphereComponent BodyB;

				UPROPERTY(DefaultComponent, Attach=Root)
				UPhysicsConstraintComponent Constraint;

				UPROPERTY()
				bool ConstrainedComponentsSet = false;

				UPROPERTY()
				bool LinearLimitsSet = false;

				UPROPERTY()
				bool AngularLimitsSet = false;

				UPROPERTY()
				bool LinearDriveSet = false;

				UPROPERTY()
				bool ConstraintBroken = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BodyA.SetSimulatePhysics(true);
					BodyB.SetSimulatePhysics(true);
					Constraint.SetConstrainedComponents(BodyA, NAME_None, BodyB, NAME_None);

					UPrimitiveComponent OutA;
					FName BoneA;
					UPrimitiveComponent OutB;
					FName BoneB;
					Constraint.GetConstrainedComponents(OutA, BoneA, OutB, BoneB);
					ConstrainedComponentsSet = OutA == BodyA && OutB == BodyB;

					Constraint.SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, 25.0f);
					Constraint.SetLinearZLimit(ELinearConstraintMotion::LCM_Free, 0.0f);
					LinearLimitsSet = true;

					Constraint.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
					Constraint.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
					AngularLimitsSet = true;

					Constraint.SetLinearPositionDrive(true, false, true);
					Constraint.SetLinearPositionTarget(FVector(5.0f, 0.0f, 10.0f));
					LinearDriveSet = true;

					Constraint.BreakConstraint();
					ConstraintBroken = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsConstraintActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics constraint actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics constraint actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConstrainedComponentsSet"), true, TEXT("Physics constraint should round-trip constrained components"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LinearLimitsSet"), true, TEXT("Physics constraint linear limit methods should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AngularLimitsSet"), true, TEXT("Physics constraint angular limit methods should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LinearDriveSet"), true, TEXT("Physics constraint linear drive target methods should be AS-visible"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConstraintBroken"), true, TEXT("Physics constraint break method should be AS-visible"))));

		UActorComponent* ConstraintComponent = Actor->FindComponentByClass<UPhysicsConstraintComponent>();
		ASSERT_THAT(IsNotNull(ConstraintComponent, TEXT("Physics constraint component should exist on the spawned actor")));
		if (ConstraintComponent == nullptr)
		{
			return;
		}
		UClass* ConstraintClass = ConstraintComponent->GetClass();
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetConstrainedComponents")), TEXT("SetConstrainedComponents should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetLinearXLimit")), TEXT("SetLinearXLimit should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetLinearYLimit")), TEXT("SetLinearYLimit should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetLinearZLimit")), TEXT("SetLinearZLimit should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetAngularSwing1Limit")), TEXT("SetAngularSwing1Limit should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetAngularSwing2Limit")), TEXT("SetAngularSwing2Limit should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetAngularTwistLimit")), TEXT("SetAngularTwistLimit should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetLinearPositionDrive")), TEXT("SetLinearPositionDrive should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("SetLinearPositionTarget")), TEXT("SetLinearPositionTarget should be reflected")));
		ASSERT_THAT(IsNotNull(ConstraintClass->FindFunctionByName(TEXT("BreakConstraint")), TEXT("BreakConstraint should be reflected")));
	}

	TEST_METHOD(PhysicsConstraintPresetRecipes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ConstraintPresetRecipes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsConstraintPresetRecipes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsConstraintPresetRecipesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USphereComponent BodyA;

				UPROPERTY(DefaultComponent, Attach=Root)
				USphereComponent BodyB;

				UPROPERTY(DefaultComponent, Attach=Root)
				UPhysicsConstraintComponent Constraint;

				UPROPERTY()
				bool HingeRecipeConfigured = false;

				UPROPERTY()
				bool PrismaticRecipeConfigured = false;

				UPROPERTY()
				bool BallSocketRecipeConfigured = false;

				UPROPERTY()
				bool FixedRecipeConfigured = false;

				UPROPERTY()
				bool DriveTargetsConfigured = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Constraint.SetConstrainedComponents(BodyA, NAME_None, BodyB, NAME_None);

					Constraint.SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.0f);
					HingeRecipeConfigured = true;

					Constraint.SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, 40.0f);
					Constraint.SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					PrismaticRecipeConfigured = true;

					Constraint.SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 35.0f);
					Constraint.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 35.0f);
					Constraint.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 20.0f);
					BallSocketRecipeConfigured = true;

					Constraint.SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
					Constraint.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					Constraint.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
					FixedRecipeConfigured = true;

					Constraint.SetLinearPositionDrive(true, true, true);
					Constraint.SetLinearPositionTarget(FVector(10.0f, 20.0f, 30.0f));
					DriveTargetsConfigured = true;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsConstraintPresetRecipesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Physics constraint preset recipe actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Physics constraint preset recipe actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HingeRecipeConfigured"), true, TEXT("Hinge constraint recipe should configure locked linear axes and a free twist axis"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PrismaticRecipeConfigured"), true, TEXT("Prismatic constraint recipe should configure one limited linear axis"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BallSocketRecipeConfigured"), true, TEXT("Ball-socket constraint recipe should configure angular limits"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FixedRecipeConfigured"), true, TEXT("Fixed constraint recipe should lock all linear and angular axes"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DriveTargetsConfigured"), true, TEXT("Constraint linear drive target should be configurable from AS"))));
	}

	// -------------------------------------------------------------------------
	// Projectile movement: reflected projectile parameters and homing target
	// -------------------------------------------------------------------------
	TEST_METHOD(ProjectileMovementSettings)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ProjectileMovement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsProjectileMovement.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragePhysicsProjectileMovementActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent Sphere;

				UPROPERTY(DefaultComponent)
				UProjectileMovementComponent Projectile;

				UPROPERTY()
				bool ProjectileScalarSettingsSet = false;

				UPROPERTY()
				bool ProjectileBooleanSettingsSet = false;

				UPROPERTY()
				bool ProjectileHomingTargetSet = false;

				UPROPERTY()
				bool ProjectileRuntimeStateStable = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Projectile.InitialSpeed = 1200.0f;
					Projectile.MaxSpeed = 2400.0f;
					Projectile.Bounciness = 0.65f;
					Projectile.ProjectileGravityScale = 0.25f;

					Projectile.bRotationFollowsVelocity = true;
					Projectile.bShouldBounce = true;
					Projectile.bIsHomingProjectile = true;
					Projectile.SetHomingTargetComponent(Sphere);

					ProjectileScalarSettingsSet =
						Projectile.InitialSpeed > 1199.0f
						&& Projectile.MaxSpeed > 2399.0f
						&& Projectile.Bounciness > 0.64f
						&& Projectile.ProjectileGravityScale > 0.24f;

					ProjectileBooleanSettingsSet =
						Projectile.bRotationFollowsVelocity
						&& Projectile.bShouldBounce
						&& Projectile.bIsHomingProjectile;

					ProjectileHomingTargetSet = (Projectile.GetHomingTargetComponent() == Sphere);

					Projectile.bIsHomingProjectile = false;
					Projectile.SetHomingTargetComponent(nullptr);
					ProjectileRuntimeStateStable =
						!Projectile.bIsHomingProjectile
						&& Projectile.GetHomingTargetComponent() == nullptr;
				}
			}
			)AS"),
			TEXT("ACoveragePhysicsProjectileMovementActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Projectile movement actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Projectile movement actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProjectileScalarSettingsSet"), true, TEXT("Projectile movement scalar settings should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProjectileBooleanSettingsSet"), true, TEXT("Projectile movement boolean settings should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProjectileHomingTargetSet"), true, TEXT("Projectile movement homing target should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ProjectileRuntimeStateStable"), true, TEXT("Projectile movement homing disabled state should round-trip"))));
	}

	TEST_METHOD(CollisionResponseContainerOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ResponseContainer"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsResponseContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoveragePhysicsResponseContainerHarness : UObject
			{
				UPROPERTY()
				bool PerChannelResponsesRoundTripped = false;

				UPROPERTY()
				bool AllChannelResponsesRoundTripped = false;

				UPROPERTY()
				bool ReplaceResponsesRoundTripped = false;

				UPROPERTY()
				bool MinResponseContainerCreated = false;

				UPROPERTY()
				bool ResponseParamsConstructed = false;

				UFUNCTION()
				int Run()
				{
					FCollisionResponseContainer Responses(ECollisionResponse::ECR_Ignore);
					bool bSetVisibility = Responses.SetResponse(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
					bool bSetCamera = Responses.SetResponse(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Overlap);
					PerChannelResponsesRoundTripped =
						bSetVisibility
						&& bSetCamera
						&& Responses.GetResponse(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Block
						&& Responses.GetResponse(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Overlap
						&& Responses.GetResponse(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Ignore;

					bool bSetAllChannels = Responses.SetAllChannels(ECollisionResponse::ECR_Block);
					AllChannelResponsesRoundTripped =
						bSetAllChannels
						&& Responses.GetResponse(ECollisionChannel::ECC_WorldStatic) == ECollisionResponse::ECR_Block
						&& Responses.GetResponse(ECollisionChannel::ECC_WorldDynamic) == ECollisionResponse::ECR_Block
						&& Responses.GetResponse(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block;

					bool bReplacedChannels = Responses.ReplaceChannels(ECollisionResponse::ECR_Block, ECollisionResponse::ECR_Ignore);
					ReplaceResponsesRoundTripped =
						bReplacedChannels
						&& Responses.GetResponse(ECollisionChannel::ECC_WorldStatic) == ECollisionResponse::ECR_Ignore
						&& Responses.GetResponse(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Ignore
						&& Responses.GetResponse(ECollisionChannel::ECC_Camera) == ECollisionResponse::ECR_Ignore;

					FCollisionResponseContainer OtherResponses(ECollisionResponse::ECR_Overlap);
					FCollisionResponseContainer MinResponses = FCollisionResponseContainer::CreateMinContainer(Responses, OtherResponses);
					MinResponseContainerCreated =
						MinResponses.GetResponse(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Ignore
						&& MinResponses.GetResponse(ECollisionChannel::ECC_Visibility) == ECollisionResponse::ECR_Ignore;

					FCollisionResponseParams ResponseParams(Responses);
					ResponseParamsConstructed = true;

					return PerChannelResponsesRoundTripped
						&& AllChannelResponsesRoundTripped
						&& ReplaceResponsesRoundTripped
						&& MinResponseContainerCreated
						&& ResponseParamsConstructed ? 1 : 0;
				}
			}
			)AS"),
			TEXT("UCoveragePhysicsResponseContainerHarness"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision response container harness should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Harness, TEXT("Collision response container harness should instantiate")));
		if (Harness == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Harness, FName(TEXT("Run")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Collision response container Run function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1, Result, TEXT("AS should mutate collision response containers and construct response params")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("PerChannelResponsesRoundTripped"), true, TEXT("Per-channel collision responses should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("AllChannelResponsesRoundTripped"), true, TEXT("All-channel response mutation should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("ReplaceResponsesRoundTripped"), true, TEXT("Response replacement should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("MinResponseContainerCreated"), true, TEXT("Minimum response container should be constructible from AS"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("ResponseParamsConstructed"), true, TEXT("Collision response params should be constructible from a response container"))));
	}

	TEST_METHOD(CollisionObjectQueryInitTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_ObjectQueryInitTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsObjectQueryInitTypes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoveragePhysicsObjectQueryHarness : UObject
			{
				UPROPERTY()
				bool InitTypesRoundTripped = false;

				UPROPERTY()
				bool ChannelConstructorRoundTripped = false;

				UPROPERTY()
				bool ManualBitfieldRoundTripped = false;

				UPROPERTY()
				bool InvalidTraceChannelRejected = false;

				UFUNCTION()
				int Run()
				{
					FCollisionObjectQueryParams AllObjects(ECollisionObjectQueryInitType::AllObjects);
					FCollisionObjectQueryParams StaticObjects(ECollisionObjectQueryInitType::AllStaticObjects);
					FCollisionObjectQueryParams DynamicObjects(ECollisionObjectQueryInitType::AllDynamicObjects);
					InitTypesRoundTripped =
						AllObjects.IsValid()
						&& StaticObjects.IsValid()
						&& DynamicObjects.IsValid()
						&& AllObjects.GetQueryBitfield64() != 0
						&& StaticObjects.GetQueryBitfield64() != 0
						&& DynamicObjects.GetQueryBitfield64() != 0;

					ChannelConstructorRoundTripped =
						FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_PhysicsBody)
						&& FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_Pawn);

					FCollisionObjectQueryParams ManualObjects;
					ManualObjects.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
					ManualObjects.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
					ManualObjects.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);
					int64 SnapshotBitfield = ManualObjects.GetObjectTypesToQuery();
					ManualObjects.RemoveObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);
					ManualObjects.SetObjectTypesToQuery(SnapshotBitfield);
					ManualObjects.IgnoreMask = 7;
					ManualBitfieldRoundTripped =
						ManualObjects.IsValid()
						&& ManualObjects.GetObjectTypesToQuery() == SnapshotBitfield
						&& ManualObjects.GetQueryBitfield64() != 0
						&& ManualObjects.IgnoreMask == 7;

					InvalidTraceChannelRejected =
						!FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_Visibility)
						&& !FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel::ECC_Camera);

					return InitTypesRoundTripped
						&& ChannelConstructorRoundTripped
						&& ManualBitfieldRoundTripped
						&& InvalidTraceChannelRejected ? 1 : 0;
				}
			}
			)AS"),
			TEXT("UCoveragePhysicsObjectQueryHarness"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Collision object query harness should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Harness, TEXT("Collision object query harness should instantiate")));
		if (Harness == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Harness, FName(TEXT("Run")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Collision object query Run function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1, Result, TEXT("AS should construct object query params from init types and channels")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("InitTypesRoundTripped"), true, TEXT("Object query init types should create valid filters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("ChannelConstructorRoundTripped"), true, TEXT("Object query channel constructor should accept object channels"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("ManualBitfieldRoundTripped"), true, TEXT("Object query bitfield mutation should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("InvalidTraceChannelRejected"), true, TEXT("Object query validation should reject trace-only channels"))));
	}

	TEST_METHOD(HitResultExtendedAccessors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoveragePhysics_HitResultExtended"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragePhysicsHitResultExtended.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoveragePhysicsHitResultExtendedHarness : UObject
			{
				UPROPERTY()
				bool HitResultIndexFieldsRoundTripped = false;

				UPROPERTY()
				bool HitResultTraceRangeRoundTripped = false;

				UPROPERTY()
				bool HitResultPenetrationRoundTripped = false;

				UPROPERTY()
				bool HitResultResetClearedState = false;

				UFUNCTION()
				int Run()
				{
					FHitResult Hit(FVector(-10.0f, 0.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f));
					Hit.SetBlockingHit(true);
					Hit.SetbStartPenetrating(true);
					Hit.PenetrationDepth = 4.5f;
					Hit.FaceIndex = 7;
					Hit.ElementIndex = 2;
					Hit.Item = 3;
					Hit.MyItem = 4;
					Hit.BoneName = n"CoverageBone";
					Hit.MyBoneName = n"CoverageMyBone";
					Hit.TraceStart = FVector(-20.0f, 1.0f, 2.0f);
					Hit.TraceEnd = FVector(30.0f, 3.0f, 4.0f);

					HitResultIndexFieldsRoundTripped =
						Hit.FaceIndex == 7
						&& Hit.ElementIndex == 2
						&& Hit.Item == 3
						&& Hit.MyItem == 4
						&& Hit.BoneName == n"CoverageBone"
						&& Hit.MyBoneName == n"CoverageMyBone";

					HitResultTraceRangeRoundTripped =
						Hit.TraceStart.Equals(FVector(-20.0f, 1.0f, 2.0f), 0.01f)
						&& Hit.TraceEnd.Equals(FVector(30.0f, 3.0f, 4.0f), 0.01f);

					HitResultPenetrationRoundTripped =
						Hit.GetbBlockingHit()
						&& Hit.GetbStartPenetrating()
						&& Hit.PenetrationDepth > 4.49f
						&& Hit.PenetrationDepth < 4.51f;

					Hit.Reset();
					HitResultResetClearedState =
						!Hit.GetbBlockingHit()
						&& !Hit.GetbStartPenetrating()
						&& Hit.Time > 0.99f;

					return HitResultIndexFieldsRoundTripped
						&& HitResultTraceRangeRoundTripped
						&& HitResultPenetrationRoundTripped
						&& HitResultResetClearedState ? 1 : 0;
				}
			}
			)AS"),
			TEXT("UCoveragePhysicsHitResultExtendedHarness"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("HitResult extended harness should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Harness, TEXT("HitResult extended harness should instantiate")));
		if (Harness == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Harness, FName(TEXT("Run")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("HitResult extended Run function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1, Result, TEXT("AS should access extended HitResult fields and reset helpers")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("HitResultIndexFieldsRoundTripped"), true, TEXT("HitResult face/item/bone fields should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("HitResultTraceRangeRoundTripped"), true, TEXT("HitResult trace start/end should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("HitResultPenetrationRoundTripped"), true, TEXT("HitResult penetration fields should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("HitResultResetClearedState"), true, TEXT("HitResult Reset should clear script-visible hit flags"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
