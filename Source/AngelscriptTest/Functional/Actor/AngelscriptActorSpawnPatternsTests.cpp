#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (SpawnActor invocation patterns)
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorSpawnPatternsTests,
	"Angelscript.TestModule.Functional.Actor.SpawnPatterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MultipleSpawnSyntaxesProduceValidActors)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalActorSpawnPatterns"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* SourceActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalActorSpawnPatterns.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalSpawnTargetActor : AActor
{
	UPROPERTY()
	int32 TargetTag = 0;
}

UCLASS()
class AFunctionalSpawnSourceActor : AActor
{
	UPROPERTY()
	int32 PositionalSpawnedCount = 0;

	UPROPERTY()
	int32 NamedSpawnedCount = 0;

	UPROPERTY()
	int32 DeferredSpawnedCount = 0;

	UPROPERTY()
	int32 CastSpawnedCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		AFunctionalSpawnTargetActor PositionalSpawn = Cast<AFunctionalSpawnTargetActor>(SpawnActor(
			AFunctionalSpawnTargetActor::StaticClass(),
			FVector(100.0, 0.0, 0.0),
			FRotator::ZeroRotator));
		if (PositionalSpawn != nullptr) { PositionalSpawnedCount += 1; }

		AFunctionalSpawnTargetActor NamedSpawn = Cast<AFunctionalSpawnTargetActor>(SpawnActor(
			AFunctionalSpawnTargetActor::StaticClass(),
			Location = FVector(0.0, 100.0, 0.0),
			Rotation = FRotator::ZeroRotator));
		if (NamedSpawn != nullptr) { NamedSpawnedCount += 1; }

		AActor DeferredSpawn = SpawnActor(
			AFunctionalSpawnTargetActor::StaticClass(),
			Location = FVector(0.0, 0.0, 100.0),
			Rotation = FRotator::ZeroRotator,
			bDeferredSpawn = true);
		if (DeferredSpawn != nullptr)
		{
			AFunctionalSpawnTargetActor TypedDeferred = Cast<AFunctionalSpawnTargetActor>(DeferredSpawn);
			if (TypedDeferred != nullptr) { TypedDeferred.TargetTag = 99; }
			FinishSpawningActor(DeferredSpawn);
			DeferredSpawnedCount += 1;
		}

		TSubclassOf<AFunctionalSpawnTargetActor> TargetSubclass = AFunctionalSpawnTargetActor::StaticClass();
		AFunctionalSpawnTargetActor TypedSpawn = Cast<AFunctionalSpawnTargetActor>(SpawnActor(TargetSubclass));
		if (TypedSpawn != nullptr) { CastSpawnedCount += 1; }
	}
}
)AS"),
			TEXT("AFunctionalSpawnSourceActor"));
		if (SourceActorClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SourceActor = SpawnScriptActor(*TestRunner, Spawner, SourceActorClass);
		if (SourceActor == nullptr) { return; }
		BeginPlayActor(Engine, *SourceActor);

		auto VerifyCount = [&](const TCHAR* PropertyName, const TCHAR* Description)
		{
			int32 Count = 0;
			ReadPropertyValue<FIntProperty>(*TestRunner, SourceActor, PropertyName, Count);
			TestRunner->TestEqual(Description, Count, 1);
		};

		VerifyCount(TEXT("PositionalSpawnedCount"), TEXT("SpawnActor with positional Location/Rotation should succeed"));
		VerifyCount(TEXT("NamedSpawnedCount"), TEXT("SpawnActor with named Location/Rotation parameters should succeed"));
		VerifyCount(TEXT("DeferredSpawnedCount"), TEXT("SpawnActor with bDeferredSpawn = true followed by FinishSpawningActor should succeed"));
		VerifyCount(TEXT("CastSpawnedCount"), TEXT("SpawnActor with TSubclassOf and Cast should succeed"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
