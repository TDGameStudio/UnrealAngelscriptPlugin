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
// corresponding to OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md section 5.
//
// Axes covered here:
//   * SceneComponentTransform      - Get/SetWorldLocation/Rotation/Scale
//   * SceneComponentRelativeTransform - SetRelativeLocation/Rotation
//   * SceneComponentAttachment     - AttachToComponent, DetachFromComponent
//   * SceneComponentAttachmentRules - KeepWorld, KeepRelative, SnapToTarget
//   * SceneComponentHierarchy      - GetAttachParent, GetAttachChildren
//   * SceneComponentSockets        - GetSocketLocation
//   * SceneComponentTags           - ComponentTags, ComponentHasTag
//
// Pattern D (script execution): compile AS actors with scene components,
// spawn them, manipulate transforms and attachment, verify results.
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageSceneComponentTest,
	"Angelscript.TestModule.Coverage.SceneComponent",
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

	template <typename StructType>
	static bool ReadStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, StructType& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetStructByPath<StructType>(Test, Object, Path, OutValue), Message);
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component world transform actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FVector NewLocation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("NewLocation"), NewLocation, TEXT("NewLocation should be readable"))));
		ASSERT_THAT(IsTrue(NewLocation.Equals(FVector(100.0f, 200.0f, 300.0f), 0.01f), TEXT("Location should be set correctly")));

		FRotator NewRotation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("NewRotation"), NewRotation, TEXT("NewRotation should be readable"))));
		ASSERT_THAT(IsTrue(NewRotation.Equals(FRotator(0.0f, 90.0f, 0.0f), 0.01f), TEXT("Rotation should be set correctly")));

		FVector NewScale;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("NewScale"), NewScale, TEXT("NewScale should be readable"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component relative transform actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FVector RelativeLocation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("ChildRelativeLocation"), RelativeLocation, TEXT("ChildRelativeLocation should be readable"))));
		ASSERT_THAT(IsTrue(RelativeLocation.Equals(FVector(50.0f, 0.0f, 0.0f), 0.01f), TEXT("Relative location should be set correctly")));

		FRotator RelativeRotation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("ChildRelativeRotation"), RelativeRotation, TEXT("ChildRelativeRotation should be readable"))));
		ASSERT_THAT(IsTrue(RelativeRotation.Equals(FRotator(0.0f, 45.0f, 0.0f), 0.01f), TEXT("Relative rotation should be set correctly")));

		FVector WorldLocation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("ChildWorldLocation"), WorldLocation, TEXT("ChildWorldLocation should be readable"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component attachment actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InitiallyAttached"), false, TEXT("Component should not be initially attached"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("AfterAttach"), true, TEXT("Component should be attached after AttachToComponent"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("AfterDetach"), false, TEXT("Component should be detached after DetachFromComponent"))));
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
				FVector KeepWorldRelativeLocation;

				UPROPERTY()
				FVector KeepRelativeLocation;

				UPROPERTY()
				FVector KeepRelativeRelativeLocation;

				UPROPERTY()
				FVector SnapToTargetLocation;

				UPROPERTY()
				FVector SnapToTargetRelativeLocation;

				UPROPERTY()
				bool KeepWorldAttachedToRoot = false;

				UPROPERTY()
				bool KeepRelativeAttachedToRoot = false;

				UPROPERTY()
				bool SnapToTargetAttachedToRoot = false;

				UPROPERTY()
				bool KeepWorldParentIsRoot = false;

				UPROPERTY()
				bool KeepRelativeParentIsRoot = false;

				UPROPERTY()
				bool SnapToTargetParentIsRoot = false;

				UPROPERTY()
				int RootChildrenCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set root to offset position
					Root.SetWorldLocation(FVector(100.0f, 100.0f, 0.0f));

					// KeepWorld rule: world position unchanged
					TestComp1.SetWorldLocation(FVector(50.0f, 50.0f, 0.0f));
					TestComp1.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
					KeepWorldLocation = TestComp1.GetComponentLocation();
					KeepWorldRelativeLocation = TestComp1.RelativeLocation;
					KeepWorldAttachedToRoot = TestComp1.IsAttachedTo(Root);
					KeepWorldParentIsRoot = TestComp1.GetAttachParent() == Root;

					// KeepRelative rule: relative offset preserved
					TestComp2.SetRelativeLocation(FVector(20.0f, 0.0f, 0.0f));
					TestComp2.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					KeepRelativeLocation = TestComp2.GetComponentLocation();
					KeepRelativeRelativeLocation = TestComp2.RelativeLocation;
					KeepRelativeAttachedToRoot = TestComp2.IsAttachedTo(Root);
					KeepRelativeParentIsRoot = TestComp2.GetAttachParent() == Root;

					// SnapToTarget rule: snaps to parent
					TestComp3.SetWorldLocation(FVector(200.0f, 200.0f, 0.0f));
					TestComp3.AttachToComponent(Root, NAME_None, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, false);
					SnapToTargetLocation = TestComp3.GetComponentLocation();
					SnapToTargetRelativeLocation = TestComp3.RelativeLocation;
					SnapToTargetAttachedToRoot = TestComp3.IsAttachedTo(Root);
					SnapToTargetParentIsRoot = TestComp3.GetAttachParent() == Root;

					TArray<USceneComponent> RootChildren = Root.GetAttachChildren();
					RootChildrenCount = RootChildren.Num();
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentAttachmentRulesActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component attachment rules actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component attachment rules actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// KeepWorld: should stay at (50, 50, 0)
		FVector KeepWorldLocation;
		const bool bReadKeepWorldLocation = ReadStructByPath(*TestRunner, Actor, TEXT("KeepWorldLocation"), KeepWorldLocation, TEXT("KeepWorldLocation should be readable"));
		ASSERT_THAT(IsTrue(bReadKeepWorldLocation, TEXT("KeepWorldLocation should be readable")));
		if (!bReadKeepWorldLocation)
		{
			return;
		}
		ASSERT_THAT(IsTrue(KeepWorldLocation.Equals(FVector(50.0f, 50.0f, 0.0f), 0.01f), TEXT("KeepWorld should preserve world location")));

		FVector KeepWorldRelativeLocation;
		const bool bReadKeepWorldRelativeLocation = ReadStructByPath(*TestRunner, Actor, TEXT("KeepWorldRelativeLocation"), KeepWorldRelativeLocation, TEXT("KeepWorldRelativeLocation should be readable"));
		ASSERT_THAT(IsTrue(bReadKeepWorldRelativeLocation, TEXT("KeepWorldRelativeLocation should be readable")));
		if (!bReadKeepWorldRelativeLocation)
		{
			return;
		}
		ASSERT_THAT(IsTrue(KeepWorldRelativeLocation.Equals(FVector(-50.0f, -50.0f, 0.0f), 0.01f), TEXT("KeepWorld should derive a relative offset that preserves world location")));

		// KeepRelative: should be root (100, 100, 0) + relative (20, 0, 0) = (120, 100, 0)
		FVector KeepRelativeLocation;
		const bool bReadKeepRelativeLocation = ReadStructByPath(*TestRunner, Actor, TEXT("KeepRelativeLocation"), KeepRelativeLocation, TEXT("KeepRelativeLocation should be readable"));
		ASSERT_THAT(IsTrue(bReadKeepRelativeLocation, TEXT("KeepRelativeLocation should be readable")));
		if (!bReadKeepRelativeLocation)
		{
			return;
		}
		ASSERT_THAT(IsTrue(KeepRelativeLocation.Equals(FVector(120.0f, 100.0f, 0.0f), 0.01f), TEXT("KeepRelative should preserve relative offset")));

		FVector KeepRelativeRelativeLocation;
		const bool bReadKeepRelativeRelativeLocation = ReadStructByPath(*TestRunner, Actor, TEXT("KeepRelativeRelativeLocation"), KeepRelativeRelativeLocation, TEXT("KeepRelativeRelativeLocation should be readable"));
		ASSERT_THAT(IsTrue(bReadKeepRelativeRelativeLocation, TEXT("KeepRelativeRelativeLocation should be readable")));
		if (!bReadKeepRelativeRelativeLocation)
		{
			return;
		}
		ASSERT_THAT(IsTrue(KeepRelativeRelativeLocation.Equals(FVector(20.0f, 0.0f, 0.0f), 0.01f), TEXT("KeepRelative should preserve relative location")));

		// SnapToTarget: should snap to root (100, 100, 0)
		FVector SnapToTargetLocation;
		const bool bReadSnapToTargetLocation = ReadStructByPath(*TestRunner, Actor, TEXT("SnapToTargetLocation"), SnapToTargetLocation, TEXT("SnapToTargetLocation should be readable"));
		ASSERT_THAT(IsTrue(bReadSnapToTargetLocation, TEXT("SnapToTargetLocation should be readable")));
		if (!bReadSnapToTargetLocation)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SnapToTargetLocation.Equals(FVector(100.0f, 100.0f, 0.0f), 0.01f), TEXT("SnapToTarget should snap to parent location")));

		FVector SnapToTargetRelativeLocation;
		const bool bReadSnapToTargetRelativeLocation = ReadStructByPath(*TestRunner, Actor, TEXT("SnapToTargetRelativeLocation"), SnapToTargetRelativeLocation, TEXT("SnapToTargetRelativeLocation should be readable"));
		ASSERT_THAT(IsTrue(bReadSnapToTargetRelativeLocation, TEXT("SnapToTargetRelativeLocation should be readable")));
		if (!bReadSnapToTargetRelativeLocation)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SnapToTargetRelativeLocation.Equals(FVector::ZeroVector, 0.01f), TEXT("SnapToTarget should zero relative location")));

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("KeepWorldAttachedToRoot"), true, TEXT("KeepWorld component should be attached to Root"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("KeepRelativeAttachedToRoot"), true, TEXT("KeepRelative component should be attached to Root"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("SnapToTargetAttachedToRoot"), true, TEXT("SnapToTarget component should be attached to Root"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("KeepWorldParentIsRoot"), true, TEXT("KeepWorld component should report Root as attach parent"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("KeepRelativeParentIsRoot"), true, TEXT("KeepRelative component should report Root as attach parent"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("SnapToTargetParentIsRoot"), true, TEXT("SnapToTarget component should report Root as attach parent"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("RootChildrenCount"), 3, TEXT("Root should report all attachment-rule children"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component hierarchy actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("Child1HasParent"), true, TEXT("Child1 should have Root as parent"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("GrandChildParentIsChild1"), true, TEXT("GrandChild should have Child1 as parent"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("RootChildrenCount"), 2, TEXT("Root should have 2 children"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("Child1ChildrenCount"), 1, TEXT("Child1 should have 1 child"))));
	}

	// -------------------------------------------------------------------------
	// Scene component sockets: AttachSocket metadata, GetAttachSocketName, GetSocketLocation
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentSocketAttachment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_SocketAttachment"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentSocketAttachment.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentSocketAttachmentActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="CoverageSocket")
				USceneComponent SocketChild;

				UPROPERTY(DefaultComponent)
				USceneComponent RuntimeAttached;

				UPROPERTY()
				FName DefaultAttachSocket;

				UPROPERTY()
				FName RuntimeAttachSocket;

				UPROPERTY()
				bool DefaultChildAttached = false;

				UPROPERTY()
				bool RuntimeChildAttached = false;

				UPROPERTY()
				bool RootSocketLocationMatchesRoot = false;

				UPROPERTY()
				bool ChildSocketLocationMatchesChild = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DefaultChildAttached = SocketChild.GetAttachParent() == Root;
					DefaultAttachSocket = SocketChild.GetAttachSocketName();

					RuntimeAttached.AttachToComponent(Root, n"RuntimeSocket",
						EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					RuntimeChildAttached = RuntimeAttached.GetAttachParent() == Root;
					RuntimeAttachSocket = RuntimeAttached.GetAttachSocketName();

					FVector RootLocation = Root.GetComponentLocation();
					RootSocketLocationMatchesRoot = Root.GetSocketLocation(n"CoverageSocket").Equals(RootLocation, 0.01f);

					FVector ChildLocation = SocketChild.GetComponentLocation();
					ChildSocketLocationMatchesChild = SocketChild.GetSocketLocation(NAME_None).Equals(ChildLocation, 0.01f);
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentSocketAttachmentActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component socket attachment actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component socket attachment actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FName DefaultAttachSocket = NAME_None;
		FName RuntimeAttachSocket = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("DefaultAttachSocket"), DefaultAttachSocket), TEXT("DefaultAttachSocket should be readable")));
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("RuntimeAttachSocket"), RuntimeAttachSocket), TEXT("RuntimeAttachSocket should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("CoverageSocket")), DefaultAttachSocket, TEXT("AttachSocket should persist to the default component runtime attachment")));
		ASSERT_THAT(AreEqual(FName(TEXT("RuntimeSocket")), RuntimeAttachSocket, TEXT("AttachToComponent socket parameter should persist to the runtime attachment")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultChildAttached"), true, TEXT("AttachSocket child should attach to Root"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeChildAttached"), true, TEXT("Runtime socket child should attach to Root"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootSocketLocationMatchesRoot"), true, TEXT("Scene component socket location should fall back to component transform"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildSocketLocationMatchesChild"), true, TEXT("NAME_None socket location should match child transform"))));
	}

	// -------------------------------------------------------------------------
	// Scene component tags: ComponentTags, ComponentHasTag
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentTags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_Tags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentTags.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSceneComponentTagsActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child;

				UPROPERTY()
				bool RootHasCoverageTag = false;

				UPROPERTY()
				bool ChildHasCoverageTag = false;

				UPROPERTY()
				bool ChildRejectsMissingTag = false;

				UPROPERTY()
				int RootTagCount = 0;

				UPROPERTY()
				int ChildTagCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Root.ComponentTags.Add(n"RootCoverageTag");
					Child.ComponentTags.Add(n"SceneCoverageTag");
					Child.ComponentTags.Add(n"SharedCoverageTag");

					RootHasCoverageTag = Root.ComponentHasTag(n"RootCoverageTag");
					ChildHasCoverageTag = Child.ComponentHasTag(n"SceneCoverageTag");
					ChildRejectsMissingTag = !Child.ComponentHasTag(n"MissingCoverageTag");
					RootTagCount = Root.ComponentTags.Num();
					ChildTagCount = Child.ComponentTags.Num();
				}
			}
			)AS"),
			TEXT("ACoverageSceneComponentTagsActor"));

		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene component tags actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component tags actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("RootHasCoverageTag"), true, TEXT("Root scene component should report its tag"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ChildHasCoverageTag"), true, TEXT("Child scene component should report its tag"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ChildRejectsMissingTag"), true, TEXT("Child scene component should reject a missing tag"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("RootTagCount"), 1, TEXT("Root scene component should expose its ComponentTags array"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("ChildTagCount"), 2, TEXT("Child scene component should expose all ComponentTags entries"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene component complete transform actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FVector FinalLocation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("FinalLocation"), FinalLocation, TEXT("FinalLocation should be readable"))));
		ASSERT_THAT(IsTrue(FinalLocation.Equals(FVector(100.0f, 200.0f, 300.0f), 0.01f), TEXT("Final location should match set transform")));

		FRotator FinalRotation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("FinalRotation"), FinalRotation, TEXT("FinalRotation should be readable"))));
		ASSERT_THAT(IsTrue(FinalRotation.Equals(FRotator(10.0f, 20.0f, 30.0f), 0.1f), TEXT("Final rotation should match set transform")));

		FVector FinalScale;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("FinalScale"), FinalScale, TEXT("FinalScale should be readable"))));
		ASSERT_THAT(IsTrue(FinalScale.Equals(FVector(1.5f, 1.5f, 1.5f), 0.01f), TEXT("Final scale should match set transform")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
