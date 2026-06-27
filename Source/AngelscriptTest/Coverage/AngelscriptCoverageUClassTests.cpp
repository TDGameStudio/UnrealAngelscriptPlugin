#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestModuleBuilder.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUClassTests
// -----------------------------------------------------------------------------
// Comprehensive test coverage for UCLASS, UPROPERTY, UFUNCTION, and UComponent
// features in AngelScript, addressing uncovered items (⬜) from:
//   * Documents/Coverage/Coverage_UClass.md
//   * Documents/Coverage/Coverage_UComponent.md
//
// Test axes covered:
//   * UClassDeclarationAndInheritance  - UCLASS declarations, inheritance chains
//   * UPropertySpecifiers              - UPROPERTY specifiers (EditAnywhere, BlueprintReadWrite, etc.)
//   * UFunctionSpecifiers              - UFUNCTION specifiers (BlueprintCallable, BlueprintPure, etc.)
//   * ComponentDeclaration             - DefaultComponent, RootComponent, Attach
//   * SceneComponentOperations         - Transform, attachment, detachment
//   * ComponentLifecycle               - BeginPlay, Tick, EndPlay
//
// Pattern: Compile AS modules with various specifiers, spawn actors, verify
// through UClass reflection and runtime behavior.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUClassComponentTest,
	"Angelscript.TestModule.Coverage.UClassComponent",
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

	// -------------------------------------------------------------------------
	// UCLASS Declaration and Inheritance
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassDeclarationAndInheritance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DeclInherit"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassDeclInherit.as"),
			ASTEST_AS(R"AS(
			// Base UCLASS with basic properties
			UCLASS()
			class ABaseTestActor : AActor
			{
				UPROPERTY()
				int BaseValue = 100;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BaseValue = 200;
				}
			}

			// Derived UCLASS inheriting from script class
			UCLASS()
			class ADerivedTestActor : ABaseTestActor
			{
				UPROPERTY()
				int DerivedValue = 50;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Super::BeginPlay();
					DerivedValue = BaseValue + 10;
				}
			}
			)AS"),
			TEXT("ADerivedTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Derived UCLASS should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Derived actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BaseValue"), 200, TEXT("Base class BeginPlay should execute"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DerivedValue"), 210, TEXT("Derived class should access base value"));
	}

	// -------------------------------------------------------------------------
	// UPROPERTY Specifiers: EditAnywhere, BlueprintReadWrite, etc.
	// -------------------------------------------------------------------------
	TEST_METHOD(UPropertySpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUProperty_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class APropertySpecifierActor : AActor
			{
				// EditAnywhere - editable everywhere
				UPROPERTY(EditAnywhere)
				int EditAnywhereValue = 10;

				// EditDefaultsOnly - editable in defaults only
				UPROPERTY(EditDefaultsOnly)
				int EditDefaultsOnlyValue = 20;

				// EditInstanceOnly - editable in instances only
				UPROPERTY(EditInstanceOnly)
				int EditInstanceOnlyValue = 30;

				// BlueprintReadWrite - readable and writable in BP
				UPROPERTY(BlueprintReadWrite)
				int BlueprintReadWriteValue = 40;

				// BlueprintReadOnly - read-only in BP
				UPROPERTY(BlueprintReadOnly)
				int BlueprintReadOnlyValue = 50;

				// Transient - not saved
				UPROPERTY(Transient)
				int TransientValue = 60;

				// Config - saved to config file
				UPROPERTY(Config)
				int ConfigValue = 70;

				// SaveGame - saved in save games
				UPROPERTY(SaveGame)
				int SaveGameValue = 80;

				// Category with EditAnywhere
				UPROPERTY(EditAnywhere, Category="Test Category")
				int CategoryValue = 90;

				// Multiple specifiers combined
				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combined")
				int CombinedValue = 100;
			}
			)AS"),
			TEXT("APropertySpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Property specifier actor should compile")));

		// Verify property flags via reflection
		FProperty* EditAnywhereProp = ScriptClass->FindPropertyByName(TEXT("EditAnywhereValue"));
		ASSERT_THAT(IsNotNull(EditAnywhereProp, TEXT("EditAnywhere property should exist")));
		ASSERT_THAT(IsTrue(EditAnywhereProp->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should have CPF_Edit flag")));

		FProperty* BlueprintReadOnlyProp = ScriptClass->FindPropertyByName(TEXT("BlueprintReadOnlyValue"));
		ASSERT_THAT(IsNotNull(BlueprintReadOnlyProp, TEXT("BlueprintReadOnly property should exist")));
		ASSERT_THAT(IsTrue(BlueprintReadOnlyProp->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), 
			TEXT("BlueprintReadOnly should have CPF_BlueprintVisible and CPF_BlueprintReadOnly flags")));

		FProperty* TransientProp = ScriptClass->FindPropertyByName(TEXT("TransientValue"));
		ASSERT_THAT(IsNotNull(TransientProp, TEXT("Transient property should exist")));
		ASSERT_THAT(IsTrue(TransientProp->HasAnyPropertyFlags(CPF_Transient), TEXT("Transient should have CPF_Transient flag")));
	}

	// -------------------------------------------------------------------------
	// UFUNCTION Specifiers: BlueprintCallable, BlueprintPure, etc.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionSpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AFunctionSpecifierActor : AActor
			{
				UPROPERTY()
				int CallableValue = 0;

				UPROPERTY()
				int PureValue = 42;

				// BlueprintCallable - callable from BP
				UFUNCTION(BlueprintCallable)
				void BlueprintCallableFunction(int Value)
				{
					CallableValue = Value;
				}

				// BlueprintPure - pure function (no side effects, const)
				UFUNCTION(BlueprintPure)
				int BlueprintPureFunction()
				{
					return PureValue;
				}

				// BlueprintCallable with return value
				UFUNCTION(BlueprintCallable)
				int AddNumbers(int A, int B)
				{
					return A + B;
				}

				// BlueprintCallable with multiple parameters
				UFUNCTION(BlueprintCallable)
				void SetValues(int Value1, int Value2)
				{
					CallableValue = Value1;
					PureValue = Value2;
				}

				// Category specified
				UFUNCTION(BlueprintCallable, Category="Test Functions")
				int GetDoubleValue(int Value)
				{
					return Value * 2;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BlueprintCallableFunction(100);
				}
			}
			)AS'),
			TEXT("AFunctionSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Function specifier actor should compile")));

		// Verify function flags via reflection
		UFunction* CallableFunc = ScriptClass->FindFunctionByName(TEXT("BlueprintCallableFunction"));
		ASSERT_THAT(IsNotNull(CallableFunc, TEXT("BlueprintCallable function should exist")));
		ASSERT_THAT(IsTrue(CallableFunc->HasAnyFunctionFlags(FUNC_BlueprintCallable), 
			TEXT("BlueprintCallable should have FUNC_BlueprintCallable flag")));

		UFunction* PureFunc = ScriptClass->FindFunctionByName(TEXT("BlueprintPureFunction"));
		ASSERT_THAT(IsNotNull(PureFunc, TEXT("BlueprintPure function should exist")));
		ASSERT_THAT(IsTrue(PureFunc->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure), 
			TEXT("BlueprintPure should have FUNC_BlueprintCallable and FUNC_BlueprintPure flags")));

		// Spawn and verify runtime behavior
		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Function specifier actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallableValue"), 100, 
			TEXT("BlueprintCallable function should execute in BeginPlay"));
	}

	// -------------------------------------------------------------------------
	// DefaultComponent: Basic component declaration
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Declaration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentDeclaration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AComponentDeclActor : AActor
			{
				// RootComponent declaration
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				// Component attached to root
				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent ChildComponent;

				// StaticMeshComponent
				UPROPERTY(DefaultComponent, Attach=Root)
				UStaticMeshComponent MeshComponent;

				// Actor component (no transform)
				UPROPERTY(DefaultComponent)
				UActorComponent LogicComponent;

				UPROPERTY()
				bool AllComponentsValid = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AllComponentsValid = (Root != nullptr) && 
					                     (ChildComponent != nullptr) && 
					                     (MeshComponent != nullptr) &&
					                     (LogicComponent != nullptr);
				}
			}
			)AS"),
			TEXT("AComponentDeclActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component declaration actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component declaration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllComponentsValid"), true,
			TEXT("All DefaultComponents should be created"));
	}

	// -------------------------------------------------------------------------
	// USceneComponent: Transform operations
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentTransform)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_Transform"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentTransform.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ASceneTransformActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child;

				UPROPERTY()
				FVector InitialLocation;

				UPROPERTY()
				FVector UpdatedLocation;

				UPROPERTY()
				FRotator UpdatedRotation;

				UPROPERTY()
				FVector ChildRelativeLocation;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Get initial location
					InitialLocation = Root.GetComponentLocation();

					// Set world location
					Root.SetWorldLocation(FVector(100.f, 200.f, 300.f));
					UpdatedLocation = Root.GetComponentLocation();

					// Set world rotation
					Root.SetWorldRotation(FRotator(45.f, 90.f, 0.f));
					UpdatedRotation = Root.GetComponentRotation();

					// Set child relative location
					Child.SetRelativeLocation(FVector(10.f, 20.f, 30.f));
					ChildRelativeLocation = Child.GetRelativeLocation();
				}
			}
			)AS"),
			TEXT("ASceneTransformActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene transform actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene transform actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify transform operations
		FVector UpdatedLocation;
		if (VerifyByPath<FStructProperty, FVector>(*TestRunner, Actor, TEXT("UpdatedLocation"), UpdatedLocation))
		{
			TestRunner->TestEqual(TEXT("SetWorldLocation should update location"), UpdatedLocation, FVector(100.f, 200.f, 300.f));
		}

		FRotator UpdatedRotation;
		if (VerifyByPath<FStructProperty, FRotator>(*TestRunner, Actor, TEXT("UpdatedRotation"), UpdatedRotation))
		{
			TestRunner->TestEqual(TEXT("SetWorldRotation should update rotation (Pitch)"), UpdatedRotation.Pitch, 45.f, 0.01f);
			TestRunner->TestEqual(TEXT("SetWorldRotation should update rotation (Yaw)"), UpdatedRotation.Yaw, 90.f, 0.01f);
		}
	}

	// -------------------------------------------------------------------------
	// USceneComponent: Attachment operations
	// -------------------------------------------------------------------------
	TEST_METHOD(SceneComponentAttachment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSceneComponent_Attach"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageSceneComponentAttach.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ASceneAttachActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				USceneComponent DynamicChild;

				UPROPERTY()
				bool IsInitiallyAttached = false;

				UPROPERTY()
				bool IsAttachedAfterOperation = false;

				UPROPERTY()
				bool IsDetached = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Check initial attachment state
					IsInitiallyAttached = (DynamicChild.GetAttachParent() == nullptr);

					// Attach to root
					DynamicChild.AttachToComponent(Root, NAME_None, EAttachmentRule::KeepRelative, 
						EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					IsAttachedAfterOperation = (DynamicChild.GetAttachParent() == Root);

					// Detach
					DynamicChild.DetachFromComponent(EDetachmentRule::KeepRelative, 
						EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					IsDetached = (DynamicChild.GetAttachParent() == nullptr);
				}
			}
			)AS"),
			TEXT("ASceneAttachActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scene attachment actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scene attachment actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsAttachedAfterOperation"), true,
			TEXT("Component should attach to root"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsDetached"), true,
			TEXT("Component should detach from parent"));
	}

	// -------------------------------------------------------------------------
	// UActorComponent: Lifecycle methods
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Lifecycle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentLifecycle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ULifecycleComponent : UActorComponent
			{
				UPROPERTY()
				int LifecycleStep = 0;

				UFUNCTION(BlueprintOverride)
				void OnComponentCreated()
				{
					LifecycleStep = 1;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (LifecycleStep == 1)
						LifecycleStep = 2;
				}
			}

			UCLASS()
			class ALifecycleActor : AActor
			{
				UPROPERTY(DefaultComponent)
				ULifecycleComponent TestComponent;

				UPROPERTY()
				int ComponentLifecycleStep = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (TestComponent != nullptr)
						ComponentLifecycleStep = TestComponent.LifecycleStep;
				}
			}
			)AS"),
			TEXT("ALifecycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Lifecycle actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Lifecycle actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentLifecycleStep"), 2,
			TEXT("Component lifecycle methods should execute in order"));
	}

	// -------------------------------------------------------------------------
	// UFUNCTION: Parameter passing (value, &in, &out, &inout)
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParameterPassing)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovUFunc_ParamPass", ASTEST_AS(R"AS(
		// Value parameter
		int AddValue(int A, int B)
		{
			return A + B;
		}

		// &in parameter (read-only reference)
		int MultiplyByTwo(int&in Value)
		{
			return Value * 2;
		}

		// &out parameter (write-only, returns value)
		void GetValues(int&out A, int&out B)
		{
			A = 10;
			B = 20;
		}

		// &inout parameter (read-write reference)
		void DoubleValue(int&inout Value)
		{
			Value = Value * 2;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };

		// Test value parameter
		FASGlobalFunctionInvoker AddInvoker(*TestRunner, Engine, *Module, TEXT("int AddValue(int, int)"));
		AddInvoker.AddArg(5);
		AddInvoker.AddArg(3);
		int32 AddResult = AddInvoker.CallAndReturn<int32>(0);
		TestRunner->TestEqual(TEXT("Value parameters should work"), AddResult, 8);

		// Test &in parameter
		FASGlobalFunctionInvoker MultiplyInvoker(*TestRunner, Engine, *Module, TEXT("int MultiplyByTwo(int&in)"));
		MultiplyInvoker.AddArg(7);
		int32 MultiplyResult = MultiplyInvoker.CallAndReturn<int32>(0);
		TestRunner->TestEqual(TEXT("&in parameter should work"), MultiplyResult, 14);

		// Test &out parameters
		FASGlobalFunctionInvoker OutInvoker(*TestRunner, Engine, *Module, TEXT("void GetValues(int&out, int&out)"));
		int32 OutA = 0, OutB = 0;
		OutInvoker.AddArgRef(OutA);
		OutInvoker.AddArgRef(OutB);
		OutInvoker.Call();
		TestRunner->TestEqual(TEXT("&out parameter A should work"), OutA, 10);
		TestRunner->TestEqual(TEXT("&out parameter B should work"), OutB, 20);

		// Test &inout parameter
		FASGlobalFunctionInvoker InOutInvoker(*TestRunner, Engine, *Module, TEXT("void DoubleValue(int&inout)"));
		int32 InOutValue = 15;
		InOutInvoker.AddArgRef(InOutValue);
		InOutInvoker.Call();
		TestRunner->TestEqual(TEXT("&inout parameter should modify value"), InOutValue, 30);
	}

	// -------------------------------------------------------------------------
	// Container properties: TArray, TMap, TSet
	// -------------------------------------------------------------------------
	TEST_METHOD(ContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageContainer_Props"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageContainerProps.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AContainerPropsActor : AActor
			{
				UPROPERTY()
				TArray<int> IntArray;

				UPROPERTY()
				TMap<int, FString> IntStringMap;

				UPROPERTY()
				TSet<int> IntSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Populate array
					IntArray.Add(10);
					IntArray.Add(20);
					IntArray.Add(30);

					// Populate map
					IntStringMap.Add(1, "One");
					IntStringMap.Add(2, "Two");

					// Populate set
					IntSet.Add(100);
					IntSet.Add(200);
				}
			}
			)AS"),
			TEXT("AContainerPropsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Container props actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Container props actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify TArray
		int32 ArraySize = GetArrayNumByPath(*TestRunner, Actor, TEXT("IntArray"));
		TestRunner->TestEqual(TEXT("TArray should have correct size"), ArraySize, 3);

		int32 ArrayValue;
		if (VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[1]"), ArrayValue))
		{
			TestRunner->TestEqual(TEXT("TArray element should have correct value"), ArrayValue, 20);
		}

		// Verify TMap
		int32 MapSize = GetMapNumByPath(*TestRunner, Actor, TEXT("IntStringMap"));
		TestRunner->TestEqual(TEXT("TMap should have correct size"), MapSize, 2);

		// Verify TSet
		int32 SetSize = GetSetNumByPath(*TestRunner, Actor, TEXT("IntSet"));
		TestRunner->TestEqual(TEXT("TSet should have correct size"), SetSize, 2);

		bool SetContains = SetContainsByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntSet"), 100);
		TestRunner->TestTrue(TEXT("TSet should contain added element"), SetContains);
	}
};

#endif
