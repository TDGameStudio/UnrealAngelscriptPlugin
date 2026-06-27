#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageSceneComponentTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript USceneComponent features (transform and attachment),
// corresponding to Documents/Coverage/Coverage_UComponent.md section 5.
//
// Axes covered here:
//   * SceneComponentTransform      - Get/SetWorldLocation/Rotation/Scale
//   * SceneComponentRelativeTransform - SetRelativeLocation/Rotation
//   * SceneComponentAttachment     - AttachToComponent, DetachFromComponent
//   * SceneComponentAttachmentRules - KeepWorld, KeepRelative, SnapToTarget
//   * SceneComponentHierarchy      - GetAttachParent, GetAttachChildren
//   * SceneComponentSockets        - GetSocketLocation
//
// Pattern D (script execution): compile AS actors with scene components,
// spawn them, manipulate transforms and attachment, verify results.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_UComponent.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageSceneComponentTest,
	"Angelscript.TestModule.Coverage.SceneComponent",
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
	// Scene component world transform: Get/SetWorldLocation/Rotation/Scale
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentWorldTransform)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_WorldTransform"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentWorldTransform.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentWorldTransformActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY()
				FVector InitialLocation;

				UPROPERTY()
				FVector NewLocation;

				UPROPERTY()
				FRotator NewRotation;

				UPROPERTY()
				FVector NewScale;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitialLocation = Root.GetComponentLocation();

					// Set world transform
					Root.SetWorldLocation(FVector(100.0f, 200.0f, 300.0f));
					Root.SetWorldRotation(FRotator(0.0f, 90.0f, 0.0f));
					Root.SetWorldScale3D(FVector(2.0f, 2.0f, 2.0f));

					// Read back
					NewLocation = Root.GetComponentLocation();
					NewRotation = Root.GetComponentRotation();
					NewScale = Root.GetComponentScale();
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentWorldTransformActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component world transform actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component world transform actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		FVector NewLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("NewLocation"), NewLocation);
		ASSERT_THAT(IsTrue(NewLocation.Equals(FVector(100.0f, 200.0f, 300.0f), 0.01f), TEXT("Location should be set correctly")));

		FRotator NewRotation;
		GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("NewRotation"), NewRotation);
		ASSERT_THAT(IsTrue(NewRotation.Equals(FRotator(0.0f, 90.0f, 0.0f), 0.01f), TEXT("Rotation should be set correctly")));

		FVector NewScale;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("NewScale"), NewScale);
		ASSERT_THAT(IsTrue(NewScale.Equals(FVector(2.0f, 2.0f, 2.0f), 0.01f), TEXT("Scale should be set correctly")));
	}

	// -------------------------------------------------------------------------
	// Scene component relative transform: SetRelativeLocation/Rotation
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentRelativeTransform)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_RelativeTransform"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentRelativeTransform.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentRelativeTransformActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child;

				UPROPERTY()
				FVector ChildRelativeLocation;

				UPROPERTY()
				FRotator ChildRelativeRotation;

				UPROPERTY()
				FVector ChildWorldLocation;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set root to known position
					Root.SetWorldLocation(FVector(100.0f, 0.0f, 0.0f));

					// Set child relative transform
					Child.SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
					Child.SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));

					// Read back
					ChildRelativeLocation = Child.RelativeLocation;
					ChildRelativeRotation = Child.RelativeRotation;
					ChildWorldLocation = Child.GetComponentLocation();
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentRelativeTransformActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component relative transform actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component relative transform actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		FVector RelativeLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("ChildRelativeLocation"), RelativeLocation);
		ASSERT_THAT(IsTrue(RelativeLocation.Equals(FVector(50.0f, 0.0f, 0.0f), 0.01f), TEXT("Relative location should be set correctly")));

		FRotator RelativeRotation;
		GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("ChildRelativeRotation"), RelativeRotation);
		ASSERT_THAT(IsTrue(RelativeRotation.Equals(FRotator(0.0f, 45.0f, 0.0f), 0.01f), TEXT("Relative rotation should be set correctly")));

		FVector WorldLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("ChildWorldLocation"), WorldLocation);
		ASSERT_THAT(IsTrue(WorldLocation.Equals(FVector(150.0f, 0.0f, 0.0f), 0.01f), TEXT("World location should be calculated correctly")));
	}

	// -------------------------------------------------------------------------
	// Scene component attachment: AttachToComponent, DetachFromComponent
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentAttachment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_Attachment"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentAttachment.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentAttachmentActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				USceneComponent Detached;

				UPROPERTY()
				bool InitiallyAttached = true;

				UPROPERTY()
				bool AfterAttach = false;

				UPROPERTY()
				bool AfterDetach = true;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Check initial state
					InitiallyAttached = Detached.IsAttachedTo(Root);

					// Attach
					Detached.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					AfterAttach = Detached.IsAttachedTo(Root);

					// Detach
					Detached.DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
					AfterDetach = Detached.IsAttachedTo(Root);
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentAttachmentActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component attachment actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component attachment actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyAttached"), false, TEXT("Component should not be initially attached"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterAttach"), true, TEXT("Component should be attached after AttachToComponent"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterDetach"), false, TEXT("Component should be detached after DetachFromComponent"));
	}

	// -------------------------------------------------------------------------
	// Scene component attachment rules: KeepWorld, KeepRelative, SnapToTarget
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentAttachmentRules)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_AttachmentRules"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentAttachmentRules.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentAttachmentRulesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				USceneComponent TestComp1;

				UPROPERTY(DefaultComponent)
				USceneComponent TestComp2;

				UPROPERTY(DefaultComponent)
				USceneComponent TestComp3;

				UPROPERTY()
				FVector KeepWorldLocation;

				UPROPERTY()
				FVector KeepRelativeLocation;

				UPROPERTY()
				FVector SnapToTargetLocation;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set root to offset position
					Root.SetWorldLocation(FVector(100.0f, 100.0f, 0.0f));

					// KeepWorld rule: world position unchanged
					TestComp1.SetWorldLocation(FVector(50.0f, 50.0f, 0.0f));
					TestComp1.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
					KeepWorldLocation = TestComp1.GetComponentLocation();

					// KeepRelative rule: relative offset preserved
					TestComp2.SetRelativeLocation(FVector(20.0f, 0.0f, 0.0f));
					TestComp2.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					KeepRelativeLocation = TestComp2.GetComponentLocation();

					// SnapToTarget rule: snaps to parent
					TestComp3.SetWorldLocation(FVector(200.0f, 200.0f, 0.0f));
					TestComp3.AttachToComponent(Root, NAME_None, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, false);
					SnapToTargetLocation = TestComp3.GetComponentLocation();
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentAttachmentRulesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component attachment rules actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component attachment rules actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// KeepWorld: should stay at (50, 50, 0)
		FVector KeepWorldLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("KeepWorldLocation"), KeepWorldLocation);
		ASSERT_THAT(IsTrue(KeepWorldLocation.Equals(FVector(50.0f, 50.0f, 0.0f), 0.01f), TEXT("KeepWorld should preserve world location")));

		// KeepRelative: should be root (100, 100, 0) + relative (20, 0, 0) = (120, 100, 0)
		FVector KeepRelativeLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("KeepRelativeLocation"), KeepRelativeLocation);
		ASSERT_THAT(IsTrue(KeepRelativeLocation.Equals(FVector(120.0f, 100.0f, 0.0f), 0.01f), TEXT("KeepRelative should preserve relative offset")));

		// SnapToTarget: should snap to root (100, 100, 0)
		FVector SnapToTargetLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("SnapToTargetLocation"), SnapToTargetLocation);
		ASSERT_THAT(IsTrue(SnapToTargetLocation.Equals(FVector(100.0f, 100.0f, 0.0f), 0.01f), TEXT("SnapToTarget should snap to parent location")));
	}

	// -------------------------------------------------------------------------
	// Scene component hierarchy: GetAttachParent, GetAttachChildren
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentHierarchy)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_Hierarchy"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentHierarchy.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentHierarchyActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child1;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child2;

				UPROPERTY(DefaultComponent, Attach=Child1)
				USceneComponent GrandChild;

				UPROPERTY()
				bool Child1HasParent = false;

				UPROPERTY()
				bool GrandChildParentIsChild1 = false;

				UPROPERTY()
				int RootChildrenCount = 0;

				UPROPERTY()
				int Child1ChildrenCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					USceneComponent Child1Parent = Child1.GetAttachParent();
					Child1HasParent = (Child1Parent == Root);

					USceneComponent GrandChildParent = GrandChild.GetAttachParent();
					GrandChildParentIsChild1 = (GrandChildParent == Child1);

					TArray<USceneComponent> RootChildren = Root.GetAttachChildren();
					RootChildrenCount = RootChildren.Num();

					TArray<USceneComponent> Child1Children = Child1.GetAttachChildren();
					Child1ChildrenCount = Child1Children.Num();
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentHierarchyActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component hierarchy actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component hierarchy actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("Child1HasParent"), true, TEXT("Child1 should have Root as parent"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GrandChildParentIsChild1"), true, TEXT("GrandChild should have Child1 as parent"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RootChildrenCount"), 2, TEXT("Root should have 2 children"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Child1ChildrenCount"), 1, TEXT("Child1 should have 1 child"));
	}

	// -------------------------------------------------------------------------
	// Scene component complete transform: GetComponentTransform, SetWorldTransform
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentCompleteTransform)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_CompleteTransform"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentCompleteTransform.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentCompleteTransformActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY()
				FVector FinalLocation;

				UPROPERTY()
				FRotator FinalRotation;

				UPROPERTY()
				FVector FinalScale;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create and set a complete transform
					FTransform NewTransform;
					NewTransform.Location = FVector(100.0f, 200.0f, 300.0f);
					NewTransform.Rotation = FQuat(FRotator(10.0f, 20.0f, 30.0f));
					NewTransform.Scale3D = FVector(1.5f, 1.5f, 1.5f);

					Root.SetWorldTransform(NewTransform);

					// Read back using GetComponentTransform
					FTransform CurrentTransform = Root.GetComponentTransform();
					FinalLocation = CurrentTransform.Location;
					FinalRotation = CurrentTransform.Rotation.Rotator();
					FinalScale = CurrentTransform.Scale3D;
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentCompleteTransformActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component complete transform actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component complete transform actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		FVector FinalLocation;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("FinalLocation"), FinalLocation);
		ASSERT_THAT(IsTrue(FinalLocation.Equals(FVector(100.0f, 200.0f, 300.0f), 0.01f), TEXT("Final location should match set transform")));

		FRotator FinalRotation;
		GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("FinalRotation"), FinalRotation);
		ASSERT_THAT(IsTrue(FinalRotation.Equals(FRotator(10.0f, 20.0f, 30.0f), 0.1f), TEXT("Final rotation should match set transform")));

		FVector FinalScale;
		GetStructByPath<FVector>(*TestRunner, Actor, TEXT("FinalScale"), FinalScale);
		ASSERT_THAT(IsTrue(FinalScale.Equals(FVector(1.5f, 1.5f, 1.5f), 0.01f), TEXT("Final scale should match set transform")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
