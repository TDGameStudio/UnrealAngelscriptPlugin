#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Actor_ComponentManagement_Private
{
	UActorComponent* FindComponentByName(AActor* Actor, FName ComponentName)
	{
		if (Actor == nullptr)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetFName() == ComponentName)
			{
				return Component;
			}
		}

		return nullptr;
	}

	int32 CountComponentsOfClass(AActor* Actor, UClass* ComponentClass)
	{
		if (Actor == nullptr || ComponentClass == nullptr)
		{
			return 0;
		}

		int32 Count = 0;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsA(ComponentClass))
			{
				++Count;
			}
		}

		return Count;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptActorComponentManagementTest,
	"Angelscript.TestModule.Actor.Component.Management",
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

	TEST_METHOD(CreateSceneComponentsRegistersRootAndAttachment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorComponentManagementCreateScene"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorComponentManagementCreateScene.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorComponentManagementCreateScene : AActor
{
	UPROPERTY()
	USceneComponent FirstCreated;

	UPROPERTY()
	USceneComponent SecondCreated;

	UFUNCTION()
	int CreateRuntimeComponents()
	{
		FirstCreated = Cast<USceneComponent>(CreateComponent(USceneComponent::StaticClass(), n"RuntimeRoot"));
		if (FirstCreated == nullptr)
			return 10;

		SecondCreated = Cast<USceneComponent>(CreateComponent(USceneComponent::StaticClass(), n"RuntimeChild"));
		if (SecondCreated == nullptr)
			return 20;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorComponentManagementCreateScene"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("CreateRuntimeComponents")));
		if (!Invoker.IsValid()) return;
		if (!TestRunner->TestEqual(TEXT("Script should create both scene components"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1)) return;

		UObject* FirstObject = nullptr;
		UObject* SecondObject = nullptr;
		if (!GetObjectByPath(*TestRunner, Actor, TEXT("FirstCreated"), FirstObject)
			|| !GetObjectByPath(*TestRunner, Actor, TEXT("SecondCreated"), SecondObject))
		{
			return;
		}

		USceneComponent* First = Cast<USceneComponent>(FirstObject);
		USceneComponent* Second = Cast<USceneComponent>(SecondObject);
		if (!TestRunner->TestNotNull(TEXT("First created component should be a scene component"), First)
			|| !TestRunner->TestNotNull(TEXT("Second created component should be a scene component"), Second))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("First runtime scene component should become actor root"), Actor->GetRootComponent(), First);
		TestRunner->TestEqual(TEXT("Second runtime scene component should attach to root"), Second->GetAttachParent(), First);
		TestRunner->TestTrue(TEXT("First runtime component should be registered"), First->IsRegistered());
		TestRunner->TestTrue(TEXT("Second runtime component should be registered"), Second->IsRegistered());
		TestRunner->TestEqual(TEXT("First runtime component should have the actor as owner"), First->GetOwner(), Actor);
		TestRunner->TestEqual(TEXT("Second runtime component should have the actor as owner"), Second->GetOwner(), Actor);
	}

	TEST_METHOD(StaticTypedAccessorsCreateGetAndReuse)
	{
		using namespace AngelscriptTest_Actor_ComponentManagement_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorComponentManagementTypedAccessors"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorComponentManagementTypedAccessors.as"),
			TEXT(R"AS(
UCLASS()
class UTestActorComponentManagementTypedScene : USceneComponent
{
}

UCLASS()
class ATestActorComponentManagementTypedAccessors : AActor
{
	UPROPERTY()
	UTestActorComponentManagementTypedScene Created;

	UPROPERTY()
	UTestActorComponentManagementTypedScene Found;

	UPROPERTY()
	UTestActorComponentManagementTypedScene Reused;

	UFUNCTION()
	int RunTypedAccessorTest()
	{
		Created = UTestActorComponentManagementTypedScene::Create(this, n"TypedScene");
		if (Created == nullptr)
			return 10;

		Found = UTestActorComponentManagementTypedScene::Get(this, n"TypedScene");
		if (Found == nullptr || Found != Created)
			return 20;

		Reused = UTestActorComponentManagementTypedScene::GetOrCreate(this, n"TypedScene");
		if (Reused == nullptr || Reused != Created)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorComponentManagementTypedAccessors"));
		if (ScriptClass == nullptr) return;

		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UTestActorComponentManagementTypedScene"));
		if (!TestRunner->TestNotNull(TEXT("Typed test component class should be generated"), ComponentClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunTypedAccessorTest")));
		if (!Invoker.IsValid()) return;
		if (!TestRunner->TestEqual(TEXT("Static typed component accessors should create, get, and reuse the component"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1)) return;

		UObject* CreatedObject = nullptr;
		UObject* FoundObject = nullptr;
		UObject* ReusedObject = nullptr;
		if (!GetObjectByPath(*TestRunner, Actor, TEXT("Created"), CreatedObject)
			|| !GetObjectByPath(*TestRunner, Actor, TEXT("Found"), FoundObject)
			|| !GetObjectByPath(*TestRunner, Actor, TEXT("Reused"), ReusedObject))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Created component should use the generated typed component class"), CreatedObject != nullptr && CreatedObject->IsA(ComponentClass));
		TestRunner->TestEqual(TEXT("Static Get should return the component created through Create"), FoundObject, CreatedObject);
		TestRunner->TestEqual(TEXT("Static GetOrCreate should reuse the existing named component"), ReusedObject, CreatedObject);
		TestRunner->TestEqual(TEXT("Only one typed component should exist after Create/Get/GetOrCreate"), CountComponentsOfClass(Actor, ComponentClass), 1);
	}

	TEST_METHOD(NameAndClassFilteringAreStrict)
	{
		using namespace AngelscriptTest_Actor_ComponentManagement_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorComponentManagementNameClassFilter"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorComponentManagementNameClassFilter.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorComponentManagementNameClassFilter : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UStaticMeshComponent MeshScene;

	UFUNCTION()
	int RunNameClassFilterTest()
	{
		if (GetComponent(UStaticMeshComponent::StaticClass(), n"RootScene") != nullptr)
			return 10;

		if (GetComponent(USceneComponent::StaticClass(), n"MissingScene") != nullptr)
			return 20;

		UActorComponent RootByExactName = GetComponent(USceneComponent::StaticClass(), n"RootScene");
		if (RootByExactName == nullptr || RootByExactName != RootScene)
			return 30;

		UActorComponent MeshByExactName = GetComponent(UStaticMeshComponent::StaticClass(), n"MeshScene");
		if (MeshByExactName == nullptr || MeshByExactName != MeshScene)
			return 40;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorComponentManagementNameClassFilter"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunNameClassFilterTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("GetComponent should require both class and name to match"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1);

		TestRunner->TestNotNull(TEXT("RootScene should exist as a component on the actor"), FindComponentByName(Actor, TEXT("RootScene")));
		TestRunner->TestNotNull(TEXT("MeshScene should exist as a component on the actor"), FindComponentByName(Actor, TEXT("MeshScene")));
	}

	TEST_METHOD(GetAllComponentsFiltersSubclassesAndAppends)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorComponentManagementGetAllAppend"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorComponentManagementGetAllAppend.as"),
			TEXT(R"AS(
UCLASS()
class UTestActorComponentManagementSceneA : USceneComponent {}

UCLASS()
class UTestActorComponentManagementSceneB : USceneComponent {}

UCLASS()
class ATestActorComponentManagementGetAllAppend : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestActorComponentManagementSceneA SceneA;

	UPROPERTY(DefaultComponent, Attach = SceneA)
	UTestActorComponentManagementSceneB SceneB;

	UFUNCTION()
	int RunGetAllAppendTest()
	{
		TArray<UActorComponent> AllScenes;
		AllScenes.Add(SceneA);
		GetAllComponents(USceneComponent::StaticClass(), AllScenes);
		if (AllScenes.Num() != 3)
			return 10;

		TArray<UActorComponent> OnlyB;
		GetAllComponents(UTestActorComponentManagementSceneB::StaticClass(), OnlyB);
		if (OnlyB.Num() != 1 || OnlyB[0] != SceneB)
			return 20;

		TArray<UActorComponent> NoMesh;
		GetAllComponents(UStaticMeshComponent::StaticClass(), NoMesh);
		if (NoMesh.Num() != 0)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorComponentManagementGetAllAppend"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunGetAllAppendTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("GetAllComponents should filter subclasses and append to the output array"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
