#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/ReferenceChainSearch.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_GC_AngelscriptGCTestCaseTests_Private
{
	using namespace AngelscriptFunctionalTestUtils;

	void InitializeGCTestCaseSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}

	void LogReferenceChainIfAlive(FAutomationTestBase& Test, const UObject* Obj, const TCHAR* Context)
	{
		if (!Obj)
		{
			return;
		}
		FReferenceChainSearch Search(const_cast<UObject*>(Obj), EReferenceChainSearchMode::Shortest);
		FString ChainReport = Search.GetRootPath();
		if (ChainReport.IsEmpty())
		{
			Test.AddWarning(FString::Printf(TEXT("[GC Diagnostic] %s: object %s still alive but no external reference chain found (may be held by internal root)"),
				Context, *Obj->GetPathName()));
		}
		else
		{
			Test.AddWarning(FString::Printf(TEXT("[GC Diagnostic] %s: object %s still alive. Reference chain:\n%s"),
				Context, *Obj->GetPathName(), *ChainReport));
		}
	}

	template <typename ComponentType = UActorComponent>
	ComponentType* CreateGCTestCaseScriptComponent(
		FAutomationTestBase& Test,
		AActor& OwnerActor,
		UClass* ComponentClass,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should compile to a valid component class"), Context), ComponentClass))
		{
			return nullptr;
		}

		UActorComponent* Component = NewObject<UActorComponent>(&OwnerActor, ComponentClass);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should instantiate a runtime component"), Context), Component))
		{
			return nullptr;
		}

		OwnerActor.AddInstanceComponent(Component);
		Component->OnComponentCreated();
		Component->RegisterComponent();
		Component->Activate(true);

		ComponentType* TypedComponent = Cast<ComponentType>(Component);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should produce the expected component base type"), Context), TypedComponent))
		{
			return nullptr;
		}

		return TypedComponent;
	}
}


TEST_CLASS_WITH_FLAGS(
	FAngelscriptGCTest,
	"Angelscript.TestModule.GC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ActorDestroy)
	{
		using namespace AngelscriptTest_GC_AngelscriptGCTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestGCActorDestroy"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestGCActorDestroy.as"),
			TEXT(R"AS(
UCLASS()
class ATestGCActorDestroy : AActor
{
}
)AS"),
			TEXT("ATestGCActorDestroy"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		FActorTestSpawner Spawner;
		InitializeGCTestCaseSpawner(Spawner);
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor));
		BeginPlayActor(Engine, *Actor);

		TWeakObjectPtr<AActor> WeakActor = Actor;
		Actor->Destroy();
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);
		CollectGarbage(RF_NoFlags, true);

		if (WeakActor.IsValid())
		{
			LogReferenceChainIfAlive(*TestRunner, WeakActor.Get(), TEXT("GC.ActorDestroy"));
		}
		TestRunner->TestTrue(TEXT("TestCase GC actor destroy should complete without leaving a live actor reference"), !WeakActor.IsValid());
		}
	}

	TEST_METHOD(ComponentDestroy)
	{
		using namespace AngelscriptTest_GC_AngelscriptGCTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestGCComponentDestroy"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ComponentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestGCComponentDestroy.as"),
			TEXT(R"AS(
UCLASS()
class UTestGCComponentDestroy : UActorComponent
{
}
)AS"),
			TEXT("UTestGCComponentDestroy"));
		ASSERT_THAT(IsNotNull(ComponentClass));

		FActorTestSpawner Spawner;
		InitializeGCTestCaseSpawner(Spawner);
		AActor& HostActor = Spawner.SpawnActor<AActor>();
		UActorComponent* Component = CreateGCTestCaseScriptComponent(*TestRunner, HostActor, ComponentClass, TEXT("GC.ComponentDestroy"));
		ASSERT_THAT(IsNotNull(Component));

		TWeakObjectPtr<UActorComponent> WeakComponent = Component;
		Component->DestroyComponent();
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);
		CollectGarbage(RF_NoFlags, true);

		if (WeakComponent.IsValid())
		{
			LogReferenceChainIfAlive(*TestRunner, WeakComponent.Get(), TEXT("GC.ComponentDestroy"));
		}
		TestRunner->TestTrue(TEXT("TestCase GC component destroy should complete without leaving a live component reference"), !WeakComponent.IsValid());
		}
	}

	TEST_METHOD(WorldTeardown)
	{
		using namespace AngelscriptTest_GC_AngelscriptGCTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestGCWorldTeardown"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestGCWorldTeardown.as"),
			TEXT(R"AS(
UCLASS()
class ATestGCWorldTeardownActor : AActor
{
}

UCLASS()
class UTestGCWorldTeardownComponent : UActorComponent
{
}
)AS"),
			TEXT("ATestGCWorldTeardownActor"));
		ASSERT_THAT(IsNotNull(ActorClass));

		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UTestGCWorldTeardownComponent"));
		ASSERT_THAT(IsNotNull(ComponentClass));

		TWeakObjectPtr<UWorld> WeakWorld;
		TWeakObjectPtr<AActor> WeakActor;
		TWeakObjectPtr<UActorComponent> WeakComponent;
		{
			FActorTestSpawner Spawner;
			InitializeGCTestCaseSpawner(Spawner);
			WeakWorld = &Spawner.GetWorld();

			AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
			ASSERT_THAT(IsNotNull(Actor));
			BeginPlayActor(Engine, *Actor);

			UActorComponent* Component = CreateGCTestCaseScriptComponent(*TestRunner, *Actor, ComponentClass, TEXT("GC.WorldTeardown"));
			ASSERT_THAT(IsNotNull(Component));

			WeakActor = Actor;
			WeakComponent = Component;
		}

		CollectGarbage(RF_NoFlags, true);

		if (WeakWorld.IsValid())
		{
			LogReferenceChainIfAlive(*TestRunner, WeakWorld.Get(), TEXT("GC.WorldTeardown.World"));
		}
		if (WeakActor.IsValid())
		{
			LogReferenceChainIfAlive(*TestRunner, WeakActor.Get(), TEXT("GC.WorldTeardown.Actor"));
		}
		if (WeakComponent.IsValid())
		{
			LogReferenceChainIfAlive(*TestRunner, WeakComponent.Get(), TEXT("GC.WorldTeardown.Component"));
		}
		TestRunner->TestTrue(TEXT("TestCase GC world teardown should release the world after scope cleanup"), !WeakWorld.IsValid());
		TestRunner->TestTrue(TEXT("TestCase GC world teardown should release spawned actors after scope cleanup"), !WeakActor.IsValid());
		TestRunner->TestTrue(TEXT("TestCase GC world teardown should release spawned components after scope cleanup"), !WeakComponent.IsValid());
		}
	}
};

#endif
