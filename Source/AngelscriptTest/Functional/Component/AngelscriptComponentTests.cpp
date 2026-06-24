#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

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
	static constexpr float ComponentTestCaseDeltaTime = 0.016f;

	static void InitializeComponentTestCaseSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}

	template <typename ComponentType = UActorComponent>
	static ComponentType* CreateComponentTestCaseScriptComponent(
		FAutomationTestBase& Test,
		AActor& OwnerActor,
		UClass* ComponentClass,
		const TCHAR* Context)
	{
		if (!CheckNotNull(Test, *FString::Printf(TEXT("%s should compile to a valid component class"), Context), ComponentClass))
		{
			return nullptr;
		}

		UActorComponent* Component = NewObject<UActorComponent>(&OwnerActor, ComponentClass);
		if (!CheckNotNull(Test, *FString::Printf(TEXT("%s should instantiate a runtime component"), Context), Component))
		{
			return nullptr;
		}

		OwnerActor.AddInstanceComponent(Component);
		Component->OnComponentCreated();
		Component->RegisterComponent();
		Component->Activate(true);

		ComponentType* TypedComponent = Cast<ComponentType>(Component);
		if (!CheckNotNull(Test, *FString::Printf(TEXT("%s should produce the expected component base type"), Context), TypedComponent))
		{
			return nullptr;
		}

		return TypedComponent;
	}

	static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bActual, Message);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Value, Message);
	}

	TEST_METHOD(BeginPlay)
	{
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
class UTestComponentBeginPlay : UActorComponent
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

		ASSERT_THAT(IsTrue(bReady, TEXT("TestCase component BeginPlay should set the readiness flag")));
		}
	}

	TEST_METHOD(Tick)
	{
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
class UTestComponentTick : UActorComponent
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

		ASSERT_THAT(IsTrue(TickCount >= 5, TEXT("TestCase component Tick should run during manual world ticking")));
		}
	}

	TEST_METHOD(ReceiveEndPlay)
	{
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
class UTestComponentReceiveEndPlay : UActorComponent
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

		ASSERT_THAT(IsTrue(bCleanedUp, TEXT("TestCase component EndPlay should run when the owning actor is destroyed")));
		}
	}

	TEST_METHOD(ActorOwner)
	{
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
class UTestComponentActorOwner : UActorComponent
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
		if (!CheckNotNull(*TestRunner, TEXT("TestCase component owner-access class should be generated"), ComponentClass)) { return; }

		FActorTestSpawner Spawner;
		InitializeComponentTestCaseSpawner(Spawner);
		AActor* HostActor = SpawnScriptActor(*TestRunner, Spawner, OwnerActorClass);
		if (HostActor == nullptr) { return; }

		UActorComponent* Component = CreateComponentTestCaseScriptComponent(*TestRunner, *HostActor, ComponentClass, TEXT("Component.ActorOwner"));
		if (Component == nullptr) { return; }

		BeginPlayActor(Engine, *HostActor);

		int32 ReadOwnerValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Component, TEXT("ReadOwnerValue"), ReadOwnerValue)) { return; }

		ASSERT_THAT(AreEqual(42, ReadOwnerValue, TEXT("TestCase component should read the owning script actor's property in BeginPlay")));
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
	static void InitializeComponentTestCaseSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}

	static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bActual, Message);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Value, Message);
	}

	TEST_METHOD(Basic)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("TestCase default-component root class should be generated"), RootComponentClass)) { return; }

		USceneComponent* RootComponent = Actor->GetRootComponent();
		if (!CheckNotNull(*TestRunner, TEXT("TestCase actor should create a default root component"), RootComponent)) { return; }

		ASSERT_THAT(IsTrue(RootComponent->IsA(RootComponentClass), TEXT("TestCase actor root component should be the scripted default component")));
	}

	TEST_METHOD(Multiple)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("TestCase multi-default root class should be generated"), RootSceneClass)
			|| !CheckNotNull(*TestRunner, TEXT("TestCase multi-default child class should be generated"), BillboardClass))
		{ return; }

		USceneComponent* RootScene = Actor->GetRootComponent();
		if (!CheckNotNull(*TestRunner, TEXT("TestCase actor should create a scripted root scene component"), RootScene)) { return; }
		if (!CheckTrue(*TestRunner, TEXT("TestCase actor root component should use the scripted root component class"), RootScene->IsA(RootSceneClass))) { return; }

		UBillboardComponent* Billboard = nullptr;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsA(BillboardClass))
			{
				Billboard = Cast<UBillboardComponent>(Component);
				break;
			}
		}
		if (!CheckNotNull(*TestRunner, TEXT("TestCase actor should create the attached billboard component"), Billboard)) { return; }

		ASSERT_THAT(IsTrue(Billboard->GetAttachParent() == RootScene, TEXT("TestCase actor attached default component should preserve the scripted hierarchy")));
	}

	TEST_METHOD(NativeTypes)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("TestCase actor should create a native static mesh root component"), RootScene)) { return; }
		if (!CheckTrue(*TestRunner, TEXT("TestCase actor root component should use UStaticMeshComponent"), RootScene->IsA(UStaticMeshComponent::StaticClass()))) { return; }

		UBillboardComponent* Billboard = nullptr;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (UBillboardComponent* TypedBillboard = Cast<UBillboardComponent>(Component))
			{
				Billboard = TypedBillboard;
				break;
			}
		}
		if (!CheckNotNull(*TestRunner, TEXT("TestCase actor should create the native billboard component"), Billboard)) { return; }

		ASSERT_THAT(IsTrue(Billboard->GetAttachParent() == RootScene, TEXT("TestCase actor native billboard component should attach to the native mesh root")));
	}

	TEST_METHOD(DeepAttachChain)
	{
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

		if (!CheckTrue(*TestRunner, TEXT("Deep attach chain should compile"), bCompiled)) { return; }

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ADeepAttachActor"));
		if (!CheckNotNull(*TestRunner, TEXT("Class should be materialized"), GeneratedClass)) { return; }

		FProperty* MidProp = GeneratedClass->FindPropertyByName(TEXT("MidScene"));
		FProperty* LeafProp = GeneratedClass->FindPropertyByName(TEXT("LeafScene"));
		ASSERT_THAT(IsNotNull(MidProp, TEXT("MidScene property should exist on class")));
		ASSERT_THAT(IsNotNull(LeafProp, TEXT("LeafScene property should exist on class")));
		}
	}

	TEST_METHOD(OverrideComponentMultiLayerInheritance)
	{
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

		if (!CheckTrue(*TestRunner, TEXT("Multi-layer OverrideComponent should compile"), bCompiled)) { return; }

		UClass* TopClass = FindGeneratedClass(&Engine, TEXT("ATopLayerActor"));
		ASSERT_THAT(IsNotNull(TopClass, TEXT("Top layer class should be materialized")));
		}
	}

	TEST_METHOD(NativeActorWithExtraScriptComponent)
	{
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

		if (!CheckTrue(*TestRunner, TEXT("Native actor with extra script component should compile"), bCompiled)) { return; }

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AExtendedCharacter"));
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Extended character class should be materialized")));
		}
	}

	TEST_METHOD(OverrideComponentMetadataMultiLayer)
	{
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

		if (!CheckTrue(*TestRunner, TEXT("Override metadata multi-layer should compile"), bCompiled)) { return; }

		UClass* TopClass = FindGeneratedClass(&Engine, TEXT("AMetaTopActor"));
		if (!CheckNotNull(*TestRunner, TEXT("Top class should exist"), TopClass)) { return; }

		UClass* MidClass = FindGeneratedClass(&Engine, TEXT("AMetaMidActor"));
		if (!CheckNotNull(*TestRunner, TEXT("Mid class should exist"), MidClass)) { return; }

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("AMetaBaseActor"));
		if (!CheckNotNull(*TestRunner, TEXT("Base class should exist"), BaseClass)) { return; }

		ASSERT_THAT(IsTrue(TopClass->IsChildOf(MidClass), TEXT("Top class should inherit from Mid")));
		ASSERT_THAT(IsTrue(MidClass->IsChildOf(BaseClass), TEXT("Mid class should inherit from Base")));
		}
	}
};

#endif
