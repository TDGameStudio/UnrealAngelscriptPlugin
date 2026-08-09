// ============================================================================
// AngelscriptWorldCollisionFunctionLibraryComponentTests.cpp
//
// World collision component-query FunctionLibrary binding contract smoke.
// Detailed hit/miss parity lives in Coverage (`13-physics-collision`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionFunctionLibraryComponentTest,
	"Angelscript.TestModule.FunctionLibraries.WorldCollisionComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UBoxComponent* AddCollisionBox(
		AActor& Owner,
		FName ComponentName,
		const FVector& BoxExtent,
		const FVector& WorldLocation)
	{
		UBoxComponent* BoxComponent = NewObject<UBoxComponent>(&Owner, ComponentName);
		check(BoxComponent != nullptr);
		Owner.AddInstanceComponent(BoxComponent);
		Owner.SetRootComponent(BoxComponent);
		BoxComponent->RegisterComponent();
		BoxComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		BoxComponent->SetGenerateOverlapEvents(true);
		BoxComponent->SetBoxExtent(BoxExtent);
		BoxComponent->SetWorldLocation(WorldLocation);
		return BoxComponent;
	}

public:
	TEST_METHOD(ComponentQueryEntrypointSmoke)
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
			"ASWorldCollisionComponentEntrypointSmoke",
			ASTEST_AS(R"AS(
				int VerifyComponentQueryEntrypointSmoke(UPrimitiveComponent QueryComponent)
				{
					TArray<FHitResult> Hits;
					TArray<FOverlapResult> Overlaps;

					FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
					Params.AddIgnoredComponent(QueryComponent);

					FCollisionObjectQueryParams ObjectQueryParams;
					ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

					const bool bSweep = System::ComponentSweepMulti(
						Hits,
						QueryComponent,
						FVector(-200.0f, 0.0f, 0.0f),
						FVector(200.0f, 0.0f, 0.0f),
						FQuat::Identity,
						Params);

					const bool bOverlap = System::ComponentOverlapMulti(
						Overlaps,
						QueryComponent,
						FVector(0.0f, 150.0f, 0.0f),
						FQuat::Identity,
						Params,
						ObjectQueryParams);

					return bSweep
						&& bOverlap
						&& Hits.Num() > 0
						&& Overlaps.Num() > 0 ? 1 : 0;
				}

				int VerifyNullComponentQueryGuards()
				{
					TArray<FHitResult> Hits;
					TArray<FOverlapResult> Overlaps;

					FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
					FCollisionObjectQueryParams ObjectQueryParams;
					ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

					const bool bSweep = System::ComponentSweepMulti(
						Hits,
						nullptr,
						FVector(-200.0f, 0.0f, 0.0f),
						FVector(200.0f, 0.0f, 0.0f),
						FQuat::Identity,
						Params);

					const bool bOverlap = System::ComponentOverlapMulti(
						Overlaps,
						nullptr,
						FVector(0.0f, 150.0f, 0.0f),
						FQuat::Identity,
						Params,
						ObjectQueryParams);

					return !bSweep
						&& !bOverlap
						&& Hits.Num() == 0
						&& Overlaps.Num() == 0 ? 1 : 0;
				}
				)AS"));
		if (Module == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = AddCollisionBox(
			BlockingActor,
			FName(TEXT("WorldCollisionComponentContractBlocker")),
			FVector(50.0f, 50.0f, 50.0f),
			FVector::ZeroVector);
		AActor& OverlapActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* OverlapBox = AddCollisionBox(
			OverlapActor,
			FName(TEXT("WorldCollisionComponentContractOverlap")),
			FVector(40.0f, 40.0f, 40.0f),
			FVector(0.0f, 150.0f, 0.0f));
		AActor& QueryActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* QueryBox = AddCollisionBox(
			QueryActor,
			FName(TEXT("WorldCollisionComponentContractQuery")),
			FVector(30.0f, 30.0f, 30.0f),
			FVector(300.0f, 0.0f, 0.0f));
		ASSERT_THAT(IsNotNull(BlockingBox, TEXT("World collision component blocker should be created")));
		ASSERT_THAT(IsNotNull(OverlapBox, TEXT("World collision component overlap target should be created")));
		ASSERT_THAT(IsNotNull(QueryBox, TEXT("World collision component query should be created")));

		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		int32 SmokeResult = 0;
		if (!WorldCollisionExecuteIntFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int VerifyComponentQueryEntrypointSmoke(UPrimitiveComponent QueryComponent)"),
			[this, QueryBox](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("VerifyComponentQueryEntrypointSmoke"));
			},
			TEXT("VerifyComponentQueryEntrypointSmoke"),
			SmokeResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, SmokeResult, TEXT("Component sweep and overlap bindings should dispatch")));

		int32 NullGuardResult = 0;
		if (!WorldCollisionExecuteIntFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int VerifyNullComponentQueryGuards()"),
			[](asIScriptContext&) { return true; },
			TEXT("VerifyNullComponentQueryGuards"),
			NullGuardResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, NullGuardResult, TEXT("Component query null guards should remain exposed")));
	}
};

#endif
