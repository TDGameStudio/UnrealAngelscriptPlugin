#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMixinTests
// -----------------------------------------------------------------------------
// Coverage landing file for AS mixin functions. The AS 2.33 fork used here does
// not support `mixin class`; supported production coverage is the Hazelight-style
// free-function mixin form: `mixin void Func(Receiver Self, ...)`.
//
// Existing syntax-level expectations live in Syntax/AngelscriptSyntaxMixinTests.
// This file exercises the runtime-dispatch surface that projects free functions
// as receiver methods.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMixinTest,
	"Angelscript.TestModule.Coverage.Mixin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(FreeFunctionMixinDispatchAndDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_DispatchDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinDispatchDefaults.as"),
			ASTEST_AS(R"AS(
			mixin void MarkReady(ACoverageMixinHostActor Self)
			{
				Self.bReady = true;
			}

			mixin void AddScore(ACoverageMixinHostActor Self, int Amount = 5)
			{
				Self.Score += Amount;
			}

			mixin void CopyScoreTo(ACoverageMixinHostActor Self, ACoverageMixinHostActor Other, int Bonus = 1)
			{
				if (Other != nullptr)
				{
					Other.Score = Self.Score + Bonus;
				}
			}

			UCLASS()
			class ACoverageMixinHostActor : AActor
			{
				UPROPERTY()
				bool bReady = false;

				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				int OtherScore = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					this.MarkReady();
					this.AddScore();
					this.AddScore(7);

					// Spawn a no-op peer subclass (not the host class itself) so the
					// spawned receiver does not recursively run host BeginPlay/SpawnActor.
					ACoverageMixinHostActor Other = Cast<ACoverageMixinHostActor>(SpawnActor(ACoverageMixinPeerActor::StaticClass()));
					this.CopyScoreTo(Other, 3);
					if (Other != nullptr)
					{
						OtherScore = Other.Score;
						Other.DestroyActor();
					}
				}
			}

			UCLASS()
			class ACoverageMixinPeerActor : ACoverageMixinHostActor
			{
				// No-op BeginPlay keeps the spawned peer from recursively spawning more peers.
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
				}
			}
			)AS"),
			TEXT("ACoverageMixinHostActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("mixin host actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("mixin host actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bReady"), true,
			TEXT("single-receiver mixin should mutate receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Score"), 12,
			TEXT("default and explicit mixin args should accumulate"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OtherScore"), 15,
			TEXT("multi-receiver mixin should dispatch with extra args"))));
	}

	TEST_METHOD(MixinMethodsCanBeComposed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_Composition"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinComposition.as"),
			ASTEST_AS(R"AS(
			mixin bool IsScoreAtLeast(ACoverageMixinCompositionActor Self, int Threshold)
			{
				return Self.Score >= Threshold;
			}

			mixin void ClampScore(ACoverageMixinCompositionActor Self, int MinValue, int MaxValue)
			{
				if (Self.Score < MinValue)
				{
					Self.Score = MinValue;
				}
				else if (Self.Score > MaxValue)
				{
					Self.Score = MaxValue;
				}
			}

			UCLASS()
			class ACoverageMixinCompositionActor : AActor
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				bool bBelowBeforeClamp = false;

				UPROPERTY()
				bool bWithinAfterClamp = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Score = 25;
					bBelowBeforeClamp = this.IsScoreAtLeast(30);
					this.ClampScore(0, 20);
					bWithinAfterClamp = this.IsScoreAtLeast(20);
				}
			}
			)AS"),
			TEXT("ACoverageMixinCompositionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("composed mixin actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("composed mixin actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBelowBeforeClamp"), false,
			TEXT("query mixin should return false before clamp"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Score"), 20,
			TEXT("mutating mixin should clamp receiver state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bWithinAfterClamp"), true,
			TEXT("query mixin should observe clamped state"))));
	}

	TEST_METHOD(MixinClassSyntaxRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected data type"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMixin_ClassSyntaxUnsupported"),
			ASTEST_AS(R"AS(
			mixin class UCoverageMixinUnsupportedA
			{
				int SharedValue = 1;
			}

			mixin class UCoverageMixinUnsupportedB
			{
				int SharedValue = 2;
			}

			UCLASS()
			class ACoverageMixinUnsupportedMultipleConflictActor : AActor
			{
				mixin UCoverageMixinUnsupportedA;
				mixin UCoverageMixinUnsupportedB;
			}
			)AS"),
			TEXT("mixin class syntax should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(MixinOverloadsResolveAcrossScriptInheritance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_InheritanceConflict"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinInheritanceConflict.as"),
			ASTEST_AS(R"AS(
			mixin void MarkSource(ACoverageMixinConflictParent Self)
			{
				Self.RouteValue = 10;
			}

			mixin void MarkSource(ACoverageMixinConflictChild Self)
			{
				Self.RouteValue = 20;
				Self.ChildRouteValue = 30;
			}

			mixin void AddLayer(ACoverageMixinConflictParent Self, int Amount)
			{
				Self.LayerTotal += Amount;
			}

			mixin void AddLayer(ACoverageMixinConflictChild Self, int Amount)
			{
				Self.LayerTotal += Amount * 10;
				Self.ChildLayerTotal += Amount;
			}

			UCLASS()
			class ACoverageMixinConflictParent : AActor
			{
				UPROPERTY()
				int RouteValue = 0;

				UPROPERTY()
				int LayerTotal = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					this.MarkSource();
					this.AddLayer(2);
				}
			}

			UCLASS()
			class ACoverageMixinConflictChild : ACoverageMixinConflictParent
			{
				UPROPERTY()
				int ChildRouteValue = 0;

				UPROPERTY()
				int ChildLayerTotal = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					this.MarkSource();
					this.AddLayer(3);
				}
			}

			UCLASS()
			class ACoverageMixinConflictGrandchild : ACoverageMixinConflictChild
			{
				UPROPERTY()
				int GrandchildRouteValue = 0;

				UPROPERTY()
				int GrandchildLayerTotal = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// A grandchild descends from BOTH receiver types, so an auto-dispatched
					// `this.MarkSource()` is ambiguous (AS overload resolution does not rank by
					// inheritance distance when neither overload is an exact match). Select the
					// nearest (child) overload through an explicit child-typed receiver view.
					ACoverageMixinConflictChild ChildView = this;
					ChildView.MarkSource();
					ChildView.AddLayer(4);
					GrandchildRouteValue = ChildRouteValue + RouteValue;
					GrandchildLayerTotal = ChildLayerTotal + LayerTotal;
				}
			}
			)AS"),
			TEXT("ACoverageMixinConflictParent"));
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("mixin conflict parent should compile")));
		if (ParentClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageMixinConflictChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("mixin conflict child should compile")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(ParentClass), TEXT("mixin conflict child should inherit the parent receiver type")));

		UClass* GrandchildClass = FindGeneratedClass(&Engine, TEXT("ACoverageMixinConflictGrandchild"));
		ASSERT_THAT(IsNotNull(GrandchildClass, TEXT("mixin conflict grandchild should compile")));
		if (GrandchildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(GrandchildClass->IsChildOf(ChildClass), TEXT("mixin conflict grandchild should inherit the child receiver type")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ParentActor = SpawnScriptActor(*TestRunner, Spawner, ParentClass);
		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		AActor* GrandchildActor = SpawnScriptActor(*TestRunner, Spawner, GrandchildClass);
		ASSERT_THAT(IsNotNull(ParentActor, TEXT("mixin conflict parent should spawn")));
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("mixin conflict child should spawn")));
		ASSERT_THAT(IsNotNull(GrandchildActor, TEXT("mixin conflict grandchild should spawn")));
		if (ParentActor == nullptr || ChildActor == nullptr || GrandchildActor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *ParentActor);
		BeginPlayActor(Engine, *ChildActor);
		BeginPlayActor(Engine, *GrandchildActor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ParentActor, TEXT("RouteValue"), 10,
			TEXT("parent receiver should select parent mixin overload"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ParentActor, TEXT("LayerTotal"), 2,
			TEXT("parent receiver should use parent overload arithmetic"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("RouteValue"), 20,
			TEXT("child receiver should prefer child-specific mixin overload"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("ChildRouteValue"), 30,
			TEXT("child-specific mixin overload should mutate child property"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("LayerTotal"), 30,
			TEXT("child receiver should resolve overloaded mixin with child receiver type"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("ChildLayerTotal"), 3,
			TEXT("child mixin overload should update child-only state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, GrandchildActor, TEXT("RouteValue"), 20,
			TEXT("grandchild receiver should inherit the nearest child mixin overload"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, GrandchildActor, TEXT("ChildRouteValue"), 30,
			TEXT("grandchild receiver should allow inherited child overload to mutate child state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, GrandchildActor, TEXT("LayerTotal"), 40,
			TEXT("grandchild receiver should resolve overloaded mixin through inherited child receiver type"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, GrandchildActor, TEXT("ChildLayerTotal"), 4,
			TEXT("grandchild mixin dispatch should update inherited child-only state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, GrandchildActor, TEXT("GrandchildRouteValue"), 50,
			TEXT("grandchild should observe inherited overload route state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, GrandchildActor, TEXT("GrandchildLayerTotal"), 44,
			TEXT("grandchild should observe inherited overload layer state"))));
	}

	TEST_METHOD(MixinConflictResolutionUsesExplicitBaseReceiverView)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_BaseReceiverConflict"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinBaseReceiverConflict.as"),
			ASTEST_AS(R"AS(
			mixin void RouteConflict(ACoverageMixinBaseReceiverParent Self)
			{
				Self.RouteTrace = Self.RouteTrace * 10 + 1;
				Self.ParentRouteCount += 1;
			}

			mixin void RouteConflict(ACoverageMixinBaseReceiverChild Self)
			{
				Self.RouteTrace = Self.RouteTrace * 10 + 2;
				Self.ChildRouteCount += 1;
			}

			UCLASS()
			class ACoverageMixinBaseReceiverParent : AActor
			{
				UPROPERTY()
				int RouteTrace = 0;

				UPROPERTY()
				int ParentRouteCount = 0;
			}

			UCLASS()
			class ACoverageMixinBaseReceiverChild : ACoverageMixinBaseReceiverParent
			{
				UPROPERTY()
				int ChildRouteCount = 0;

				UPROPERTY()
				int ParentViewTrace = 0;

				UPROPERTY()
				int ChildViewTrace = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ACoverageMixinBaseReceiverParent ParentView = Cast<ACoverageMixinBaseReceiverParent>(this);
					if (ParentView != nullptr)
					{
						ParentView.RouteConflict();
						ParentViewTrace = RouteTrace;
					}

					this.RouteConflict();
					ChildViewTrace = RouteTrace;
				}
			}
			)AS"),
			TEXT("ACoverageMixinBaseReceiverParent"));
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("mixin base receiver conflict parent should compile")));
		if (ParentClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageMixinBaseReceiverChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("mixin base receiver conflict child should compile")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(ParentClass), TEXT("mixin base receiver conflict child should inherit the parent receiver type")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("mixin base receiver conflict child should spawn")));
		if (ChildActor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *ChildActor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("ParentViewTrace"), 1,
			TEXT("explicit base receiver view should select the base mixin overload on a child instance"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("ChildViewTrace"), 12,
			TEXT("child receiver view should then select the child mixin overload on the same instance"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("ParentRouteCount"), 1,
			TEXT("base mixin overload should mutate base UPROPERTY state once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("ChildRouteCount"), 1,
			TEXT("child mixin overload should mutate child UPROPERTY state once"))));
	}

	TEST_METHOD(MixinDispatchesVirtualCallsToOverrides)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_VirtualDispatch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinVirtualDispatch.as"),
			ASTEST_AS(R"AS(
			mixin int ReadVirtualValue(ACoverageMixinVirtualParent Self)
			{
				return Self.GetVirtualValue();
			}

			mixin void StoreVirtualValue(ACoverageMixinVirtualParent Self)
			{
				Self.MixinResult = Self.ReadVirtualValue();
			}

			UCLASS()
			class ACoverageMixinVirtualParent : AActor
			{
				UPROPERTY()
				int MixinResult = 0;

				UPROPERTY()
				int DirectResult = 0;

				UFUNCTION(BlueprintEvent)
				int GetVirtualValue()
				{
					return 11;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					this.StoreVirtualValue();
					DirectResult = GetVirtualValue();
				}
			}

			UCLASS()
			class ACoverageMixinVirtualChild : ACoverageMixinVirtualParent
			{
				UFUNCTION(BlueprintOverride)
				int GetVirtualValue()
				{
					return 77;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					this.StoreVirtualValue();
					DirectResult = GetVirtualValue();
				}
			}
			)AS"),
			TEXT("ACoverageMixinVirtualParent"));
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("mixin virtual parent should compile")));
		if (ParentClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageMixinVirtualChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("mixin virtual child should compile")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(ParentClass), TEXT("mixin virtual child should inherit the parent receiver type")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ParentActor = SpawnScriptActor(*TestRunner, Spawner, ParentClass);
		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(ParentActor, TEXT("mixin virtual parent should spawn")));
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("mixin virtual child should spawn")));
		if (ParentActor == nullptr || ChildActor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *ParentActor);
		BeginPlayActor(Engine, *ChildActor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ParentActor, TEXT("MixinResult"), 11,
			TEXT("parent mixin virtual call should use parent implementation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ParentActor, TEXT("DirectResult"), 11,
			TEXT("parent direct virtual call should use parent implementation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("MixinResult"), 77,
			TEXT("mixin virtual call should dispatch to child override"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("DirectResult"), 77,
			TEXT("child direct virtual call should match mixin dispatch"))));
	}

	TEST_METHOD(MixinReadsAndWritesUPropertyBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_UPropertyBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinUPropertyBoundary.as"),
			ASTEST_AS(R"AS(
			mixin bool CopyLinkedScore(ACoverageMixinPropertyActor Self)
			{
				if (Self.LinkedActor == nullptr)
				{
					Self.bSawNullLinkedActor = true;
					return false;
				}

				Self.Score = Self.LinkedActor.Score + Self.Bonus;
				Self.LinkedActor.Score += 4;
				return true;
			}

			mixin void ApplyNameMarker(ACoverageMixinPropertyActor Self, FName Marker)
			{
				Self.Marker = Marker;
			}

			UCLASS()
			class ACoverageMixinPropertyActor : AActor
			{
				UPROPERTY()
				ACoverageMixinPropertyActor LinkedActor;

				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				int Bonus = 3;

				UPROPERTY()
				int LinkedScoreAfterMixin = 0;

				UPROPERTY()
				bool bSawNullLinkedActor = false;

				UPROPERTY()
				bool bCopiedLinkedScore = false;

				UPROPERTY()
				FName Marker = NAME_None;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					bCopiedLinkedScore = this.CopyLinkedScore();

					// Spawn a no-op peer subclass (not the same class) so the linked actor
					// does not recursively run BeginPlay/SpawnActor.
					LinkedActor = Cast<ACoverageMixinPropertyActor>(SpawnActor(ACoverageMixinPropertyPeerActor::StaticClass()));
					if (LinkedActor != nullptr)
					{
						LinkedActor.Score = 40;
						bCopiedLinkedScore = this.CopyLinkedScore();
						LinkedScoreAfterMixin = LinkedActor.Score;
						LinkedActor.DestroyActor();
					}

					this.ApplyNameMarker(n"MixinTouchedProperty");
				}
			}

			UCLASS()
			class ACoverageMixinPropertyPeerActor : ACoverageMixinPropertyActor
			{
				// No-op BeginPlay keeps the linked peer from recursively spawning.
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
				}
			}
			)AS"),
			TEXT("ACoverageMixinPropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("mixin UPROPERTY boundary actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("mixin UPROPERTY boundary actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSawNullLinkedActor"), true,
			TEXT("mixin should observe null UObject UPROPERTY boundary"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bCopiedLinkedScore"), true,
			TEXT("mixin should return success after UPROPERTY object reference is assigned"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Score"), 43,
			TEXT("mixin should read linked UPROPERTY state and write receiver UPROPERTY state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LinkedScoreAfterMixin"), 44,
			TEXT("mixin should mutate UPROPERTY object reference state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("Marker"), FName(TEXT("MixinTouchedProperty")),
			TEXT("mixin should write FName UPROPERTY state"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
