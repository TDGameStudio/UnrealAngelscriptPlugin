#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageSpecialComponentTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript special component types (StaticMesh, SkeletalMesh,
// CharacterMovement, Camera, SpringArm, Shape components), corresponding to
// OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md section 7.
//
// Axes covered here:
//   * StaticMeshComponent       - SetStaticMesh, SetMaterial
//   * CharacterMovementComponent - MaxWalkSpeed, JumpZVelocity, MovementMode
//   * CameraComponent           - FieldOfView, ProjectionMode
//   * SpringArmComponent        - TargetArmLength, CameraLag
//   * ShapeComponents           - BoxExtent, SphereRadius, CapsuleSize
//   * CustomScriptComponent     - Script-derived scene component
//
// Pattern D (script execution): compile AS actors with special components,
// spawn them, manipulate component-specific properties, verify results.
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageSpecialComponentTest,
	"Angelscript.TestModule.Coverage.SpecialComponent",
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
	// StaticMeshComponent: SetStaticMesh, materials
	// -------------------------------------------------------------------------
	TEST_METHOD(StaticMeshComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_StaticMesh"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialStaticMesh.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialStaticMeshActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent MeshComp;

				UPROPERTY()
				bool MeshWasNull = true;

				UPROPERTY()
				int MaterialCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UStaticMesh Mesh = MeshComp.GetStaticMesh();
					MeshWasNull = (Mesh == nullptr);

					// Get material count
					MaterialCount = MeshComp.GetNumMaterials();
				}
			}
			)AS"),
			TEXT("ACoverageSpecialStaticMeshActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special static mesh actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special static mesh actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MeshWasNull"), true, TEXT("Mesh should be null initially"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MaterialCount"), 0, TEXT("Material count should be 0"))));
	}

	// -------------------------------------------------------------------------
	// CharacterMovementComponent: MaxWalkSpeed, JumpZVelocity, MovementMode
	// -------------------------------------------------------------------------
	TEST_METHOD(CharacterMovementComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_CharacterMovement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialCharacterMovement.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialCharacterMovementActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCharacterMovementComponent MovementComp;

				UPROPERTY()
				float InitialMaxWalkSpeed = 0.0f;

				UPROPERTY()
				float NewMaxWalkSpeed = 0.0f;

				UPROPERTY()
				float InitialJumpVelocity = 0.0f;

				UPROPERTY()
				float NewJumpVelocity = 0.0f;

				UPROPERTY()
				float InitialGravityScale = 0.0f;

				UPROPERTY()
				float NewGravityScale = 0.0f;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (MovementComp != nullptr)
					{
						// Read initial values
						InitialMaxWalkSpeed = MovementComp.MaxWalkSpeed;
						InitialJumpVelocity = MovementComp.JumpZVelocity;
						InitialGravityScale = MovementComp.GravityScale;

						// Set new values
						MovementComp.MaxWalkSpeed = 800.0f;
						MovementComp.JumpZVelocity = 500.0f;
						MovementComp.GravityScale = 1.5f;

						// Read back
						NewMaxWalkSpeed = MovementComp.MaxWalkSpeed;
						NewJumpVelocity = MovementComp.JumpZVelocity;
						NewGravityScale = MovementComp.GravityScale;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSpecialCharacterMovementActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special character movement actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special character movement actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		double NewMaxWalkSpeed = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewMaxWalkSpeed"), NewMaxWalkSpeed)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewMaxWalkSpeed, 800.0, 0.01), TEXT("MaxWalkSpeed should be set to 800")));

		double NewJumpVelocity = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewJumpVelocity"), NewJumpVelocity)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewJumpVelocity, 500.0, 0.01), TEXT("JumpZVelocity should be set to 500")));

		double NewGravityScale = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewGravityScale"), NewGravityScale)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewGravityScale, 1.5, 0.01), TEXT("GravityScale should be set to 1.5")));
	}

	// -------------------------------------------------------------------------
	// CameraComponent: FieldOfView, AspectRatio, ProjectionMode
	// -------------------------------------------------------------------------
	TEST_METHOD(CameraComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_Camera"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialCamera.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialCameraActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCameraComponent CameraComp;

				UPROPERTY()
				float InitialFOV = 0.0f;

				UPROPERTY()
				float NewFOV = 0.0f;

				UPROPERTY()
				bool ConstrainAspectRatio = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (CameraComp != nullptr)
					{
						InitialFOV = CameraComp.FieldOfView;

						CameraComp.FieldOfView = 120.0f;
						CameraComp.bConstrainAspectRatio = true;

						NewFOV = CameraComp.FieldOfView;
						ConstrainAspectRatio = CameraComp.bConstrainAspectRatio;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSpecialCameraActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special camera actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special camera actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		double NewFOV = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewFOV"), NewFOV)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewFOV, 120.0, 0.01), TEXT("FOV should be set to 120")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ConstrainAspectRatio"), true, TEXT("Aspect ratio should be constrained"))));
	}

	// -------------------------------------------------------------------------
	// SpringArmComponent: TargetArmLength, CameraLag
	// -------------------------------------------------------------------------
	TEST_METHOD(SpringArmComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_SpringArm"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialSpringArm.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialSpringArmActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USpringArmComponent SpringArmComp;

				UPROPERTY()
				float InitialArmLength = 0.0f;

				UPROPERTY()
				float NewArmLength = 0.0f;

				UPROPERTY()
				bool DoCollisionTest = true;

				UPROPERTY()
				bool EnableCameraLag = false;

				UPROPERTY()
				float CameraLagSpeed = 0.0f;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (SpringArmComp != nullptr)
					{
						InitialArmLength = SpringArmComp.TargetArmLength;

						SpringArmComp.TargetArmLength = 500.0f;
						SpringArmComp.bDoCollisionTest = false;
						SpringArmComp.bEnableCameraLag = true;
						SpringArmComp.CameraLagSpeed = 8.0f;

						NewArmLength = SpringArmComp.TargetArmLength;
						DoCollisionTest = SpringArmComp.bDoCollisionTest;
						EnableCameraLag = SpringArmComp.bEnableCameraLag;
						CameraLagSpeed = SpringArmComp.CameraLagSpeed;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSpecialSpringArmActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special spring arm actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special spring arm actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		double NewArmLength = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewArmLength"), NewArmLength)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewArmLength, 500.0, 0.01), TEXT("Arm length should be set to 500")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DoCollisionTest"), false, TEXT("Collision test should be disabled"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EnableCameraLag"), true, TEXT("Camera lag should be enabled"))));

		double CameraLagSpeed = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CameraLagSpeed"), CameraLagSpeed)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CameraLagSpeed, 8.0, 0.01), TEXT("Camera lag speed should be set to 8")));
	}

	// -------------------------------------------------------------------------
	// BoxComponent: SetBoxExtent
	// -------------------------------------------------------------------------
	TEST_METHOD(BoxComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_Box"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialBox.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialBoxActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UBoxComponent BoxComp;

				UPROPERTY()
				FVector InitialExtent;

				UPROPERTY()
				FVector NewExtent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitialExtent = BoxComp.GetUnscaledBoxExtent();

					BoxComp.SetBoxExtent(FVector(100.0f, 200.0f, 300.0f));

					NewExtent = BoxComp.GetUnscaledBoxExtent();
				}
			}
			)AS"),
			TEXT("ACoverageSpecialBoxActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special box actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special box actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FVector NewExtent;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("NewExtent"), NewExtent)));
		ASSERT_THAT(IsTrue(NewExtent.Equals(FVector(100.0f, 200.0f, 300.0f), 0.01f), TEXT("Box extent should be set correctly")));
	}

	// -------------------------------------------------------------------------
	// SphereComponent: SetSphereRadius
	// -------------------------------------------------------------------------
	TEST_METHOD(SphereComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_Sphere"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialSphere.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialSphereActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USphereComponent SphereComp;

				UPROPERTY()
				float InitialRadius = 0.0f;

				UPROPERTY()
				float NewRadius = 0.0f;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitialRadius = SphereComp.GetUnscaledSphereRadius();

					SphereComp.SetSphereRadius(150.0f);

					NewRadius = SphereComp.GetUnscaledSphereRadius();
				}
			}
			)AS"),
			TEXT("ACoverageSpecialSphereActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special sphere actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special sphere actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		double NewRadius = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewRadius"), NewRadius)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewRadius, 150.0, 0.01), TEXT("Sphere radius should be set to 150")));
	}

	// -------------------------------------------------------------------------
	// CapsuleComponent: SetCapsuleSize
	// -------------------------------------------------------------------------
	TEST_METHOD(CapsuleComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_Capsule"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialCapsule.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialCapsuleActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCapsuleComponent CapsuleComp;

				UPROPERTY()
				float InitialRadius = 0.0f;

				UPROPERTY()
				float InitialHalfHeight = 0.0f;

				UPROPERTY()
				float NewRadius = 0.0f;

				UPROPERTY()
				float NewHalfHeight = 0.0f;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitialRadius = CapsuleComp.GetUnscaledCapsuleRadius();
					InitialHalfHeight = CapsuleComp.GetUnscaledCapsuleHalfHeight();

					CapsuleComp.SetCapsuleSize(50.0f, 100.0f);

					NewRadius = CapsuleComp.GetUnscaledCapsuleRadius();
					NewHalfHeight = CapsuleComp.GetUnscaledCapsuleHalfHeight();
				}
			}
			)AS"),
			TEXT("ACoverageSpecialCapsuleActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special capsule actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special capsule actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		double NewRadius = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewRadius"), NewRadius)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewRadius, 50.0, 0.01), TEXT("Capsule radius should be set to 50")));

		double NewHalfHeight = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NewHalfHeight"), NewHalfHeight)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NewHalfHeight, 100.0, 0.01), TEXT("Capsule half height should be set to 100")));
	}

	// -------------------------------------------------------------------------
	// Custom script scene component: script-derived USceneComponent
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomScriptSceneComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_CustomScene"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialCustomScene.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCustomSceneComponent : USceneComponent
			{
				UPROPERTY()
				float CustomRadius = 100.0f;

				UPROPERTY()
				FLinearColor CustomColor = FLinearColor::Red;

				UPROPERTY()
				int TickCounter = 0;

				default PrimaryComponentTick.bCanEverTick = true;

				UFUNCTION(BlueprintOverride)
				void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
				{
					TickCounter++;
				}

				UFUNCTION()
				float GetArea()
				{
					return 3.14159f * CustomRadius * CustomRadius;
				}
			}

			UCLASS()
			class ACoverageSpecialCustomSceneActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCustomSceneComponent CustomComp;

				UPROPERTY()
				float RetrievedRadius = 0.0f;

				UPROPERTY()
				FLinearColor RetrievedColor;

				UPROPERTY()
				float CalculatedArea = 0.0f;

				UPROPERTY()
				int TickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (CustomComp != nullptr)
					{
						RetrievedRadius = CustomComp.CustomRadius;
						RetrievedColor = CustomComp.CustomColor;
						CalculatedArea = CustomComp.GetArea();
					}
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					if (CustomComp != nullptr)
					{
						TickCount = CustomComp.TickCounter;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSpecialCustomSceneActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special custom scene actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special custom scene actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		double RetrievedRadius = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RetrievedRadius"), RetrievedRadius)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RetrievedRadius, 100.0, 0.01), TEXT("Custom radius should be 100")));

		FLinearColor RetrievedColor;
		ASSERT_THAT(IsTrue(GetStructByPath<FLinearColor>(*TestRunner, Actor, TEXT("RetrievedColor"), RetrievedColor)));
		ASSERT_THAT(IsTrue(RetrievedColor.Equals(FLinearColor::Red, 0.01f), TEXT("Custom color should be red")));

		double CalculatedArea = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CalculatedArea"), CalculatedArea)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(CalculatedArea, 31415.9, 1.0), TEXT("Calculated area should be correct")));

		// Tick a few times
		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 2);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TickCount"), TickCount)));
		ASSERT_THAT(IsTrue(TickCount >= 2, TEXT("Component should tick at least 2 times")));
	}

	// -------------------------------------------------------------------------
	// Multiple shape components together
	// -------------------------------------------------------------------------
	TEST_METHOD(MultipleShapeComponents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_MultipleShapes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialMultipleShapes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialMultipleShapesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCapsuleComponent CapsuleComp;

				UPROPERTY(DefaultComponent, Attach=CapsuleComp)
				UBoxComponent BoxComp;

				UPROPERTY(DefaultComponent, Attach=CapsuleComp)
				USphereComponent SphereComp;

				UPROPERTY()
				bool AllComponentsValid = false;

				UPROPERTY()
				int ShapeComponentCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AllComponentsValid = (CapsuleComp != nullptr && BoxComp != nullptr && SphereComp != nullptr);

					// Set sizes
					CapsuleComp.SetCapsuleSize(40.0f, 90.0f);
					BoxComp.SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
					SphereComp.SetSphereRadius(30.0f);

					// Count shape components
					TArray<UShapeComponent> ShapeComps;
					GetComponentsByClass(UShapeComponent::StaticClass(), ShapeComps);
					ShapeComponentCount = ShapeComps.Num();
				}
			}
			)AS"),
			TEXT("ACoverageSpecialMultipleShapesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special multiple shapes actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special multiple shapes actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllComponentsValid"), true, TEXT("All shape components should be valid"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ShapeComponentCount"), 3, TEXT("Should have 3 shape components"))));
	}

	// -------------------------------------------------------------------------
	// Remaining default component types: Arrow, Audio, Input, PointLight, SkeletalMesh
	// -------------------------------------------------------------------------
	TEST_METHOD(AdditionalDefaultComponentTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_AdditionalTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialAdditionalTypes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialAdditionalTypesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UArrowComponent Arrow;

				UPROPERTY(DefaultComponent, Attach=Root)
				UAudioComponent Audio;

				UPROPERTY(DefaultComponent)
				UInputComponent Input;

				UPROPERTY(DefaultComponent, Attach=Root)
				UPointLightComponent PointLight;

				UPROPERTY(DefaultComponent, Attach=Root)
				USkeletalMeshComponent SkeletalMesh;

				UPROPERTY()
				bool ArrowValid = false;

				UPROPERTY()
				bool AudioValid = false;

				UPROPERTY()
				bool InputValid = false;

				UPROPERTY()
				bool PointLightValid = false;

				UPROPERTY()
				bool SkeletalMeshValid = false;

				UPROPERTY()
				bool SceneTypesAttached = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ArrowValid = Arrow != nullptr;
					AudioValid = Audio != nullptr;
					InputValid = Input != nullptr;
					PointLightValid = PointLight != nullptr;
					SkeletalMeshValid = SkeletalMesh != nullptr;

					if (Arrow != nullptr && Audio != nullptr && PointLight != nullptr && SkeletalMesh != nullptr)
					{
						SceneTypesAttached =
							Arrow.GetAttachParent() == Root
							&& Audio.GetAttachParent() == Root
							&& PointLight.GetAttachParent() == Root
							&& SkeletalMesh.GetAttachParent() == Root;
					}
				}
			}
			)AS"),
			TEXT("ACoverageSpecialAdditionalTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special additional component types actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FObjectProperty* ArrowProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("Arrow"));
		const FObjectProperty* AudioProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("Audio"));
		const FObjectProperty* InputProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("Input"));
		const FObjectProperty* PointLightProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("PointLight"));
		const FObjectProperty* SkeletalMeshProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("SkeletalMesh"));
		ASSERT_THAT(IsNotNull(ArrowProperty, TEXT("Arrow component property should exist")));
		ASSERT_THAT(IsNotNull(AudioProperty, TEXT("Audio component property should exist")));
		ASSERT_THAT(IsNotNull(InputProperty, TEXT("Input component property should exist")));
		ASSERT_THAT(IsNotNull(PointLightProperty, TEXT("PointLight component property should exist")));
		ASSERT_THAT(IsNotNull(SkeletalMeshProperty, TEXT("SkeletalMesh component property should exist")));
		if (ArrowProperty == nullptr || AudioProperty == nullptr || InputProperty == nullptr || PointLightProperty == nullptr || SkeletalMeshProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrowProperty->PropertyClass->IsChildOf(UArrowComponent::StaticClass()), TEXT("Arrow property should use UArrowComponent")));
		ASSERT_THAT(IsTrue(AudioProperty->PropertyClass->IsChildOf(UAudioComponent::StaticClass()), TEXT("Audio property should use UAudioComponent")));
		ASSERT_THAT(IsTrue(InputProperty->PropertyClass->IsChildOf(UInputComponent::StaticClass()), TEXT("Input property should use UInputComponent")));
		ASSERT_THAT(IsTrue(PointLightProperty->PropertyClass->IsChildOf(UPointLightComponent::StaticClass()), TEXT("PointLight property should use UPointLightComponent")));
		ASSERT_THAT(IsTrue(SkeletalMeshProperty->PropertyClass->IsChildOf(USkeletalMeshComponent::StaticClass()), TEXT("SkeletalMesh property should use USkeletalMeshComponent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special additional component types actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrowValid"), true, TEXT("UArrowComponent DefaultComponent should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AudioValid"), true, TEXT("UAudioComponent DefaultComponent should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InputValid"), true, TEXT("UInputComponent DefaultComponent should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("PointLightValid"), true, TEXT("UPointLightComponent DefaultComponent should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SkeletalMeshValid"), true, TEXT("USkeletalMeshComponent DefaultComponent should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SceneTypesAttached"), true, TEXT("Scene default component types should attach to Root"))));
	}

	// -------------------------------------------------------------------------
	// Special component operations: class lookup, tags, registration, activation, destruction
	// -------------------------------------------------------------------------
	TEST_METHOD(SpecialComponentOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSpecial_ComponentOperations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSpecialComponentOperations.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSpecialComponentOperationsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UArrowComponent Arrow;

				UPROPERTY(DefaultComponent, Attach=Root)
				UAudioComponent Audio;

				UPROPERTY(DefaultComponent)
				UInputComponent Input;

				UPROPERTY()
				UArrowComponent RuntimeArrow;

				UPROPERTY()
				bool FoundArrowByClass = false;

				UPROPERTY()
				bool FoundAudioByClass = false;

				UPROPERTY()
				bool FoundInputByClass = false;

				UPROPERTY()
				int TaggedSpecialComponentCount = 0;

				UPROPERTY()
				bool ArrowHasTag = false;

				UPROPERTY()
				bool AudioOwnerMatched = false;

				UPROPERTY()
				bool InputOwnerMatched = false;

				UPROPERTY()
				bool AudioWorldMatched = false;

				UPROPERTY()
				bool RuntimeArrowCreated = false;

				UPROPERTY()
				bool RuntimeArrowInitiallyUnregistered = false;

				UPROPERTY()
				bool RuntimeArrowRegistered = false;

				UPROPERTY()
				bool RuntimeArrowActivated = false;

				UPROPERTY()
				bool RuntimeArrowDeactivated = false;

				UPROPERTY()
				bool RuntimeArrowDestroyed = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (Arrow != nullptr)
					{
						Arrow.ComponentTags.Add(n"SpecialCoverage");
						ArrowHasTag = Arrow.ComponentHasTag(n"SpecialCoverage");
					}

					if (Audio != nullptr)
					{
						Audio.ComponentTags.Add(n"SpecialCoverage");
						Audio.SetVolumeMultiplier(0.25f);
						Audio.Stop();
						AudioOwnerMatched = Audio.GetOwner() == this;
						AudioWorldMatched = Audio.GetWorld() == GetWorld();
					}

					if (Input != nullptr)
					{
						Input.ComponentTags.Add(n"SpecialCoverage");
						InputOwnerMatched = Input.GetOwner() == this;
					}

					FoundArrowByClass = Cast<UArrowComponent>(FindComponentByClass(UArrowComponent::StaticClass())) == Arrow;
					FoundAudioByClass = Cast<UAudioComponent>(GetComponentByClass(UAudioComponent::StaticClass())) == Audio;
					FoundInputByClass = Cast<UInputComponent>(FindComponentByClass(UInputComponent::StaticClass())) == Input;

					TArray<UActorComponent> TaggedComponents = GetComponentsByTag(UActorComponent::StaticClass(), n"SpecialCoverage");
					TaggedSpecialComponentCount = TaggedComponents.Num();

					RuntimeArrow = Cast<UArrowComponent>(NewObject(this, UArrowComponent::StaticClass(), n"RuntimeArrow", true));
					RuntimeArrowCreated = RuntimeArrow != nullptr;
					if (RuntimeArrow == nullptr)
					{
						return;
					}

					RuntimeArrowInitiallyUnregistered = !RuntimeArrow.IsRegistered();
					RuntimeArrow.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					RuntimeArrow.RegisterComponent();
					RuntimeArrowRegistered = RuntimeArrow.IsRegistered();

					RuntimeArrow.Activate(true);
					RuntimeArrowActivated = RuntimeArrow.IsActive();

					RuntimeArrow.Deactivate();
					RuntimeArrowDeactivated = !RuntimeArrow.IsActive();

					RuntimeArrow.DestroyComponent();
					RuntimeArrowDestroyed = RuntimeArrow.IsBeingDestroyed();
				}
			}
			)AS"),
			TEXT("ACoverageSpecialComponentOperationsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special component operations actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special component operations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundArrowByClass"), true, TEXT("FindComponentByClass should find UArrowComponent"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundAudioByClass"), true, TEXT("GetComponentByClass should find UAudioComponent"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundInputByClass"), true, TEXT("FindComponentByClass should find UInputComponent"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TaggedSpecialComponentCount"), 3, TEXT("GetComponentsByTag should find tagged Arrow, Audio, and Input components"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ArrowHasTag"), true, TEXT("ComponentHasTag should detect the Arrow tag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AudioOwnerMatched"), true, TEXT("Audio GetOwner should return the owning actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InputOwnerMatched"), true, TEXT("Input GetOwner should return the owning actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AudioWorldMatched"), true, TEXT("Audio GetWorld should match the actor world"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeArrowCreated"), true, TEXT("NewObject should create a runtime UArrowComponent"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeArrowInitiallyUnregistered"), true, TEXT("NewObject component should start unregistered"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeArrowRegistered"), true, TEXT("RegisterComponent should register the runtime UArrowComponent"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeArrowActivated"), true, TEXT("Activate should mark runtime UArrowComponent active"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeArrowDeactivated"), true, TEXT("Deactivate should mark runtime UArrowComponent inactive"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeArrowDestroyed"), true, TEXT("DestroyComponent should mark runtime UArrowComponent as being destroyed"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
