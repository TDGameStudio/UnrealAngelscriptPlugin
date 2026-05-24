#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptComponent.h"
#include "Components/ActorTestSpawner.h"
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestGCActorDestroyTest,
	"Angelscript.TestModule.GC.ActorDestroy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestGCComponentDestroyTest,
	"Angelscript.TestModule.GC.ComponentDestroy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestGCWorldTeardownTest,
	"Angelscript.TestModule.GC.WorldTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTestGCActorDestroyTest::RunTest(const FString& Parameters)
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
		*this,
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
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeGCTestCaseSpawner(Spawner);
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	TWeakObjectPtr<AActor> WeakActor = Actor;
	Actor->Destroy();
	TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);
	CollectGarbage(RF_NoFlags, true);

	if (WeakActor.IsValid())
	{
		LogReferenceChainIfAlive(*this, WeakActor.Get(), TEXT("GC.ActorDestroy"));
	}
	TestTrue(TEXT("TestCase GC actor destroy should complete without leaving a live actor reference"), !WeakActor.IsValid());
	}

	return true;
}

bool FAngelscriptTestGCComponentDestroyTest::RunTest(const FString& Parameters)
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
		*this,
		Engine,
		ModuleName,
		TEXT("TestGCComponentDestroy.as"),
		TEXT(R"AS(
UCLASS()
class UTestGCComponentDestroy : UAngelscriptComponent
{
}
)AS"),
		TEXT("UTestGCComponentDestroy"));
	if (ComponentClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeGCTestCaseSpawner(Spawner);
	AActor& HostActor = Spawner.SpawnActor<AActor>();
	UActorComponent* Component = CreateGCTestCaseScriptComponent(*this, HostActor, ComponentClass, TEXT("GC.ComponentDestroy"));
	if (Component == nullptr)
	{
		return false;
	}

	TWeakObjectPtr<UActorComponent> WeakComponent = Component;
	Component->DestroyComponent();
	TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);
	CollectGarbage(RF_NoFlags, true);

	if (WeakComponent.IsValid())
	{
		LogReferenceChainIfAlive(*this, WeakComponent.Get(), TEXT("GC.ComponentDestroy"));
	}
	TestTrue(TEXT("TestCase GC component destroy should complete without leaving a live component reference"), !WeakComponent.IsValid());
	}

	return true;
}

bool FAngelscriptTestGCWorldTeardownTest::RunTest(const FString& Parameters)
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
		*this,
		Engine,
		ModuleName,
		TEXT("TestGCWorldTeardown.as"),
		TEXT(R"AS(
UCLASS()
class ATestGCWorldTeardownActor : AActor
{
}

UCLASS()
class UTestGCWorldTeardownComponent : UAngelscriptComponent
{
}
)AS"),
		TEXT("ATestGCWorldTeardownActor"));
	if (ActorClass == nullptr)
	{
		return false;
	}

	UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UTestGCWorldTeardownComponent"));
	if (!TestNotNull(TEXT("TestCase GC world-teardown component class should exist"), ComponentClass))
	{
		return false;
	}

	TWeakObjectPtr<UWorld> WeakWorld;
	TWeakObjectPtr<AActor> WeakActor;
	TWeakObjectPtr<UActorComponent> WeakComponent;
	{
		FActorTestSpawner Spawner;
	InitializeGCTestCaseSpawner(Spawner);
		WeakWorld = &Spawner.GetWorld();

		AActor* Actor = SpawnScriptActor(*this, Spawner, ActorClass);
		if (Actor == nullptr)
		{
			return false;
		}
		BeginPlayActor(Engine, *Actor);

	UActorComponent* Component = CreateGCTestCaseScriptComponent(*this, *Actor, ComponentClass, TEXT("GC.WorldTeardown"));
		if (Component == nullptr)
		{
			return false;
		}

		WeakActor = Actor;
		WeakComponent = Component;
	}

	CollectGarbage(RF_NoFlags, true);

	if (WeakWorld.IsValid())
	{
		LogReferenceChainIfAlive(*this, WeakWorld.Get(), TEXT("GC.WorldTeardown.World"));
	}
	if (WeakActor.IsValid())
	{
		LogReferenceChainIfAlive(*this, WeakActor.Get(), TEXT("GC.WorldTeardown.Actor"));
	}
	if (WeakComponent.IsValid())
	{
		LogReferenceChainIfAlive(*this, WeakComponent.Get(), TEXT("GC.WorldTeardown.Component"));
	}
	TestTrue(TEXT("TestCase GC world teardown should release the world after scope cleanup"), !WeakWorld.IsValid());
	TestTrue(TEXT("TestCase GC world teardown should release spawned actors after scope cleanup"), !WeakActor.IsValid());
	TestTrue(TEXT("TestCase GC world teardown should release spawned components after scope cleanup"), !WeakComponent.IsValid());
	}

	return true;
}

#endif
