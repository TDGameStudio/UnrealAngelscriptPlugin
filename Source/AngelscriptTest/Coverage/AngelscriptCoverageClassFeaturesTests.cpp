#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageClassFeaturesTests
// -----------------------------------------------------------------------------
// Comprehensive class feature coverage for AngelScript, following the matrix
// from Documents/Coverage/Coverage_UClass.md (Submatrices 4-6: Features).
//
// Test axes covered:
//   * DefaultKeywordOverride        - default keyword overriding parent properties
//   * DefaultKeywordMethods         - default keyword calling parent methods
//   * AccessModifiers               - private, protected, public visibility
//   * AbstractClass                 - Abstract classes cannot be instantiated
//   * InterfaceImplementation       - Implementing interfaces, Cast<IInterface>
//   * ComponentDeclaration          - DefaultComponent, RootComponent, Attach
//   * ComponentTypes                - Various component types (Scene, Static, etc.)
//   * InheritanceChain              - Multi-level inheritance, super calls
//   * ClassCasting                  - Cast upward/downward in hierarchy
//
// Pattern: Compile script modules with class features, spawn actors, validate
// behavior through property inspection and method calls.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageClassFeaturesTest,
	"Angelscript.TestModule.Coverage.ClassFeatures",
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
	// default keyword: override parent class default property values
	// -------------------------------------------------------------------------
	TEST_METHOD(DefaultKeywordOverride)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_DefaultOverride"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesDefaultOverride.as"),
			ASTEST_AS(R"AS(
			// Base class with default values
			UCLASS()
			class ABaseDefaultActor : AActor
			{
				UPROPERTY()
				int Health = 100;

				UPROPERTY()
				float Speed = 500.0f;

				UPROPERTY()
				FString Name = "Base";
			}

			// Derived class overriding defaults
			UCLASS()
			class ADerivedDefaultActor : ABaseDefaultActor
			{
				default Health = 200;
				default Speed = 600.0f;
				default Name = "Derived";

				UPROPERTY()
				int Armor = 50;
			}

			// Deep derived with multiple override levels
			UCLASS()
			class ADeepDefaultActor : ADerivedDefaultActor
			{
				default Health = 300;
				default Armor = 100;
			}
			)AS"),
			TEXT("ADerivedDefaultActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Derived class with default overrides should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Derived actor should spawn")));

		// Verify overridden defaults
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Health"), 200, TEXT("Health should use derived default"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("Speed"), 600.0f, TEXT("Speed should use derived default"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Name"), FString(TEXT("Derived")), TEXT("Name should use derived default"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Armor"), 50, TEXT("Armor should use declared default"));

		// Test deep derived class
		UClass* DeepClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_DefaultOverride.ADeepDefaultActor")));
		ASSERT_THAT(IsNotNull(DeepClass, TEXT("Deep derived class should compile")));

		AActor* DeepActor = SpawnScriptActor(*TestRunner, Spawner, DeepClass);
		ASSERT_THAT(IsNotNull(DeepActor, TEXT("Deep derived actor should spawn")));

		VerifyByPath<FIntProperty, int32>(*TestRunner, DeepActor, TEXT("Health"), 300, TEXT("Health should use deep default"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, DeepActor, TEXT("Armor"), 100, TEXT("Armor should use deep default"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, DeepActor, TEXT("Speed"), 600.0f, TEXT("Speed should inherit from mid-level default"));
	}

	// -------------------------------------------------------------------------
	// default keyword: calling parent class methods
	// -------------------------------------------------------------------------
	TEST_METHOD(DefaultKeywordMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_DefaultMethods"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesDefaultMethods.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ADefaultMethodActor : AActor
			{
				UPROPERTY()
				int DefaultMethodsCalled = 0;

				// Use default to call parent methods
				default SetReplicates(true);
				default SetReplicateMovement(true);

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Verify default method calls took effect
					if (GetIsReplicated())
					{
						DefaultMethodsCalled++;
					}
					if (IsReplicatingMovement())
					{
						DefaultMethodsCalled++;
					}
				}
			}
			)AS"),
			TEXT("ADefaultMethodActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Default method class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default method actor should spawn")));

		BeginPlayActor(Engine, *Actor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DefaultMethodsCalled"), 2, TEXT("Default method calls should take effect"));
	}

	// -------------------------------------------------------------------------
	// Access modifiers: private, protected, public
	// -------------------------------------------------------------------------
	TEST_METHOD(AccessModifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_AccessModifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesAccessModifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AAccessModifierBase : AActor
			{
				private int PrivateValue = 100;

				protected int ProtectedValue = 200;

				public int PublicValue = 300;

				UPROPERTY()
				int TestResult = 0;

				// Private method
				private void PrivateMethod()
				{
					PrivateValue = 111;
				}

				// Protected method
				protected void ProtectedMethod()
				{
					ProtectedValue = 222;
				}

				// Public method
				public void PublicMethod()
				{
					PublicValue = 333;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test access within same class
					PrivateMethod();
					ProtectedMethod();
					PublicMethod();

					if (PrivateValue == 111 && ProtectedValue == 222 && PublicValue == 333)
					{
						TestResult = 1;
					}
				}
			}

			UCLASS()
			class AAccessModifierDerived : AAccessModifierBase
			{
				UPROPERTY()
				int DerivedTestResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Super::BeginPlay();

					// Derived class can access protected members
					ProtectedValue = 444;
					ProtectedMethod();

					// Derived class can access public members
					PublicValue = 555;
					PublicMethod();

					if (ProtectedValue == 222 && PublicValue == 333)
					{
						DerivedTestResult = 1;
					}
				}
			}
			)AS"),
			TEXT("AAccessModifierBase"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Access modifier base class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Access modifier actor should spawn")));

		BeginPlayActor(Engine, *Actor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TestResult"), 1, TEXT("Base class should access all modifiers"));

		// Test derived class access
		UClass* DerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_AccessModifiers.AAccessModifierDerived")));
		ASSERT_THAT(IsNotNull(DerivedClass, TEXT("Derived class should compile")));

		AActor* DerivedActor = SpawnScriptActor(*TestRunner, Spawner, DerivedClass);
		ASSERT_THAT(IsNotNull(DerivedActor, TEXT("Derived actor should spawn")));

		BeginPlayActor(Engine, *DerivedActor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, DerivedActor, TEXT("DerivedTestResult"), 1, TEXT("Derived class should access protected and public"));
	}

	// -------------------------------------------------------------------------
	// Abstract class: cannot be instantiated
	// -------------------------------------------------------------------------
	TEST_METHOD(AbstractClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_Abstract"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesAbstract.as"),
			ASTEST_AS(R"AS(
			// Abstract base class - cannot instantiate
			UCLASS(Abstract, Blueprintable)
			class AAbstractGameplayBase : AActor
			{
				UPROPERTY()
				int BaseHealth = 100;

				UPROPERTY()
				int BaseArmor = 50;

				// Virtual method to be overridden
				void ApplyDamage(int Damage)
				{
					BaseHealth -= Damage;
				}
			}

			// Concrete derived class - can instantiate
			UCLASS()
			class AConcreteGameplayActor : AAbstractGameplayBase
			{
				UPROPERTY()
				int Shield = 25;

				UPROPERTY()
				int DamageApplied = 0;

				void ApplyDamage(int Damage)
				{
					// Override parent method
					if (Shield > 0)
					{
						Shield -= Damage;
					}
					else
					{
						Super::ApplyDamage(Damage);
					}
					DamageApplied = Damage;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ApplyDamage(10);
				}
			}
			)AS"),
			TEXT("AAbstractGameplayBase"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Abstract base class should compile")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Class should be marked as abstract")));

		// Concrete class should be instantiable
		UClass* ConcreteClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Abstract.AConcreteGameplayActor")));
		ASSERT_THAT(IsNotNull(ConcreteClass, TEXT("Concrete class should compile")));
		ASSERT_THAT(IsFalse(ConcreteClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Concrete class should not be abstract")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ConcreteActor = SpawnScriptActor(*TestRunner, Spawner, ConcreteClass);
		ASSERT_THAT(IsNotNull(ConcreteActor, TEXT("Concrete actor should spawn")));

		BeginPlayActor(Engine, *ConcreteActor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, ConcreteActor, TEXT("Shield"), 15, TEXT("Shield should be reduced by damage"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, ConcreteActor, TEXT("DamageApplied"), 10, TEXT("Damage should be applied"));
	}

	// -------------------------------------------------------------------------
	// Interface implementation: single and multiple interfaces
	// -------------------------------------------------------------------------
	TEST_METHOD(InterfaceImplementation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_Interface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesInterface.as"),
			ASTEST_AS(R"AS(
			// Define interfaces
			interface IInteractable
			{
				void Interact();
			}

			interface IDamageable
			{
				void TakeDamage(int Amount);
			}

			// Single interface implementation
			UCLASS()
			class ASingleInterfaceActor : AActor, IInteractable
			{
				UPROPERTY()
				int InteractCount = 0;

				void Interact()
				{
					InteractCount++;
				}
			}

			// Multiple interface implementation
			UCLASS()
			class AMultiInterfaceActor : AActor, IInteractable, IDamageable
			{
				UPROPERTY()
				int InteractCount = 0;

				UPROPERTY()
				int Health = 100;

				void Interact()
				{
					InteractCount++;
				}

				void TakeDamage(int Amount)
				{
					Health -= Amount;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Interact();
					TakeDamage(25);
				}
			}
			)AS"),
			TEXT("ASingleInterfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Single interface class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SingleActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(SingleActor, TEXT("Single interface actor should spawn")));

		// Test multiple interface implementation
		UClass* MultiClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Interface.AMultiInterfaceActor")));
		ASSERT_THAT(IsNotNull(MultiClass, TEXT("Multi-interface class should compile")));

		AActor* MultiActor = SpawnScriptActor(*TestRunner, Spawner, MultiClass);
		ASSERT_THAT(IsNotNull(MultiActor, TEXT("Multi-interface actor should spawn")));

		BeginPlayActor(Engine, *MultiActor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, MultiActor, TEXT("InteractCount"), 1, TEXT("Interact should be called"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, MultiActor, TEXT("Health"), 75, TEXT("TakeDamage should be called"));
	}

	// -------------------------------------------------------------------------
	// Component declaration: DefaultComponent, RootComponent, Attach
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_Components"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesComponents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AComponentActor : AActor
			{
				// Root component
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				// Attached to root
				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent ChildComponent;

				// Static mesh component
				UPROPERTY(DefaultComponent, Attach=Root)
				UStaticMeshComponent MeshComponent;

				UPROPERTY()
				int ComponentCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Count initialized components
					if (Root != nullptr)
						ComponentCount++;
					if (ChildComponent != nullptr)
						ComponentCount++;
					if (MeshComponent != nullptr)
						ComponentCount++;

					// Verify attachment
					if (ChildComponent != nullptr && ChildComponent.GetAttachParent() == Root)
						ComponentCount += 10;
					if (MeshComponent != nullptr && MeshComponent.GetAttachParent() == Root)
						ComponentCount += 10;
				}
			}
			)AS"),
			TEXT("AComponentActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component declaration class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component actor should spawn")));

		BeginPlayActor(Engine, *Actor);
		// Should be 3 components + 20 for attachments = 23
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentCount"), 23, TEXT("All components should be created and attached"));
	}

	// -------------------------------------------------------------------------
	// Component types: various component types
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_ComponentTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesComponentTypes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AComponentTypesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent SceneRoot;

				UPROPERTY(DefaultComponent, Attach=SceneRoot)
				UStaticMeshComponent StaticMesh;

				UPROPERTY(DefaultComponent, Attach=SceneRoot)
				USceneComponent ChildScene;

				UPROPERTY(DefaultComponent)
				UActorComponent ActorComp;

				UPROPERTY()
				int ComponentTypeCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (SceneRoot != nullptr)
						ComponentTypeCount++;
					if (StaticMesh != nullptr)
						ComponentTypeCount++;
					if (ChildScene != nullptr)
						ComponentTypeCount++;
					if (ActorComp != nullptr)
						ComponentTypeCount++;
				}
			}
			)AS"),
			TEXT("AComponentTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component types class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component types actor should spawn")));

		BeginPlayActor(Engine, *Actor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentTypeCount"), 4, TEXT("All component types should be created"));
	}

	// -------------------------------------------------------------------------
	// Inheritance chain: multi-level inheritance with super calls
	// -------------------------------------------------------------------------
	TEST_METHOD(InheritanceChain)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_Inheritance"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesInheritance.as"),
			ASTEST_AS(R"AS(
			// Level 1: Base
			UCLASS()
			class AInheritanceBase : AActor
			{
				UPROPERTY()
				int BaseValue = 1;

				UPROPERTY()
				int CallChain = 0;

				void BaseMethod()
				{
					CallChain = CallChain * 10 + 1;
				}
			}

			// Level 2: Mid
			UCLASS()
			class AInheritanceMid : AInheritanceBase
			{
				UPROPERTY()
				int MidValue = 2;

				void MidMethod()
				{
					Super::BaseMethod();
					CallChain = CallChain * 10 + 2;
				}
			}

			// Level 3: Derived
			UCLASS()
			class AInheritanceDerived : AInheritanceMid
			{
				UPROPERTY()
				int DerivedValue = 3;

				void DerivedMethod()
				{
					Super::MidMethod();
					CallChain = CallChain * 10 + 3;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DerivedMethod();
				}
			}

			// Level 4: Deep
			UCLASS()
			class AInheritanceDeep : AInheritanceDerived
			{
				UPROPERTY()
				int DeepValue = 4;

				void DeepMethod()
				{
					Super::DerivedMethod();
					CallChain = CallChain * 10 + 4;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DeepMethod();
				}
			}
			)AS"),
			TEXT("AInheritanceDeep"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Deep inheritance class should compile")));

		// Verify inheritance hierarchy
		UClass* BaseClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Inheritance.AInheritanceBase")));
		UClass* MidClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Inheritance.AInheritanceMid")));
		UClass* DerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Inheritance.AInheritanceDerived")));

		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Base class should compile")));
		ASSERT_THAT(IsNotNull(MidClass, TEXT("Mid class should compile")));
		ASSERT_THAT(IsNotNull(DerivedClass, TEXT("Derived class should compile")));

		ASSERT_THAT(IsTrue(MidClass->IsChildOf(BaseClass), TEXT("Mid should inherit from Base")));
		ASSERT_THAT(IsTrue(DerivedClass->IsChildOf(MidClass), TEXT("Derived should inherit from Mid")));
		ASSERT_THAT(IsTrue(ScriptClass->IsChildOf(DerivedClass), TEXT("Deep should inherit from Derived")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Deep inheritance actor should spawn")));

		BeginPlayActor(Engine, *Actor);
		// Call chain should be: 1 -> 12 -> 123 -> 1234
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallChain"), 1234, TEXT("Super calls should follow inheritance chain"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BaseValue"), 1, TEXT("Base value should be inherited"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MidValue"), 2, TEXT("Mid value should be inherited"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DerivedValue"), 3, TEXT("Derived value should be inherited"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DeepValue"), 4, TEXT("Deep value should be present"));
	}

	// -------------------------------------------------------------------------
	// Class casting: Cast upward and downward in hierarchy
	// -------------------------------------------------------------------------
	TEST_METHOD(ClassCasting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_Casting"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesCasting.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACastBase : AActor
			{
				UPROPERTY()
				int BaseValue = 100;
			}

			UCLASS()
			class ACastDerived : ACastBase
			{
				UPROPERTY()
				int DerivedValue = 200;
			}

			UCLASS()
			class ACastTester : AActor
			{
				UPROPERTY()
				int UpcastSuccess = 0;

				UPROPERTY()
				int DowncastSuccess = 0;

				UPROPERTY()
				int InvalidCastFailed = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create derived instance
					ACastDerived Derived = Cast<ACastDerived>(SpawnActor(ACastDerived::StaticClass()));
					if (Derived != nullptr)
					{
						Derived.DerivedValue = 300;

						// Upcast (implicit)
						ACastBase Base = Derived;
						if (Base != nullptr && Base.BaseValue == 100)
						{
							UpcastSuccess = 1;
						}

						// Downcast (explicit)
						ACastDerived DownCasted = Cast<ACastDerived>(Base);
						if (DownCasted != nullptr && DownCasted.DerivedValue == 300)
						{
							DowncastSuccess = 1;
						}

						// Invalid cast should fail
						ACastTester InvalidCast = Cast<ACastTester>(Base);
						if (InvalidCast == nullptr)
						{
							InvalidCastFailed = 1;
						}
					}
				}
			}
			)AS"),
			TEXT("ACastTester"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Casting test class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Cast tester actor should spawn")));

		BeginPlayActor(Engine, *Actor);
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("UpcastSuccess"), 1, TEXT("Upcast should succeed"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DowncastSuccess"), 1, TEXT("Downcast should succeed"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InvalidCastFailed"), 1, TEXT("Invalid cast should return nullptr"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
