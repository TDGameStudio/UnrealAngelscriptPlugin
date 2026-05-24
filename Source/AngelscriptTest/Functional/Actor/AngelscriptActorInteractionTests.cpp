#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorInteractionTest,
	"Angelscript.TestModule.Actor.Interaction",
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

	TEST_METHOD(PointDamage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorPointDamage"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorPointDamage.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorPointDamage : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	float LastFloatValue = 0.0;

	UPROPERTY()
	FVector LastVectorValue = FVector::ZeroVector;

	UPROPERTY()
	FName LastNameValue;

	UFUNCTION(BlueprintOverride)
	void PointDamage(float Damage, const UDamageType DamageType, FVector HitLocation,
		FVector HitNormal, UPrimitiveComponent HitComponent, FName BoneName,
		FVector ShotFromDirection, AController InstigatedBy,
		AActor DamageCauser, FHitResult HitInfo)
	{
		LastFloatValue = Damage;
		LastVectorValue = HitLocation;
		LastNameValue = BoneName;
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorPointDamage"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		const FVector ExpectedHitLocation(100.f, 200.f, 300.f);
		const FName ExpectedBoneName(TEXT("spine_01"));
		FHitResult HitResult;
		HitResult.Location = ExpectedHitLocation;
		HitResult.ImpactPoint = ExpectedHitLocation;
		HitResult.BoneName = ExpectedBoneName;
		UGameplayStatics::ApplyPointDamage(Actor, 42.0f, FVector::ForwardVector, HitResult, nullptr, nullptr, nullptr);

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("LastFloatValue"), 42.0,
			TEXT("PointDamage should route the applied damage value into LastFloatValue"));
		{
			FVector ActualHitLocation = FVector::ZeroVector;
			if (!GetStructByPath<FVector>(*TestRunner, Actor, TEXT("LastVectorValue"), ActualHitLocation)) return;
			TestRunner->TestTrue(TEXT("PointDamage should route the hit location into LastVectorValue"),
				ActualHitLocation.Equals(ExpectedHitLocation));
		}
		VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("LastNameValue"), ExpectedBoneName,
			TEXT("PointDamage should route the bone name into LastNameValue"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("PointDamage should fire exactly once"));
	}

	TEST_METHOD(RadialDamage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorRadialDamage"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorRadialDamage.as"),
			TEXT(R"AS(
UCLASS()
class UTestActorRadialDamageSphere : USphereComponent
{
}

UCLASS()
class ATestActorRadialDamage : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestActorRadialDamageSphere DamageSphere;

	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	float LastFloatValue = 0.0;

	UPROPERTY()
	FVector LastVectorValue = FVector::ZeroVector;

	default DamageSphere.SetSphereRadius(64.0f);

	UFUNCTION(BlueprintOverride)
	void RadialDamage(float DamageReceived, const UDamageType DamageType, FVector Origin,
		FHitResult HitInfo, AController InstigatedBy, AActor DamageCauser)
	{
		LastFloatValue = DamageReceived;
		LastVectorValue = Origin;
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorRadialDamage"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		TArray<AActor*> IgnoredActors;
		const bool bAppliedDamage = UGameplayStatics::ApplyRadialDamage(
			&W.GetWorld(), 24.0f, Actor->GetActorLocation(), 128.0f,
			nullptr, IgnoredActors, nullptr, nullptr, true);
		if (!TestRunner->TestTrue(TEXT("RadialDamage should apply damage"), bAppliedDamage)) return;

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("LastFloatValue"), 24.0,
			TEXT("RadialDamage should route the applied damage value into LastFloatValue"));
		{
			FVector ActualOrigin = FVector::ZeroVector;
			if (!GetStructByPath<FVector>(*TestRunner, Actor, TEXT("LastVectorValue"), ActualOrigin)) return;
			TestRunner->TestTrue(TEXT("RadialDamage should route the explosion origin into LastVectorValue"),
				ActualOrigin.Equals(Actor->GetActorLocation()));
		}
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("RadialDamage should fire exactly once"));
	}

	TEST_METHOD(MultiSpawn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorMultiSpawn"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorMultiSpawn.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorMultiSpawn : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorMultiSpawn"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		TArray<AActor*> SpawnedActors;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AActor* Actor = W.SpawnActorOfClass(ScriptClass);
			if (!TestRunner->TestNotNull(TEXT("MultiSpawn actor should spawn"), Actor)) return;
			W.BeginPlay(*Actor);
			SpawnedActors.Add(Actor);
		}

		int32 TotalBeginPlayCount = 0;
		for (AActor* SpawnedActor : SpawnedActors)
		{
			int32 EventCallCount = 0;
			if (!GetByPath<FIntProperty, int32>(*TestRunner, SpawnedActor, TEXT("EventCallCount"), EventCallCount)) return;
			TotalBeginPlayCount += EventCallCount;
		}

		TestRunner->TestTrue(TEXT("MultiSpawn should execute BeginPlay on every spawned instance"), TotalBeginPlayCount >= 3);
		TestRunner->TestTrue(TEXT("MultiSpawn should create distinct actor instances"),
			SpawnedActors[0] != SpawnedActors[1] &&
			SpawnedActors[1] != SpawnedActors[2] &&
			SpawnedActors[0] != SpawnedActors[2]);
	}

	TEST_METHOD(CrossCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorCrossCall"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorAClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorCrossCall.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorCrossCallB : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UFUNCTION()
	void ReceiveCallFromA()
	{
		EventCallCount += 1;
	}
}

UCLASS()
class ATestActorCrossCallA : AActor
{
	UPROPERTY()
	ATestActorCrossCallB TargetActor;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		if (TargetActor != null)
		{
			TargetActor.ReceiveCallFromA();
		}
	}
}
)AS"),
			TEXT("ATestActorCrossCallA"));
		if (ActorAClass == nullptr) return;

		UClass* ActorBClass = FindGeneratedClass(&Engine, TEXT("ATestActorCrossCallB"));
		if (!TestRunner->TestNotNull(TEXT("CrossCall target class should be generated"), ActorBClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* ActorA = W.SpawnActorOfClass(ActorAClass);
		AActor* ActorB = W.SpawnActorOfClass(ActorBClass);
		if (!TestRunner->TestNotNull(TEXT("Actor A should spawn"), ActorA)
			|| !TestRunner->TestNotNull(TEXT("Actor B should spawn"), ActorB)) return;

		if (!SetObjectByPath(*TestRunner, ActorA, TEXT("TargetActor"), ActorB)) return;

		EnableActorTick(*ActorA);
		W.BeginPlay(*ActorA);
		W.BeginPlay(*ActorB);
		TickWorld(W.GetSpawner().GetWorld(), DefaultActorTestDeltaTime, 1);

		int32 EventCallCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, ActorB, TEXT("EventCallCount"), EventCallCount)) return;
		TestRunner->TestTrue(TEXT("CrossCall should let one script actor invoke another's UFUNCTION"), EventCallCount >= 1);
	}

	TEST_METHOD(AnyDamage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorAnyDamage"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorAnyDamage.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorAnyDamage : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	float LastFloatValue = 0.0;

	UPROPERTY()
	AActor LastActorRef = nullptr;

	UFUNCTION(BlueprintOverride)
	void AnyDamage(float Damage, const UDamageType DamageType, AController InstigatedBy, AActor DamageCauser)
	{
		LastFloatValue = Damage;
		LastActorRef = DamageCauser;
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorAnyDamage"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		UGameplayStatics::ApplyDamage(Actor, 55.0f, nullptr, Actor, nullptr);

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("LastFloatValue"), 55.0,
			TEXT("AnyDamage should route the applied damage value into LastFloatValue"));
		{
			UObject* DamageCauser = nullptr;
			if (!GetObjectByPath(*TestRunner, Actor, TEXT("LastActorRef"), DamageCauser)) return;
			TestRunner->TestEqual(TEXT("AnyDamage should pass the damage causer"),
				DamageCauser, static_cast<UObject*>(Actor));
		}
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("AnyDamage should fire exactly once"));
	}

	TEST_METHOD(ActorBeginOverlap)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorBeginOverlap"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ReceiverClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorBeginOverlap.as"),
			TEXT(R"AS(
UCLASS()
class ATestOverlapReceiver : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	AActor LastActorRef = nullptr;

	UFUNCTION(BlueprintOverride)
	void ActorBeginOverlap(AActor OtherActor)
	{
		LastActorRef = OtherActor;
		EventCallCount += 1;
	}
}

UCLASS()
class ATestOverlapTrigger : AActor
{
}
)AS"),
			TEXT("ATestOverlapReceiver"));
		if (ReceiverClass == nullptr) return;

		UClass* TriggerClass = FindGeneratedClass(&Engine, TEXT("ATestOverlapTrigger"));
		if (!TestRunner->TestNotNull(TEXT("Trigger class should be generated"), TriggerClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Receiver = W.SpawnActorOfClass(ReceiverClass, FActorSpawnParameters(), FVector::ZeroVector);
		if (!TestRunner->TestNotNull(TEXT("Receiver should spawn"), Receiver)) return;
		AActor* Trigger = W.SpawnActorOfClass(TriggerClass);
		if (!TestRunner->TestNotNull(TEXT("Trigger should spawn"), Trigger)) return;

		W.BeginPlay(*Receiver);
		W.BeginPlay(*Trigger);
		VerifyByPath<FIntProperty, int32>(*TestRunner, Receiver, TEXT("EventCallCount"), 0,
			TEXT("ActorBeginOverlap should not fire before NotifyActorBeginOverlap"));

		Receiver->NotifyActorBeginOverlap(Trigger);

		int32 EventCallCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Receiver, TEXT("EventCallCount"), EventCallCount)) return;
		TestRunner->TestEqual(TEXT("ActorBeginOverlap should fire once"), EventCallCount, 1);
		{
			UObject* OtherActor = nullptr;
			if (!GetObjectByPath(*TestRunner, Receiver, TEXT("LastActorRef"), OtherActor)) return;
			TestRunner->TestEqual(TEXT("ActorBeginOverlap should pass the overlapping actor"),
				OtherActor, static_cast<UObject*>(Trigger));
		}
	}

	TEST_METHOD(ActorEndOverlap)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorEndOverlap"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ReceiverClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorEndOverlap.as"),
			TEXT(R"AS(
UCLASS()
class ATestEndOverlapReceiver : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	AActor LastActorRef = nullptr;

	UFUNCTION(BlueprintOverride)
	void ActorEndOverlap(AActor OtherActor)
	{
		LastActorRef = OtherActor;
		EventCallCount += 1;
	}
}

UCLASS()
class ATestEndOverlapTrigger : AActor
{
}
)AS"),
			TEXT("ATestEndOverlapReceiver"));
		if (ReceiverClass == nullptr) return;

		UClass* TriggerClass = FindGeneratedClass(&Engine, TEXT("ATestEndOverlapTrigger"));
		if (!TestRunner->TestNotNull(TEXT("Trigger class should be generated"), TriggerClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Receiver = W.SpawnActorOfClass(ReceiverClass);
		AActor* Trigger = W.SpawnActorOfClass(TriggerClass);
		if (!TestRunner->TestNotNull(TEXT("Receiver should spawn"), Receiver)
			|| !TestRunner->TestNotNull(TEXT("Trigger should spawn"), Trigger)) return;

		W.BeginPlay(*Receiver);
		W.BeginPlay(*Trigger);

		Receiver->NotifyActorEndOverlap(Trigger);

		int32 EventCallCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Receiver, TEXT("EventCallCount"), EventCallCount)) return;
		TestRunner->TestEqual(TEXT("ActorEndOverlap should fire once"), EventCallCount, 1);
		{
			UObject* OtherActor = nullptr;
			if (!GetObjectByPath(*TestRunner, Receiver, TEXT("LastActorRef"), OtherActor)) return;
			TestRunner->TestEqual(TEXT("ActorEndOverlap should pass the departing actor"),
				OtherActor, static_cast<UObject*>(Trigger));
		}
	}

	TEST_METHOD(ComponentBeginOverlapAddUFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestComponentBeginOverlapAddUFunction"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ReceiverClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestComponentBeginOverlapAddUFunction.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentBeginOverlapReceiver : USphereComponent
{
	UPROPERTY()
	int BeginOverlapCount = 0;

	UPROPERTY()
	AActor LastOtherActor = nullptr;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		OnComponentBeginOverlap.AddUFunction(this, n"HandleComponentBeginOverlap");
	}

	UFUNCTION()
	void HandleComponentBeginOverlap(
		UPrimitiveComponent OverlappedComponent,
		AActor OtherActor,
		UPrimitiveComponent OtherComponent,
		int OtherBodyIndex,
		bool bFromSweep,
		const FHitResult&in Hit)
	{
		LastOtherActor = OtherActor;
		BeginOverlapCount += 1;
	}
}

UCLASS()
class ATestComponentBeginOverlapOwner : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestComponentBeginOverlapReceiver Trigger;
}

UCLASS()
class ATestComponentBeginOverlapOther : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USphereComponent Trigger;
}
)AS"),
			TEXT("ATestComponentBeginOverlapOwner"));
		if (ReceiverClass == nullptr) return;

		UClass* OtherClass = FindGeneratedClass(&Engine, TEXT("ATestComponentBeginOverlapOther"));
		if (!TestRunner->TestNotNull(TEXT("Other overlap class should be generated"), OtherClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* ReceiverActor = W.SpawnActorOfClass(ReceiverClass);
		AActor* OtherActor = W.SpawnActorOfClass(OtherClass);
		if (!TestRunner->TestNotNull(TEXT("Receiver actor should spawn"), ReceiverActor)
			|| !TestRunner->TestNotNull(TEXT("Other actor should spawn"), OtherActor))
		{
			return;
		}

		UPrimitiveComponent* ReceiverComponent = Cast<UPrimitiveComponent>(ReceiverActor->GetRootComponent());
		UPrimitiveComponent* OtherComponent = Cast<UPrimitiveComponent>(OtherActor->GetRootComponent());
		if (!TestRunner->TestNotNull(TEXT("Receiver root should be a primitive component"), ReceiverComponent)
			|| !TestRunner->TestNotNull(TEXT("Other root should be a primitive component"), OtherComponent))
		{
			return;
		}

		W.BeginPlay(*ReceiverActor);
		W.BeginPlay(*OtherActor);

		FHitResult Hit;
		ReceiverComponent->OnComponentBeginOverlap.Broadcast(ReceiverComponent, OtherActor, OtherComponent, 0, false, Hit);

		VerifyByPath<FIntProperty, int32>(*TestRunner, ReceiverComponent, TEXT("BeginOverlapCount"), 1,
			TEXT("Component begin overlap delegate should invoke the AddUFunction script handler"));
		{
			UObject* LastOtherActor = nullptr;
			if (!GetObjectByPath(*TestRunner, ReceiverComponent, TEXT("LastOtherActor"), LastOtherActor)) return;
			TestRunner->TestEqual(TEXT("Component begin overlap handler should receive the other actor"),
				LastOtherActor, static_cast<UObject*>(OtherActor));
		}
	}

	TEST_METHOD(SpawnActorInvalidClassThrowsException)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorSpawnInvalidClass"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorSpawnInvalidClass.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorSpawnInvalidClass : AActor
{
	UFUNCTION()
	void RunSpawnInvalidClassTest()
	{
		AActor Spawned = SpawnActor(nullptr, FVector::ZeroVector, FRotator::ZeroRotator, n"InvalidSpawn");
	}
}
)AS"),
			TEXT("ATestActorSpawnInvalidClass"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		TestRunner->AddExpectedError(TEXT("Angelscript"), EAutomationExpectedErrorFlags::Contains, 0);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSpawnInvalidClassTest")));
		if (!Invoker.IsValid()) return;
		Invoker.Call();
	}

	TEST_METHOD(DelegateBroadcast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorDelegateBroadcast"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* BroadcasterClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorDelegateBroadcast.as"),
			TEXT(R"AS(
event void FOnTestEvent(float Value);

UCLASS()
class ATestDelegateBroadcaster : AActor
{
	UPROPERTY()
	FOnTestEvent OnTestEvent;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		OnTestEvent.Broadcast(99.0f);
	}
}

UCLASS()
class ATestDelegateListener : AActor
{
	UPROPERTY()
	ATestDelegateBroadcaster Broadcaster;

	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	float LastFloatValue = 0.0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		if (Broadcaster != null)
		{
			Broadcaster.OnTestEvent.AddUFunction(this, n"HandleTestEvent");
		}
	}

	UFUNCTION()
	void HandleTestEvent(float Value)
	{
		LastFloatValue = Value;
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestDelegateBroadcaster"));
		if (BroadcasterClass == nullptr) return;

		UClass* ListenerClass = FindGeneratedClass(&Engine, TEXT("ATestDelegateListener"));
		if (!TestRunner->TestNotNull(TEXT("Listener class should be generated"), ListenerClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Broadcaster = W.SpawnActorOfClass(BroadcasterClass);
		AActor* Listener = W.SpawnActorOfClass(ListenerClass);
		if (!TestRunner->TestNotNull(TEXT("Broadcaster should spawn"), Broadcaster)
			|| !TestRunner->TestNotNull(TEXT("Listener should spawn"), Listener)) return;

		if (!SetObjectByPath(*TestRunner, Listener, TEXT("Broadcaster"), Broadcaster)) return;

		EnableActorTick(*Broadcaster);
		W.BeginPlay(*Broadcaster);
		W.BeginPlay(*Listener);
		TickWorld(W.GetSpawner().GetWorld(), DefaultActorTestDeltaTime, 1);

		int32 EventCallCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Listener, TEXT("EventCallCount"), EventCallCount)) return;
		TestRunner->TestTrue(TEXT("DelegateBroadcast should let the listener receive at least one broadcast"), EventCallCount >= 1);
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Listener, TEXT("LastFloatValue"), 99.0,
			TEXT("DelegateBroadcast should pass the correct value through the delegate"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
