#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private
{
	using namespace AngelscriptFunctionalTestUtils;

	constexpr float ComponentTestCaseDeltaTime = 0.016f;

	void InitializeComponentTestCaseSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}

	template <typename ComponentType = UActorComponent>
	ComponentType* CreateComponentTestCaseScriptComponent(
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

namespace DeepAttachChainTest
{
	static const FName ModuleName(TEXT("ASComponent.DeepAttachChain"));
	static const FString ScriptFilename(TEXT("ASComponent_DeepAttachChain.as"));
}

namespace OverrideMultiLayerTest
{
	static const FName ModuleName(TEXT("ASComponent.OverrideMultiLayer"));
	static const FString ScriptFilename(TEXT("ASComponent_OverrideMultiLayer.as"));
}

namespace NativeActorExtraComponentTest
{
	static const FName ModuleName(TEXT("ASComponent.NativeActorExtra"));
	static const FString ScriptFilename(TEXT("ASComponent_NativeActorExtra.as"));
}

namespace OverrideMetadataMultiLayerTest
{
	static const FName ModuleName(TEXT("ASComponent.OverrideMetadataMultiLayer"));
	static const FString ScriptFilename(TEXT("ASComponent_OverrideMetadataMultiLayer.as"));
}

// ============================================================================
// Component lifecycle tests (BeginPlay, Tick, EndPlay, ActorOwner)
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptComponentTests,
	"Angelscript.TestModule.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BeginPlay)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestComponentBeginPlay"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestComponentBeginPlay.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentBeginPlay : UAngelscriptComponent
{
	UPROPERTY()
	bool bReady = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		bReady = true;
	}
}
)AS"),
			TEXT("UTestComponentBeginPlay"));
		if (ScriptClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		AActor& HostActor = Spawner.SpawnActor<AActor>();
		UActorComponent* Component = CreateComponentTestCaseScriptComponent(*TestRunner, HostActor, ScriptClass, TEXT("Component.BeginPlay"));
		if (Component == nullptr) { return; }

		BeginPlayActor(Engine, HostActor);

		bool bReady = false;
		if (!ReadPropertyValue<FBoolProperty>(*TestRunner, Component, TEXT("bReady"), bReady)) { return; }

		TestRunner->TestTrue(TEXT("TestCase component BeginPlay should set the readiness flag"), bReady);
		}
	}

	TEST_METHOD(Tick)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestComponentTick"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestComponentTick.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentTick : UAngelscriptComponent
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
			TEXT("UTestComponentTick"));
		if (ScriptClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		AActor& HostActor = Spawner.SpawnActor<AActor>();
		UActorComponent* Component = CreateComponentTestCaseScriptComponent(*TestRunner, HostActor, ScriptClass, TEXT("Component.Tick"));
		if (Component == nullptr) { return; }

		Component->PrimaryComponentTick.bCanEverTick = true;
		Component->SetComponentTickEnabled(true);
		BeginPlayActor(Engine, HostActor);
		TickWorld(Engine, Spawner.GetWorld(), ComponentTestCaseDeltaTime, 5);

		int32 TickCount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Component, TEXT("TickCount"), TickCount)) { return; }

		TestRunner->TestTrue(TEXT("TestCase component Tick should run during manual world ticking"), TickCount >= 5);
		}
	}

	TEST_METHOD(ReceiveEndPlay)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestComponentReceiveEndPlay"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestComponentReceiveEndPlay.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentReceiveEndPlay : UAngelscriptComponent
{
	UPROPERTY()
	bool bCleanedUp = false;

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		bCleanedUp = true;
	}
}
)AS"),
			TEXT("UTestComponentReceiveEndPlay"));
		if (ScriptClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		AActor& HostActor = Spawner.SpawnActor<AActor>();
		UActorComponent* Component = CreateComponentTestCaseScriptComponent(*TestRunner, HostActor, ScriptClass, TEXT("Component.ReceiveEndPlay"));
		if (Component == nullptr) { return; }

		BeginPlayActor(Engine, HostActor);
		HostActor.Destroy();
		TickWorld(Engine, Spawner.GetWorld(), 0.0f, 1);

		bool bCleanedUp = false;
		if (!ReadPropertyValue<FBoolProperty>(*TestRunner, Component, TEXT("bCleanedUp"), bCleanedUp)) { return; }

		TestRunner->TestTrue(TEXT("TestCase component EndPlay should run when the owning actor is destroyed"), bCleanedUp);
		}
	}

	TEST_METHOD(ActorOwner)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestComponentActorOwner"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OwnerActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestComponentActorOwner.as"),
			TEXT(R"AS(
UCLASS()
class ATestComponentOwnerActor : AActor
{
	UPROPERTY()
	int OwnerValue = 42;
}

UCLASS()
class UTestComponentActorOwner : UAngelscriptComponent
{
	UPROPERTY()
	int ReadOwnerValue = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ATestComponentOwnerActor OwnerActor = Cast<ATestComponentOwnerActor>(GetOwner());
		if (OwnerActor != null)
		{
			ReadOwnerValue = OwnerActor.OwnerValue;
		}
	}
}
)AS"),
			TEXT("ATestComponentOwnerActor"));
		if (OwnerActorClass == nullptr) { return; }

		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UTestComponentActorOwner"));
		if (!TestRunner->TestNotNull(TEXT("TestCase component owner-access class should be generated"), ComponentClass)) { return; }

		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		AActor* HostActor = SpawnScriptActor(*TestRunner, Spawner, OwnerActorClass);
		if (HostActor == nullptr) { return; }

		UActorComponent* Component = CreateComponentTestCaseScriptComponent(*TestRunner, *HostActor, ComponentClass, TEXT("Component.ActorOwner"));
		if (Component == nullptr) { return; }

		BeginPlayActor(Engine, *HostActor);

		int32 ReadOwnerValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Component, TEXT("ReadOwnerValue"), ReadOwnerValue)) { return; }

		TestRunner->TestEqual(TEXT("TestCase component should read the owning script actor's property in BeginPlay"), ReadOwnerValue, 42);
		}
	}
};

// ============================================================================
// DefaultComponent tests (Basic, Multiple, NativeTypes, DeepAttach, Override, NativeActor, Metadata)
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptDefaultComponentTests,
	"Angelscript.TestModule.Component.DefaultComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		DestroySharedTestEngine();
		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(*TestRunner, TEXT("Default component test case tests require a production engine after subsystem initialization."));
		if (ProductionEngine == nullptr) { return; }
		FAngelscriptEngine& Engine = *ProductionEngine;
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentBasic"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestDefaultComponentBasic.as"),
			TEXT(R"AS(
UCLASS()
class UTestDefaultComponentBasicRoot : USceneComponent
{
}

UCLASS()
class ATestDefaultComponentBasic : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestDefaultComponentBasicRoot RootScene;
}
)AS"),
			TEXT("ATestDefaultComponentBasic"));
		if (ScriptClass == nullptr) { return; }

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr) { return; }

		BeginPlayActor(Engine, *Actor);

		UClass* RootComponentClass = FindGeneratedClass(&Engine, TEXT("UTestDefaultComponentBasicRoot"));
		if (!TestRunner->TestNotNull(TEXT("TestCase default-component root class should be generated"), RootComponentClass)) { return; }

		USceneComponent* RootComponent = Actor->GetRootComponent();
		if (!TestRunner->TestNotNull(TEXT("TestCase actor should create a default root component"), RootComponent)) { return; }

		TestRunner->TestTrue(TEXT("TestCase actor root component should be the scripted default component"), RootComponent->IsA(RootComponentClass));
	}

	TEST_METHOD(Multiple)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		DestroySharedTestEngine();
		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(*TestRunner, TEXT("Default component test case tests require a production engine after subsystem initialization."));
		if (ProductionEngine == nullptr) { return; }
		FAngelscriptEngine& Engine = *ProductionEngine;
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentMultiple"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestDefaultComponentMultiple.as"),
			TEXT(R"AS(
UCLASS()
class UTestDefaultComponentMultipleRoot : USceneComponent
{
}

UCLASS()
class UTestDefaultComponentMultipleBillboard : UBillboardComponent
{
}

UCLASS()
class ATestDefaultComponentMultiple : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestDefaultComponentMultipleRoot RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UTestDefaultComponentMultipleBillboard Billboard;
}
)AS"),
			TEXT("ATestDefaultComponentMultiple"));
		if (ScriptClass == nullptr) { return; }

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr) { return; }

		BeginPlayActor(Engine, *Actor);

		UClass* RootSceneClass = FindGeneratedClass(&Engine, TEXT("UTestDefaultComponentMultipleRoot"));
		UClass* BillboardClass = FindGeneratedClass(&Engine, TEXT("UTestDefaultComponentMultipleBillboard"));
		if (!TestRunner->TestNotNull(TEXT("TestCase multi-default root class should be generated"), RootSceneClass)
			|| !TestRunner->TestNotNull(TEXT("TestCase multi-default child class should be generated"), BillboardClass))
		{ return; }

		USceneComponent* RootScene = Actor->GetRootComponent();
		if (!TestRunner->TestNotNull(TEXT("TestCase actor should create a scripted root scene component"), RootScene)) { return; }
		if (!TestRunner->TestTrue(TEXT("TestCase actor root component should use the scripted root component class"), RootScene->IsA(RootSceneClass))) { return; }

		UBillboardComponent* Billboard = nullptr;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsA(BillboardClass))
			{
				Billboard = Cast<UBillboardComponent>(Component);
				break;
			}
		}
		if (!TestRunner->TestNotNull(TEXT("TestCase actor should create the attached billboard component"), Billboard)) { return; }

		TestRunner->TestTrue(TEXT("TestCase actor attached default component should preserve the scripted hierarchy"), Billboard->GetAttachParent() == RootScene);
	}

	TEST_METHOD(NativeTypes)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;
		DestroySharedTestEngine();
		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(*TestRunner, TEXT("Default component test case tests require a production engine after subsystem initialization."));
		if (ProductionEngine == nullptr) { return; }
		FAngelscriptEngine& Engine = *ProductionEngine;
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentNativeTypes"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestDefaultComponentNativeTypes.as"),
			TEXT(R"AS(
UCLASS()
class ATestDefaultComponentNativeTypes : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UStaticMeshComponent Mesh;

	UPROPERTY(DefaultComponent, Attach = Mesh)
	UBillboardComponent Billboard;
}
)AS"),
			TEXT("ATestDefaultComponentNativeTypes"));
		if (ScriptClass == nullptr) { return; }

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr) { return; }

		BeginPlayActor(Engine, *Actor);

		USceneComponent* RootScene = Actor->GetRootComponent();
		if (!TestRunner->TestNotNull(TEXT("TestCase actor should create a native static mesh root component"), RootScene)) { return; }
		if (!TestRunner->TestTrue(TEXT("TestCase actor root component should use UStaticMeshComponent"), RootScene->IsA(UStaticMeshComponent::StaticClass()))) { return; }

		UBillboardComponent* Billboard = nullptr;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (UBillboardComponent* TypedBillboard = Cast<UBillboardComponent>(Component))
			{
				Billboard = TypedBillboard;
				break;
			}
		}
		if (!TestRunner->TestNotNull(TEXT("TestCase actor should create the native billboard component"), Billboard)) { return; }

		TestRunner->TestTrue(TEXT("TestCase actor native billboard component should attach to the native mesh root"), Billboard->GetAttachParent() == RootScene);
	}

	TEST_METHOD(DeepAttachChain)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DeepAttachChainTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DeepAttachChainTest::ModuleName,
			DeepAttachChainTest::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class ADeepAttachActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	USceneComponent MidScene;

	UPROPERTY(DefaultComponent, Attach = MidScene)
	USceneComponent LeafScene;
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Deep attach chain should compile"), bCompiled)) { return; }

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ADeepAttachActor"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass)) { return; }

		FProperty* MidProp = GeneratedClass->FindPropertyByName(TEXT("MidScene"));
		FProperty* LeafProp = GeneratedClass->FindPropertyByName(TEXT("LeafScene"));
		TestRunner->TestNotNull(TEXT("MidScene property should exist on class"), MidProp);
		TestRunner->TestNotNull(TEXT("LeafScene property should exist on class"), LeafProp);
		}
	}

	TEST_METHOD(OverrideComponentMultiLayerInheritance)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*OverrideMultiLayerTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			OverrideMultiLayerTest::ModuleName,
			OverrideMultiLayerTest::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class ABaseLayerActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	USceneComponent BaseChild;
}

UCLASS()
class AMidLayerActor : ABaseLayerActor
{
	UPROPERTY(OverrideComponent = BaseChild)
	UStaticMeshComponent MidReplacement;
}

UCLASS()
class ATopLayerActor : AMidLayerActor
{
	UPROPERTY(DefaultComponent)
	USceneComponent TopExtra;
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Multi-layer OverrideComponent should compile"), bCompiled)) { return; }

		UClass* TopClass = FindGeneratedClass(&Engine, TEXT("ATopLayerActor"));
		TestRunner->TestNotNull(TEXT("Top layer class should be materialized"), TopClass);
		}
	}

	TEST_METHOD(NativeActorWithExtraScriptComponent)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*NativeActorExtraComponentTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			NativeActorExtraComponentTest::ModuleName,
			NativeActorExtraComponentTest::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class AExtendedCharacter : ACharacter
{
	UPROPERTY(DefaultComponent)
	USceneComponent ExtraMarker;
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Native actor with extra script component should compile"), bCompiled)) { return; }

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AExtendedCharacter"));
		TestRunner->TestNotNull(TEXT("Extended character class should be materialized"), GeneratedClass);
		}
	}

	TEST_METHOD(OverrideComponentMetadataMultiLayer)
	{
		using namespace AngelscriptTest_Component_AngelscriptComponentTestCaseTests_Private;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*OverrideMetadataMultiLayerTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			OverrideMetadataMultiLayerTest::ModuleName,
			OverrideMetadataMultiLayerTest::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class AMetaBaseActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	USceneComponent MetaChild;
}

UCLASS()
class AMetaMidActor : AMetaBaseActor
{
	UPROPERTY(OverrideComponent = MetaChild)
	UStaticMeshComponent MidMetaReplacement;
}

UCLASS()
class AMetaTopActor : AMetaMidActor
{
	UPROPERTY(DefaultComponent)
	USceneComponent TopMetaExtra;
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Override metadata multi-layer should compile"), bCompiled)) { return; }

		UClass* TopClass = FindGeneratedClass(&Engine, TEXT("AMetaTopActor"));
		if (!TestRunner->TestNotNull(TEXT("Top class should exist"), TopClass)) { return; }

		UClass* MidClass = FindGeneratedClass(&Engine, TEXT("AMetaMidActor"));
		if (!TestRunner->TestNotNull(TEXT("Mid class should exist"), MidClass)) { return; }

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("AMetaBaseActor"));
		if (!TestRunner->TestNotNull(TEXT("Base class should exist"), BaseClass)) { return; }

		TestRunner->TestTrue(TEXT("Top class should inherit from Mid"), TopClass->IsChildOf(MidClass));
		TestRunner->TestTrue(TEXT("Mid class should inherit from Base"), MidClass->IsChildOf(BaseClass));
		}
	}
};

#endif
