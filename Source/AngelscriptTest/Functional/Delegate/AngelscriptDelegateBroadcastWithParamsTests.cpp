#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (event Broadcast / delegate IsBound+Execute)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDelegateBroadcastWithParamsTests,
	"Angelscript.TestModule.Functional.Delegate.BroadcastWithParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EventBroadcastFanOutsToAllListenersAndDelegateExecuteReturnsValue)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalDelegateBroadcastWithParams"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalDelegateBroadcastWithParams.as"),
			TEXT(R"AS(
event void FFunctionalDamageEvent(float Damage, bool bWasCrit, FVector Origin);
delegate bool FFunctionalCanInteract(int32 Querier);

UCLASS()
class AFunctionalBroadcastActor : AActor
{
	UPROPERTY()
	int32 ListenerOneCount = 0;

	UPROPERTY()
	int32 ListenerTwoCount = 0;

	UPROPERTY()
	float LastDamage = 0.0;

	UPROPERTY()
	bool LastCanInteractResult = false;

	UPROPERTY()
	bool DelegateWasBound = false;

	UPROPERTY()
	FFunctionalDamageEvent DamageEvent;

	UPROPERTY()
	FFunctionalCanInteract CanInteract;

	UFUNCTION()
	void HandleDamageOne(float Damage, bool bWasCrit, FVector Origin)
	{
		ListenerOneCount += 1;
		LastDamage = Damage;
	}

	UFUNCTION()
	void HandleDamageTwo(float Damage, bool bWasCrit, FVector Origin)
	{
		ListenerTwoCount += 1;
	}

	UFUNCTION()
	bool ResolveCanInteract(int32 Querier)
	{
		return Querier > 0;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		DamageEvent.AddUFunction(this, n"HandleDamageOne");
		DamageEvent.AddUFunction(this, n"HandleDamageTwo");
		DamageEvent.Broadcast(42.0, false, FVector(1.0, 2.0, 3.0));

		DamageEvent.Unbind(this, n"HandleDamageTwo");
		DamageEvent.Broadcast(7.0, true, FVector::ZeroVector);

		CanInteract.BindUFunction(this, n"ResolveCanInteract");
		DelegateWasBound = CanInteract.IsBound();
		LastCanInteractResult = CanInteract.Execute(3);
	}
}
)AS"),
			TEXT("AFunctionalBroadcastActor"));
		if (ActorClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		if (Actor == nullptr) { return; }
		BeginPlayActor(Engine, *Actor);

		int32 ListenerOneCount = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ListenerOneCount"), ListenerOneCount);
		TestRunner->TestEqual(TEXT("Listener One should fire on both broadcasts"), ListenerOneCount, 2);

		int32 ListenerTwoCount = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ListenerTwoCount"), ListenerTwoCount);
		TestRunner->TestEqual(TEXT("Listener Two should only fire on the first broadcast (unbound before second)"), ListenerTwoCount, 1);

		float LastDamage = 0.0f;
		FFloatProperty* DoubleAsFloat = FindFProperty<FFloatProperty>(ActorClass, TEXT("LastDamage"));
		FDoubleProperty* DoubleProp = FindFProperty<FDoubleProperty>(ActorClass, TEXT("LastDamage"));
		if (DoubleProp != nullptr)
		{
			TestRunner->TestEqual(TEXT("LastDamage from second broadcast should be 7.0"), DoubleProp->GetPropertyValue_InContainer(Actor), 7.0);
		}
		else if (DoubleAsFloat != nullptr)
		{
			TestRunner->TestEqual(TEXT("LastDamage from second broadcast should be 7.0"), DoubleAsFloat->GetPropertyValue_InContainer(Actor), 7.0f);
		}
		else
		{
			TestRunner->AddError(TEXT("LastDamage property not found as either FFloatProperty or FDoubleProperty"));
		}

		bool bDelegateWasBound = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("DelegateWasBound"), bDelegateWasBound);
		TestRunner->TestTrue(TEXT("CanInteract delegate should report IsBound after BindUFunction"), bDelegateWasBound);

		bool bLastCanInteractResult = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("LastCanInteractResult"), bLastCanInteractResult);
		TestRunner->TestTrue(TEXT("CanInteract delegate Execute(3) should return true"), bLastCanInteractResult);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
