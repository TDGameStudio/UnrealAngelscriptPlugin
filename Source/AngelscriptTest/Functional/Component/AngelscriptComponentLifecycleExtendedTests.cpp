#include "CQTest.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestWorld.h"

#include "Core/AngelscriptComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Component_LifecycleExtended_Private
{
	template <typename ComponentType = UActorComponent>
	ComponentType* ReadComponentProperty(FAutomationTestBase& Test, UObject* Object, const TCHAR* PropertyName)
	{
		UObject* ComponentObject = nullptr;
		if (!GetObjectByPath(Test, Object, PropertyName, ComponentObject))
		{
			return nullptr;
		}

		ComponentType* Component = Cast<ComponentType>(ComponentObject);
		Test.TestNotNull(
			*FString::Printf(TEXT("Property '%s' should point to the expected component type"), PropertyName),
			Component);
		return Component;
	}

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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptComponentLifecycleExtendedTest,
	"Angelscript.TestModule.Component.LifecycleExtended",
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

	TEST_METHOD(HasBegunPlayTransitionsInWorld)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestComponentLifecycleHasBegunPlay"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestComponentLifecycleHasBegunPlay.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentLifecycleBeginPlayProbe : UAngelscriptComponent
{
	UPROPERTY()
	bool bSawBeginPlay = false;

	UPROPERTY()
	bool bHadNotBegunPlayInsideOverride = false;

	UPROPERTY()
	AActor OwnerAtBeginPlay;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		bSawBeginPlay = true;
		bHadNotBegunPlayInsideOverride = !HasBegunPlay();
		OwnerAtBeginPlay = GetOwner();
	}
}

UCLASS()
class ATestComponentLifecycleHasBegunPlay : AActor
{
	UPROPERTY(DefaultComponent)
	UTestComponentLifecycleBeginPlayProbe Probe;
}
)AS"),
			TEXT("ATestComponentLifecycleHasBegunPlay"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		UActorComponent* Probe = ReadComponentProperty(*TestRunner, Actor, TEXT("Probe"));
		if (Probe == nullptr) return;

		TestRunner->TestFalse(TEXT("Script component should not report begun play before actor BeginPlay"), Probe->HasBegunPlay());
		W.BeginPlay(*Actor);
		TestRunner->TestTrue(TEXT("Script component should report begun play after actor BeginPlay"), Probe->HasBegunPlay());

		bool bSawBeginPlay = false;
		bool bHadNotBegunPlayInsideOverride = false;
		if (!ReadPropertyValue<FBoolProperty>(*TestRunner, Probe, TEXT("bSawBeginPlay"), bSawBeginPlay)
			|| !ReadPropertyValue<FBoolProperty>(*TestRunner, Probe, TEXT("bHadNotBegunPlayInsideOverride"), bHadNotBegunPlayInsideOverride))
		{
			return;
		}

		UObject* OwnerAtBeginPlay = nullptr;
		if (!GetObjectByPath(*TestRunner, Probe, TEXT("OwnerAtBeginPlay"), OwnerAtBeginPlay)) return;

		TestRunner->TestTrue(TEXT("BeginPlay override should run on the script component"), bSawBeginPlay);
		TestRunner->TestTrue(TEXT("HasBegunPlay binding should still be false inside the component BeginPlay override"), bHadNotBegunPlayInsideOverride);
		TestRunner->TestEqual(TEXT("Component BeginPlay should observe its owning actor"), OwnerAtBeginPlay, static_cast<UObject*>(Actor));
	}

	TEST_METHOD(ComponentTickDispatchIsExact)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestComponentLifecycleExactTick"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestComponentLifecycleExactTick.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentLifecycleExactTickProbe : UAngelscriptComponent
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		TickCount += 1;
	}
}

UCLASS()
class ATestComponentLifecycleExactTick : AActor
{
	UPROPERTY(DefaultComponent)
	UTestComponentLifecycleExactTickProbe Probe;
}
)AS"),
			TEXT("ATestComponentLifecycleExactTick"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		UActorComponent* Probe = ReadComponentProperty(*TestRunner, Actor, TEXT("Probe"));
		if (Probe == nullptr) return;

		Probe->PrimaryComponentTick.bCanEverTick = true;
		Probe->SetComponentTickEnabled(true);
		W.BeginPlay(*Actor);

		constexpr int32 ExpectedTicks = 4;
		W.DispatchComponentTick(*Probe, 0.016f, ExpectedTicks);

		int32 TickCount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Probe, TEXT("TickCount"), TickCount)) return;

		TestRunner->TestEqual(TEXT("Direct component tick dispatch should call the script component exactly N times"), TickCount, ExpectedTicks);
	}

	TEST_METHOD(DestroyComponentUnregistersRuntimeComponent)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestComponentLifecycleDestroy"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestComponentLifecycleDestroy.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentLifecycleDestroyProbe : UAngelscriptComponent
{
	UFUNCTION()
	int DestroySelf()
	{
		DestroyComponent();
		return 1;
	}
}

UCLASS()
class ATestComponentLifecycleDestroy : AActor
{
	UPROPERTY(DefaultComponent)
	UTestComponentLifecycleDestroyProbe Probe;
}
)AS"),
			TEXT("ATestComponentLifecycleDestroy"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		UActorComponent* Probe = ReadComponentProperty(*TestRunner, Actor, TEXT("Probe"));
		if (Probe == nullptr) return;
		if (!TestRunner->TestTrue(TEXT("Default script component should start registered"), Probe->IsRegistered())) return;

		FFunctionInvoker Invoker(*TestRunner, Probe, FName(TEXT("DestroySelf")));
		if (!Invoker.IsValid()) return;
		if (!TestRunner->TestEqual(TEXT("Script component should call DestroyComponent successfully"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1)) return;

		TestRunner->TestTrue(TEXT("DestroyComponent should mark the script component as being destroyed"), Probe->IsBeingDestroyed());
		TestRunner->TestFalse(TEXT("DestroyComponent should unregister the script component"), Probe->IsRegistered());
	}

	TEST_METHOD(EndPlayReceivesDestroyedReason)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestComponentLifecycleEndPlayReason"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestComponentLifecycleEndPlayReason.as"),
			TEXT(R"AS(
UCLASS()
class UTestComponentLifecycleEndPlayProbe : UAngelscriptComponent
{
	UPROPERTY()
	int EndPlayCount = 0;

	UPROPERTY()
	EEndPlayReason LastReason = EEndPlayReason::Quit;

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		EndPlayCount += 1;
		LastReason = Reason;
	}
}

UCLASS()
class ATestComponentLifecycleEndPlayReason : AActor
{
	UPROPERTY(DefaultComponent)
	UTestComponentLifecycleEndPlayProbe Probe;
}
)AS"),
			TEXT("ATestComponentLifecycleEndPlayReason"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		UActorComponent* Probe = ReadComponentProperty(*TestRunner, Actor, TEXT("Probe"));
		if (Probe == nullptr) return;

		W.BeginPlay(*Actor);
		W.DestroyAndDrain(*Actor);

		int32 EndPlayCount = 0;
		int64 LastReason = -1;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Probe, TEXT("EndPlayCount"), EndPlayCount)
			|| !GetEnumByPath(*TestRunner, Probe, TEXT("LastReason"), LastReason))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Component EndPlay should run exactly once when the owning actor is destroyed"), EndPlayCount, 1);
		TestRunner->TestEqual(TEXT("Component EndPlay should receive EEndPlayReason::Destroyed"), LastReason, static_cast<int64>(EEndPlayReason::Destroyed));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptDefaultComponentExtendedTest,
	"Angelscript.TestModule.Component.DefaultComponent.Extended",
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

	TEST_METHOD(DefaultComponentPropertiesPointToInstances)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentExtendedProperties"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDefaultComponentExtendedProperties.as"),
			TEXT(R"AS(
UCLASS()
class ATestDefaultComponentExtendedProperties : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root)
	USceneComponent Child;
}
)AS"),
			TEXT("ATestDefaultComponentExtendedProperties"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		USceneComponent* Root = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("Root"));
		USceneComponent* Child = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("Child"));
		if (Root == nullptr || Child == nullptr) return;

		TestRunner->TestEqual(TEXT("Root DefaultComponent property should point to the actor root component"), Root, Actor->GetRootComponent());
		TestRunner->TestEqual(TEXT("Child DefaultComponent property should point to an attached instance"), Child->GetAttachParent(), Root);
		TestRunner->TestEqual(TEXT("Root DefaultComponent should use native default-subobject creation method"), Root->CreationMethod, EComponentCreationMethod::Native);
		TestRunner->TestEqual(TEXT("Child DefaultComponent should use native default-subobject creation method"), Child->CreationMethod, EComponentCreationMethod::Native);
		TestRunner->TestEqual(TEXT("Root component property should have the scripted component name"), Root->GetFName(), FName(TEXT("Root")));
		TestRunner->TestEqual(TEXT("Child component property should have the scripted component name"), Child->GetFName(), FName(TEXT("Child")));
	}

	TEST_METHOD(ScriptedRootAndAttachedBillboardMaterialize)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentCoverageHierarchy"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDefaultComponentCoverageHierarchy.as"),
			TEXT(R"AS(
UCLASS()
class UTestCoverageRootComponent : USceneComponent
{
}

UCLASS()
class UTestCoverageBillboardComponent : UBillboardComponent
{
}

UCLASS()
class ATestDefaultComponentCoverageHierarchy : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UTestCoverageRootComponent ScriptedRoot;

	UPROPERTY(DefaultComponent, Attach = ScriptedRoot)
	UTestCoverageBillboardComponent AttachedBillboard;
}
)AS"),
			TEXT("ATestDefaultComponentCoverageHierarchy"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		USceneComponent* ScriptedRoot = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("ScriptedRoot"));
		UBillboardComponent* AttachedBillboard = ReadComponentProperty<UBillboardComponent>(*TestRunner, Actor, TEXT("AttachedBillboard"));
		if (ScriptedRoot == nullptr || AttachedBillboard == nullptr) return;

		UClass* RootClass = FindGeneratedClass(&Engine, TEXT("UTestCoverageRootComponent"));
		UClass* BillboardClass = FindGeneratedClass(&Engine, TEXT("UTestCoverageBillboardComponent"));
		if (!TestRunner->TestNotNull(TEXT("Scripted root component class should be generated"), RootClass)
			|| !TestRunner->TestNotNull(TEXT("Scripted billboard component class should be generated"), BillboardClass))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Scripted root property should point at the actor root"), ScriptedRoot, Actor->GetRootComponent());
		TestRunner->TestTrue(TEXT("Scripted root should use the generated component class"), ScriptedRoot->IsA(RootClass));
		TestRunner->TestTrue(TEXT("Attached billboard should use the generated billboard class"), AttachedBillboard->IsA(BillboardClass));
		TestRunner->TestEqual(TEXT("Attached billboard should preserve the scripted attach hierarchy"),
			AttachedBillboard->GetAttachParent(), ScriptedRoot);
	}

	TEST_METHOD(AttachSocketPersistsAtRuntime)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentExtendedAttachSocket"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDefaultComponentExtendedAttachSocket.as"),
			TEXT(R"AS(
UCLASS()
class ATestDefaultComponentExtendedAttachSocket : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root, AttachSocket = "NamedSocket")
	USceneComponent Child;
}
)AS"),
			TEXT("ATestDefaultComponentExtendedAttachSocket"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		USceneComponent* Root = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("Root"));
		USceneComponent* Child = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("Child"));
		if (Root == nullptr || Child == nullptr) return;

		TestRunner->TestEqual(TEXT("AttachSocket child should attach to the declared parent"), Child->GetAttachParent(), Root);
		TestRunner->TestEqual(TEXT("AttachSocket metadata should persist to the runtime scene attachment"), Child->GetAttachSocketName(), FName(TEXT("NamedSocket")));
	}

	TEST_METHOD(OverrideComponentMaterializesReplacement)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentExtendedOverride"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDefaultComponentExtendedOverride.as"),
			TEXT(R"AS(
UCLASS()
class ATestDefaultComponentExtendedOverrideBase : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root)
	USceneComponent BaseChild;
}

UCLASS()
class ATestDefaultComponentExtendedOverrideChild : ATestDefaultComponentExtendedOverrideBase
{
	UPROPERTY(OverrideComponent = BaseChild)
	UStaticMeshComponent Replacement;
}
)AS"),
			TEXT("ATestDefaultComponentExtendedOverrideChild"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		USceneComponent* Root = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("Root"));
		UStaticMeshComponent* ReplacementProperty = ReadComponentProperty<UStaticMeshComponent>(*TestRunner, Actor, TEXT("Replacement"));
		UActorComponent* BaseChildComponent = FindComponentByName(Actor, TEXT("BaseChild"));
		if (Root == nullptr || ReplacementProperty == nullptr || !TestRunner->TestNotNull(TEXT("Overridden BaseChild component should exist by its base component name"), BaseChildComponent)) return;

		UStaticMeshComponent* ReplacementInstance = Cast<UStaticMeshComponent>(BaseChildComponent);
		if (!TestRunner->TestNotNull(TEXT("OverrideComponent should materialize BaseChild as a UStaticMeshComponent"), ReplacementInstance)) return;

		TestRunner->TestEqual(TEXT("OverrideComponent property should point at the replaced base component instance"), ReplacementProperty, ReplacementInstance);
		TestRunner->TestEqual(TEXT("OverrideComponent should preserve the base component object name"), ReplacementInstance->GetFName(), FName(TEXT("BaseChild")));
		TestRunner->TestEqual(TEXT("OverrideComponent replacement should keep the base attachment parent"), ReplacementInstance->GetAttachParent(), Root);
	}

	TEST_METHOD(NativeActorExtraComponentAttachesToInheritedRoot)
	{
		using namespace AngelscriptTest_Component_LifecycleExtended_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDefaultComponentExtendedNativeRoot"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDefaultComponentExtendedNativeRoot.as"),
			TEXT(R"AS(
UCLASS()
class ATestDefaultComponentExtendedNativeRoot : ACharacter
{
	UPROPERTY(DefaultComponent)
	USceneComponent ExtraMarker;
}
)AS"),
			TEXT("ATestDefaultComponentExtendedNativeRoot"));
		if (ActorClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;

		USceneComponent* ExtraMarker = ReadComponentProperty<USceneComponent>(*TestRunner, Actor, TEXT("ExtraMarker"));
		USceneComponent* InheritedRoot = Actor->GetRootComponent();
		if (ExtraMarker == nullptr || !TestRunner->TestNotNull(TEXT("Native actor should keep its inherited root component"), InheritedRoot)) return;

		TestRunner->TestNotEqual(TEXT("Extra script component should not replace the native inherited root"), ExtraMarker, InheritedRoot);
		TestRunner->TestEqual(TEXT("Extra script component should attach to the native inherited root"), ExtraMarker->GetAttachParent(), InheritedRoot);
		TestRunner->TestEqual(TEXT("Extra script component should preserve its script property name"), ExtraMarker->GetFName(), FName(TEXT("ExtraMarker")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
