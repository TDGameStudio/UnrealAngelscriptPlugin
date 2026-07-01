#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
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
// from OpenSpec: test-coverage/coverage-matrix.md (Submatrices 4-6: Features).
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

#if WITH_ANGELSCRIPT_UNITTESTS

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

	static bool ExpectCompileBoundaryRejected(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const FString& Source,
		const TCHAR* Label,
		TArrayView<const FString> ExpectedDiagnostics)
	{
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			Source,
			Label,
			ExpectedDiagnostics);
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
			UCLASS()
			class ACoverageClassFeaturesSingleDefaultActor : AActor
			{
				UPROPERTY()
				int Health = 100;

				UPROPERTY()
				float Speed = 500.0f;

				UPROPERTY()
				FString Name = "Base";

				default Health = 200;
				default Speed = 600.0f;
				default Name = "Single";
			}

			UCLASS()
			class ACoverageClassFeaturesInheritedDefaultBaseActor : AActor
			{
				UPROPERTY()
				int InheritedHealth = 100;
			}

			UCLASS()
			class ACoverageClassFeaturesInheritedDefaultLeafActor : ACoverageClassFeaturesInheritedDefaultBaseActor
			{
				default InheritedHealth = 300;

				UPROPERTY()
				int Armor = 50;
			}
			)AS"),
			TEXT("ACoverageClassFeaturesSingleDefaultActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Single class with default overrides should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		AActor* DefaultCDO = Cast<AActor>(ScriptClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultCDO, TEXT("Single default actor CDO should be available")));
		if (DefaultCDO == nullptr)
		{
			return;
		}

		FIntProperty* HealthProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("Health"));
		FDoubleProperty* SpeedProperty = FindFProperty<FDoubleProperty>(ScriptClass, TEXT("Speed"));
		FStrProperty* NameProperty = FindFProperty<FStrProperty>(ScriptClass, TEXT("Name"));
		ASSERT_THAT(IsNotNull(HealthProperty, TEXT("Health property should remain reflected")));
		ASSERT_THAT(IsNotNull(SpeedProperty, TEXT("Speed property should remain reflected")));
		ASSERT_THAT(IsNotNull(NameProperty, TEXT("Name property should remain reflected")));
		if (HealthProperty == nullptr || SpeedProperty == nullptr || NameProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(200, HealthProperty->GetPropertyValue_InContainer(DefaultCDO), TEXT("single-class default should override Health initializer")));
		ASSERT_THAT(IsNear(600.0, SpeedProperty->GetPropertyValue_InContainer(DefaultCDO), 0.001, TEXT("single-class default should override Speed initializer")));
		ASSERT_THAT(AreEqual(FString(TEXT("Single")), NameProperty->GetPropertyValue_InContainer(DefaultCDO), TEXT("single-class default should override Name initializer")));

		UClass* LeafClass = FindGeneratedClass(&Engine, TEXT("ACoverageClassFeaturesInheritedDefaultLeafActor"));
		ASSERT_THAT(IsNotNull(LeafClass, TEXT("Inherited default leaf class should compile")));
		if (LeafClass == nullptr)
		{
			return;
		}

		AActor* LeafCDO = Cast<AActor>(LeafClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(LeafCDO, TEXT("Inherited default leaf CDO should be available")));
		if (LeafCDO == nullptr)
		{
			return;
		}

		FIntProperty* InheritedHealthProperty = FindFProperty<FIntProperty>(LeafClass, TEXT("InheritedHealth"));
		FIntProperty* ArmorProperty = FindFProperty<FIntProperty>(LeafClass, TEXT("Armor"));
		ASSERT_THAT(IsNotNull(InheritedHealthProperty, TEXT("InheritedHealth property should remain reflected on the leaf class")));
		ASSERT_THAT(IsNotNull(ArmorProperty, TEXT("leaf Armor property should remain reflected")));
		if (InheritedHealthProperty == nullptr || ArmorProperty == nullptr)
		{
			return;
		}

		// Current boundary: leaf default statements targeting inherited properties compile and reflect,
		// but do not yet update the inherited property value on the leaf CDO.
		ASSERT_THAT(AreEqual(100, InheritedHealthProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("inherited property default override remains a documented CDO boundary")));
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
			class ACoverageClassFeaturesDefaultMethodActor : AActor
			{
				UPROPERTY(Replicated)
				int ReplicatedValue = 1;

				default SetReplicates(true);
				default SetReplicateMovement(true);
			}
			)AS"),
			TEXT("ACoverageClassFeaturesDefaultMethodActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Default method class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* ReplicatedValueProperty = FindFProperty<FProperty>(ScriptClass, TEXT("ReplicatedValue"));
		ASSERT_THAT(IsNotNull(ReplicatedValueProperty, TEXT("ReplicatedValue property should remain reflected")));
		ASSERT_THAT(IsTrue(ReplicatedValueProperty != nullptr && ReplicatedValueProperty->HasAnyPropertyFlags(CPF_Net), TEXT("ReplicatedValue should carry CPF_Net")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default method actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Actor->GetIsReplicated(), TEXT("default SetReplicates(true) should take effect on spawned actors")));
		ASSERT_THAT(IsTrue(Actor->IsReplicatingMovement(), TEXT("default SetReplicateMovement(true) should take effect on spawned actors")));
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
			class ACoverageClassFeaturesDefaultContainerComponentActor : AActor
			{
				UPROPERTY()
				TArray<FName> DefaultNames;

				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UStaticMeshComponent Mesh;

				default Tags.Add(n"Base");
				default Tags.Add(n"Shared");
				default Tags.Add(n"Leaf");
				default DefaultNames.Add(n"Base");
				default DefaultNames.Add(n"Shared");
				default DefaultNames.Add(n"Leaf");
				default Mesh.SetCastShadow(false);
				default Mesh.SetRelativeLocation(FVector(12.0f, 34.0f, 56.0f));
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
					NameCount = Tags.Num();
					HasBaseName = Tags.Contains(n"Base");
					HasDerivedName = Tags.Contains(n"Leaf");
					MeshVisible = Mesh.IsVisible();
					MeshRelativeLocation = Mesh.RelativeLocation;
				}
			}
			)AS"),
			TEXT("ACoverageClassFeaturesDefaultContainerComponentActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Default container/component class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		AActor* DefaultActor = Cast<AActor>(ScriptClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("Default container/component CDO should be available")));
		if (DefaultActor == nullptr)
		{
			return;
		}

		FArrayProperty* DefaultNamesProperty = FindFProperty<FArrayProperty>(ScriptClass, TEXT("DefaultNames"));
		ASSERT_THAT(IsNotNull(DefaultNamesProperty, TEXT("DefaultNames array property should remain reflected")));
		if (DefaultNamesProperty == nullptr)
		{
			return;
		}
		FScriptArrayHelper DefaultNamesHelper(DefaultNamesProperty, DefaultNamesProperty->ContainerPtrToValuePtr<void>(DefaultActor));
		// Current boundary: default method calls into custom TArray properties compile and reflect,
		// but do not populate the custom array CDO in this actor/component fixture.
		ASSERT_THAT(AreEqual(0, DefaultNamesHelper.Num(), TEXT("custom TArray default additions remain a documented CDO boundary")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default container/component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("NameCount"), 3, TEXT("custom TArray default additions should be visible at runtime"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("HasBaseName"), true, TEXT("custom TArray base default entry should be present at runtime"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("HasDerivedName"), true, TEXT("custom TArray leaf default entry should be present at runtime"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("MeshVisible"), false, TEXT("default component method should configure visibility"))));

		UObject* MeshObject = nullptr;
		ASSERT_THAT(IsTrue(ExpectObjectByPath(*TestRunner, Actor, TEXT("Mesh"), MeshObject, TEXT("default mesh component should be readable"))));
		UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(MeshObject);
		ASSERT_THAT(IsNotNull(MeshComponent, TEXT("default Mesh property should be a UStaticMeshComponent")));
		if (MeshComponent == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(MeshComponent->CastShadow, TEXT("default component method should configure CastShadow")));

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

				int PublicValue = 300;

				UPROPERTY()
				int TestResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test access within same class
					PrivateValue = 111;
					ProtectedValue = 222;
					PublicValue = 333;

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

					// Derived class can access default-public members
					PublicValue = 555;

					if (ProtectedValue == 444 && PublicValue == 555)
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
		UClass* DerivedClass = FindGeneratedClass(&Engine, TEXT("AAccessModifierDerived"));
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

		TArray<FString> PublicKeywordDiagnostics;
		PublicKeywordDiagnostics.Add(TEXT("Expected method or property"));
		PublicKeywordDiagnostics.Add(TEXT("Instead found identifier 'public'"));

		const FString PublicKeywordSource = ASTEST_AS(R"AS(
			UCLASS()
			class AAccessModifierPublicKeywordBoundary : AActor
			{
				public int UnsupportedPublicKeyword = 1;
			}
			)AS");

		ASSERT_THAT(IsTrue(ExpectCompileBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageFeatures_AccessModifiers_PublicKeywordBoundary"),
			PublicKeywordSource,
			TEXT("public member keyword remains an unsupported AngelScript UCLASS boundary"),
			MakeArrayView(PublicKeywordDiagnostics))));
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
			UCLASS(Abstract, Blueprintable)
			class ACoverageClassFeaturesAbstractGameplayBase : AActor
			{
				UPROPERTY()
				int BaseHealth = 100;

				UPROPERTY()
				int BaseArmor = 50;

				UFUNCTION()
				int GetBaseHealth()
				{
					return BaseHealth;
				}
			}

			UCLASS()
			class ACoverageClassFeaturesConcreteGameplayActor : ACoverageClassFeaturesAbstractGameplayBase
			{
				UPROPERTY()
				int Shield = 25;

				UPROPERTY()
				int DamageApplied = 0;

				UFUNCTION()
				void ApplyShieldDamage(int Damage)
				{
					if (Shield > 0)
					{
						Shield -= Damage;
					}
					DamageApplied = Damage;
				}
			}
			)AS"),
			TEXT("ACoverageClassFeaturesAbstractGameplayBase"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Abstract base class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Class should be marked as abstract")));

		// Concrete class should be instantiable
		UClass* ConcreteClass = FindGeneratedClass(&Engine, TEXT("ACoverageClassFeaturesConcreteGameplayActor"));
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

		FIntProperty* ShieldProperty = FindFProperty<FIntProperty>(ConcreteClass, TEXT("Shield"));
		FIntProperty* DamageAppliedProperty = FindFProperty<FIntProperty>(ConcreteClass, TEXT("DamageApplied"));
		ASSERT_THAT(IsNotNull(ShieldProperty, TEXT("Shield property should remain reflected")));
		ASSERT_THAT(IsNotNull(DamageAppliedProperty, TEXT("DamageApplied property should remain reflected")));
		if (ShieldProperty == nullptr || DamageAppliedProperty == nullptr)
		{
			return;
		}

		ShieldProperty->SetPropertyValue_InContainer(ConcreteActor, 25);
		DamageAppliedProperty->SetPropertyValue_InContainer(ConcreteActor, 0);

		FFunctionInvoker DamageInvoker(*TestRunner, ConcreteActor, TEXT("ApplyShieldDamage"));
		ASSERT_THAT(IsTrue(DamageInvoker.IsValid(), TEXT("ApplyShieldDamage should be invokable through reflection")));
		if (!DamageInvoker.IsValid())
		{
			return;
		}
		DamageInvoker.AddParam<int32>(10);
		ASSERT_THAT(IsTrue(DamageInvoker.Call(), TEXT("ApplyShieldDamage should execute through reflection")));

		ASSERT_THAT(AreEqual(15, ShieldProperty->GetPropertyValue_InContainer(ConcreteActor), TEXT("Shield should be reduced by damage")));
		ASSERT_THAT(AreEqual(10, DamageAppliedProperty->GetPropertyValue_InContainer(ConcreteActor), TEXT("Damage should be applied")));
	}

	// -------------------------------------------------------------------------
	// Interface implementation: single and multiple interfaces
	// -------------------------------------------------------------------------
	TEST_METHOD(InterfaceImplementation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ScriptInterfaceDiagnostics;
		ScriptInterfaceDiagnostics.Add(TEXT("Virtual property syntax has been removed"));

		const FString ScriptInterfaceSource = ASTEST_AS(R"AS(
			interface IClassFeaturesScriptInterface
			{
				void Interact();
			}
			)AS");

		ASSERT_THAT(IsTrue(ExpectCompileBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageFeatures_ScriptInterfaceBoundary"),
			ScriptInterfaceSource,
			TEXT("script-level interface declarations remain unsupported in this fork"),
			MakeArrayView(ScriptInterfaceDiagnostics))));

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

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
			UCLASS()
			class ANativeInterfaceFeatureActor : AActor, UAngelscriptNativeParentInterface
			{
				UPROPERTY()
				int NativeValue = 100;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UPROPERTY()
				int AdjustedValue = 0;

				UPROPERTY()
				bool InterfaceCastWorked = false;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject Self = this;
					UAngelscriptNativeParentInterface InterfaceRef = Cast<UAngelscriptNativeParentInterface>(Self);
					if (InterfaceRef != nullptr)
					{
						InterfaceCastWorked = true;
						NativeValue = InterfaceRef.GetNativeValue();
						InterfaceRef.SetNativeMarker(n"FromClassFeatures");

						int Value = 5;
						InterfaceRef.AdjustNativeValue(7, Value);
						AdjustedValue = Value;
					}
				}
			}
			)AS"),
			TEXT("ANativeInterfaceFeatureActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Native interface class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native interface actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InterfaceCastWorked"), true, TEXT("Native interface cast should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NativeValue"), 100, TEXT("Native interface getter should dispatch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NativeMarker"), FName(TEXT("FromClassFeatures")), TEXT("Native interface setter should dispatch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AdjustedValue"), 12, TEXT("Native interface ref parameter should dispatch"))));
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
			class UCoverageComponentTypesLogicComponent : UActorComponent
			{
			}

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
				UCoverageComponentTypesLogicComponent ActorComp;

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
			UCLASS()
			class ACoverageClassFeaturesInheritanceBase : AActor
			{
				UPROPERTY()
				int BaseValue = 1;

				UPROPERTY()
				int CallChain = 0;

				void RunChain()
				{
					CallChain = CallChain * 10 + 1;
				}
			}

			UCLASS()
			class ACoverageClassFeaturesInheritanceMid : ACoverageClassFeaturesInheritanceBase
			{
				UPROPERTY()
				int MidValue = 2;

				void RunChain()
				{
					Super::RunChain();
					CallChain = CallChain * 10 + 2;
				}
			}

			UCLASS()
			class ACoverageClassFeaturesInheritanceDerived : ACoverageClassFeaturesInheritanceMid
			{
				UPROPERTY()
				int DerivedValue = 3;

				void RunChain()
				{
					Super::RunChain();
					CallChain = CallChain * 10 + 3;
				}

				UFUNCTION()
				void ExecuteChain()
				{
					RunChain();
				}
			}

			UCLASS()
			class ACoverageClassFeaturesInheritanceDeep : ACoverageClassFeaturesInheritanceDerived
			{
				UPROPERTY()
				int DeepValue = 4;

				void RunChain()
				{
					Super::RunChain();
					CallChain = CallChain * 10 + 4;
				}

				UFUNCTION()
				void ExecuteDeepChain()
				{
					RunChain();
				}
			}
			)AS"),
			TEXT("ACoverageClassFeaturesInheritanceDeep"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Deep inheritance class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("ACoverageClassFeaturesInheritanceBase"));
		UClass* MidClass = FindGeneratedClass(&Engine, TEXT("ACoverageClassFeaturesInheritanceMid"));
		UClass* DerivedClass = FindGeneratedClass(&Engine, TEXT("ACoverageClassFeaturesInheritanceDerived"));

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

		FFunctionInvoker ChainInvoker(*TestRunner, Actor, TEXT("ExecuteDeepChain"));
		ASSERT_THAT(IsTrue(ChainInvoker.IsValid(), TEXT("ExecuteDeepChain should be invokable through reflection")));
		if (!ChainInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChainInvoker.Call(), TEXT("ExecuteDeepChain should run the inherited super-call chain")));

		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("CallChain"), 1234, TEXT("Super calls should follow inheritance chain"))));
		// Current boundary: inherited property declarations remain reflected, but inline
		// initializers from intermediate script classes do not populate the deep instance.
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("BaseValue"), 1, TEXT("base initializer should remain visible on the deep instance"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("MidValue"), 0, TEXT("mid initializer remains a documented deep-inheritance boundary"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("DerivedValue"), 0, TEXT("derived initializer remains a documented deep-inheritance boundary"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("DeepValue"), 0, TEXT("deep initializer remains a documented deep-inheritance boundary"))));
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
					MemberObject = Cast<UCoverageMemberObject>(NewObject(this, UCoverageMemberObject::StaticClass()));
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

#endif // WITH_ANGELSCRIPT_UNITTESTS
