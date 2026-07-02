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
//   Four legacy automation tests merged into one TEST_CLASS.
//   Uses CompileAnnotatedModuleFromMemory + FindGeneratedClass pattern.
//   ComponentActivationAndTag uses ASTEST_CREATE_ENGINE_FULL for world context.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "ClassGenerator/ASClass.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

#if WITH_ANGELSCRIPT_UNITTESTS


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

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

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
			ASTEST_AS(R"AS(
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
						{
							return 0;
						}
						return 1;
					}
				}
				)AS"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Compile annotated actor module using native bindings should succeed")));

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("ABindingExampleActor"));
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Generated actor class should exist")));

		UFunction* ReadNativeBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReadNativeBindings"));
		ASSERT_THAT(IsNotNull(ReadNativeBindingsFunction, TEXT("Native actor binding test function should exist")));

		AActor* RuntimeActor = RuntimeClass->GetDefaultObject<AActor>();
		ASSERT_THAT(IsNotNull(RuntimeActor, TEXT("Generated actor default object should exist")));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeActor, ReadNativeBindingsFunction, Result), TEXT("Native actor binding reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(1, Result, TEXT("Script class should call bridged native AActor and UObject methods")));
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
			ASTEST_AS(R"AS(
				UCLASS()
				class UBindingSceneComponent : USceneComponent
				{
					UFUNCTION()
					int ReadComponentBindings()
					{
						FScopedMovementUpdate ScopedMove(this);
						if (!IsValid(GetOwner()))
						{
							return 10;
						}
						if (!IsValid(GetPackage()) || !IsValid(GetOutermost()))
						{
							return 20;
						}

						Deactivate();
						Activate();

						SetRelativeLocation(FVector(1.0, 2.0, 3.0));
						SetComponentVelocity(FVector(4.0, 5.0, 6.0));
						FVector Relative = GetRelativeLocation();
						FTransform Transform = GetComponentTransform();
						FVector Velocity = GetComponentVelocity();

						if (!Relative.Equals(FVector(1.0, 2.0, 3.0)))
						{
							return 30;
						}
						if (!Transform.GetTranslation().Equals(Relative))
						{
							return 40;
						}
						if (!Velocity.Equals(FVector(4.0, 5.0, 6.0)))
						{
							return 45;
						}
						if (GetNumChildrenComponents() != 0)
						{
							return 50;
						}
						UActorComponent FoundByClass = GetOwner().GetComponent(USceneComponent::StaticClass());
						if (!IsValid(FoundByClass))
						{
							return 80;
						}
						if (!(FoundByClass.GetName() == n"ScriptScene"))
						{
							return 90;
						}
						UActorComponent FoundByName = GetOwner().GetComponent(USceneComponent::StaticClass(), n"ScriptScene");
						if (!IsValid(FoundByName))
						{
							return 100;
						}
						if (!(FoundByName.GetName() == n"ScriptScene"))
						{
							return 110;
						}
						if (!IsValid(USceneComponent::Get(GetOwner())))
						{
							return 120;
						}
						if (!(USceneComponent::Get(GetOwner()).GetName() == n"ScriptScene"))
						{
							return 130;
						}
						if (!IsValid(USceneComponent::Get(GetOwner(), n"ScriptScene")))
						{
							return 140;
						}
						if (!(USceneComponent::Get(GetOwner(), n"ScriptScene").GetName() == n"ScriptScene"))
						{
							return 150;
						}

						TArray<USceneComponent> SceneComponents;
						SceneComponents.Add(USceneComponent::Get(GetOwner()));
						SceneComponents.Empty();
						GetOwner().GetComponentsByClass(SceneComponents);
						if (SceneComponents.Num() != 1)
						{
							return 160;
						}
						if (!IsValid(SceneComponents[0]) || !(SceneComponents[0].GetName() == n"ScriptScene"))
						{
							return 170;
						}

						TArray<UActorComponent> AllComponents;
						GetOwner().GetComponentsByClass(AllComponents);
						if (AllComponents.Num() != 1)
						{
							return 180;
						}
						if (!IsValid(AllComponents[0]) || !(AllComponents[0].GetName() == n"ScriptScene"))
						{
							return 190;
						}

						return ComponentHasTag(NAME_None) ? 0 : 1;
					}
				}
				)AS"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Compile annotated scene component module using native bindings should succeed")));

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UBindingSceneComponent"));
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Generated scene component class should exist")));

		UFunction* ReadComponentBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReadComponentBindings"));
		ASSERT_THAT(IsNotNull(ReadComponentBindingsFunction, TEXT("Native component binding test function should exist")));

		AActor* OuterActor = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass());
		ASSERT_THAT(IsNotNull(OuterActor, TEXT("Transient outer actor should be created")));

		USceneComponent* RuntimeComponent = NewObject<USceneComponent>(OuterActor, RuntimeClass, TEXT("ScriptScene"));
		ASSERT_THAT(IsNotNull(RuntimeComponent, TEXT("Generated scene component instance should be created")));

		OuterActor->AddOwnedComponent(RuntimeComponent);

		int32 Result = 0;
		ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, ReadComponentBindingsFunction, Result), TEXT("Native component binding reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(1, Result, TEXT("Script component should call bridged native component methods")));
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
			ASTEST_AS(R"AS(
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
				)AS"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Compile annotated destroy component module should succeed")));

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UDestroyBindingComponent"));
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Generated destroy component class should exist")));

		UFunction* DestroySelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("DestroySelf"));
		ASSERT_THAT(IsNotNull(DestroySelfFunction, TEXT("Destroy component function should exist")));

		AActor* OuterActor = NewObject<AActor>(GetTransientPackage(), AActor::StaticClass());
		ASSERT_THAT(IsNotNull(OuterActor, TEXT("Transient actor should be created for destroy component test")));

		UActorComponent* RuntimeComponent = NewObject<UActorComponent>(OuterActor, RuntimeClass, TEXT("DestroyBindingComponent"));
		ASSERT_THAT(IsNotNull(RuntimeComponent, TEXT("Destroy binding component should be created")));

		OuterActor->AddOwnedComponent(RuntimeComponent);

		int32 Result = 0;
		if (!this->Assert.IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, DestroySelfFunction, Result), TEXT("Destroy component reflected call should execute on the game thread")))
		{
			return;
		}

		if (!this->Assert.AreEqual(1, Result, TEXT("Destroy component function should return success")))
		{
			return;
		}

		const bool bDestroyStateReported = this->Assert.IsTrue(RuntimeComponent->IsBeingDestroyed(), TEXT("DestroyComponent binding should mark the component as being destroyed"));
		(void)bDestroyStateReported;
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
			ASTEST_AS(R"AS(
				UCLASS()
				class UBindingActivationComponent : UActorComponent
				{
					UFUNCTION()
					int VerifyTagBindings()
					{
						if (!ComponentHasTag(n"Probe"))
						{
							return 0;
						}
						if (ComponentHasTag(NAME_None))
						{
							return 0;
						}
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
				)AS"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Compile annotated activation component module should succeed")));

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UBindingActivationComponent"));
		ASSERT_THAT(IsNotNull(RuntimeClass, TEXT("Generated activation component class should exist")));

		UFunction* VerifyTagBindingsFunction = FindGeneratedFunction(RuntimeClass, TEXT("VerifyTagBindings"));
		UFunction* DeactivateSelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("DeactivateSelf"));
		UFunction* ReactivateSelfFunction = FindGeneratedFunction(RuntimeClass, TEXT("ReactivateSelf"));
		ASSERT_THAT(IsNotNull(VerifyTagBindingsFunction, TEXT("VerifyTagBindings function should exist")));
		ASSERT_THAT(IsNotNull(DeactivateSelfFunction, TEXT("DeactivateSelf function should exist")));
		ASSERT_THAT(IsNotNull(ReactivateSelfFunction, TEXT("ReactivateSelf function should exist")));

		UActorComponent* RuntimeComponent = NewObject<UActorComponent>(&HostActor, RuntimeClass, TEXT("ActivationBindingComponent"));
		ASSERT_THAT(IsNotNull(RuntimeComponent, TEXT("Generated activation component instance should be created")));

		HostActor.AddInstanceComponent(RuntimeComponent);
		RuntimeComponent->ComponentTags.Add(TEXT("Probe"));
		RuntimeComponent->RegisterComponent();
		ASSERT_THAT(IsTrue(RuntimeComponent->IsRegistered(), TEXT("Activation component should register into the spawned world")));

		RuntimeComponent->Activate(true);
		ASSERT_THAT(IsTrue(RuntimeComponent->IsActive(), TEXT("Activation component should start active before script toggles it")));

		int32 TagResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, VerifyTagBindingsFunction, TagResult), TEXT("VerifyTagBindings reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(1, TagResult, TEXT("Tag compatibility probe should pass")));

		int32 DeactivateResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, DeactivateSelfFunction, DeactivateResult), TEXT("DeactivateSelf reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(1, DeactivateResult, TEXT("DeactivateSelf should report success")));
		ASSERT_THAT(IsFalse(RuntimeComponent->IsActive(), TEXT("Deactivate binding should clear the active state")));

		int32 ReactivateResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, ReactivateSelfFunction, ReactivateResult), TEXT("ReactivateSelf reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(1, ReactivateResult, TEXT("ReactivateSelf should report success")));

		ASSERT_THAT(IsTrue(RuntimeComponent->IsActive(), TEXT("Activate(true) binding should restore the active state")));
	}
};

#endif
