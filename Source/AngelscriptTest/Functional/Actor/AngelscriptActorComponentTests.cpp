#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Components/ActorComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorComponentTest,
	"Angelscript.TestModule.Actor.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsTrue(bActual, Message);
}

template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(Expected, Actual, Message);
}

template <typename ValueType>
static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}

template <typename ValueType>
static bool CheckNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNull(Value, Message);
}

static int32 CountComponentsByClass(const AActor* Actor, const UClass* ComponentClass)
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
static ComponentType* FindComponentByName(const AActor* Actor, const FName ComponentName)
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

static bool AreAllComponentsRegistered(const AActor* Actor)
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

static FString DescribeComponents(const TArray<UActorComponent*>& Components)
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

static bool InvokeComponentArrayOut(
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
static bool InvokeComponentReturn(
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

static bool InvokeVoid(FAutomationTestBase& Test, UObject* Target, FName FunctionName)
{
	FFunctionInvoker Invoker(Test, Target, FunctionName);
	return Invoker.IsValid() && Invoker.Call();
}

static bool ReadComponentArrayProperty(
	FAutomationTestBase& Test,
	UObject* Object,
	FName PropertyName,
	TArray<UActorComponent*>& OutComponents)
{
	if (!CheckNotNull(Test, TEXT("Component array property read requires a valid object"), Object))
	{
		return false;
	}

	FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(Object->GetClass(), PropertyName);
	if (!CheckNotNull(Test, *FString::Printf(TEXT("%s should be a reflected TArray property"), *PropertyName.ToString()), ArrayProperty))
	{
		return false;
	}

	FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner);
	if (!CheckNotNull(Test, *FString::Printf(TEXT("%s should contain UObject references"), *PropertyName.ToString()), InnerObjectProperty))
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

public:
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
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		USceneComponent* DynamicRoot = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateDynamicRootForCpp")), DynamicRoot)) return;
		if (!CheckNotNull(*TestRunner, TEXT("CreateComponent should return the first dynamic scene component to C++"), DynamicRoot)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent dynamic root returned to C++: %s:%s"), *DynamicRoot->GetName(), *DynamicRoot->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("DynamicRoot")), DynamicRoot->GetFName(), TEXT("CreateComponent root should preserve the requested object name")));
		ASSERT_THAT(AreEqual(Actor, DynamicRoot->GetOwner(), TEXT("CreateComponent root should be owned by the script actor")));
		ASSERT_THAT(AreEqual(DynamicRoot, Actor->GetRootComponent(), TEXT("CreateComponent should promote the first scene component to root")));

		USceneComponent* DynamicChild = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateDynamicChildForCpp")), DynamicChild)) return;
		if (!CheckNotNull(*TestRunner, TEXT("CreateComponent should return the second dynamic scene component to C++"), DynamicChild)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent dynamic child returned to C++: %s:%s"), *DynamicChild->GetName(), *DynamicChild->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("DynamicChild")), DynamicChild->GetFName(), TEXT("CreateComponent child should preserve the requested object name")));
		ASSERT_THAT(AreEqual(Actor, DynamicChild->GetOwner(), TEXT("CreateComponent child should be owned by the script actor")));
		ASSERT_THAT(AreEqual(DynamicRoot, DynamicChild->GetAttachParent(), TEXT("CreateComponent should attach later scene components to the root")));

		UBillboardComponent* DynamicBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateDynamicBillboardForCpp")), DynamicBillboard)) return;
		if (!CheckNotNull(*TestRunner, TEXT("CreateComponent should return the dynamic billboard component to C++"), DynamicBillboard)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent dynamic billboard returned to C++: %s:%s"), *DynamicBillboard->GetName(), *DynamicBillboard->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("DynamicBillboard")), DynamicBillboard->GetFName(), TEXT("CreateComponent billboard should preserve the requested object name")));
		ASSERT_THAT(AreEqual(Actor, DynamicBillboard->GetOwner(), TEXT("CreateComponent billboard should be owned by the script actor")));
		ASSERT_THAT(AreEqual(DynamicRoot, DynamicBillboard->GetAttachParent(), TEXT("CreateComponent should attach later scene-derived components to the root")));

		ASSERT_THAT(IsTrue(AreAllComponentsRegistered(Actor), TEXT("CreateComponent should register every created component")));
		ASSERT_THAT(AreEqual(3, CountComponentsByClass(Actor, USceneComponent::StaticClass()), TEXT("CreateComponent should leave exactly three scene components on this actor")));

		UActorComponent* FoundDynamicRoot = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindDynamicRootForCpp")), FoundDynamicRoot)) return;
		ASSERT_THAT(IsTrue(FoundDynamicRoot == DynamicRoot, TEXT("CreateComponent should make the dynamic root discoverable by name")));

		UActorComponent* FoundDynamicBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindDynamicBillboardForCpp")), FoundDynamicBillboard)) return;
		ASSERT_THAT(IsTrue(FoundDynamicBillboard == DynamicBillboard, TEXT("CreateComponent should make the dynamic billboard discoverable by name and class")));

		UActorComponent* WrongTypeBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindDynamicBillboardAsWrongTypeForCpp")), WrongTypeBillboard)) return;
		ASSERT_THAT(IsNull(WrongTypeBillboard, TEXT("CreateComponent should not return a named component through an unrelated class")));

		USceneComponent* ReturnedNamedScene = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateNamedSceneForCpp")), ReturnedNamedScene)) return;
		if (!CheckNotNull(*TestRunner, TEXT("CreateComponent should return the newly created named component to C++"), ReturnedNamedScene)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("CreateComponent returned named component to C++: %s:%s"), *ReturnedNamedScene->GetName(), *ReturnedNamedScene->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("CppReturnedNamedScene")), ReturnedNamedScene->GetFName(), TEXT("Returned named component should preserve the requested object name")));
		ASSERT_THAT(AreEqual(Actor, ReturnedNamedScene->GetOwner(), TEXT("Returned named component should be owned by the script actor")));
		ASSERT_THAT(IsTrue(ReturnedNamedScene->IsRegistered(), TEXT("Returned named component should be registered")));
		ASSERT_THAT(AreEqual(DynamicRoot, ReturnedNamedScene->GetAttachParent(), TEXT("Returned named component should attach to the existing dynamic root")));
		ASSERT_THAT(AreEqual(ReturnedNamedScene, FindComponentByName<USceneComponent>(Actor, TEXT("CppReturnedNamedScene")), TEXT("CreateComponent should expose the returned named component in the actor component list")));
	}

	TEST_METHOD(GetComponent)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		USceneComponent* RootScene = FindComponentByName<USceneComponent>(Actor, TEXT("RootScene"));
		UStaticMeshComponent* Mesh = FindComponentByName<UStaticMeshComponent>(Actor, TEXT("Mesh"));
		UBillboardComponent* Billboard = FindComponentByName<UBillboardComponent>(Actor, TEXT("Billboard"));
		if (!CheckNotNull(*TestRunner, TEXT("GetComponent fixture should have a root scene component"), RootScene)
			|| !CheckNotNull(*TestRunner, TEXT("GetComponent fixture should have a static mesh component"), Mesh)
			|| !CheckNotNull(*TestRunner, TEXT("GetComponent fixture should have a billboard component"), Billboard))
		{
			return;
		}

		UActorComponent* FirstSceneByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindFirstSceneByClassForCpp")), FirstSceneByClass)) return;
		if (!CheckNotNull(*TestRunner, TEXT("GetComponent should find a scene component by class"), FirstSceneByClass)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetComponent first scene by class returned to C++: %s:%s"), *FirstSceneByClass->GetName(), *FirstSceneByClass->GetClass()->GetName()));
		ASSERT_THAT(IsTrue(FirstSceneByClass->IsA(USceneComponent::StaticClass()), TEXT("GetComponent result should satisfy the requested scene class")));

		UActorComponent* MeshByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMeshByClassForCpp")), MeshByClass)) return;
		if (!CheckNotNull(*TestRunner, TEXT("GetComponent should find Mesh by static mesh class"), MeshByClass)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetComponent mesh by class returned to C++: %s:%s"), *MeshByClass->GetName(), *MeshByClass->GetClass()->GetName()));
		ASSERT_THAT(IsTrue(MeshByClass == Mesh, TEXT("GetComponent should find Mesh by static mesh class")));

		UActorComponent* RootByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindRootByClassAndNameForCpp")), RootByName)) return;
		ASSERT_THAT(IsTrue(RootByName == RootScene, TEXT("GetComponent should find RootScene by class and name")));

		UActorComponent* MeshAsScene = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMeshByParentClassAndNameForCpp")), MeshAsScene)) return;
		ASSERT_THAT(IsTrue(MeshAsScene == Mesh, TEXT("GetComponent should match a derived component when querying parent class plus name")));

		UActorComponent* WrongTypeByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindBillboardWithWrongClassForCpp")), WrongTypeByName)) return;
		ASSERT_THAT(IsNull(WrongTypeByName, TEXT("GetComponent should return null for matching name with wrong class")));

		UActorComponent* MissingByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMissingSceneByNameForCpp")), MissingByName)) return;
		ASSERT_THAT(IsNull(MissingByName, TEXT("GetComponent should return null for a missing component name")));

		UActorComponent* MissingByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("FindMissingComponentByClassForCpp")), MissingByClass)) return;
		ASSERT_THAT(IsNull(MissingByClass, TEXT("GetComponent should return null for an absent component class")));

		ASSERT_THAT(AreEqual(3, CountComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("GetComponent fixture should not create extra components")));
	}

	TEST_METHOD(GetOrCreateComponent)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		USceneComponent* RootScene = FindComponentByName<USceneComponent>(Actor, TEXT("RootScene"));
		if (!CheckNotNull(*TestRunner, TEXT("GetOrCreateComponent should keep the original root scene"), RootScene)) return;

		UActorComponent* ExistingRootByName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetExistingRootByNameForCpp")), ExistingRootByName)) return;
		if (!CheckNotNull(*TestRunner, TEXT("GetOrCreateComponent should return the default root by name"), ExistingRootByName)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetOrCreateComponent existing root by name returned to C++: %s:%s"), *ExistingRootByName->GetName(), *ExistingRootByName->GetClass()->GetName()));
		ASSERT_THAT(IsTrue(ExistingRootByName == RootScene, TEXT("GetOrCreateComponent should reuse the default root by name")));

		UActorComponent* ExistingRootByClass = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetExistingRootByClassForCpp")), ExistingRootByClass)) return;
		ASSERT_THAT(IsTrue(ExistingRootByClass == RootScene, TEXT("GetOrCreateComponent should reuse the default root by class")));
		ASSERT_THAT(AreEqual(RootScene, Actor->GetRootComponent(), TEXT("GetOrCreateComponent should not replace the root component")));

		USceneComponent* LazyScene = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateLazySceneForCpp")), LazyScene)) return;
		if (!CheckNotNull(*TestRunner, TEXT("GetOrCreateComponent should create one lazy scene component"), LazyScene)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetOrCreateComponent lazy scene returned to C++: %s:%s"), *LazyScene->GetName(), *LazyScene->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("LazyScene")), LazyScene->GetFName(), TEXT("GetOrCreateComponent lazy scene should preserve the requested name")));
		ASSERT_THAT(AreEqual(Actor, LazyScene->GetOwner(), TEXT("GetOrCreateComponent lazy scene should be owned by the actor")));
		ASSERT_THAT(AreEqual(RootScene, LazyScene->GetAttachParent(), TEXT("GetOrCreateComponent should attach created scene components to the root")));

		UActorComponent* LazySceneAgain = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetLazySceneAgainForCpp")), LazySceneAgain)) return;
		ASSERT_THAT(IsTrue(LazySceneAgain == LazyScene, TEXT("GetOrCreateComponent should not duplicate a named scene component")));

		UBillboardComponent* LazyBillboard = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("CreateLazyBillboardForCpp")), LazyBillboard)) return;
		if (!CheckNotNull(*TestRunner, TEXT("GetOrCreateComponent should create one lazy billboard component"), LazyBillboard)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetOrCreateComponent lazy billboard returned to C++: %s:%s"), *LazyBillboard->GetName(), *LazyBillboard->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("LazyBillboard")), LazyBillboard->GetFName(), TEXT("GetOrCreateComponent lazy billboard should preserve the requested name")));
		ASSERT_THAT(AreEqual(Actor, LazyBillboard->GetOwner(), TEXT("GetOrCreateComponent lazy billboard should be owned by the actor")));
		ASSERT_THAT(AreEqual(RootScene, LazyBillboard->GetAttachParent(), TEXT("GetOrCreateComponent should attach created scene-derived components to the root")));

		UActorComponent* BillboardBySceneName = nullptr;
		if (!InvokeComponentReturn(*TestRunner, Actor, FName(TEXT("GetLazyBillboardBySceneClassForCpp")), BillboardBySceneName)) return;
		ASSERT_THAT(IsTrue(BillboardBySceneName == LazyBillboard, TEXT("GetOrCreateComponent should reuse derived components when parent class and name match")));
		ASSERT_THAT(AreEqual(3, CountComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("GetOrCreateComponent should not duplicate components when called repeatedly")));
	}

	TEST_METHOD(GetAllComponents)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		ASSERT_THAT(AreEqual(7, CountComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("GetAllComponents fixture should contain seven actor components")));
		ASSERT_THAT(AreEqual(7, CountComponentsByClass(Actor, USceneComponent::StaticClass()), TEXT("GetAllComponents fixture should contain seven scene components")));
		ASSERT_THAT(AreEqual(2, CountComponentsByClass(Actor, UBillboardComponent::StaticClass()), TEXT("GetAllComponents fixture should contain two billboard components")));

		UActorComponent* CompA = FindComponentByName<UActorComponent>(Actor, TEXT("CompA"));
		UActorComponent* CompB = FindComponentByName<UActorComponent>(Actor, TEXT("CompB"));
		UActorComponent* CompB2 = FindComponentByName<UActorComponent>(Actor, TEXT("CompB2"));
		UActorComponent* DerivedB = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedB"));
		UActorComponent* DerivedB2 = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedB2"));
		UActorComponent* Billboard = FindComponentByName<UActorComponent>(Actor, TEXT("Billboard"));
		UActorComponent* Billboard2 = FindComponentByName<UActorComponent>(Actor, TEXT("Billboard2"));
		if (!CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose CompA"), CompA)
			|| !CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose CompB"), CompB)
			|| !CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose CompB2"), CompB2)
			|| !CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose DerivedB"), DerivedB)
			|| !CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose DerivedB2"), DerivedB2)
			|| !CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose Billboard"), Billboard)
			|| !CheckNotNull(*TestRunner, TEXT("GetAllComponents C++ fixture should expose Billboard2"), Billboard2))
		{
			return;
		}

		FFunctionInvoker ReturnRootInvoker(*TestRunner, Actor, FName(TEXT("ReturnRootForCpp")));
		if (!ReturnRootInvoker.IsValid()) return;
		UActorComponent* ReturnedRoot = ReturnRootInvoker.CallAndReturn<UActorComponent*>(nullptr);
		ASSERT_THAT(AreEqual(CompA, ReturnedRoot, TEXT("Script should return CompA to C++ as a component object")));

		FFunctionInvoker ReturnDerivedInvoker(*TestRunner, Actor, FName(TEXT("ReturnDerivedForCpp")));
		if (!ReturnDerivedInvoker.IsValid()) return;
		UActorComponent* ReturnedDerived = ReturnDerivedInvoker.CallAndReturn<UActorComponent*>(nullptr);
		ASSERT_THAT(AreEqual(DerivedB, ReturnedDerived, TEXT("Script should return DerivedB to C++ as a component object")));

		TArray<UActorComponent*> AllActorComponents;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillAllActorComponentsForCpp")), {}, AllActorComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents all actor components returned to C++: [%s]"), *DescribeComponents(AllActorComponents)));
		ASSERT_THAT(AreEqual(7, AllActorComponents.Num(), TEXT("All actor components returned to C++ should include every fixture component")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(CompA), TEXT("All actor components returned to C++ should include CompA")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(CompB), TEXT("All actor components returned to C++ should include CompB")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(CompB2), TEXT("All actor components returned to C++ should include CompB2")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(DerivedB), TEXT("All actor components returned to C++ should include DerivedB")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(DerivedB2), TEXT("All actor components returned to C++ should include DerivedB2")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(Billboard), TEXT("All actor components returned to C++ should include Billboard")));
		ASSERT_THAT(IsTrue(AllActorComponents.Contains(Billboard2), TEXT("All actor components returned to C++ should include Billboard2")));

		TArray<UActorComponent*> AllSceneComponents;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillAllSceneComponentsForCpp")), {}, AllSceneComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents all scene components returned to C++: [%s]"), *DescribeComponents(AllSceneComponents)));
		ASSERT_THAT(AreEqual(7, AllSceneComponents.Num(), TEXT("All scene components returned to C++ should include every fixture component")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(CompA), TEXT("All scene components returned to C++ should include CompA")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(CompB), TEXT("All scene components returned to C++ should include CompB")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(CompB2), TEXT("All scene components returned to C++ should include CompB2")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(DerivedB), TEXT("All scene components returned to C++ should include DerivedB")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(DerivedB2), TEXT("All scene components returned to C++ should include DerivedB2")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(Billboard), TEXT("All scene components returned to C++ should include Billboard")));
		ASSERT_THAT(IsTrue(AllSceneComponents.Contains(Billboard2), TEXT("All scene components returned to C++ should include Billboard2")));

		TArray<UActorComponent*> BFamily;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillBFamilyForCpp")), {}, BFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents B-family returned to C++: [%s]"), *DescribeComponents(BFamily)));
		ASSERT_THAT(AreEqual(4, BFamily.Num(), TEXT("B-family array returned to C++ should contain every base and derived component")));
		ASSERT_THAT(IsTrue(BFamily.Contains(CompB), TEXT("B-family array returned to C++ should include CompB")));
		ASSERT_THAT(IsTrue(BFamily.Contains(CompB2), TEXT("B-family array returned to C++ should include CompB2")));
		ASSERT_THAT(IsTrue(BFamily.Contains(DerivedB), TEXT("B-family array returned to C++ should include DerivedB")));
		ASSERT_THAT(IsTrue(BFamily.Contains(DerivedB2), TEXT("B-family array returned to C++ should include DerivedB2")));

		TArray<UActorComponent*> DerivedOnly;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillDerivedBOnlyForCpp")), {}, DerivedOnly)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents derived-only returned to C++: [%s]"), *DescribeComponents(DerivedOnly)));
		ASSERT_THAT(AreEqual(2, DerivedOnly.Num(), TEXT("Derived-only array returned to C++ should contain only derived instances")));
		ASSERT_THAT(IsTrue(DerivedOnly.Contains(DerivedB), TEXT("Derived-only array returned to C++ should include DerivedB")));
		ASSERT_THAT(IsTrue(DerivedOnly.Contains(DerivedB2), TEXT("Derived-only array returned to C++ should include DerivedB2")));
		ASSERT_THAT(IsFalse(DerivedOnly.Contains(CompB), TEXT("Derived-only array returned to C++ should not include base CompB")));

		TArray<UActorComponent*> Billboards;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillBillboardsForCpp")), {}, Billboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents billboards returned to C++: [%s]"), *DescribeComponents(Billboards)));
		ASSERT_THAT(AreEqual(2, Billboards.Num(), TEXT("Billboard array returned to C++ should contain both billboards")));
		ASSERT_THAT(IsTrue(Billboards.Contains(Billboard), TEXT("Billboard array returned to C++ should include Billboard")));
		ASSERT_THAT(IsTrue(Billboards.Contains(Billboard2), TEXT("Billboard array returned to C++ should include Billboard2")));

		TArray<UActorComponent*> NoStaticMeshMatches;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("FillNoStaticMeshMatchesForCpp")), {}, NoStaticMeshMatches)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents no static mesh matches returned to C++: [%s]"), *DescribeComponents(NoStaticMeshMatches)));
		ASSERT_THAT(AreEqual(0, NoStaticMeshMatches.Num(), TEXT("No-match array returned to C++ should stay empty")));

		TArray<UActorComponent*> SeededComponents;
		SeededComponents.Add(CompA);
		TArray<UActorComponent*> SeededBillboards;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("AppendBillboardsForCpp")), SeededComponents, SeededBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents seeded billboard append returned to C++: [%s]"), *DescribeComponents(SeededBillboards)));
		if (!CheckEqual(*TestRunner, TEXT("Seeded array returned to C++ should preserve seed and append both billboards"), SeededBillboards.Num(), 3)) return;
		ASSERT_THAT(AreEqual(CompA, SeededBillboards[0], TEXT("Seeded array returned to C++ should keep CompA as the seed element")));
		ASSERT_THAT(IsTrue(SeededBillboards.Contains(Billboard), TEXT("Seeded array returned to C++ should append Billboard")));
		ASSERT_THAT(IsTrue(SeededBillboards.Contains(Billboard2), TEXT("Seeded array returned to C++ should append Billboard2")));

		FFunctionInvoker StoreArraysInvoker(*TestRunner, Actor, FName(TEXT("StoreArraysForCpp")));
		if (!StoreArraysInvoker.IsValid() || !StoreArraysInvoker.Call()) return;

		TArray<UActorComponent*> StoredBFamily;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("LastBFamilyForCpp")), StoredBFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents B-family stored property read by C++: [%s]"), *DescribeComponents(StoredBFamily)));
		ASSERT_THAT(AreEqual(4, StoredBFamily.Num(), TEXT("Stored B-family array should contain four components")));
		ASSERT_THAT(IsTrue(StoredBFamily.Contains(CompB), TEXT("Stored B-family array should include CompB")));
		ASSERT_THAT(IsTrue(StoredBFamily.Contains(CompB2), TEXT("Stored B-family array should include CompB2")));
		ASSERT_THAT(IsTrue(StoredBFamily.Contains(DerivedB), TEXT("Stored B-family array should include DerivedB")));
		ASSERT_THAT(IsTrue(StoredBFamily.Contains(DerivedB2), TEXT("Stored B-family array should include DerivedB2")));

		TArray<UActorComponent*> StoredAllComponents;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("LastAllComponentsForCpp")), StoredAllComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents all components stored property read by C++: [%s]"), *DescribeComponents(StoredAllComponents)));
		ASSERT_THAT(AreEqual(7, StoredAllComponents.Num(), TEXT("Stored all-components array should contain seven components")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(CompA), TEXT("Stored all-components array should include CompA")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(CompB), TEXT("Stored all-components array should include CompB")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(CompB2), TEXT("Stored all-components array should include CompB2")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(DerivedB), TEXT("Stored all-components array should include DerivedB")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(DerivedB2), TEXT("Stored all-components array should include DerivedB2")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(Billboard), TEXT("Stored all-components array should include Billboard")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(Billboard2), TEXT("Stored all-components array should include Billboard2")));

		TArray<UActorComponent*> StoredBillboards;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("LastBillboardsForCpp")), StoredBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("GetAllComponents billboards stored property read by C++: [%s]"), *DescribeComponents(StoredBillboards)));
		ASSERT_THAT(AreEqual(2, StoredBillboards.Num(), TEXT("Stored billboard array should contain two components")));
		ASSERT_THAT(IsTrue(StoredBillboards.Contains(Billboard), TEXT("Stored billboard array should include Billboard")));
		ASSERT_THAT(IsTrue(StoredBillboards.Contains(Billboard2), TEXT("Stored billboard array should include Billboard2")));
	}

	TEST_METHOD(ReturnComponentsToCpp)
	{
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
		if (!CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp should generate the base component class"), ReturnBaseClass)
			|| !CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp should generate the derived component class"), ReturnDerivedClass))
		{
			return;
		}

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		UActorComponent* BaseA = FindComponentByName<UActorComponent>(Actor, TEXT("BaseA"));
		UActorComponent* BaseB = FindComponentByName<UActorComponent>(Actor, TEXT("BaseB"));
		UActorComponent* DerivedA = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedA"));
		UActorComponent* DerivedB = FindComponentByName<UActorComponent>(Actor, TEXT("DerivedB"));
		UActorComponent* BillboardA = FindComponentByName<UActorComponent>(Actor, TEXT("BillboardA"));
		UActorComponent* BillboardB = FindComponentByName<UActorComponent>(Actor, TEXT("BillboardB"));
		if (!CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp fixture should expose BaseA"), BaseA)
			|| !CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp fixture should expose BaseB"), BaseB)
			|| !CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp fixture should expose DerivedA"), DerivedA)
			|| !CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp fixture should expose DerivedB"), DerivedB)
			|| !CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp fixture should expose BillboardA"), BillboardA)
			|| !CheckNotNull(*TestRunner, TEXT("ReturnComponentsToCpp fixture should expose BillboardB"), BillboardB))
		{
			return;
		}

		ASSERT_THAT(IsTrue(BaseA->IsA(ReturnBaseClass), TEXT("BaseA should use the generated base component class")));
		ASSERT_THAT(IsTrue(DerivedA->IsA(ReturnDerivedClass), TEXT("DerivedA should use the generated derived component class")));

		FFunctionInvoker ReturnBaseInvoker(*TestRunner, Actor, FName(TEXT("ReturnBaseAForCpp")));
		if (!ReturnBaseInvoker.IsValid()) return;
		UActorComponent* ReturnedBaseA = ReturnBaseInvoker.CallAndReturn<UActorComponent*>(nullptr);
		if (!CheckNotNull(*TestRunner, TEXT("ReturnBaseAForCpp should return a component object"), ReturnedBaseA)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp single component returned to C++: %s:%s"), *ReturnedBaseA->GetName(), *ReturnedBaseA->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(BaseA, ReturnedBaseA, TEXT("ReturnBaseAForCpp should return BaseA to C++")));

		FFunctionInvoker ReturnDerivedInvoker(*TestRunner, Actor, FName(TEXT("ReturnDerivedBForCpp")));
		if (!ReturnDerivedInvoker.IsValid()) return;
		UActorComponent* ReturnedDerivedB = ReturnDerivedInvoker.CallAndReturn<UActorComponent*>(nullptr);
		if (!CheckNotNull(*TestRunner, TEXT("ReturnDerivedBForCpp should return a component object"), ReturnedDerivedB)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp derived component returned to C++: %s:%s"), *ReturnedDerivedB->GetName(), *ReturnedDerivedB->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(DerivedB, ReturnedDerivedB, TEXT("ReturnDerivedBForCpp should return DerivedB to C++")));

		FFunctionInvoker ReturnByNameInvoker(*TestRunner, Actor, FName(TEXT("ReturnComponentByNameForCpp")));
		if (!ReturnByNameInvoker.IsValid()) return;
		ReturnByNameInvoker.AddParam<FName>(FName(TEXT("BillboardB")));
		UActorComponent* ReturnedByName = ReturnByNameInvoker.CallAndReturn<UActorComponent*>(nullptr);
		if (!CheckNotNull(*TestRunner, TEXT("ReturnComponentByNameForCpp should return a component object"), ReturnedByName)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp component returned by name to C++: %s:%s"), *ReturnedByName->GetName(), *ReturnedByName->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(BillboardB, ReturnedByName, TEXT("ReturnComponentByNameForCpp should return the named BillboardB component")));

		TArray<UActorComponent*> BaseFamily;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("ReturnBaseFamilyArrayForCpp")), {}, BaseFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp base-family array returned to C++: [%s]"), *DescribeComponents(BaseFamily)));
		ASSERT_THAT(AreEqual(4, BaseFamily.Num(), TEXT("Returned base-family array should contain base and derived instances")));
		ASSERT_THAT(IsTrue(BaseFamily.Contains(BaseA), TEXT("Returned base-family array should include BaseA")));
		ASSERT_THAT(IsTrue(BaseFamily.Contains(BaseB), TEXT("Returned base-family array should include BaseB")));
		ASSERT_THAT(IsTrue(BaseFamily.Contains(DerivedA), TEXT("Returned base-family array should include DerivedA")));
		ASSERT_THAT(IsTrue(BaseFamily.Contains(DerivedB), TEXT("Returned base-family array should include DerivedB")));

		TArray<UActorComponent*> AllComponents;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("ReturnAllComponentsArrayForCpp")), {}, AllComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp all-component array returned to C++: [%s]"), *DescribeComponents(AllComponents)));
		ASSERT_THAT(AreEqual(7, AllComponents.Num(), TEXT("Returned all-component array should include every default component")));
		ASSERT_THAT(IsTrue(AllComponents.Contains(BaseA), TEXT("Returned all-component array should include BaseA")));
		ASSERT_THAT(IsTrue(AllComponents.Contains(BaseB), TEXT("Returned all-component array should include BaseB")));
		ASSERT_THAT(IsTrue(AllComponents.Contains(DerivedA), TEXT("Returned all-component array should include DerivedA")));
		ASSERT_THAT(IsTrue(AllComponents.Contains(DerivedB), TEXT("Returned all-component array should include DerivedB")));
		ASSERT_THAT(IsTrue(AllComponents.Contains(BillboardA), TEXT("Returned all-component array should include BillboardA")));
		ASSERT_THAT(IsTrue(AllComponents.Contains(BillboardB), TEXT("Returned all-component array should include BillboardB")));

		TArray<UActorComponent*> SeededComponents;
		SeededComponents.Add(BaseA);
		TArray<UActorComponent*> SeededBillboards;
		if (!InvokeComponentArrayOut(*TestRunner, Actor, FName(TEXT("AppendBillboardArrayForCpp")), SeededComponents, SeededBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp seeded billboard array returned to C++: [%s]"), *DescribeComponents(SeededBillboards)));
		if (!CheckEqual(*TestRunner, TEXT("Returned seeded billboard array should preserve the seed and append both billboards"), SeededBillboards.Num(), 3)) return;
		ASSERT_THAT(AreEqual(BaseA, SeededBillboards[0], TEXT("Returned seeded billboard array should keep BaseA as the seed element")));
		ASSERT_THAT(IsTrue(SeededBillboards.Contains(BillboardA), TEXT("Returned seeded billboard array should include BillboardA")));
		ASSERT_THAT(IsTrue(SeededBillboards.Contains(BillboardB), TEXT("Returned seeded billboard array should include BillboardB")));

		FFunctionInvoker StoreArraysInvoker(*TestRunner, Actor, FName(TEXT("StoreComponentArraysForCpp")));
		if (!StoreArraysInvoker.IsValid() || !StoreArraysInvoker.Call()) return;

		TArray<UActorComponent*> StoredBaseFamily;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("StoredBaseFamily")), StoredBaseFamily)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp stored base-family property read by C++: [%s]"), *DescribeComponents(StoredBaseFamily)));
		ASSERT_THAT(AreEqual(4, StoredBaseFamily.Num(), TEXT("Stored base-family property should contain four components")));
		ASSERT_THAT(IsTrue(StoredBaseFamily.Contains(BaseA), TEXT("Stored base-family property should include BaseA")));
		ASSERT_THAT(IsTrue(StoredBaseFamily.Contains(BaseB), TEXT("Stored base-family property should include BaseB")));
		ASSERT_THAT(IsTrue(StoredBaseFamily.Contains(DerivedA), TEXT("Stored base-family property should include DerivedA")));
		ASSERT_THAT(IsTrue(StoredBaseFamily.Contains(DerivedB), TEXT("Stored base-family property should include DerivedB")));

		TArray<UActorComponent*> StoredAllComponents;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("StoredAllComponents")), StoredAllComponents)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp stored all-component property read by C++: [%s]"), *DescribeComponents(StoredAllComponents)));
		ASSERT_THAT(AreEqual(7, StoredAllComponents.Num(), TEXT("Stored all-component property should contain seven components")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(BaseA), TEXT("Stored all-component property should include BaseA")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(BaseB), TEXT("Stored all-component property should include BaseB")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(DerivedA), TEXT("Stored all-component property should include DerivedA")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(DerivedB), TEXT("Stored all-component property should include DerivedB")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(BillboardA), TEXT("Stored all-component property should include BillboardA")));
		ASSERT_THAT(IsTrue(StoredAllComponents.Contains(BillboardB), TEXT("Stored all-component property should include BillboardB")));

		TArray<UActorComponent*> StoredBillboards;
		if (!ReadComponentArrayProperty(*TestRunner, Actor, FName(TEXT("StoredBillboards")), StoredBillboards)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp stored billboard property read by C++: [%s]"), *DescribeComponents(StoredBillboards)));
		ASSERT_THAT(AreEqual(2, StoredBillboards.Num(), TEXT("Stored billboard property should contain two components")));
		ASSERT_THAT(IsTrue(StoredBillboards.Contains(BillboardA), TEXT("Stored billboard property should include BillboardA")));
		ASSERT_THAT(IsTrue(StoredBillboards.Contains(BillboardB), TEXT("Stored billboard property should include BillboardB")));

		FFunctionInvoker CreateNamedInvoker(*TestRunner, Actor, FName(TEXT("ReturnCreatedNamedSceneForCpp")));
		if (!CreateNamedInvoker.IsValid()) return;
		USceneComponent* CreatedNamedScene = CreateNamedInvoker.CallAndReturn<USceneComponent*>(nullptr);
		if (!CheckNotNull(*TestRunner, TEXT("ReturnCreatedNamedSceneForCpp should return a dynamically created named component"), CreatedNamedScene)) return;
		TestRunner->AddInfo(FString::Printf(TEXT("ReturnComponentsToCpp dynamically created named component returned to C++: %s:%s"), *CreatedNamedScene->GetName(), *CreatedNamedScene->GetClass()->GetName()));
		ASSERT_THAT(AreEqual(FName(TEXT("CppExplicitNamedScene")), CreatedNamedScene->GetFName(), TEXT("Returned dynamically created component should preserve its requested name")));
		ASSERT_THAT(AreEqual(Actor, CreatedNamedScene->GetOwner(), TEXT("Returned dynamically created component should be owned by the actor")));
		ASSERT_THAT(IsTrue(CreatedNamedScene->IsRegistered(), TEXT("Returned dynamically created component should be registered")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
