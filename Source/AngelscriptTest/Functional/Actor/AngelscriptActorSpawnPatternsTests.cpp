#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (SpawnActor invocation patterns)
#if WITH_ANGELSCRIPT_UNITTESTS


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
			ASSERT_THAT(AreEqual(1, Count, Description));
		};

		VerifyCount(TEXT("PositionalSpawnedCount"), TEXT("SpawnActor with positional Location/Rotation should succeed"));
		VerifyCount(TEXT("NamedSpawnedCount"), TEXT("SpawnActor with named Location/Rotation parameters should succeed"));
		VerifyCount(TEXT("DeferredSpawnedCount"), TEXT("SpawnActor with bDeferredSpawn = true followed by FinishSpawningActor should succeed"));
		VerifyCount(TEXT("CastSpawnedCount"), TEXT("SpawnActor with TSubclassOf and Cast should succeed"));
	}

	TEST_METHOD(SpawnParametersDriveTransformTypedAndDeferredSpawns)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalActorSpawnParameters"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* SourceActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalActorSpawnParameters.as"),
			TEXT(R"AS(
UCLASS()
class ASpawnParametersTargetActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY()
	int32 TargetTag = 0;
}

UCLASS()
class ASpawnParametersSourceActor : AActor
{
	UPROPERTY()
	int32 SpawnParametersResult = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = n"SpawnParametersTarget";
		Parameters.Owner = this;
		Parameters.NameMode = ESpawnActorNameMode::Requested;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.SetbDeferConstruction(true);

		FTransform SpawnTransform = FTransform(FRotator::ZeroRotator, FVector(125.0, 250.0, 375.0), FVector::OneVector);
		ASpawnParametersTargetActor Spawned = ASpawnParametersTargetActor::Spawn(SpawnTransform, Parameters);
		if (Spawned == nullptr)
		{
			SpawnParametersResult = 10;
			return;
		}
		if (Spawned.GetOwner() != this)
		{
			SpawnParametersResult = 20;
			return;
		}
		Spawned.TargetTag = 42;
		FinishSpawningActor(Spawned, SpawnTransform);
		if (!Spawned.GetActorLocation().Equals(FVector(125.0, 250.0, 375.0)))
		{
			SpawnParametersResult = 30;
			return;
		}
		if (Spawned.TargetTag != 42)
		{
			SpawnParametersResult = 40;
			return;
		}

		FActorSpawnParameters GlobalParameters = Parameters;
		GlobalParameters.Name = n"GlobalSpawnParametersTarget";
		GlobalParameters.SetbDeferConstruction(false);
		AActor GlobalSpawned = SpawnActor(ASpawnParametersTargetActor::StaticClass(), SpawnTransform, GlobalParameters);
		if (GlobalSpawned == nullptr || GlobalSpawned.GetOwner() != this)
		{
			SpawnParametersResult = 50;
			return;
		}

		FActorSpawnParameters WorldParameters;
		WorldParameters.Name = n"WorldSpawnParametersTarget";
		WorldParameters.Owner = this;
		AActor WorldSpawned = GetWorld().SpawnActor(ASpawnParametersTargetActor::StaticClass(), SpawnTransform, WorldParameters);
		if (WorldSpawned == nullptr || WorldSpawned.GetOwner() != this)
		{
			SpawnParametersResult = 60;
			return;
		}

		FActorSpawnParameters PersistentParameters;
		PersistentParameters.Name = n"PersistentSpawnParametersTarget";
		PersistentParameters.Owner = this;
		AActor PersistentSpawned = SpawnPersistentActor(ASpawnParametersTargetActor::StaticClass(), SpawnTransform, PersistentParameters);
		SpawnParametersResult = PersistentSpawned != nullptr && PersistentSpawned.GetOwner() == this ? 1 : 70;
	}
}
)AS"),
			TEXT("ASpawnParametersSourceActor"));
		ASSERT_THAT(IsNotNull(SourceActorClass, TEXT("Actor spawn-parameter API should compile")));
		if (SourceActorClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SourceActor = SpawnScriptActor(*TestRunner, Spawner, SourceActorClass);
		ASSERT_THAT(IsNotNull(SourceActor, TEXT("Spawn-parameter source actor should spawn")));
		if (SourceActor == nullptr) return;
		BeginPlayActor(Engine, *SourceActor);

		int32 SpawnParametersResult = INDEX_NONE;
		ReadPropertyValue<FIntProperty>(*TestRunner, SourceActor, TEXT("SpawnParametersResult"), SpawnParametersResult);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Spawn-parameter API result code: %d (1=success, 10/20/30/40/50/60/70 identify the failed stage)"),
			SpawnParametersResult));
		ASSERT_THAT(AreEqual(
			1,
			SpawnParametersResult,
			TEXT("FActorSpawnParameters should support typed, global, UWorld, and persistent Actor spawn APIs")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
