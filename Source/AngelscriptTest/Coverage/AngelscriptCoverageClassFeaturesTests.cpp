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
#include "UObject/Object.h"
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
private:
	static bool ExpectBoolByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, bool Expected, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(VerifyByPath<FBoolProperty, bool>(Test, Object, Path, Expected, Message), Message);
	}

	static bool ExpectIntByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32 Expected, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(VerifyByPath<FIntProperty, int32>(Test, Object, Path, Expected, Message), Message);
	}

	static bool ExpectObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject*& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetObjectByPath(Test, Object, Path, OutValue), Message);
	}

	static UClass* FindGeneratedClassChecked(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ClassName)
	{
		UClass* GeneratedClass = FindGeneratedClass(&Engine, ClassName);
		FNoDiscardAsserter LocalAssert(Test);
		const bool bFoundGeneratedClass = LocalAssert.IsNotNull(
			GeneratedClass,
			*FString::Printf(TEXT("Generated class '%s' should exist"), *ClassName.ToString()));
		if (!bFoundGeneratedClass)
		{
			return nullptr;
		}
		return GeneratedClass;
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Derived actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		// Verify overridden defaults
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Health"), 200, TEXT("Health should use derived default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("Speed"), 600.0f, TEXT("Speed should use derived default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Name"), FString(TEXT("Derived")), TEXT("Name should use derived default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Armor"), 50, TEXT("Armor should use declared default"))));

		// Test deep derived class
		UClass* DeepClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_DefaultOverride.ADeepDefaultActor")));
		ASSERT_THAT(IsNotNull(DeepClass, TEXT("Deep derived class should compile")));
		if (DeepClass == nullptr)
		{
			return;
		}

		AActor* DeepActor = SpawnScriptActor(*TestRunner, Spawner, DeepClass);
		ASSERT_THAT(IsNotNull(DeepActor, TEXT("Deep derived actor should spawn")));
		if (DeepActor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, DeepActor, TEXT("Health"), 300, TEXT("Health should use deep default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, DeepActor, TEXT("Armor"), 100, TEXT("Armor should use deep default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, DeepActor, TEXT("Speed"), 600.0f, TEXT("Speed should inherit from mid-level default"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default method actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DefaultMethodsCalled"), 2, TEXT("Default method calls should take effect"))));
	}

	// -------------------------------------------------------------------------
	// default keyword: containers and default component properties
	// -------------------------------------------------------------------------
	TEST_METHOD(DefaultKeywordContainersAndComponents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_DefaultContainersComponents"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesDefaultContainersComponents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ADefaultContainerComponentBaseActor : AActor
			{
				UPROPERTY()
				TArray<FName> Names;

				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UStaticMeshComponent Mesh;

				default Names.Add(n"Base");
				default Names.Add(n"Shared");
				default Mesh.SetCastShadow(false);
				default Mesh.SetRelativeLocation(FVector(12.0f, 34.0f, 56.0f));
			}

			UCLASS()
			class ADefaultContainerComponentDerivedActor : ADefaultContainerComponentBaseActor
			{
				default Names.Add(n"Derived");
				default Mesh.SetVisibility(false);

				UPROPERTY()
				int NameCount = 0;

				UPROPERTY()
				bool HasBaseName = false;

				UPROPERTY()
				bool HasDerivedName = false;

				UPROPERTY()
				bool MeshCastShadow = true;

				UPROPERTY()
				bool MeshVisible = true;

				UPROPERTY()
				FVector MeshRelativeLocation;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					NameCount = Names.Num();
					HasBaseName = Names.Contains(n"Base");
					HasDerivedName = Names.Contains(n"Derived");
					MeshCastShadow = Mesh.CastShadow;
					MeshVisible = Mesh.IsVisible();
					MeshRelativeLocation = Mesh.RelativeLocation;
				}
			}
			)AS"),
			TEXT("ADefaultContainerComponentDerivedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Default container/component class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default container/component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("NameCount"), 3, TEXT("default container additions should accumulate through inheritance"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("HasBaseName"), true, TEXT("base default container entry should be present"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("HasDerivedName"), true, TEXT("derived default container entry should be present"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("MeshCastShadow"), false, TEXT("default component method should configure CastShadow"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("MeshVisible"), false, TEXT("derived default component method should configure visibility"))));

		FVector MeshRelativeLocation;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("MeshRelativeLocation"), MeshRelativeLocation), TEXT("Mesh relative location should be readable")));
		ASSERT_THAT(IsTrue(MeshRelativeLocation.Equals(FVector(12.0f, 34.0f, 56.0f), 0.01f), TEXT("default component relative location should persist")));
	}

	// -------------------------------------------------------------------------
	// UCLASS specifiers and metadata: flags, config, class groups, tooltips
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassSpecifiersAndMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_UClassSpecifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesUClassSpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS(DefaultToInstanced, EditInlineNew, HideDropdown, Config=Game, DefaultConfig, ClassGroup="Coverage", HideCategories="Rendering", ComponentWrapperClass, meta=(DisplayName="Coverage Specifier Object", ShortTooltip="Short coverage tooltip", ToolTip="Full coverage tooltip", IsBlueprintBase="true", ChildCanTick, IgnoreCategoryKeywordsInSubclasses))
			class UCoverageSpecifierObject : UObject
			{
				UPROPERTY(Config)
				int ConfigValue = 7;
			}
			)AS"),
			TEXT("UCoverageSpecifierObject"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UCLASS specifier object should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("DefaultToInstanced should set CLASS_DefaultToInstanced")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_EditInlineNew), TEXT("EditInlineNew should set CLASS_EditInlineNew")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_HideDropDown), TEXT("HideDropdown should set CLASS_HideDropDown")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Config), TEXT("Config=Game should set CLASS_Config")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig should set CLASS_DefaultConfig for config classes")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), ScriptClass->ClassConfigName, TEXT("Config=Game should set ClassConfigName")));

		FProperty* ConfigProperty = ScriptClass->FindPropertyByName(TEXT("ConfigValue"));
		ASSERT_THAT(IsNotNull(ConfigProperty, TEXT("Config UPROPERTY should be generated")));
		if (ConfigProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConfigProperty->HasAnyPropertyFlags(CPF_Config), TEXT("UPROPERTY(Config) should set CPF_Config")));

		ASSERT_THAT(AreEqual(FString(TEXT("Coverage")), ScriptClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("ClassGroup should emit ClassGroupNames metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), ScriptClass->GetMetaData(TEXT("HideCategories")), TEXT("HideCategories should emit metadata")));
		ASSERT_THAT(IsTrue(ScriptClass->HasMetaData(TEXT("ComponentWrapperClass")), TEXT("ComponentWrapperClass should emit metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Specifier Object")), ScriptClass->GetMetaData(TEXT("DisplayName")), TEXT("DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Short coverage tooltip")), ScriptClass->GetMetaData(TEXT("ShortTooltip")), TEXT("ShortTooltip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Full coverage tooltip")), ScriptClass->GetMetaData(TEXT("ToolTip")), TEXT("ToolTip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), ScriptClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("IsBlueprintBase metadata should round-trip")));
		ASSERT_THAT(IsTrue(ScriptClass->HasMetaData(TEXT("ChildCanTick")), TEXT("ChildCanTick metadata should exist")));
		ASSERT_THAT(IsTrue(ScriptClass->HasMetaData(TEXT("IgnoreCategoryKeywordsInSubclasses")), TEXT("IgnoreCategoryKeywordsInSubclasses metadata should exist")));
	}

	// -------------------------------------------------------------------------
	// UCLASS specifier combinations: Abstract/Blueprintable, NotBlueprintable/BlueprintType, category override
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassSpecifierCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_UClassSpecifierCombinations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesUClassSpecifierCombinations.as"),
			ASTEST_AS(R"AS(
			UCLASS(Blueprintable, Abstract)
			class ACoverageAbstractBlueprintableActor : AActor
			{
			}

			UCLASS(NotBlueprintable, BlueprintType)
			class UCoverageVariableOnlyObject : UObject
			{
			}

			UCLASS(HideCategories="Rendering")
			class UCoverageHiddenCategoryBaseObject : UObject
			{
			}

			UCLASS(HideCategories="Rendering", meta=(ShowCategories="Rendering"))
			class UCoverageShownCategoryDerivedObject : UCoverageHiddenCategoryBaseObject
			{
			}

			UCLASS()
			class ACoverageSpecifierCombinationHarnessActor : AActor
			{
			}
			)AS"),
			TEXT("ACoverageSpecifierCombinationHarnessActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UCLASS specifier combination harness should compile")));

		UClass* AbstractClass = FindGeneratedClassChecked(*TestRunner, Engine, TEXT("ACoverageAbstractBlueprintableActor"));
		UClass* VariableOnlyClass = FindGeneratedClassChecked(*TestRunner, Engine, TEXT("UCoverageVariableOnlyObject"));
		UClass* HiddenBaseClass = FindGeneratedClassChecked(*TestRunner, Engine, TEXT("UCoverageHiddenCategoryBaseObject"));
		UClass* ShownDerivedClass = FindGeneratedClassChecked(*TestRunner, Engine, TEXT("UCoverageShownCategoryDerivedObject"));
		ASSERT_THAT(IsNotNull(AbstractClass, TEXT("Abstract Blueprintable class should be generated")));
		ASSERT_THAT(IsNotNull(VariableOnlyClass, TEXT("NotBlueprintable BlueprintType class should be generated")));
		ASSERT_THAT(IsNotNull(HiddenBaseClass, TEXT("HideCategories base class should be generated")));
		ASSERT_THAT(IsNotNull(ShownDerivedClass, TEXT("ShowCategories derived class should be generated")));
		if (AbstractClass == nullptr || VariableOnlyClass == nullptr || HiddenBaseClass == nullptr || ShownDerivedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AbstractClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Blueprintable + Abstract should set CLASS_Abstract")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AbstractClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("Blueprintable + Abstract should remain a Blueprint base")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), VariableOnlyClass->GetMetaData(TEXT("BlueprintType")), TEXT("NotBlueprintable + BlueprintType should keep BlueprintType metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), VariableOnlyClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("NotBlueprintable + BlueprintType should not be a Blueprint base")));
		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), HiddenBaseClass->GetMetaData(TEXT("HideCategories")), TEXT("HideCategories base metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), ShownDerivedClass->GetMetaData(TEXT("ShowCategories")), TEXT("ShowCategories override metadata should round-trip")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Access modifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TestResult"), 1, TEXT("Base class should access all modifiers"))));

		// Test derived class access
		UClass* DerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_AccessModifiers.AAccessModifierDerived")));
		ASSERT_THAT(IsNotNull(DerivedClass, TEXT("Derived class should compile")));
		if (DerivedClass == nullptr)
		{
			return;
		}

		AActor* DerivedActor = SpawnScriptActor(*TestRunner, Spawner, DerivedClass);
		ASSERT_THAT(IsNotNull(DerivedActor, TEXT("Derived actor should spawn")));
		if (DerivedActor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *DerivedActor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, DerivedActor, TEXT("DerivedTestResult"), 1, TEXT("Derived class should access protected and public"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Class should be marked as abstract")));

		// Concrete class should be instantiable
		UClass* ConcreteClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Abstract.AConcreteGameplayActor")));
		ASSERT_THAT(IsNotNull(ConcreteClass, TEXT("Concrete class should compile")));
		if (ConcreteClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ConcreteClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Concrete class should not be abstract")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ConcreteActor = SpawnScriptActor(*TestRunner, Spawner, ConcreteClass);
		ASSERT_THAT(IsNotNull(ConcreteActor, TEXT("Concrete actor should spawn")));
		if (ConcreteActor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *ConcreteActor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ConcreteActor, TEXT("Shield"), 15, TEXT("Shield should be reduced by damage"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ConcreteActor, TEXT("DamageApplied"), 10, TEXT("Damage should be applied"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SingleActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(SingleActor, TEXT("Single interface actor should spawn")));
		if (SingleActor == nullptr)
		{
			return;
		}

		// Test multiple interface implementation
		UClass* MultiClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Interface.AMultiInterfaceActor")));
		ASSERT_THAT(IsNotNull(MultiClass, TEXT("Multi-interface class should compile")));
		if (MultiClass == nullptr)
		{
			return;
		}

		AActor* MultiActor = SpawnScriptActor(*TestRunner, Spawner, MultiClass);
		ASSERT_THAT(IsNotNull(MultiActor, TEXT("Multi-interface actor should spawn")));
		if (MultiActor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *MultiActor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, MultiActor, TEXT("InteractCount"), 1, TEXT("Interact should be called"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, MultiActor, TEXT("Health"), 75, TEXT("TakeDamage should be called"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		// Should be 3 components + 20 for attachments = 23
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentCount"), 23, TEXT("All components should be created and attached"))));
	}

	// -------------------------------------------------------------------------
	// Component declaration specifiers: AttachSocket and ShowOnActor
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentSpecifierMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_ComponentSpecifierMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesComponentSpecifierMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AComponentSpecifierMetadataActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="CoverageSocket", ShowOnActor, EditAnywhere, BlueprintReadOnly)
				USceneComponent Child;

				UPROPERTY()
				bool ChildAttachedToRoot = false;

				UPROPERTY()
				FName ChildAttachSocket;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ChildAttachedToRoot = Child.GetAttachParent() == Root;
					ChildAttachSocket = Child.GetAttachSocketName();
				}
			}
			)AS"),
			TEXT("AComponentSpecifierMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component specifier metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* ChildProperty = ScriptClass->FindPropertyByName(TEXT("Child"));
		ASSERT_THAT(IsNotNull(ChildProperty, TEXT("Default component property should be generated")));
		if (ChildProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("DefaultComponent + ShowOnActor/EditAnywhere should be editable")));
		ASSERT_THAT(IsTrue(ChildProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly default component should be Blueprint visible")));
		ASSERT_THAT(IsTrue(ChildProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly default component should set CPF_BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(ChildProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("DefaultComponent should be instanced/exported")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageSocket")), ChildProperty->GetMetaData(TEXT("AttachSocket")), TEXT("AttachSocket metadata should round-trip")));
		ASSERT_THAT(IsTrue(ChildProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should add EditInline metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component specifier metadata actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ChildAttachedToRoot"), true, TEXT("Attach specifier should attach child to root"))));

		FName ChildAttachSocket = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("ChildAttachSocket"), ChildAttachSocket), TEXT("ChildAttachSocket should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("CoverageSocket")), ChildAttachSocket, TEXT("AttachSocket should persist to runtime attachment")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component types actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComponentTypeCount"), 4, TEXT("All component types should be created"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		// Verify inheritance hierarchy
		UClass* BaseClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Inheritance.AInheritanceBase")));
		UClass* MidClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Inheritance.AInheritanceMid")));
		UClass* DerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageFeatures_Inheritance.AInheritanceDerived")));

		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Base class should compile")));
		ASSERT_THAT(IsNotNull(MidClass, TEXT("Mid class should compile")));
		ASSERT_THAT(IsNotNull(DerivedClass, TEXT("Derived class should compile")));
		if (BaseClass == nullptr || MidClass == nullptr || DerivedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(MidClass->IsChildOf(BaseClass), TEXT("Mid should inherit from Base")));
		ASSERT_THAT(IsTrue(DerivedClass->IsChildOf(MidClass), TEXT("Derived should inherit from Mid")));
		ASSERT_THAT(IsTrue(ScriptClass->IsChildOf(DerivedClass), TEXT("Deep should inherit from Derived")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Deep inheritance actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		// Call chain should be: 1 -> 12 -> 123 -> 1234
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallChain"), 1234, TEXT("Super calls should follow inheritance chain"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BaseValue"), 1, TEXT("Base value should be inherited"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MidValue"), 2, TEXT("Mid value should be inherited"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DerivedValue"), 3, TEXT("Derived value should be inherited"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DeepValue"), 4, TEXT("Deep value should be present"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Cast tester actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("UpcastSuccess"), 1, TEXT("Upcast should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DowncastSuccess"), 1, TEXT("Downcast should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InvalidCastFailed"), 1, TEXT("Invalid cast should return nullptr"))));
	}

	// -------------------------------------------------------------------------
	// Composition: object, actor, component and TSubclassOf member references
	// -------------------------------------------------------------------------
	TEST_METHOD(CompositionReferences)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFeatures_CompositionReferences"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFeaturesCompositionReferences.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageMemberObject : UObject
			{
				UPROPERTY()
				int Value = 31;
			}

			UCLASS()
			class UCoverageMemberComponent : UActorComponent
			{
				UPROPERTY()
				int ComponentValue = 41;
			}

			UCLASS()
			class ACoverageCompositionReferenceActor : AActor
			{
				UPROPERTY()
				UCoverageMemberObject MemberObject;

				UPROPERTY()
				AActor OtherActor;

				UPROPERTY(DefaultComponent)
				UCoverageMemberComponent MemberComponent;

				UPROPERTY()
				TSubclassOf<AActor> ActorClass;

				UPROPERTY()
				bool MemberObjectAssigned = false;

				UPROPERTY()
				bool ActorReferenceAssigned = false;

				UPROPERTY()
				bool ComponentReferenceAssigned = false;

				UPROPERTY()
				bool SubclassAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MemberObject = NewObject(this, UCoverageMemberObject::StaticClass());
					OtherActor = this;
					ActorClass = ACoverageCompositionReferenceActor::StaticClass();

					MemberObjectAssigned = MemberObject != nullptr && MemberObject.Value == 31;
					ActorReferenceAssigned = OtherActor == this;
					ComponentReferenceAssigned = MemberComponent != nullptr && MemberComponent.ComponentValue == 41;
					SubclassAssigned = ActorClass.Get() == ACoverageCompositionReferenceActor::StaticClass();
				}
			}
			)AS"),
			TEXT("ACoverageCompositionReferenceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Composition reference actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Composition reference actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UObject* MemberObject = nullptr;
		UObject* MemberComponent = nullptr;
		ASSERT_THAT(IsTrue(ExpectObjectByPath(*TestRunner, Actor, TEXT("MemberObject"), MemberObject, TEXT("MemberObject reference should be readable"))));
		ASSERT_THAT(IsTrue(ExpectObjectByPath(*TestRunner, Actor, TEXT("MemberComponent"), MemberComponent, TEXT("MemberComponent reference should be readable"))));
		ASSERT_THAT(IsNotNull(MemberObject, TEXT("Member object should be assigned at runtime")));
		ASSERT_THAT(IsNotNull(MemberComponent, TEXT("Member component should be assigned at construction")));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("MemberObjectAssigned"), true, TEXT("UObject member reference should be assignable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ActorReferenceAssigned"), true, TEXT("Actor member reference should be assignable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ComponentReferenceAssigned"), true, TEXT("Component member reference should be assignable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("SubclassAssigned"), true, TEXT("TSubclassOf member reference should be assignable"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
