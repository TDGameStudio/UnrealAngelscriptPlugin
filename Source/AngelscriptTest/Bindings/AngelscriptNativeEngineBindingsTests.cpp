// ============================================================================
// AngelscriptNativeEngineBindingsTests.cpp
//
// Native engine/actor/component method binding coverage — CQTest refactor.
// Automation IDs:
//   Angelscript.TestModule.Bindings.NativeEngine.FAngelscriptNativeEngineBindingsTest.*
//
// Sections:
//   NativeActorMethods              — AActor native method bridging
//   NativeComponentMethods          — USceneComponent native method bridging
//   ComponentDestroy                — DestroyComponent binding
//   ComponentActivationAndTag       — Activate/Deactivate/ComponentHasTag
//
// CQTest adaptation notes:
//   Four IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Uses CompileAnnotatedModuleFromMemory + FindGeneratedClass pattern.
//   ComponentActivationAndTag uses ASTEST_CREATE_ENGINE_FULL for world context.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "ClassGenerator/ASClass.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeEngineBindingsTest,
	"Angelscript.TestModule.Bindings.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: NativeActorMethods
	// ====================================================================

	TEST_METHOD(NativeActorMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("ASNativeActorBindingTest"),
			TEXT("ASNativeActorBindingTest.as"),
			TEXT(R"(
UCLASS()
class ABindingExampleActor : AActor
{
	UFUNCTION()
	int ReadNativeBindings()
	{
		FVector Location = GetActorLocation();
		FRotator Rotation = GetActorRotation();
		UClass RuntimeType = GetClass();
		FName ClassName = RuntimeType.GetName();
		FString Path = GetPathName();
		FString FullName = GetFullName();
		bool bActorType = IsA(RuntimeType);

		if (Path.Len() < 0 || FullName.Len() < 0 || !bActorType)
			return 0;
		return 1;
	}
}
)"));
		if (!TestRunner->TestTrue(TEXT("Compile annotated actor module using native bindings should succeed"), bCompiled))
		{
			return;
		}

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("ABindingExampleActor"));
		if (!TestRunner->TestNotNull(TEXT("Generated actor class should exist"), RuntimeClass))
		{
			return;
		}

		UFunction* ReadNativeBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReadNativeBindings"));
		if (!TestRunner->TestNotNull(TEXT("Native actor binding test function should exist"), ReadNativeBindingsFunction))
		{
			return;
		}

		AActor* RuntimeActor = RuntimeClass->GetDefaultObject<AActor>();
		if (!TestRunner->TestNotNull(TEXT("Generated actor default object should exist"), RuntimeActor))
		{
			return;
		}

		int32 Result = 0;
		if (!TestRunner->TestTrue(TEXT("Native actor binding reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeActor, ReadNativeBindingsFunction, Result)))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Script class should call bridged native AActor and UObject methods"), Result, 1);
	}

	// ====================================================================
	// Section: NativeComponentMethods
	// ====================================================================

	TEST_METHOD(NativeComponentMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("ASNativeComponentBindingTest"),
			TEXT("ASNativeComponentBindingTest.as"),
			TEXT(R"(
UCLASS()
class UBindingSceneComponent : USceneComponent
{
	UFUNCTION()
	int ReadComponentBindings()
	{
		FScopedMovementUpdate ScopedMove(this);
		if (!IsValid(GetOwner()))
			return 10;
		if (!IsValid(GetPackage()) || !IsValid(GetOutermost()))
			return 20;

		Deactivate();
		Activate();

		SetRelativeLocation(FVector(1.0, 2.0, 3.0));
		SetComponentVelocity(FVector(4.0, 5.0, 6.0));
		FVector Relative = GetRelativeLocation();
		FTransform Transform = GetComponentTransform();
		FVector Velocity = GetComponentVelocity();

		if (!Relative.Equals(FVector(1.0, 2.0, 3.0)))
			return 30;
		if (!Transform.GetTranslation().Equals(Relative))
			return 40;
		if (!Velocity.Equals(FVector(4.0, 5.0, 6.0)))
			return 45;
		if (GetNumChildrenComponents() != 0)
			return 50;
		UActorComponent FoundByClass = GetOwner().GetComponent(USceneComponent::StaticClass());
		if (!IsValid(FoundByClass))
			return 80;
		if (!(FoundByClass.GetName() == n"ScriptScene"))
			return 90;
		UActorComponent FoundByName = GetOwner().GetComponent(USceneComponent::StaticClass(), n"ScriptScene");
		if (!IsValid(FoundByName))
			return 100;
		if (!(FoundByName.GetName() == n"ScriptScene"))
			return 110;
		if (!IsValid(USceneComponent::Get(GetOwner())))
			return 120;
		if (!(USceneComponent::Get(GetOwner()).GetName() == n"ScriptScene"))
			return 130;
		if (!IsValid(USceneComponent::Get(GetOwner(), n"ScriptScene")))
			return 140;
		if (!(USceneComponent::Get(GetOwner(), n"ScriptScene").GetName() == n"ScriptScene"))
			return 150;

		TArray<USceneComponent> SceneComponents;
		SceneComponents.Add(USceneComponent::Get(GetOwner()));
		SceneComponents.Empty();
		GetOwner().GetComponentsByClass(SceneComponents);
		if (SceneComponents.Num() != 1)
			return 160;
		if (!IsValid(SceneComponents[0]) || !(SceneComponents[0].GetName() == n"ScriptScene"))
			return 170;

		TArray<UActorComponent> AllComponents;
		GetOwner().GetComponentsByClass(AllComponents);
		if (AllComponents.Num() != 1)
			return 180;
		if (!IsValid(AllComponents[0]) || !(AllComponents[0].GetName() == n"ScriptScene"))
			return 190;

		return ComponentHasTag(NAME_None) ? 0 : 1;
	}
}
)"));
		if (!TestRunner->TestTrue(TEXT("Compile annotated scene component module using native bindings should succeed"), bCompiled))
		{
			return;
		}

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UBindingSceneComponent"));
		if (!TestRunner->TestNotNull(TEXT("Generated scene component class should exist"), RuntimeClass))
		{
			return;
		}

		UFunction* ReadComponentBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReadComponentBindings"));
		if (!TestRunner->TestNotNull(TEXT("Native component binding test function should exist"), ReadComponentBindingsFunction))
		{
			return;
		}

		AActor* OuterActor = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("Transient outer actor should be created"), OuterActor))
		{
			return;
		}

		USceneComponent* RuntimeComponent = NewObject<USceneComponent>(OuterActor, RuntimeClass, TEXT("ScriptScene"));
		if (!TestRunner->TestNotNull(TEXT("Generated scene component instance should be created"), RuntimeComponent))
		{
			return;
		}

		OuterActor->AddOwnedComponent(RuntimeComponent);

		int32 Result = 0;
		if (!TestRunner->TestTrue(TEXT("Native component binding reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, ReadComponentBindingsFunction, Result)))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Script component should call bridged native component methods"), Result, 1);
	}

	// ====================================================================
	// Section: ComponentDestroy
	// ====================================================================

	TEST_METHOD(ComponentDestroy)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("ASComponentDestroyCompat"),
			TEXT("ASComponentDestroyCompat.as"),
			TEXT(R"(
UCLASS()
class UDestroyBindingComponent : UActorComponent
{
	UFUNCTION()
	int DestroySelf()
	{
		DestroyComponent();
		return 1;
	}
}
)"));
		if (!TestRunner->TestTrue(TEXT("Compile annotated destroy component module should succeed"), bCompiled))
		{
			return;
		}

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UDestroyBindingComponent"));
		if (!TestRunner->TestNotNull(TEXT("Generated destroy component class should exist"), RuntimeClass))
		{
			return;
		}

		UFunction* DestroySelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("DestroySelf"));
		if (!TestRunner->TestNotNull(TEXT("Destroy component function should exist"), DestroySelfFunction))
		{
			return;
		}

		AActor* OuterActor = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("Transient actor should be created for destroy component test"), OuterActor))
		{
			return;
		}

		UActorComponent* RuntimeComponent = NewObject<UActorComponent>(OuterActor, RuntimeClass, TEXT("DestroyBindingComponent"));
		if (!TestRunner->TestNotNull(TEXT("Destroy binding component should be created"), RuntimeComponent))
		{
			return;
		}

		OuterActor->AddOwnedComponent(RuntimeComponent);

		int32 Result = 0;
		if (!TestRunner->TestTrue(TEXT("Destroy component reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, DestroySelfFunction, Result)))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Destroy component function should return success"), Result, 1))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("DestroyComponent binding should mark the component as being destroyed"), RuntimeComponent->IsBeingDestroyed());
	}

	// ====================================================================
	// Section: ComponentActivationAndTag
	// ====================================================================

	TEST_METHOD(ComponentActivationAndTag)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& HostActor = Spawner.SpawnActor<AActor>();

		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("ASComponentActivationAndTagCompat"),
			TEXT("ASComponentActivationAndTagCompat.as"),
			TEXT(R"(
UCLASS()
class UBindingActivationComponent : UActorComponent
{
	UFUNCTION()
	int VerifyTagBindings()
	{
		if (!ComponentHasTag(n"Probe"))
			return 0;
		if (ComponentHasTag(NAME_None))
			return 0;
		return 1;
	}

	UFUNCTION()
	int DeactivateSelf()
	{
		Deactivate();
		return 1;
	}

	UFUNCTION()
	int ReactivateSelf()
	{
		Activate(true);
		return 1;
	}
}
)"));
		if (!TestRunner->TestTrue(TEXT("Compile annotated activation component module should succeed"), bCompiled))
		{
			return;
		}

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UBindingActivationComponent"));
		if (!TestRunner->TestNotNull(TEXT("Generated activation component class should exist"), RuntimeClass))
		{
			return;
		}

		UFunction* VerifyTagBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("VerifyTagBindings"));
		UFunction* DeactivateSelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("DeactivateSelf"));
		UFunction* ReactivateSelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReactivateSelf"));
		if (!TestRunner->TestNotNull(TEXT("VerifyTagBindings function should exist"), VerifyTagBindingsFunction)
			|| !TestRunner->TestNotNull(TEXT("DeactivateSelf function should exist"), DeactivateSelfFunction)
			|| !TestRunner->TestNotNull(TEXT("ReactivateSelf function should exist"), ReactivateSelfFunction))
		{
			return;
		}

		UActorComponent* RuntimeComponent = NewObject<UActorComponent>(&HostActor, RuntimeClass, TEXT("ActivationBindingComponent"));
		if (!TestRunner->TestNotNull(TEXT("Generated activation component instance should be created"), RuntimeComponent))
		{
			return;
		}

		HostActor.AddInstanceComponent(RuntimeComponent);
		RuntimeComponent->ComponentTags.Add(TEXT("Probe"));
		RuntimeComponent->RegisterComponent();
		if (!TestRunner->TestTrue(TEXT("Activation component should register into the spawned world"), RuntimeComponent->IsRegistered()))
		{
			return;
		}

		RuntimeComponent->Activate(true);
		if (!TestRunner->TestTrue(TEXT("Activation component should start active before script toggles it"), RuntimeComponent->IsActive()))
		{
			return;
		}

		int32 TagResult = 0;
		if (!TestRunner->TestTrue(TEXT("VerifyTagBindings reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, VerifyTagBindingsFunction, TagResult)))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Tag compatibility probe should pass"), TagResult, 1))
		{
			return;
		}

		int32 DeactivateResult = 0;
		if (!TestRunner->TestTrue(TEXT("DeactivateSelf reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, DeactivateSelfFunction, DeactivateResult)))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("DeactivateSelf should report success"), DeactivateResult, 1))
		{
			return;
		}
		if (!TestRunner->TestFalse(TEXT("Deactivate binding should clear the active state"), RuntimeComponent->IsActive()))
		{
			return;
		}

		int32 ReactivateResult = 0;
		if (!TestRunner->TestTrue(TEXT("ReactivateSelf reflected call should execute on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, ReactivateSelfFunction, ReactivateResult)))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ReactivateSelf should report success"), ReactivateResult, 1))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Activate(true) binding should restore the active state"), RuntimeComponent->IsActive());
	}
};

#endif
