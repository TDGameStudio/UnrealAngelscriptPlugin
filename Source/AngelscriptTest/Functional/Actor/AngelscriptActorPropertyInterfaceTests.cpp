#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

namespace AngelscriptActorPropertyInterfaceTestHelpers
{
	int32 CallScriptIntFunction(FAutomationTestBase& Test, AActor* Actor, FName FunctionName)
	{
		FFunctionInvoker Invoker(Test, Actor, FunctionName);
		if (!Invoker.IsValid()) return INDEX_NONE;
		return Invoker.CallAndReturn<int32>(INDEX_NONE);
	}

	int32 CallScriptIntFunctionWithPlayerController(
		FAutomationTestBase& Test, AActor* Actor, FName FunctionName, APlayerController* Controller)
	{
		FFunctionInvoker Invoker(Test, Actor, FunctionName);
		if (!Invoker.IsValid()) return INDEX_NONE;
		Invoker.AddParam<APlayerController*>(Controller);
		return Invoker.CallAndReturn<int32>(INDEX_NONE);
	}

	int32 CallScriptIntFunctionWithInstigator(
		FAutomationTestBase& Test, AActor* Actor, FName FunctionName,
		APawn* InstigatorPawn, AController* InstigatorController)
	{
		FFunctionInvoker Invoker(Test, Actor, FunctionName);
		if (!Invoker.IsValid()) return INDEX_NONE;
		Invoker.AddParam<APawn*>(InstigatorPawn);
		Invoker.AddParam<AController*>(InstigatorController);
		return Invoker.CallAndReturn<int32>(INDEX_NONE);
	}
}

using namespace AngelscriptActorPropertyInterfaceTestHelpers;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorPropertyInterfaceTest,
	"Angelscript.TestModule.Actor.PropertyInterface",
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

	// --- Property Tests (from AngelscriptActorPropertyTests.cpp) ---

	TEST_METHOD(UProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorUProperty"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorUProperty.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorUProperty : AActor
{
	UPROPERTY()
	int Health = 100;

	UPROPERTY()
	FString DisplayName = "TestActor";
}
)AS"),
			TEXT("ATestActorUProperty"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Health"), 100,
			TEXT("Script-defined int UPROPERTY should keep its default value after spawn"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("DisplayName"), FString(TEXT("TestActor")),
			TEXT("Script-defined FString UPROPERTY should keep its default value after spawn"));
	}

	TEST_METHOD(UFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorUFunction"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorUFunction.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorUFunction : AActor
{
	UPROPERTY()
	int Health = 100;

	UFUNCTION()
	int GetHealth()
	{
		return Health;
	}
}
)AS"),
			TEXT("ATestActorUFunction"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("GetHealth")));
		if (!Invoker.IsValid()) return;
		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(100, Result, TEXT("Script-defined UFUNCTION should return the scripted property value")));
	}

	TEST_METHOD(DefaultValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorDefaultValues"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorDefaultValues.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorDefaultValues : AActor
{
	default PrimaryActorTick.TickInterval = 0.5f;
}
)AS"),
			TEXT("ATestActorDefaultValues"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		ASSERT_THAT(IsNear(
			0.5f,
			Actor->PrimaryActorTick.TickInterval,
			KINDA_SMALL_NUMBER,
			TEXT("Script default values should apply the configured tick interval")));
	}

	// --- Interface Tests (from AngelscriptActorInterfaceTests.cpp) ---

	TEST_METHOD(InterfaceBoundMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorInterfaceBoundMethods"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorInterfaceBoundMethods.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorInterfaceBoundMethods : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UFUNCTION()
	int CheckBeforeBeginPlay()
	{
		if (!IsActorInitialized())
			return 10;
		if (HasActorBegunPlay())
			return 20;
		if (!IsHidden())
			return 30;
		if (!GetActorLocation().Equals(FVector(10.0, 20.0, 30.0)))
			return 40;
		if (!GetActorRotation().Equals(FRotator(5.0, 45.0, 15.0), 0.01))
			return 50;

		SetActorScale3D(FVector(2.0, 3.0, 4.0));
		SetActorTickInterval(0.25f);

		if (GetActorNameOrLabel().Len() <= 0)
			return 60;
		if (!IsValid(GetGameInstance()))
			return 70;

		return 1;
	}

	UFUNCTION()
	int CheckInstigator(APawn ExpectedPawn, AController ExpectedController)
	{
		if (GetActorInstigator() != ExpectedPawn)
			return 100;
		if (GetActorInstigatorController() != ExpectedController)
			return 110;

		return 1;
	}

	UFUNCTION()
	int CheckAfterBeginPlay()
	{
		if (!HasActorBegunPlay())
			return 80;
		if (!GetActorLocation().Equals(FVector(10.0, 20.0, 30.0)))
			return 90;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorInterfaceBoundMethods"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass, FActorSpawnParameters(),
			FVector(10.0, 20.0, 30.0), FRotator(5.0, 45.0, 15.0));
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));

		Actor->SetActorHiddenInGame(true);

		ASSERT_THAT(AreEqual(
			1,
			CallScriptIntFunction(*TestRunner, Actor, TEXT("CheckBeforeBeginPlay")),
			TEXT("AActor bound methods should report expected pre-BeginPlay state")));
		ASSERT_THAT(IsTrue(
			Actor->GetActorScale3D().Equals(FVector(2.0, 3.0, 4.0)),
			TEXT("SetActorScale3D binding should update native actor scale")));
		ASSERT_THAT(IsNear(
			0.25f,
			Actor->PrimaryActorTick.TickInterval,
			KINDA_SMALL_NUMBER,
			TEXT("SetActorTickInterval binding should update native tick interval")));

		APawn& InstigatorPawn = W.GetSpawner().SpawnActor<APawn>();
		APlayerController& InstigatorController = W.GetSpawner().SpawnActor<APlayerController>();
		InstigatorController.Possess(&InstigatorPawn);
		Actor->SetInstigator(&InstigatorPawn);

		ASSERT_THAT(AreEqual(
			1,
			CallScriptIntFunctionWithInstigator(*TestRunner, Actor, TEXT("CheckInstigator"), &InstigatorPawn, &InstigatorController),
			TEXT("AActor instigator bindings should return native instigator references")));

		W.BeginPlay(*Actor);
		ASSERT_THAT(AreEqual(
			1,
			CallScriptIntFunction(*TestRunner, Actor, TEXT("CheckAfterBeginPlay")),
			TEXT("AActor bound methods should report expected post-BeginPlay state")));
	}

	TEST_METHOD(InterfaceComponentAndInput)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorInterfaceComponentAndInput"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorInterfaceComponentAndInput.as"),
			TEXT(R"AS(
UCLASS()
class UTestActorInterfaceRootComponent : USceneComponent
{
}

UCLASS()
class UTestActorInterfaceExtraComponent : USceneComponent
{
}

UCLASS()
class ATestActorInterfaceComponentAndInput : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestActorInterfaceRootComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UTestActorInterfaceExtraComponent ExtraScene;

	UFUNCTION()
	int CheckComponentsAndInput(APlayerController Controller)
	{
		TArray<USceneComponent> SceneComponents;
		GetComponentsByClass(SceneComponents);
		if (SceneComponents.Num() != 2)
			return 10;

		TArray<UTestActorInterfaceExtraComponent> ExtraComponents;
		GetComponentsByClass(UTestActorInterfaceExtraComponent::StaticClass(), ExtraComponents);
		if (ExtraComponents.Num() != 1)
			return 20;

		TArray<UActorComponent> ActorComponents;
		GetComponentsByClass(USceneComponent::StaticClass(), ActorComponents);
		if (ActorComponents.Num() != 2)
			return 30;

		if (GetInputComponent() != nullptr)
			return 40;
		EnableInput(Controller);
		if (GetInputComponent() == nullptr)
			return 50;
		DisableInput(Controller);

		return 1;
	}
}
)AS"),
			TEXT("ATestActorInterfaceComponentAndInput"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));

		APlayerController& PlayerController = W.GetSpawner().SpawnActor<APlayerController>();
		ASSERT_THAT(AreEqual(
			1,
			CallScriptIntFunctionWithPlayerController(*TestRunner, Actor, TEXT("CheckComponentsAndInput"), &PlayerController),
			TEXT("AActor component and input bindings should operate from script")));
	}

	TEST_METHOD(InterfaceSpawnAndQuery)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorInterfaceSpawnAndQuery"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorInterfaceSpawnAndQuery.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorInterfaceSpawned : AActor
{
	default Tags.Add(n"ActorInterfaceSpawned");

	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY()
	int Marker = 7;
}

UCLASS()
class ATestActorInterfaceSpawnAndQuery : AActor
{
	UFUNCTION()
	int RunSpawnAndQuery()
	{
		AActor NativeSpawned = AActor::Spawn(FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator, n"ActorInterfaceNativeSpawned");
		if (!IsValid(NativeSpawned))
			return 10;

		AActor GenericSpawned = SpawnActor(ATestActorInterfaceSpawned::StaticClass(), FVector(200.0, 0.0, 0.0), FRotator::ZeroRotator, n"ActorInterfaceGenericSpawned");
		if (!IsValid(GenericSpawned))
			return 20;

		AActor DeferredSpawned = SpawnActor(ATestActorInterfaceSpawned::StaticClass(), FVector(300.0, 0.0, 0.0), FRotator::ZeroRotator, n"ActorInterfaceDeferredSpawned", true);
		if (!IsValid(DeferredSpawned))
			return 30;
		FinishSpawningActor(DeferredSpawned);
		if (!DeferredSpawned.GetActorLocation().Equals(FVector(300.0, 0.0, 0.0)))
			return 40;

		AActor DeferredTransformSpawned = SpawnActor(ATestActorInterfaceSpawned::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, n"ActorInterfaceDeferredTransformSpawned", true);
		if (!IsValid(DeferredTransformSpawned))
			return 45;
		FinishSpawningActor(DeferredTransformSpawned, FTransform(FRotator::ZeroRotator, FVector(350.0, 0.0, 0.0), FVector::OneVector));
		if (!DeferredTransformSpawned.GetActorLocation().Equals(FVector(350.0, 0.0, 0.0)))
			return 46;

		AActor PersistentSpawned = SpawnPersistentActor(ATestActorInterfaceSpawned::StaticClass(), FVector(400.0, 0.0, 0.0), FRotator::ZeroRotator, n"ActorInterfacePersistentSpawned");
		if (!IsValid(PersistentSpawned))
			return 50;

		AActor PersistentDeferredSpawned = SpawnPersistentActor(ATestActorInterfaceSpawned::StaticClass(), FVector(450.0, 0.0, 0.0), FRotator::ZeroRotator, n"ActorInterfacePersistentDeferredSpawned", true);
		if (!IsValid(PersistentDeferredSpawned))
			return 55;
		FinishSpawningActor(PersistentDeferredSpawned);
		if (!PersistentDeferredSpawned.GetActorLocation().Equals(FVector(450.0, 0.0, 0.0)))
			return 56;

		TArray<ATestActorInterfaceSpawned> TypedActors;
		GetAllActorsOfClass(TypedActors);
		if (TypedActors.Num() < 5)
			return 60;

		TArray<AActor> ExplicitClassActors;
		GetAllActorsOfClass(ATestActorInterfaceSpawned::StaticClass(), ExplicitClassActors);
		if (ExplicitClassActors.Num() < 5)
			return 70;

		TArray<AActor> TaggedActors;
		GetAllActorsOfClassWithTag(n"ActorInterfaceSpawned", TaggedActors);
		if (TaggedActors.Num() < 5)
			return 80;

		TArray<AActor> InternalClassActors;
		__Actor_GetAllByClass(ATestActorInterfaceSpawned::StaticClass(), InternalClassActors);
		if (InternalClassActors.Num() < 5)
			return 90;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorInterfaceSpawnAndQuery"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));

		W.BeginPlay(*Actor);
		ASSERT_THAT(AreEqual(
			1,
			CallScriptIntFunction(*TestRunner, Actor, TEXT("RunSpawnAndQuery")),
			TEXT("AActor spawn and world query bindings should operate from script")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
