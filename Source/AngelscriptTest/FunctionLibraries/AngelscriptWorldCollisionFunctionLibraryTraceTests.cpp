// ============================================================================
// AngelscriptWorldCollisionFunctionLibraryTraceTests.cpp
//
// World collision trace FunctionLibrary binding contract smoke. Physics
// behavior coverage lives in Coverage (`13-physics-collision`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestModuleScope.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionTraceBindingsTest,
	"Angelscript.TestModule.FunctionLibraries.WorldCollisionTrace",
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
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(TraceFunctionLibraryEntrypointSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_EntrypointSmoke"), ASTEST_AS(R"AS(
			int VerifyTraceFunctionLibraryEntrypointSmoke()
			{
				FHitResult LineHit;
				FHitResult SweepHit;
				TArray<FHitResult> MultiHits;
				TArray<FOverlapResult> Overlaps;

				FCollisionObjectQueryParams ObjectQueryParams;
				ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

				const FCollisionShape SweepShape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
				const FCollisionShape OverlapShape = FCollisionShape::MakeBox(FVector(45.0f, 45.0f, 45.0f));

				const bool bLineSingle = System::LineTraceSingleByChannel(
					LineHit,
					FVector(-200.0f, 0.0f, 0.0f),
					FVector(200.0f, 0.0f, 0.0f),
					ECollisionChannel::ECC_Visibility);

				const bool bLineMulti = System::LineTraceMultiByChannel(
					MultiHits,
					FVector(-200.0f, 0.0f, 0.0f),
					FVector(200.0f, 0.0f, 0.0f),
					ECollisionChannel::ECC_Visibility);

				const bool bSweep = System::SweepSingleByObjectType(
					SweepHit,
					FVector(-200.0f, 0.0f, 0.0f),
					FVector(200.0f, 0.0f, 0.0f),
					FQuat::Identity,
					ObjectQueryParams,
					SweepShape);

				const bool bOverlap = System::OverlapMultiByProfile(
					Overlaps,
					FVector(0.0f, 150.0f, 0.0f),
					FQuat::Identity,
					CollisionProfile::BlockAllDynamic,
					OverlapShape);

				return bLineSingle
					&& bLineMulti
					&& bSweep
					&& bOverlap
					&& MultiHits.Num() > 0
					&& Overlaps.Num() > 0 ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = AddCollisionBox(
			BlockingActor,
			FName(TEXT("WorldCollisionTraceContractBlocker")),
			FVector(50.0f, 50.0f, 50.0f),
			FVector::ZeroVector);
		AActor& OverlapActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* OverlapBox = AddCollisionBox(
			OverlapActor,
			FName(TEXT("WorldCollisionTraceContractOverlap")),
			FVector(40.0f, 40.0f, 40.0f),
			FVector(0.0f, 150.0f, 0.0f));
		ASSERT_THAT(IsNotNull(BlockingBox, TEXT("World collision trace blocker should be created")));
		ASSERT_THAT(IsNotNull(OverlapBox, TEXT("World collision trace overlap target should be created")));

		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTraceFunctionLibraryEntrypointSmoke()"),
			TEXT("Trace, sweep, and overlap function-library bindings should dispatch"),
			1)));
	}
};

#endif
