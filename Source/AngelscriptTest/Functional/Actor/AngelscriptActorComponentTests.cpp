#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Components/ActorComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

namespace AngelscriptTest_Functional_Actor_Component_Private
{
	int32 CountComponentsByClass(const AActor* Actor, const UClass* ComponentClass)
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

	template <typename ComponentType>
	ComponentType* FindComponentByName(const AActor* Actor, const FName ComponentName)
	{
		if (Actor == nullptr)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetFName() == ComponentName)
			{
				return Cast<ComponentType>(Component);
			}
		}
		return nullptr;
	}

	bool AreAllComponentsRegistered(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return false;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component == nullptr || !Component->IsRegistered())
			{
				return false;
			}
		}
		return true;
	}

	FString DescribeComponents(const TArray<UActorComponent*>& Components)
	{
		TArray<FString> Entries;
		Entries.Reserve(Components.Num());
		for (const UActorComponent* Component : Components)
		{
			if (Component == nullptr)
			{
				Entries.Add(TEXT("<null>"));
				continue;
			}

			Entries.Add(FString::Printf(TEXT("%s:%s"), *Component->GetName(), *Component->GetClass()->GetName()));
		}
		return FString::Join(Entries, TEXT(", "));
	}

	bool InvokeComponentArrayOut(
		FAutomationTestBase& Test,
		UObject* Target,
		FName FunctionName,
		const TArray<UActorComponent*>& SeedComponents,
		TArray<UActorComponent*>& OutComponents)
	{
		FFunctionInvoker Invoker(Test, Target, FunctionName);
		if (!Invoker.IsValid())
		{
			return false;
		}

		Invoker.AddParam<TArray<UActorComponent*>>(SeedComponents);
		return Invoker.Call() && Invoker.ReadParamAfterCall<TArray<UActorComponent*>>(0, OutComponents);
	}

	template <typename ComponentType>
	bool InvokeComponentReturn(
		FAutomationTestBase& Test,
		UObject* Target,
		FName FunctionName,
		ComponentType*& OutComponent)
	{
		OutComponent = nullptr;
		FFunctionInvoker Invoker(Test, Target, FunctionName);
		if (!Invoker.IsValid())
		{
			return false;
		}

		OutComponent = Invoker.CallAndReturn<ComponentType*>(nullptr);
		return true;
	}

	bool InvokeVoid(FAutomationTestBase& Test, UObject* Target, FName FunctionName)
	{
		FFunctionInvoker Invoker(Test, Target, FunctionName);
		return Invoker.IsValid() && Invoker.Call();
	}

	bool ReadComponentArrayProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		TArray<UActorComponent*>& OutComponents)
	{
		if (!Test.TestNotNull(TEXT("Component array property read requires a valid object"), Object))
		{
			return false;
		}

		FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(Object->GetClass(), PropertyName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should be a reflected TArray property"), *PropertyName.ToString()), ArrayProperty))
		{
			return false;
		}

		FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should contain UObject references"), *PropertyName.ToString()), InnerObjectProperty))
		{
			return false;
		}

		void* ArrayAddress = ArrayProperty->ContainerPtrToValuePtr<void>(Object);
		FScriptArrayHelper Helper(ArrayProperty, ArrayAddress);
		OutComponents.Reset(Helper.Num());
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			UObject* ElementObject = InnerObjectProperty->GetObjectPropertyValue(Helper.GetRawPtr(Index));
			OutComponents.Add(Cast<UActorComponent>(ElementObject));
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptActorComponentTest,
	"Angelscript.TestModule.Actor.Component",
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

	TEST_METHOD(CreateComponent)
	{
		using namespace AngelscriptTest_Functional_Actor_Component_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorCreateComponent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorCreateComponent.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorCreateComponent : AActor
{
	UFUNCTION()
	USceneComponent CreateDynamicRootForCpp()
	{
		return Cast<USceneComponent>(CreateComponent(USceneComponent::StaticClass(), n"DynamicRoot"));
	}

	UFUNCTION()
	USceneComponent CreateDynamicChildForCpp()
	{
		return Cast<USceneComponent>(CreateComponent(USceneComponent::StaticClass(), n"DynamicChild"));
	}

	UFUNCTION()
	UBillboardComponent CreateDynamicBillboardForCpp()
	{
		return Cast<UBillboardComponent>(CreateComponent(UBillboardComponent::StaticClass(), n"DynamicBillboard"));
	}

	UFUNCTION()
	USceneComponent CreateNamedSceneForCpp()
	{
		return Cast<USceneComponent>(CreateComponent(USceneComponent::StaticClass(), n"CppReturnedNamedScene"));
	}

	UFUNCTION()
	UActorComponent FindDynamicRootForCpp()
	{
		return GetComponent(USceneComponent::StaticClass(), n"DynamicRoot");
	}

	UFUNCTION()
	UActorComponent FindDynamicBillboardForCpp()
	{
		return GetComponent(UBillboardComponent::StaticClass(), n"DynamicBillboard");
	}

	UFUNCTION()
	UActorComponent FindDynamicBillboardAsWrongTypeForCpp()
	{
		return GetComponent(UStaticMeshComponent::StaticClass(), n"DynamicBillboard");
	}
}
)AS"),
			TEXT("ATestActorCreateComponent"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		USceneComponent* DynamicRoot = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateDynamicRootForCpp")), DynamicRoot)) return;
		if (!TestRunner->TestNotNull(TEXT("CreateComponent should return the first dynamic scene component to C++"), DynamicRoot)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent dynamic root returned to C++: %s:%s"), *DynamicRoot->GetName(), *DynamicRoot->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("CreateComponent root should preserve the requested object name"), DynamicRoot->GetFName(), FName(TEXT("DynamicRoot")));
		TestRunner->TestEqual(TEXT("CreateComponent root should be owned by the script actor"), DynamicRoot->GetOwner(), Actor);
		TestRunner->TestEqual(TEXT("CreateComponent should promote the first scene component to root"), Actor->GetRootComponent(), DynamicRoot);

		USceneComponent* DynamicChild = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateDynamicChildForCpp")), DynamicChild)) return;
		if (!TestRunner->TestNotNull(TEXT("CreateComponent should return the second dynamic scene component to C++"), DynamicChild)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent dynamic child returned to C++: %s:%s"), *DynamicChild->GetName(), *DynamicChild->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("CreateComponent child should preserve the requested object name"), DynamicChild->GetFName(), FName(TEXT("DynamicChild")));
		TestRunner->TestEqual(TEXT("CreateComponent child should be owned by the script actor"), DynamicChild->GetOwner(), Actor);
		TestRunner->TestEqual(TEXT("CreateComponent should attach later scene components to the root"), DynamicChild->GetAttachParent(), DynamicRoot);

		UBillboardComponent* DynamicBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateDynamicBillboardForCpp")), DynamicBillboard)) return;
		if (!TestRunner->TestNotNull(TEXT("CreateComponent should return the dynamic billboard component to C++"), DynamicBillboard)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent dynamic billboard returned to C++: %s:%s"), *DynamicBillboard->GetName(), *DynamicBillboard->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("CreateComponent billboard should preserve the requested object name"), DynamicBillboard->GetFName(), FName(TEXT("DynamicBillboard")));
		TestRunner->TestEqual(TEXT("CreateComponent billboard should be owned by the script actor"), DynamicBillboard->GetOwner(), Actor);
		TestRunner->TestEqual(TEXT("CreateComponent should attach later scene-derived components to the root"), DynamicBillboard->GetAttachParent(), DynamicRoot);

		TestRunner->TestTrue(TEXT("CreateComponent should register every created component"), AreAllComponentsRegistered(Actor));
		TestRunner->TestEqual(TEXT("CreateComponent should leave exactly three scene components on this actor"), CountComponentsByClass(Actor, USceneComponent::StaticClass()), 3);

		UActorComponent* FoundDynamicRoot = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindDynamicRootForCpp")), FoundDynamicRoot)) return;
		TestRunner->TestTrue(TEXT("CreateComponent should make the dynamic root discoverable by name"), FoundDynamicRoot == DynamicRoot);

		UActorComponent* FoundDynamicBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindDynamicBillboardForCpp")), FoundDynamicBillboard)) return;
		TestRunner->TestTrue(TEXT("CreateComponent should make the dynamic billboard discoverable by name and class"), FoundDynamicBillboard == DynamicBillboard);

		UActorComponent* WrongTypeBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindDynamicBillboardAsWrongTypeForCpp")), WrongTypeBillboard)) return;
		TestRunner->TestNull(TEXT("CreateComponent should not return a named component through an unrelated class"), WrongTypeBillboard);

		USceneComponent* ReturnedNamedScene = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateNamedSceneForCpp")), ReturnedNamedScene)) return;
		if (!TestRunner->TestNotNull(TEXT("CreateComponent should return the newly created named component to C++"), ReturnedNamedScene)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent returned named component to C++: %s:%s"), *ReturnedNamedScene->GetName(), *ReturnedNamedScene->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("Returned named component should preserve the requested object name"), ReturnedNamedScene->GetFName(), FName(TEXT("CppReturnedNamedScene")));
		TestRunner->TestEqual(TEXT("Returned named component should be owned by the script actor"), ReturnedNamedScene->GetOwner(), Actor);
		TestRunner->TestTrue(TEXT("Returned named component should be registered"), ReturnedNamedScene->IsRegistered());
		TestRunner->TestEqual(TEXT("Returned named component should attach to the existing dynamic root"), ReturnedNamedScene->GetAttachParent(), DynamicRoot);
		TestRunner->TestEqual(TEXT("CreateComponent should expose the returned named component in the actor component list"), FindComponentByName<USceneComponent>(Actor, TEXT("CppReturnedNamedScene")), ReturnedNamedScene);
	}

	TEST_METHOD(GetComponent)
	{
		using namespace AngelscriptTest_Functional_Actor_Component_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorGetComponent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorGetComponent.as"),
			TEXT(R"AS(
UCLASS()
class UTestActorGetComponentMissing : UActorComponent
{
}

UCLASS()
class ATestActorGetComponent : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UStaticMeshComponent Mesh;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UBillboardComponent Billboard;

	UFUNCTION()
	UActorComponent FindFirstSceneByClassForCpp()
	{
		return GetComponent(USceneComponent::StaticClass());
	}

	UFUNCTION()
	UActorComponent FindMeshByClassForCpp()
	{
		return GetComponent(UStaticMeshComponent::StaticClass());
	}

	UFUNCTION()
	UActorComponent FindRootByClassAndNameForCpp()
	{
		return GetComponent(USceneComponent::StaticClass(), n"RootScene");
	}

	UFUNCTION()
	UActorComponent FindMeshByParentClassAndNameForCpp()
	{
		return GetComponent(USceneComponent::StaticClass(), n"Mesh");
	}

	UFUNCTION()
	UActorComponent FindBillboardWithWrongClassForCpp()
	{
		return GetComponent(UStaticMeshComponent::StaticClass(), n"Billboard");
	}

	UFUNCTION()
	UActorComponent FindMissingSceneByNameForCpp()
	{
		return GetComponent(USceneComponent::StaticClass(), n"MissingScene");
	}

	UFUNCTION()
	UActorComponent FindMissingComponentByClassForCpp()
	{
		return GetComponent(UTestActorGetComponentMissing::StaticClass());
	}
}
)AS"),
			TEXT("ATestActorGetComponent"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		USceneComponent* RootScene = FindComponentByName<USceneComponent>(Actor, TEXT("RootScene"));
		UStaticMeshComponent* Mesh = FindComponentByName<UStaticMeshComponent>(Actor, TEXT("Mesh"));
		UBillboardComponent* Billboard = FindComponentByName<UBillboardComponent>(Actor, TEXT("Billboard"));
		if (!TestRunner->TestNotNull(TEXT("GetComponent fixture should have a root scene component"), RootScene)
			|| !TestRunner->TestNotNull(TEXT("GetComponent fixture should have a static mesh component"), Mesh)
			|| !TestRunner->TestNotNull(TEXT("GetComponent fixture should have a billboard component"), Billboard))
		{
			return;
		}

		UActorComponent* FirstSceneByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindFirstSceneByClassForCpp")), FirstSceneByClass)) return;
		if (!TestRunner->TestNotNull(TEXT("GetComponent should find a scene component by class"), FirstSceneByClass)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetComponent first scene by class returned to C++: %s:%s"), *FirstSceneByClass->GetName(), *FirstSceneByClass->GetClass()->GetName()));
		TestRunner->TestTrue(TEXT("GetComponent result should satisfy the requested scene class"), FirstSceneByClass->IsA(USceneComponent::StaticClass()));

		UActorComponent* MeshByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMeshByClassForCpp")), MeshByClass)) return;
		if (!TestRunner->TestNotNull(TEXT("GetComponent should find Mesh by static mesh class"), MeshByClass)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetComponent mesh by class returned to C++: %s:%s"), *MeshByClass->GetName(), *MeshByClass->GetClass()->GetName()));
		TestRunner->TestTrue(TEXT("GetComponent should find Mesh by static mesh class"), MeshByClass == Mesh);

		UActorComponent* RootByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindRootByClassAndNameForCpp")), RootByName)) return;
		TestRunner->TestTrue(TEXT("GetComponent should find RootScene by class and name"), RootByName == RootScene);

		UActorComponent* MeshAsScene = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMeshByParentClassAndNameForCpp")), MeshAsScene)) return;
		TestRunner->TestTrue(TEXT("GetComponent should match a derived component when querying parent class plus name"), MeshAsScene == Mesh);

		UActorComponent* WrongTypeByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindBillboardWithWrongClassForCpp")), WrongTypeByName)) return;
		TestRunner->TestNull(TEXT("GetComponent should return null for matching name with wrong class"), WrongTypeByName);

		UActorComponent* MissingByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMissingSceneByNameForCpp")), MissingByName)) return;
		TestRunner->TestNull(TEXT("GetComponent should return null for a missing component name"), MissingByName);

		UActorComponent* MissingByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMissingComponentByClassForCpp")), MissingByClass)) return;
		TestRunner->TestNull(TEXT("GetComponent should return null for an absent component class"), MissingByClass);

		TestRunner->TestEqual(TEXT("GetComponent fixture should not create extra components"), CountComponentsByClass(Actor, UActorComponent::StaticClass()), 3);
	}

	TEST_METHOD(GetOrCreateComponent)
	{
		using namespace AngelscriptTest_Functional_Actor_Component_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorGetOrCreateComponent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorGetOrCreateComponent.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorGetOrCreateComponent : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UFUNCTION()
	UActorComponent GetExistingRootByNameForCpp()
	{
		return GetOrCreateComponent(USceneComponent::StaticClass(), n"RootScene");
	}

	UFUNCTION()
	UActorComponent GetExistingRootByClassForCpp()
	{
		return GetOrCreateComponent(USceneComponent::StaticClass());
	}

	UFUNCTION()
	USceneComponent CreateLazySceneForCpp()
	{
		return Cast<USceneComponent>(GetOrCreateComponent(USceneComponent::StaticClass(), n"LazyScene"));
	}

	UFUNCTION()
	UActorComponent GetLazySceneAgainForCpp()
	{
		return GetOrCreateComponent(USceneComponent::StaticClass(), n"LazyScene");
	}

	UFUNCTION()
	UBillboardComponent CreateLazyBillboardForCpp()
	{
		return Cast<UBillboardComponent>(GetOrCreateComponent(UBillboardComponent::StaticClass(), n"LazyBillboard"));
	}

	UFUNCTION()
	UActorComponent GetLazyBillboardBySceneClassForCpp()
	{
		return GetOrCreateComponent(USceneComponent::StaticClass(), n"LazyBillboard");
	}
}
)AS"),
			TEXT("ATestActorGetOrCreateComponent"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		USceneComponent* RootScene = FindComponentByName<USceneComponent>(Actor, TEXT("RootScene"));
		if (!TestRunner->TestNotNull(TEXT("GetOrCreateComponent should keep the original root scene"), RootScene)) return;

		UActorComponent* ExistingRootByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetExistingRootByNameForCpp")), ExistingRootByName)) return;
		if (!TestRunner->TestNotNull(TEXT("GetOrCreateComponent should return the default root by name"), ExistingRootByName)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetOrCreateComponent existing root by name returned to C++: %s:%s"), *ExistingRootByName->GetName(), *ExistingRootByName->GetClass()->GetName()));
		TestRunner->TestTrue(TEXT("GetOrCreateComponent should reuse the default root by name"), ExistingRootByName == RootScene);

		UActorComponent* ExistingRootByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetExistingRootByClassForCpp")), ExistingRootByClass)) return;
		TestRunner->TestTrue(TEXT("GetOrCreateComponent should reuse the default root by class"), ExistingRootByClass == RootScene);
		TestRunner->TestEqual(TEXT("GetOrCreateComponent should not replace the root component"), Actor->GetRootComponent(), RootScene);

		USceneComponent* LazyScene = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateLazySceneForCpp")), LazyScene)) return;
		if (!TestRunner->TestNotNull(TEXT("GetOrCreateComponent should create one lazy scene component"), LazyScene)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetOrCreateComponent lazy scene returned to C++: %s:%s"), *LazyScene->GetName(), *LazyScene->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("GetOrCreateComponent lazy scene should preserve the requested name"), LazyScene->GetFName(), FName(TEXT("LazyScene")));
		TestRunner->TestEqual(TEXT("GetOrCreateComponent lazy scene should be owned by the actor"), LazyScene->GetOwner(), Actor);
		TestRunner->TestEqual(TEXT("GetOrCreateComponent should attach created scene components to the root"), LazyScene->GetAttachParent(), RootScene);

		UActorComponent* LazySceneAgain = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetLazySceneAgainForCpp")), LazySceneAgain)) return;
		TestRunner->TestTrue(TEXT("GetOrCreateComponent should not duplicate a named scene component"), LazySceneAgain == LazyScene);

		UBillboardComponent* LazyBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateLazyBillboardForCpp")), LazyBillboard)) return;
		if (!TestRunner->TestNotNull(TEXT("GetOrCreateComponent should create one lazy billboard component"), LazyBillboard)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetOrCreateComponent lazy billboard returned to C++: %s:%s"), *LazyBillboard->GetName(), *LazyBillboard->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("GetOrCreateComponent lazy billboard should preserve the requested name"), LazyBillboard->GetFName(), FName(TEXT("LazyBillboard")));
		TestRunner->TestEqual(TEXT("GetOrCreateComponent lazy billboard should be owned by the actor"), LazyBillboard->GetOwner(), Actor);
		TestRunner->TestEqual(TEXT("GetOrCreateComponent should attach created scene-derived components to the root"), LazyBillboard->GetAttachParent(), RootScene);

		UActorComponent* BillboardBySceneName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetLazyBillboardBySceneClassForCpp")), BillboardBySceneName)) return;
		TestRunner->TestTrue(TEXT("GetOrCreateComponent should reuse derived components when parent class and name match"), BillboardBySceneName == LazyBillboard);
		TestRunner->TestEqual(TEXT("GetOrCreateComponent should not duplicate components when called repeatedly"), CountComponentsByClass(Actor, UActorComponent::StaticClass()), 3);
	}

	TEST_METHOD(GetAllComponents)
	{
		using namespace AngelscriptTest_Functional_Actor_Component_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorGetAllComponents"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorGetAllComponents.as"),
			TEXT(R"AS(
UCLASS()
class UTestCompA : USceneComponent
{
}

UCLASS()
class UTestCompB : USceneComponent
{
}

UCLASS()
class UTestCompDerivedB : UTestCompB
{
}

UCLASS()
class ATestActorGetAllComponents : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestCompA CompA;

	UPROPERTY(DefaultComponent, Attach = CompA)
	UTestCompB CompB;

	UPROPERTY(DefaultComponent, Attach = CompA)
	UTestCompB CompB2;

	UPROPERTY(DefaultComponent, Attach = CompA)
	UTestCompDerivedB DerivedB;

	UPROPERTY(DefaultComponent, Attach = CompA)
	UTestCompDerivedB DerivedB2;

	UPROPERTY(DefaultComponent, Attach = CompA)
	UBillboardComponent Billboard;

	UPROPERTY(DefaultComponent, Attach = CompA)
	UBillboardComponent Billboard2;

	UPROPERTY()
	TArray<UActorComponent> LastBFamilyForCpp;

	UPROPERTY()
	TArray<UActorComponent> LastAllComponentsForCpp;

	UPROPERTY()
	TArray<UActorComponent> LastBillboardsForCpp;

	UFUNCTION()
	UActorComponent ReturnRootForCpp()
	{
		return CompA;
	}

	UFUNCTION()
	UActorComponent ReturnDerivedForCpp()
	{
		return DerivedB;
	}

	UFUNCTION()
	void FillAllActorComponentsForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UActorComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void FillAllSceneComponentsForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(USceneComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void FillBFamilyForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UTestCompB::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void FillDerivedBOnlyForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UTestCompDerivedB::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void FillBillboardsForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UBillboardComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void FillNoStaticMeshMatchesForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UStaticMeshComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void AppendBillboardsForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UBillboardComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void StoreArraysForCpp()
	{
		LastBFamilyForCpp.Empty();
		GetAllComponents(UTestCompB::StaticClass(), LastBFamilyForCpp);

		LastAllComponentsForCpp.Empty();
		GetAllComponents(UActorComponent::StaticClass(), LastAllComponentsForCpp);

		LastBillboardsForCpp.Empty();
		GetAllComponents(UBillboardComponent::StaticClass(), LastBillboardsForCpp);
	}
}
)AS"),
			TEXT("ATestActorGetAllComponents"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		TestRunner->TestEqual(TEXT("GetAllComponents fixture should contain seven actor components"), CountComponentsByClass(Actor, UActorComponent::StaticClass()), 7);
		TestRunner->TestEqual(TEXT("GetAllComponents fixture should contain seven scene components"), CountComponentsByClass(Actor, USceneComponent::StaticClass()), 7);
		TestRunner->TestEqual(TEXT("GetAllComponents fixture should contain two billboard components"), CountComponentsByClass(Actor, UBillboardComponent::StaticClass()), 2);

		UActorComponent* CompA = FindComponentByName<UActorComponent>(Actor, TEXT("CompA"));
		UActorComponent* CompB = FindComponentByName<UActorComponent>(Actor, TEXT("CompB"));
		UActorComponent* CompB2 = FindComponentByName<UActorComponent>(Actor, TEXT("CompB2"));
		UActorComponent* DerivedB = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedB"));
		UActorComponent* DerivedB2 = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedB2"));
		UActorComponent* Billboard = FindComponentByName<UActorComponent>(Actor, TEXT("Billboard"));
		UActorComponent* Billboard2 = FindComponentByName<UActorComponent>(Actor, TEXT("Billboard2"));
		if (!TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose CompA"), CompA)
			|| !TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose CompB"), CompB)
			|| !TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose CompB2"), CompB2)
			|| !TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose DerivedB"), DerivedB)
			|| !TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose DerivedB2"), DerivedB2)
			|| !TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose Billboard"), Billboard)
			|| !TestRunner->TestNotNull(TEXT("GetAllComponents C++ fixture should expose Billboard2"), Billboard2))
		{
			return;
		}

		FFunctionInvoker ReturnRootInvoker(*TestRunner, Actor, FName(TEXT("ReturnRootForCpp")));
		if (!ReturnRootInvoker.IsValid()) return;
		UActorComponent* ReturnedRoot = ReturnRootInvoker.CallAndReturn<UActorComponent*>(nullptr);
		TestRunner->TestEqual(TEXT("Script should return CompA to C++ as a component object"), ReturnedRoot, CompA);

		FFunctionInvoker ReturnDerivedInvoker(*TestRunner, Actor, FName(TEXT("ReturnDerivedForCpp")));
		if (!ReturnDerivedInvoker.IsValid()) return;
		UActorComponent* ReturnedDerived = ReturnDerivedInvoker.CallAndReturn<UActorComponent*>(nullptr);
		TestRunner->TestEqual(TEXT("Script should return DerivedB to C++ as a component object"), ReturnedDerived, DerivedB);

		TArray<UActorComponent*> AllActorComponents;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillAllActorComponentsForCpp")), {}, AllActorComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents all actor components returned to C++: [%s]"), *DescribeComponents(AllActorComponents)));
		TestRunner->TestEqual(TEXT("All actor components returned to C++ should include every fixture component"), AllActorComponents.Num(), 7);
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include CompA"), AllActorComponents.Contains(CompA));
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include CompB"), AllActorComponents.Contains(CompB));
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include CompB2"), AllActorComponents.Contains(CompB2));
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include DerivedB"), AllActorComponents.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include DerivedB2"), AllActorComponents.Contains(DerivedB2));
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include Billboard"), AllActorComponents.Contains(Billboard));
		TestRunner->TestTrue(TEXT("All actor components returned to C++ should include Billboard2"), AllActorComponents.Contains(Billboard2));

		TArray<UActorComponent*> AllSceneComponents;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillAllSceneComponentsForCpp")), {}, AllSceneComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents all scene components returned to C++: [%s]"), *DescribeComponents(AllSceneComponents)));
		TestRunner->TestEqual(TEXT("All scene components returned to C++ should include every fixture component"), AllSceneComponents.Num(), 7);
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include CompA"), AllSceneComponents.Contains(CompA));
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include CompB"), AllSceneComponents.Contains(CompB));
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include CompB2"), AllSceneComponents.Contains(CompB2));
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include DerivedB"), AllSceneComponents.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include DerivedB2"), AllSceneComponents.Contains(DerivedB2));
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include Billboard"), AllSceneComponents.Contains(Billboard));
		TestRunner->TestTrue(TEXT("All scene components returned to C++ should include Billboard2"), AllSceneComponents.Contains(Billboard2));

		TArray<UActorComponent*> BFamily;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillBFamilyForCpp")), {}, BFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents B-family returned to C++: [%s]"), *DescribeComponents(BFamily)));
		TestRunner->TestEqual(TEXT("B-family array returned to C++ should contain every base and derived component"), BFamily.Num(), 4);
		TestRunner->TestTrue(TEXT("B-family array returned to C++ should include CompB"), BFamily.Contains(CompB));
		TestRunner->TestTrue(TEXT("B-family array returned to C++ should include CompB2"), BFamily.Contains(CompB2));
		TestRunner->TestTrue(TEXT("B-family array returned to C++ should include DerivedB"), BFamily.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("B-family array returned to C++ should include DerivedB2"), BFamily.Contains(DerivedB2));

		TArray<UActorComponent*> DerivedOnly;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillDerivedBOnlyForCpp")), {}, DerivedOnly)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents derived-only returned to C++: [%s]"), *DescribeComponents(DerivedOnly)));
		TestRunner->TestEqual(TEXT("Derived-only array returned to C++ should contain only derived instances"), DerivedOnly.Num(), 2);
		TestRunner->TestTrue(TEXT("Derived-only array returned to C++ should include DerivedB"), DerivedOnly.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("Derived-only array returned to C++ should include DerivedB2"), DerivedOnly.Contains(DerivedB2));
		TestRunner->TestFalse(TEXT("Derived-only array returned to C++ should not include base CompB"), DerivedOnly.Contains(CompB));

		TArray<UActorComponent*> Billboards;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillBillboardsForCpp")), {}, Billboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents billboards returned to C++: [%s]"), *DescribeComponents(Billboards)));
		TestRunner->TestEqual(TEXT("Billboard array returned to C++ should contain both billboards"), Billboards.Num(), 2);
		TestRunner->TestTrue(TEXT("Billboard array returned to C++ should include Billboard"), Billboards.Contains(Billboard));
		TestRunner->TestTrue(TEXT("Billboard array returned to C++ should include Billboard2"), Billboards.Contains(Billboard2));

		TArray<UActorComponent*> NoStaticMeshMatches;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillNoStaticMeshMatchesForCpp")), {}, NoStaticMeshMatches)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents no static mesh matches returned to C++: [%s]"), *DescribeComponents(NoStaticMeshMatches)));
		TestRunner->TestEqual(TEXT("No-match array returned to C++ should stay empty"), NoStaticMeshMatches.Num(), 0);

		TArray<UActorComponent*> SeededComponents;
		SeededComponents.Add(CompA);
		TArray<UActorComponent*> SeededBillboards;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("AppendBillboardsForCpp")), SeededComponents, SeededBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents seeded billboard append returned to C++: [%s]"), *DescribeComponents(SeededBillboards)));
		if (!TestRunner->TestEqual(TEXT("Seeded array returned to C++ should preserve seed and append both billboards"), SeededBillboards.Num(), 3)) return;
		TestRunner->TestEqual(TEXT("Seeded array returned to C++ should keep CompA as the seed element"), SeededBillboards[0], CompA);
		TestRunner->TestTrue(TEXT("Seeded array returned to C++ should append Billboard"), SeededBillboards.Contains(Billboard));
		TestRunner->TestTrue(TEXT("Seeded array returned to C++ should append Billboard2"), SeededBillboards.Contains(Billboard2));

		FFunctionInvoker StoreArraysInvoker(*TestRunner, Actor, FName(TEXT("StoreArraysForCpp")));
		if (!StoreArraysInvoker.IsValid() || !StoreArraysInvoker.Call()) return;

		TArray<UActorComponent*> StoredBFamily;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("LastBFamilyForCpp")), StoredBFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents B-family stored property read by C++: [%s]"), *DescribeComponents(StoredBFamily)));
		TestRunner->TestEqual(TEXT("Stored B-family array should contain four components"), StoredBFamily.Num(), 4);
		TestRunner->TestTrue(TEXT("Stored B-family array should include CompB"), StoredBFamily.Contains(CompB));
		TestRunner->TestTrue(TEXT("Stored B-family array should include CompB2"), StoredBFamily.Contains(CompB2));
		TestRunner->TestTrue(TEXT("Stored B-family array should include DerivedB"), StoredBFamily.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("Stored B-family array should include DerivedB2"), StoredBFamily.Contains(DerivedB2));

		TArray<UActorComponent*> StoredAllComponents;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("LastAllComponentsForCpp")), StoredAllComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents all components stored property read by C++: [%s]"), *DescribeComponents(StoredAllComponents)));
		TestRunner->TestEqual(TEXT("Stored all-components array should contain seven components"), StoredAllComponents.Num(), 7);
		TestRunner->TestTrue(TEXT("Stored all-components array should include CompA"), StoredAllComponents.Contains(CompA));
		TestRunner->TestTrue(TEXT("Stored all-components array should include CompB"), StoredAllComponents.Contains(CompB));
		TestRunner->TestTrue(TEXT("Stored all-components array should include CompB2"), StoredAllComponents.Contains(CompB2));
		TestRunner->TestTrue(TEXT("Stored all-components array should include DerivedB"), StoredAllComponents.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("Stored all-components array should include DerivedB2"), StoredAllComponents.Contains(DerivedB2));
		TestRunner->TestTrue(TEXT("Stored all-components array should include Billboard"), StoredAllComponents.Contains(Billboard));
		TestRunner->TestTrue(TEXT("Stored all-components array should include Billboard2"), StoredAllComponents.Contains(Billboard2));

		TArray<UActorComponent*> StoredBillboards;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("LastBillboardsForCpp")), StoredBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents billboards stored property read by C++: [%s]"), *DescribeComponents(StoredBillboards)));
		TestRunner->TestEqual(TEXT("Stored billboard array should contain two components"), StoredBillboards.Num(), 2);
		TestRunner->TestTrue(TEXT("Stored billboard array should include Billboard"), StoredBillboards.Contains(Billboard));
		TestRunner->TestTrue(TEXT("Stored billboard array should include Billboard2"), StoredBillboards.Contains(Billboard2));
	}

	TEST_METHOD(ReturnComponentsToCpp)
	{
		using namespace AngelscriptTest_Functional_Actor_Component_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorReturnComponentsToCpp"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorReturnComponentsToCpp.as"),
			TEXT(R"AS(
UCLASS()
class UReturnComponentBase : USceneComponent
{
}

UCLASS()
class UReturnComponentDerived : UReturnComponentBase
{
}

UCLASS()
class ATestActorReturnComponentsToCpp : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UReturnComponentBase BaseA;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UReturnComponentBase BaseB;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UReturnComponentDerived DerivedA;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UReturnComponentDerived DerivedB;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UBillboardComponent BillboardA;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UBillboardComponent BillboardB;

	UPROPERTY()
	TArray<UActorComponent> StoredBaseFamily;

	UPROPERTY()
	TArray<UActorComponent> StoredAllComponents;

	UPROPERTY()
	TArray<UActorComponent> StoredBillboards;

	UFUNCTION()
	UActorComponent ReturnBaseAForCpp()
	{
		return BaseA;
	}

	UFUNCTION()
	UActorComponent ReturnDerivedBForCpp()
	{
		return DerivedB;
	}

	UFUNCTION()
	UActorComponent ReturnComponentByNameForCpp(FName ComponentName)
	{
		return GetComponent(UActorComponent::StaticClass(), ComponentName);
	}

	UFUNCTION()
	USceneComponent ReturnCreatedNamedSceneForCpp()
	{
		return Cast<USceneComponent>(CreateComponent(USceneComponent::StaticClass(), n"CppExplicitNamedScene"));
	}

	UFUNCTION()
	void ReturnBaseFamilyArrayForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UReturnComponentBase::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void ReturnAllComponentsArrayForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UActorComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void AppendBillboardArrayForCpp(TArray<UActorComponent>& OutComponents)
	{
		GetAllComponents(UBillboardComponent::StaticClass(), OutComponents);
	}

	UFUNCTION()
	void StoreComponentArraysForCpp()
	{
		StoredBaseFamily.Empty();
		GetAllComponents(UReturnComponentBase::StaticClass(), StoredBaseFamily);

		StoredAllComponents.Empty();
		GetAllComponents(UActorComponent::StaticClass(), StoredAllComponents);

		StoredBillboards.Empty();
		GetAllComponents(UBillboardComponent::StaticClass(), StoredBillboards);
	}
}
)AS"),
			TEXT("ATestActorReturnComponentsToCpp"));
		if (ScriptClass == nullptr) return;

		UClass* ReturnBaseClass = FindGeneratedClass(&Engine, TEXT("UReturnComponentBase"));
		UClass* ReturnDerivedClass = FindGeneratedClass(&Engine, TEXT("UReturnComponentDerived"));
		if (!TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp should generate the base component class"), ReturnBaseClass)
			|| !TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp should generate the derived component class"), ReturnDerivedClass))
		{
			return;
		}

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		UActorComponent* BaseA = FindComponentByName<UActorComponent>(Actor, TEXT("BaseA"));
		UActorComponent* BaseB = FindComponentByName<UActorComponent>(Actor, TEXT("BaseB"));
		UActorComponent* DerivedA = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedA"));
		UActorComponent* DerivedB = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedB"));
		UActorComponent* BillboardA = FindComponentByName<UActorComponent>(Actor, TEXT("BillboardA"));
		UActorComponent* BillboardB = FindComponentByName<UActorComponent>(Actor, TEXT("BillboardB"));
		if (!TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp fixture should expose BaseA"), BaseA)
			|| !TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp fixture should expose BaseB"), BaseB)
			|| !TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp fixture should expose DerivedA"), DerivedA)
			|| !TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp fixture should expose DerivedB"), DerivedB)
			|| !TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp fixture should expose BillboardA"), BillboardA)
			|| !TestRunner->TestNotNull(TEXT("ReturnComponentsToCpp fixture should expose BillboardB"), BillboardB))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("BaseA should use the generated base component class"), BaseA->IsA(ReturnBaseClass));
		TestRunner->TestTrue(TEXT("DerivedA should use the generated derived component class"), DerivedA->IsA(ReturnDerivedClass));

		FFunctionInvoker ReturnBaseInvoker(*TestRunner, Actor, FName(TEXT("ReturnBaseAForCpp")));
		if (!ReturnBaseInvoker.IsValid()) return;
		UActorComponent* ReturnedBaseA = ReturnBaseInvoker.CallAndReturn<UActorComponent*>(nullptr);
		if (!TestRunner->TestNotNull(TEXT("ReturnBaseAForCpp should return a component object"), ReturnedBaseA)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp single component returned to C++: %s:%s"), *ReturnedBaseA->GetName(), *ReturnedBaseA->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("ReturnBaseAForCpp should return BaseA to C++"), ReturnedBaseA, BaseA);

		FFunctionInvoker ReturnDerivedInvoker(*TestRunner, Actor, FName(TEXT("ReturnDerivedBForCpp")));
		if (!ReturnDerivedInvoker.IsValid()) return;
		UActorComponent* ReturnedDerivedB = ReturnDerivedInvoker.CallAndReturn<UActorComponent*>(nullptr);
		if (!TestRunner->TestNotNull(TEXT("ReturnDerivedBForCpp should return a component object"), ReturnedDerivedB)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp derived component returned to C++: %s:%s"), *ReturnedDerivedB->GetName(), *ReturnedDerivedB->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("ReturnDerivedBForCpp should return DerivedB to C++"), ReturnedDerivedB, DerivedB);

		FFunctionInvoker ReturnByNameInvoker(*TestRunner, Actor, FName(TEXT("ReturnComponentByNameForCpp")));
		if (!ReturnByNameInvoker.IsValid()) return;
		ReturnByNameInvoker.AddParam<FName>(FName(TEXT("BillboardB")));
		UActorComponent* ReturnedByName = ReturnByNameInvoker.CallAndReturn<UActorComponent*>(nullptr);
		if (!TestRunner->TestNotNull(TEXT("ReturnComponentByNameForCpp should return a component object"), ReturnedByName)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp component returned by name to C++: %s:%s"), *ReturnedByName->GetName(), *ReturnedByName->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("ReturnComponentByNameForCpp should return the named BillboardB component"), ReturnedByName, BillboardB);

		TArray<UActorComponent*> BaseFamily;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("ReturnBaseFamilyArrayForCpp")), {}, BaseFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp base-family array returned to C++: [%s]"), *DescribeComponents(BaseFamily)));
		TestRunner->TestEqual(TEXT("Returned base-family array should contain base and derived instances"), BaseFamily.Num(), 4);
		TestRunner->TestTrue(TEXT("Returned base-family array should include BaseA"), BaseFamily.Contains(BaseA));
		TestRunner->TestTrue(TEXT("Returned base-family array should include BaseB"), BaseFamily.Contains(BaseB));
		TestRunner->TestTrue(TEXT("Returned base-family array should include DerivedA"), BaseFamily.Contains(DerivedA));
		TestRunner->TestTrue(TEXT("Returned base-family array should include DerivedB"), BaseFamily.Contains(DerivedB));

		TArray<UActorComponent*> AllComponents;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("ReturnAllComponentsArrayForCpp")), {}, AllComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp all-component array returned to C++: [%s]"), *DescribeComponents(AllComponents)));
		TestRunner->TestEqual(TEXT("Returned all-component array should include every default component"), AllComponents.Num(), 7);
		TestRunner->TestTrue(TEXT("Returned all-component array should include BaseA"), AllComponents.Contains(BaseA));
		TestRunner->TestTrue(TEXT("Returned all-component array should include BaseB"), AllComponents.Contains(BaseB));
		TestRunner->TestTrue(TEXT("Returned all-component array should include DerivedA"), AllComponents.Contains(DerivedA));
		TestRunner->TestTrue(TEXT("Returned all-component array should include DerivedB"), AllComponents.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("Returned all-component array should include BillboardA"), AllComponents.Contains(BillboardA));
		TestRunner->TestTrue(TEXT("Returned all-component array should include BillboardB"), AllComponents.Contains(BillboardB));

		TArray<UActorComponent*> SeededComponents;
		SeededComponents.Add(BaseA);
		TArray<UActorComponent*> SeededBillboards;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("AppendBillboardArrayForCpp")), SeededComponents, SeededBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp seeded billboard array returned to C++: [%s]"), *DescribeComponents(SeededBillboards)));
		if (!TestRunner->TestEqual(TEXT("Returned seeded billboard array should preserve the seed and append both billboards"), SeededBillboards.Num(), 3)) return;
		TestRunner->TestEqual(TEXT("Returned seeded billboard array should keep BaseA as the seed element"), SeededBillboards[0], BaseA);
		TestRunner->TestTrue(TEXT("Returned seeded billboard array should include BillboardA"), SeededBillboards.Contains(BillboardA));
		TestRunner->TestTrue(TEXT("Returned seeded billboard array should include BillboardB"), SeededBillboards.Contains(BillboardB));

		FFunctionInvoker StoreArraysInvoker(*TestRunner, Actor, FName(TEXT("StoreComponentArraysForCpp")));
		if (!StoreArraysInvoker.IsValid() || !StoreArraysInvoker.Call()) return;

		TArray<UActorComponent*> StoredBaseFamily;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("StoredBaseFamily")), StoredBaseFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp stored base-family property read by C++: [%s]"), *DescribeComponents(StoredBaseFamily)));
		TestRunner->TestEqual(TEXT("Stored base-family property should contain four components"), StoredBaseFamily.Num(), 4);
		TestRunner->TestTrue(TEXT("Stored base-family property should include BaseA"), StoredBaseFamily.Contains(BaseA));
		TestRunner->TestTrue(TEXT("Stored base-family property should include BaseB"), StoredBaseFamily.Contains(BaseB));
		TestRunner->TestTrue(TEXT("Stored base-family property should include DerivedA"), StoredBaseFamily.Contains(DerivedA));
		TestRunner->TestTrue(TEXT("Stored base-family property should include DerivedB"), StoredBaseFamily.Contains(DerivedB));

		TArray<UActorComponent*> StoredAllComponents;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("StoredAllComponents")), StoredAllComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp stored all-component property read by C++: [%s]"), *DescribeComponents(StoredAllComponents)));
		TestRunner->TestEqual(TEXT("Stored all-component property should contain seven components"), StoredAllComponents.Num(), 7);
		TestRunner->TestTrue(TEXT("Stored all-component property should include BaseA"), StoredAllComponents.Contains(BaseA));
		TestRunner->TestTrue(TEXT("Stored all-component property should include BaseB"), StoredAllComponents.Contains(BaseB));
		TestRunner->TestTrue(TEXT("Stored all-component property should include DerivedA"), StoredAllComponents.Contains(DerivedA));
		TestRunner->TestTrue(TEXT("Stored all-component property should include DerivedB"), StoredAllComponents.Contains(DerivedB));
		TestRunner->TestTrue(TEXT("Stored all-component property should include BillboardA"), StoredAllComponents.Contains(BillboardA));
		TestRunner->TestTrue(TEXT("Stored all-component property should include BillboardB"), StoredAllComponents.Contains(BillboardB));

		TArray<UActorComponent*> StoredBillboards;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("StoredBillboards")), StoredBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp stored billboard property read by C++: [%s]"), *DescribeComponents(StoredBillboards)));
		TestRunner->TestEqual(TEXT("Stored billboard property should contain two components"), StoredBillboards.Num(), 2);
		TestRunner->TestTrue(TEXT("Stored billboard property should include BillboardA"), StoredBillboards.Contains(BillboardA));
		TestRunner->TestTrue(TEXT("Stored billboard property should include BillboardB"), StoredBillboards.Contains(BillboardB));

		FFunctionInvoker CreateNamedInvoker(*TestRunner, Actor, FName(TEXT("ReturnCreatedNamedSceneForCpp")));
		if (!CreateNamedInvoker.IsValid()) return;
		USceneComponent* CreatedNamedScene = CreateNamedInvoker.CallAndReturn<USceneComponent*>(nullptr);
		if (!TestRunner->TestNotNull(TEXT("ReturnCreatedNamedSceneForCpp should return a dynamically created named component"), CreatedNamedScene)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp dynamically created named component returned to C++: %s:%s"), *CreatedNamedScene->GetName(), *CreatedNamedScene->GetClass()->GetName()));
		TestRunner->TestEqual(TEXT("Returned dynamically created component should preserve its requested name"), CreatedNamedScene->GetFName(), FName(TEXT("CppExplicitNamedScene")));
		TestRunner->TestEqual(TEXT("Returned dynamically created component should be owned by the actor"), CreatedNamedScene->GetOwner(), Actor);
		TestRunner->TestTrue(TEXT("Returned dynamically created component should be registered"), CreatedNamedScene->IsRegistered());
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
