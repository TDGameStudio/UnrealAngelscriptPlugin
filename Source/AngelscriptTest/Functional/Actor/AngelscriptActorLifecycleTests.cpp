#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Engine/EngineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptReflectiveAccess;
using namespace AngelscriptActorTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorLifecycleTest,
	"Angelscript.TestModule.Actor.Lifecycle",
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

	TEST_METHOD(BeginPlay)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorBeginPlay"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorBeginPlay.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorBeginPlay : AActor
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
			TEXT("ATestActorBeginPlay"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("BeginPlay actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("BeginPlay should run exactly once when the script actor is spawned"));
	}

	TEST_METHOD(DefaultsAndHelperFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorDefaultsAndHelperFunction"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorDefaultsAndHelperFunction.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorDefaultsAndHelperFunction : AActor
{
	UPROPERTY()
	int Health = 125;

	UPROPERTY()
	FString DisplayName = "FunctionalActor";

	UPROPERTY()
	bool bBeginPlayTriggered = false;

	default SetReplicates(true);
	default Tags.Add(n"FunctionalActor");
	default PrimaryActorTick.TickInterval = 0.25;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		bBeginPlayTriggered = true;
	}

	UFUNCTION()
	int GetHealthValue()
	{
		return Health;
	}
}
)AS"),
			TEXT("ATestActorDefaultsAndHelperFunction"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		TestRunner->TestTrue(TEXT("Script default should enable replication"), Actor->GetIsReplicated());
		TestRunner->TestTrue(TEXT("Script default should add the expected actor tag"), Actor->ActorHasTag(TEXT("FunctionalActor")));
		TestRunner->TestTrue(TEXT("Script default should apply the configured tick interval"),
			FMath::IsNearlyEqual(Actor->PrimaryActorTick.TickInterval, 0.25f));

		W.BeginPlay(*Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Health"), 125,
			TEXT("Actor default should preserve reflected int value"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("DisplayName"), FString(TEXT("FunctionalActor")),
			TEXT("Actor default should preserve reflected string value"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBeginPlayTriggered"), true,
			TEXT("BeginPlay should set the reflected marker"));

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("GetHealthValue")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("Helper UFUNCTION should return the reflected Health value"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 125);
	}

	TEST_METHOD(BeginPlayIdempotent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorBeginPlayIdempotent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorBeginPlayIdempotent.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorBeginPlayIdempotent : AActor
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
			TEXT("ATestActorBeginPlayIdempotent"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		W.BeginPlay(*Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("BeginPlay helper should not dispatch BeginPlay again after the actor has begun play"));
	}

	TEST_METHOD(Tick)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorTick"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorTick.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorTick : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	float LastDeltaTime = 0.0f;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		EventCallCount += 1;
		LastDeltaTime = DeltaTime;
	}
}
)AS"),
			TEXT("ATestActorTick"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Tick actor should spawn"), Actor)) return;

		EnableActorTick(*Actor);
		W.BeginPlay(*Actor);
		W.Tick(DefaultActorTestDeltaTime, 5);

		int32 EventCallCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), EventCallCount)) return;
		TestRunner->TestTrue(TEXT("Tick should execute at least once per manual world tick"), EventCallCount >= 5);

		double LastDeltaTime = 0.0;
		if (!GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("LastDeltaTime"), LastDeltaTime)) return;
		TestRunner->TestTrue(TEXT("Tick should receive a positive DeltaTime"), LastDeltaTime > 0.0);
	}

	TEST_METHOD(TickRegisteredDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorTickRegisteredDispatch"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorTickRegisteredDispatch.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorTickRegisteredDispatch : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	float LastDeltaTime = 0.0f;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		EventCallCount += 1;
		LastDeltaTime = DeltaTime;
	}
}
)AS"),
			TEXT("ATestActorTickRegisteredDispatch"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		EnableActorTick(*Actor);
		W.BeginPlay(*Actor);
		TestRunner->TestTrue(TEXT("Actor should have a registered PrimaryActorTick"),
			Actor->PrimaryActorTick.IsTickFunctionRegistered());

		W.TickViaManager(DefaultActorTestDeltaTime, 3);

		int32 EventCallCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), EventCallCount)) return;
		TestRunner->TestTrue(TEXT("World tick manager should dispatch the registered script actor Tick at least once"), EventCallCount >= 1);

		double LastDeltaTime = 0.0;
		if (!GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("LastDeltaTime"), LastDeltaTime)) return;
		TestRunner->TestTrue(TEXT("World tick manager should pass a positive DeltaTime"), LastDeltaTime > 0.0);
	}

	TEST_METHOD(ReceiveEndPlay)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorReceiveEndPlay"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorReceiveEndPlay.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorReceiveEndPlay : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorReceiveEndPlay"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		Actor->Destroy();
		W.Tick(0.0f, 1);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("EndPlay should run exactly once when the script actor is destroyed"));
	}

	TEST_METHOD(ReceiveEndPlayReason)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorReceiveEndPlayReason"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorReceiveEndPlayReason.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorReceiveEndPlayReason : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	EEndPlayReason LastReason = EEndPlayReason::Quit;

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		LastReason = Reason;
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorReceiveEndPlayReason"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		Actor->Destroy();
		W.Tick(0.0f, 1);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("EndPlay should run exactly once before capturing the destroy reason"));

		int64 LastReason = -1;
		if (!GetEnumByPath(*TestRunner, Actor, TEXT("LastReason"), LastReason)) return;
		TestRunner->TestEqual(TEXT("EndPlay should receive EEndPlayReason::Destroyed"),
			LastReason, static_cast<int64>(EEndPlayReason::Destroyed));
	}

	TEST_METHOD(DestroyLifecycleOrder)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorDestroyLifecycleOrder"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorDestroyLifecycleOrder.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorDestroyLifecycleOrder : AActor
{
	UPROPERTY()
	int EndPlayCallCount = 0;

	UPROPERTY()
	int DestroyedCallCount = 0;

	UPROPERTY()
	int NextOrder = 0;

	UPROPERTY()
	int EndPlayOrder = 0;

	UPROPERTY()
	int DestroyedOrder = 0;

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		EndPlayCallCount += 1;
		NextOrder += 1;
		EndPlayOrder = NextOrder;
	}

	UFUNCTION(BlueprintOverride)
	void Destroyed()
	{
		DestroyedCallCount += 1;
		NextOrder += 1;
		DestroyedOrder = NextOrder;
	}
}
)AS"),
			TEXT("ATestActorDestroyLifecycleOrder"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		Actor->Destroy();
		W.Tick(0.0f, 1);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EndPlayCallCount"), 1,
			TEXT("Destroy() should dispatch EndPlay exactly once"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DestroyedCallCount"), 1,
			TEXT("Destroy() should dispatch Destroyed exactly once"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EndPlayOrder"), 1,
			TEXT("EndPlay should run before Destroyed during actor destruction"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DestroyedOrder"), 2,
			TEXT("Destroyed should run after EndPlay during actor destruction"));
	}

	TEST_METHOD(ReceiveDestroyed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorReceiveDestroyed"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorReceiveDestroyed.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorReceiveDestroyed : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UFUNCTION(BlueprintOverride)
	void Destroyed()
	{
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorReceiveDestroyed"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		Actor->Destroy();
		W.Tick(0.0f, 1);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("Destroyed should run exactly once when the script actor is destroyed"));
	}

	TEST_METHOD(ConstructionScript)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorConstructionScript"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorConstructionScript.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorConstructionScript : AActor
{
	UPROPERTY()
	int ConstructionCallCount = 0;

	UPROPERTY()
	int ValueA = 3;

	UPROPERTY()
	int ValueB = 4;

	UPROPERTY()
	int Product = 0;

	UFUNCTION(BlueprintOverride)
	void UserConstructionScript()
	{
		ConstructionCallCount += 1;
		Product = ValueA * ValueB;
	}
}
)AS"),
			TEXT("ATestActorConstructionScript"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ConstructionCallCount"), 1,
			TEXT("ConstructionScript should run once during actor spawn"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Product"), 12,
			TEXT("ConstructionScript should write derived property values during actor spawn"));

		if (!SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ValueA"), 5)
			|| !SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ValueB"), 6))
			return;

		{
			FAngelscriptEngineScope ActorScope(Engine, Actor);
			Actor->RerunConstructionScripts();
		}

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ConstructionCallCount"), 2,
			TEXT("RerunConstructionScripts should dispatch the script ConstructionScript again"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Product"), 30,
			TEXT("RerunConstructionScripts should recompute derived property values from reflected state"));
	}

	TEST_METHOD(Reset)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorReset"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorReset.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorReset : AActor
{
	UPROPERTY()
	int EventCallCount = 0;

	UPROPERTY()
	int ResetValue = 3;

	UFUNCTION(BlueprintOverride)
	void OnReset()
	{
		ResetValue = 7;
		EventCallCount += 1;
	}
}
)AS"),
			TEXT("ATestActorReset"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		if (!SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResetValue"), 99)) return;
		Actor->Reset();

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("OnReset should run exactly once when Reset() is called"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResetValue"), 7,
			TEXT("OnReset should write the expected sentinel value into ResetValue"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
