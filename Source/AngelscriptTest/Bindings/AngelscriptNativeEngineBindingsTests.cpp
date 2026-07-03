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
//   ActorComponentFactoryContract   — AActor component factory helpers
//   PlayerControllerContract        — APlayerController accessor helpers
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
#include "GameFramework/PlayerController.h"

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
						if (GetNumChildrenComponents() != 1)
						{
							return 50;
						}
						USceneComponent ChildByClass = GetChildComponentByClass(USceneComponent::StaticClass());
						if (!IsValid(ChildByClass) || !(ChildByClass.GetName() == n"ScriptSceneChild"))
						{
							return 55;
						}
						TArray<USceneComponent> ChildComponents;
						GetChildrenComponentsByClass(USceneComponent::StaticClass(), false, ChildComponents);
						if (ChildComponents.Num() != 1)
						{
							return 60;
						}
						if (!IsValid(ChildComponents[0]) || !(ChildComponents[0].GetName() == n"ScriptSceneChild"))
						{
							return 65;
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
						bool bFoundSceneComponent = false;
						for (int Index = 0; Index < SceneComponents.Num(); ++Index)
						{
							if (IsValid(SceneComponents[Index]) && SceneComponents[Index].GetName() == n"ScriptScene")
							{
								bFoundSceneComponent = true;
							}
						}
						if (!bFoundSceneComponent)
						{
							return 170;
						}

						TArray<UActorComponent> AllComponents;
						GetOwner().GetComponentsByClass(AllComponents);
						bool bFoundActorComponent = false;
						for (int Index = 0; Index < AllComponents.Num(); ++Index)
						{
							if (IsValid(AllComponents[Index]) && AllComponents[Index].GetName() == n"ScriptScene")
							{
								bFoundActorComponent = true;
							}
						}
						if (!bFoundActorComponent)
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
		USceneComponent* ChildComponent = NewObject<USceneComponent>(OuterActor, USceneComponent::StaticClass(), TEXT("ScriptSceneChild"));
		ASSERT_THAT(IsNotNull(ChildComponent, TEXT("Transient child scene component should be created")));
		if (ChildComponent != nullptr)
		{
			OuterActor->AddOwnedComponent(ChildComponent);
			ChildComponent->AttachToComponent(RuntimeComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}

		int32 Result = 0;
		ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, ReadComponentBindingsFunction, Result), TEXT("Native component binding reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(
			1,
			Result,
			*FString::Printf(TEXT("Script component should call bridged native component methods; AS returned %d"), Result)));
	}

	// ====================================================================
	// Section: ActorComponentFactoryContract
	// ====================================================================

	TEST_METHOD(ActorComponentFactoryContract)
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
		USceneComponent* RootComponent = NewObject<USceneComponent>(&HostActor, USceneComponent::StaticClass(), TEXT("BindingFactoryRoot"));
		ASSERT_THAT(IsNotNull(RootComponent, TEXT("Actor/component factory contract should create a root scene component")));
		if (RootComponent == nullptr)
		{
			return;
		}
		HostActor.AddInstanceComponent(RootComponent);
		HostActor.SetRootComponent(RootComponent);
		RootComponent->RegisterComponent();

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASNativeEngine_ActorComponentFactoryContract"), ASTEST_AS(R"AS(
			int VerifyActorComponentFactoryContract(AActor Host)
			{
				if (Host == nullptr)
				{
					return 0;
				}

				Host.SetReplicates(true);
				Host.SetActorScale3D(FVector(2.0f, 2.0f, 2.0f));
				Host.SetActorTickInterval(0.25f);

				if (Host.GetActorNameOrLabel().IsEmpty())
				{
					return 20;
				}

				UActorComponent Created = Host.CreateComponent(USceneComponent::StaticClass(), n"BindingCreatedScene");
				if (!IsValid(Created) || Created.GetOwner() != Host)
				{
					return 30;
				}

				UActorComponent Found = Host.GetComponent(USceneComponent::StaticClass(), n"BindingCreatedScene");
				if (Found != Created)
				{
					return 40;
				}

				UActorComponent Existing = Host.GetOrCreateComponent(USceneComponent::StaticClass(), n"BindingCreatedScene");
				if (Existing != Created)
				{
					return 50;
				}

				TArray<UActorComponent> Components;
				Host.GetAllComponents(UActorComponent::StaticClass(), Components);
				return Components.Num() > 0 ? 1 : 60;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("Actor/component factory contract module should compile")));
		if (!Mod.IsValid())
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&HostActor);

		FASGlobalFunctionInvoker Invoker(
			*TestRunner,
			Engine,
			Mod.GetModule(),
			TEXT("int VerifyActorComponentFactoryContract(AActor)"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Actor/component factory contract function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		Invoker.AddArgObject(&HostActor);
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("AActor component factory helpers should dispatch through manual bindings")));
		ASSERT_THAT(IsTrue(HostActor.GetIsReplicated(), TEXT("SetReplicates should update native actor replication state")));
		ASSERT_THAT(IsTrue(HostActor.GetActorScale3D().Equals(FVector(2.0f, 2.0f, 2.0f)), TEXT("SetActorScale3D should update native actor scale")));
		ASSERT_THAT(IsNear(0.25f, HostActor.GetActorTickInterval(), 0.001f, TEXT("SetActorTickInterval should update native actor tick interval")));
	}

	// ====================================================================
	// Section: PlayerControllerContract
	// ====================================================================

	TEST_METHOD(PlayerControllerContract)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASNativeEngine_PlayerControllerContract"), ASTEST_AS(R"AS(
			int VerifyPlayerControllerContract(APlayerController Controller)
			{
				if (Controller == nullptr)
				{
					return 0;
				}
				if (!Controller.IsPlayerController())
				{
					return 10;
				}

				ULocalPlayer LocalPlayer = Controller.GetLocalPlayer();
				APlayerState PlayerState = Controller.GetPlayerState();
				APlayerCameraManager CameraManager = Controller.GetPlayerCameraManager();
				return LocalPlayer == nullptr && PlayerState == nullptr && CameraManager == nullptr ? 1 : 20;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("PlayerController contract module should compile")));
		if (!Mod.IsValid())
		{
			return;
		}

		APlayerController* Controller = NewObject<APlayerController>(GetTransientPackage(), APlayerController::StaticClass());
		ASSERT_THAT(IsNotNull(Controller, TEXT("Transient player controller should be created")));
		if (Controller == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(
			*TestRunner,
			Engine,
			Mod.GetModule(),
			TEXT("int VerifyPlayerControllerContract(APlayerController)"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("PlayerController contract function should resolve")));
		if (!Invoker.IsValid())
		{
			return;
		}

		Invoker.AddArgObject(Controller);
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("APlayerController accessors should resolve and dispatch through manual bindings")));
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
						MarkRenderStateDirty();
						SetbTickInEditor(true);
						SetbIsEditorOnly(false);
						SetIsVisualizationComponent(false);
						EComponentCreationMethod CreationMethod = GetComponentCreationMethod();

						if (IsVisualizationComponent())
						{
							return 0;
						}
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
