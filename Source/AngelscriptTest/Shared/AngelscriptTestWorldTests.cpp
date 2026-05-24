#include "CQTest.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestWorld.h"

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// =============================================================================
// AngelscriptTestWorldTests.cpp
//
// Behavioural coverage for FAngelscriptTestWorld (Shared/AngelscriptTestWorld.h).
// Every TEST_METHOD compiles its own minimal AS module via the shared clone
// engine so the harness contract (engine scoping, world creation, spawn,
// BeginPlay dispatch, precise tick driving, destroy-and-drain) is locked in
// independently of any specific functional test.
// =============================================================================

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptTestWorldHarnessTest,
	"Angelscript.TestModule.Shared.TestWorld",
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

	// Construct the harness and verify it produces a valid world, exposes the
	// engine reference back, and pushes itself into the AS engine scope so
	// FAngelscriptEngine::TryGetCurrentEngine() resolves to the same engine.
	TEST_METHOD(ConstructionInitializesWorld)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();

		{
			FAngelscriptTestWorld W(*TestRunner, Engine);
			ASSERT_THAT(IsTrue(W.IsValid()));
			TestRunner->TestEqual(TEXT("[Construct] GetEngine() should return the supplied engine"),
				static_cast<const void*>(&W.GetEngine()), static_cast<const void*>(&Engine));
			TestRunner->TestNotNull(TEXT("[Construct] GetWorld() should return a valid UWorld"), &W.GetWorld());
			TestRunner->TestEqual(TEXT("[Construct] Harness should establish current AS engine scope"),
				static_cast<const void*>(FAngelscriptEngine::TryGetCurrentEngine()),
				static_cast<const void*>(&Engine));
		}
	}

	// Spawn a minimal AS actor through the harness and verify the returned
	// pointer participates in the harness world and is an instance of the
	// generated class.
	TEST_METHOD(SpawnActorOfClassReturnsValid)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestWorldHarnessSpawn"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestWorldHarnessSpawn.as"),
			TEXT(R"AS(
UCLASS()
class ATestWorldHarnessSpawnActor : AActor
{
}
)AS"),
			TEXT("ATestWorldHarnessSpawnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));
		TestRunner->TestTrue(TEXT("[Spawn] Returned actor should be an instance of the requested class"),
			Actor->IsA(ScriptClass));
		TestRunner->TestEqual(TEXT("[Spawn] Returned actor should belong to the harness world"),
			static_cast<const void*>(Actor->GetWorld()), static_cast<const void*>(&W.GetWorld()));
	}

	// Drive Actor->Tick exactly N times via DispatchActorTick and verify the
	// AS-side counter equals N (precise driving guarantee, no scheduler noise).
	TEST_METHOD(DispatchActorTickIsExact)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestWorldHarnessActorTick"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestWorldHarnessActorTick.as"),
			TEXT(R"AS(
UCLASS()
class ATestWorldHarnessActorTickActor : AActor
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("ATestWorldHarnessActorTickActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		W.BeginPlay(*Actor);

		constexpr int32 NumTicks = 5;
		W.DispatchActorTick(*Actor, 0.016f, NumTicks);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)));
		TestRunner->TestEqual(TEXT("[DispatchActorTick] AS TickCount should equal NumTicks"), TickCount, NumTicks);
	}

	// Drive Component->TickComponent exactly N times via DispatchComponentTick
	// and verify the AS-side counter equals N. Component must have
	// bCanEverTick + SetComponentTickEnabled enabled by the test fixture.
	TEST_METHOD(DispatchComponentTickIsExact)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestWorldHarnessComponentTick"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestWorldHarnessComponentTick.as"),
			TEXT(R"AS(
UCLASS()
class UTestWorldHarnessComponentTickComp : UAngelscriptComponent
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("UTestWorldHarnessComponentTickComp"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Host = W.SpawnActorOfClass<AActor>(AActor::StaticClass());
		ASSERT_THAT(IsNotNull(Host));

		UActorComponent* Component = NewObject<UActorComponent>(Host, ScriptClass);
		ASSERT_THAT(IsNotNull(Component));

		Host->AddInstanceComponent(Component);
		Component->OnComponentCreated();
		Component->RegisterComponent();
		Component->Activate(true);
		Component->PrimaryComponentTick.bCanEverTick = true;
		Component->SetComponentTickEnabled(true);

		W.BeginPlay(*Host);

		constexpr int32 NumTicks = 4;
		W.DispatchComponentTick(*Component, 0.016f, NumTicks);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Component, TEXT("TickCount"), TickCount)));
		TestRunner->TestEqual(TEXT("[DispatchComponentTick] AS TickCount should equal NumTicks"), TickCount, NumTicks);
	}

	// DestroyAndDrain should synchronously dispatch EndPlay (with reason
	// EEndPlayReason::Destroyed) followed by Destroyed; subsequent property
	// reads still work because the UObject memory is alive even though the
	// actor is PendingKill.
	TEST_METHOD(DestroyAndDrainTriggersEndPlayAndDestroyed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestWorldHarnessDestroyDrain"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestWorldHarnessDestroyDrain.as"),
			TEXT(R"AS(
UCLASS()
class ATestWorldHarnessDestroyDrainActor : AActor
{
	UPROPERTY()
	int EndPlayCount = 0;

	UPROPERTY()
	int DestroyedCount = 0;

	UPROPERTY()
	EEndPlayReason LastEndPlayReason = EEndPlayReason::Quit;

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		EndPlayCount += 1;
		LastEndPlayReason = Reason;
	}

	UFUNCTION(BlueprintOverride)
	void Destroyed()
	{
		DestroyedCount += 1;
	}
}
)AS"),
			TEXT("ATestWorldHarnessDestroyDrainActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		W.BeginPlay(*Actor);
		W.DestroyAndDrain(*Actor);

		int32 EndPlayCount = 0;
		int32 DestroyedCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("EndPlayCount"), EndPlayCount)));
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("DestroyedCount"), DestroyedCount)));

		TestRunner->TestEqual(TEXT("[DestroyAndDrain] EndPlay should fire exactly once"), EndPlayCount, 1);
		TestRunner->TestEqual(TEXT("[DestroyAndDrain] Destroyed should fire exactly once"), DestroyedCount, 1);

		int64 LastEndPlayReason = -1;
		ASSERT_THAT(IsTrue(GetEnumByPath(*TestRunner, Actor, TEXT("LastEndPlayReason"), LastEndPlayReason)));
		TestRunner->TestEqual(TEXT("[DestroyAndDrain] EndPlay reason should be EEndPlayReason::Destroyed"),
			LastEndPlayReason, static_cast<int64>(EEndPlayReason::Destroyed));
	}

	// Calling BeginPlay twice on the same actor must not redispatch BeginPlay
	// (the harness defers to AngelscriptFunctionalTestUtils::BeginPlayActor
	// which guards on AActor::HasActorBegunPlay()).
	TEST_METHOD(BeginPlayIsIdempotent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestWorldHarnessBeginPlayIdempotent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestWorldHarnessBeginPlayIdempotent.as"),
			TEXT(R"AS(
UCLASS()
class ATestWorldHarnessBeginPlayIdempotentActor : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}
}
)AS"),
			TEXT("ATestWorldHarnessBeginPlayIdempotentActor"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(W.IsValid()));

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));

		W.BeginPlay(*Actor);
		W.BeginPlay(*Actor);

		int32 BeginPlayCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)));
		TestRunner->TestEqual(TEXT("[BeginPlay] Repeated BeginPlay calls should not redispatch"), BeginPlayCount, 1);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
