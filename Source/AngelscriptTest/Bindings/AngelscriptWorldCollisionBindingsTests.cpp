// ============================================================================
// AngelscriptWorldCollisionBindingsTests.cpp
//
// World collision sync-query binding contract smoke. Hit/miss parity and stale
// output behavior live in Coverage (`13-physics-collision`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionBindingsTest,
	"Angelscript.TestModule.Bindings.WorldCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UBoxComponent* AddCollisionBox(
		AActor& Owner,
		const FName ComponentName,
		const FVector& BoxExtent,
		const FVector& WorldLocation)
	{
		UBoxComponent* BoxComponent = NewObject<UBoxComponent>(&Owner, ComponentName);
		check(BoxComponent != nullptr);

		Owner.AddInstanceComponent(BoxComponent);
		Owner.SetRootComponent(BoxComponent);
		BoxComponent->RegisterComponent();
		BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
		BoxComponent->SetCollisionResponseToAllChannels(ECR_Block);
		BoxComponent->SetGenerateOverlapEvents(true);
		BoxComponent->SetBoxExtent(BoxExtent);
		BoxComponent->SetWorldLocation(WorldLocation);
		return BoxComponent;
	}

public:
	TEST_METHOD(SyncQueryEntrypointSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"ASWorldCollisionSyncEntrypointSmoke",
			ASTEST_AS(R"AS(
				int VerifySyncQueryEntrypointSmoke(UPrimitiveComponent QueryComponent)
				{
					FHitResult LineHit;
					FHitResult SweepHit;
					TArray<FOverlapResult> Overlaps;
					FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));

					const bool bLineTraceCalled = System::LineTraceSingleByChannel(
						LineHit,
						FVector(-200.0f, 0.0f, 0.0f),
						FVector(200.0f, 0.0f, 0.0f),
						ECollisionChannel::ECC_Visibility);

					const bool bSweepCalled = System::SweepSingleByChannel(
						SweepHit,
						FVector(-200.0f, 0.0f, 0.0f),
						FVector(200.0f, 0.0f, 0.0f),
						FQuat::Identity,
						ECollisionChannel::ECC_Visibility,
						Shape);

					const bool bOverlapCalled = System::ComponentOverlapMultiByChannel(
						Overlaps,
						QueryComponent,
						FVector::ZeroVector,
						FQuat::Identity,
						ECollisionChannel::ECC_Visibility);

					return bLineTraceCalled
						&& bSweepCalled
						&& bOverlapCalled
						&& Overlaps.Num() > 0 ? 1 : 0;
				}
				)AS"));
		if (Module == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& TargetActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* TargetBox = AddCollisionBox(
			TargetActor,
			FName(TEXT("WorldCollisionContractTarget")),
			FVector(50.0f, 50.0f, 50.0f),
			FVector::ZeroVector);
		AActor& QueryActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* QueryBox = AddCollisionBox(
			QueryActor,
			FName(TEXT("WorldCollisionContractQuery")),
			FVector(30.0f, 30.0f, 30.0f),
			FVector(300.0f, 0.0f, 0.0f));

		ASSERT_THAT(IsNotNull(TargetBox, TEXT("World collision contract target component should be created")));
		ASSERT_THAT(IsNotNull(QueryBox, TEXT("World collision contract query component should be created")));

		FScopedTestWorldContextScope WorldContextScope(&TargetActor);

		int32 Result = 0;
		if (!WorldCollisionExecuteIntFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int VerifySyncQueryEntrypointSmoke(UPrimitiveComponent QueryComponent)"),
			[this, QueryBox](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("VerifySyncQueryEntrypointSmoke"));
			},
			TEXT("VerifySyncQueryEntrypointSmoke"),
			Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, Result, TEXT("LineTrace, Sweep, and Overlap sync query bindings should dispatch")));
	}
};

#endif
