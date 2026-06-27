#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUClassTests
// -----------------------------------------------------------------------------
// Comprehensive UCLASS specifier coverage for AngelScript, following the matrix
// from Documents/Coverage/Coverage_UClass.md (Submatrix 2: UCLASS Specifiers).
//
// Test axes covered:
//   * UClassBasicDeclaration       - Bare UCLASS, inheritance patterns
//   * UClassBlueprintSpecifiers    - Blueprintable, NotBlueprintable, BlueprintType
//   * UClassBehaviorSpecifiers     - Abstract, Transient, NotPlaceable, Deprecated
//   * UClassConfigSpecifiers       - Config=Game, DefaultConfig, GlobalUserConfig
//   * UClassDisplaySpecifiers      - ClassGroup, HideCategories, ShowCategories
//   * UClassMetaData               - DisplayName, ToolTip, ShortTooltip
//   * UClassSpecifierCombinations  - Common specifier combinations
//
// Pattern: Compile script modules with various UCLASS specifiers, validate
// through UClass reflection (ClassFlags, ClassConfigName, metadata).
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUClassTest,
	"Angelscript.TestModule.Coverage.UClass",
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
	// Basic UCLASS declarations: bare UCLASS, inheritance patterns
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_BasicDecl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassBasicDecl.as"),
			ASTEST_AS(R"AS(
			// Bare UCLASS - minimal declaration
			UCLASS()
			class ABasicActor : AActor
			{
				UPROPERTY()
				int Value = 10;
			}

			// Inheritance from script base class
			UCLASS()
			class ADerivedActor : ABasicActor
			{
				UPROPERTY()
				int DerivedValue = 20;
			}

			// Multi-level inheritance
			UCLASS()
			class ADeepDerivedActor : ADerivedActor
			{
				UPROPERTY()
				int DeepValue = 30;
			}
			)AS"),
			TEXT("ABasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Basic UCLASS should compile")));

		UClass* DerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BasicDecl.ADerivedActor")));
		ASSERT_THAT(IsNotNull(DerivedClass, TEXT("Derived class should compile")));
		ASSERT_THAT(IsTrue(DerivedClass->IsChildOf(ScriptClass), TEXT("ADerivedActor should inherit from ABasicActor")));

		UClass* DeepDerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BasicDecl.ADeepDerivedActor")));
		ASSERT_THAT(IsNotNull(DeepDerivedClass, TEXT("Multi-level derived class should compile")));
		ASSERT_THAT(IsTrue(DeepDerivedClass->IsChildOf(DerivedClass), TEXT("ADeepDerivedActor should inherit from ADerivedActor")));
	}

	// -------------------------------------------------------------------------
	// Blueprint specifiers: Blueprintable, NotBlueprintable, BlueprintType
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassBlueprintSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_BlueprintSpec"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassBlueprintSpec.as"),
			ASTEST_AS(R"AS(
			// Blueprintable - can be subclassed in Blueprint
			UCLASS(Blueprintable)
			class ABlueprintableActor : AActor
			{
				UPROPERTY()
				int Value = 1;
			}

			// NotBlueprintable - cannot be subclassed in Blueprint
			UCLASS(NotBlueprintable)
			class ANotBlueprintableActor : AActor
			{
				UPROPERTY()
				int Value = 2;
			}

			// BlueprintType - can be used as Blueprint variable type
			UCLASS(BlueprintType)
			class ABlueprintTypeActor : AActor
			{
				UPROPERTY()
				int Value = 3;
			}

			// Combination: Blueprintable + BlueprintType
			UCLASS(Blueprintable, BlueprintType)
			class AFullBlueprintActor : AActor
			{
				UPROPERTY()
				int Value = 4;
			}
			)AS"),
			TEXT("ABlueprintableActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Blueprintable class should compile")));

		UClass* NotBPClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BlueprintSpec.ANotBlueprintableActor")));
		ASSERT_THAT(IsNotNull(NotBPClass, TEXT("NotBlueprintable class should compile")));

		UClass* BPTypeClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BlueprintSpec.ABlueprintTypeActor")));
		ASSERT_THAT(IsNotNull(BPTypeClass, TEXT("BlueprintType class should compile")));

		UClass* FullBPClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BlueprintSpec.AFullBlueprintActor")));
		ASSERT_THAT(IsNotNull(FullBPClass, TEXT("Combined Blueprint class should compile")));
	}

	// -------------------------------------------------------------------------
	// Behavior specifiers: Abstract, Transient, NotPlaceable
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassBehaviorSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_BehaviorSpec"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassBehaviorSpec.as"),
			ASTEST_AS(R"AS(
			// Abstract - cannot be instantiated
			UCLASS(Abstract)
			class AAbstractActor : AActor
			{
				UPROPERTY()
				int Value = 1;
			}

			// Transient - not saved to disk
			UCLASS(Transient)
			class ATransientActor : AActor
			{
				UPROPERTY()
				int Value = 2;
			}

			// NotPlaceable - cannot be placed in level editor
			UCLASS(NotPlaceable)
			class ANotPlaceableActor : AActor
			{
				UPROPERTY()
				int Value = 3;
			}

			// Combination: Abstract + Blueprintable (common base class pattern)
			UCLASS(Abstract, Blueprintable)
			class AAbstractBaseActor : AActor
			{
				UPROPERTY()
				int BaseValue = 100;
			}

			// Concrete derived from abstract
			UCLASS()
			class AConcreteActor : AAbstractBaseActor
			{
				UPROPERTY()
				int ConcreteValue = 200;
			}
			)AS"),
			TEXT("AAbstractActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Abstract class should compile")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Should have CLASS_Abstract flag")));

		UClass* TransientClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BehaviorSpec.ATransientActor")));
		ASSERT_THAT(IsNotNull(TransientClass, TEXT("Transient class should compile")));

		UClass* NotPlaceableClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BehaviorSpec.ANotPlaceableActor")));
		ASSERT_THAT(IsNotNull(NotPlaceableClass, TEXT("NotPlaceable class should compile")));
		ASSERT_THAT(IsTrue(NotPlaceableClass->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("Should have CLASS_NotPlaceable flag")));

		UClass* AbstractBaseClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BehaviorSpec.AAbstractBaseActor")));
		ASSERT_THAT(IsNotNull(AbstractBaseClass, TEXT("Abstract + Blueprintable class should compile")));
		ASSERT_THAT(IsTrue(AbstractBaseClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Should be abstract")));

		UClass* ConcreteClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_BehaviorSpec.AConcreteActor")));
		ASSERT_THAT(IsNotNull(ConcreteClass, TEXT("Concrete derived class should compile")));
		ASSERT_THAT(IsFalse(ConcreteClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Concrete class should not be abstract")));
	}

	// -------------------------------------------------------------------------
	// Config specifiers: Config=Game, DefaultConfig
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassConfigSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_ConfigSpec"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassConfigSpec.as"),
			ASTEST_AS(R"AS(
			// Config=Game - properties can be saved to Game.ini
			UCLASS(Config=Game)
			class AConfigGameActor : AActor
			{
				UPROPERTY(Config)
				int ConfigValue = 100;

				UPROPERTY()
				int NonConfigValue = 200;
			}

			// DefaultConfig - uses default config file
			UCLASS(DefaultConfig)
			class ADefaultConfigActor : AActor
			{
				UPROPERTY(Config)
				int DefaultConfigValue = 300;
			}

			// Config=Editor - editor-specific config
			UCLASS(Config=Editor)
			class AConfigEditorActor : AActor
			{
				UPROPERTY(Config)
				int EditorConfigValue = 400;
			}
			)AS"),
			TEXT("AConfigGameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Config=Game class should compile")));
		ASSERT_THAT(IsTrue(ScriptClass->ClassConfigName == NAME_Game, TEXT("Should use Game config")));

		// Verify Config property has correct flags
		FProperty* ConfigProp = ScriptClass->FindPropertyByName(TEXT("ConfigValue"));
		ASSERT_THAT(IsNotNull(ConfigProp, TEXT("Config property should exist")));
		ASSERT_THAT(IsTrue(ConfigProp->HasAnyPropertyFlags(CPF_Config), TEXT("Property should have CPF_Config flag")));

		FProperty* NonConfigProp = ScriptClass->FindPropertyByName(TEXT("NonConfigValue"));
		ASSERT_THAT(IsNotNull(NonConfigProp, TEXT("Non-config property should exist")));
		ASSERT_THAT(IsFalse(NonConfigProp->HasAnyPropertyFlags(CPF_Config), TEXT("Non-config property should not have CPF_Config flag")));

		UClass* DefaultConfigClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_ConfigSpec.ADefaultConfigActor")));
		ASSERT_THAT(IsNotNull(DefaultConfigClass, TEXT("DefaultConfig class should compile")));

		UClass* EditorConfigClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_ConfigSpec.AConfigEditorActor")));
		ASSERT_THAT(IsNotNull(EditorConfigClass, TEXT("Config=Editor class should compile")));
		ASSERT_THAT(IsTrue(EditorConfigClass->ClassConfigName == NAME_Editor, TEXT("Should use Editor config")));
	}

	// -------------------------------------------------------------------------
	// Display specifiers: ClassGroup, HideCategories, ShowCategories
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassDisplaySpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DisplaySpec"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassDisplaySpec.as"),
			ASTEST_AS(R"AS(
			// ClassGroup - organize in editor
			UCLASS(ClassGroup=(Custom))
			class AGroupedActor : AActor
			{
				UPROPERTY()
				int Value = 1;
			}

			// HideCategories - hide property categories
			UCLASS(HideCategories=(Rendering, Collision))
			class AHideCategoriesActor : AActor
			{
				UPROPERTY()
				int Value = 2;
			}

			// ShowCategories - show previously hidden categories
			UCLASS(ShowCategories=(Rendering))
			class AShowCategoriesActor : AHideCategoriesActor
			{
				UPROPERTY()
				int DerivedValue = 3;
			}

			// CollapseCategories - collapse all categories
			UCLASS(CollapseCategories)
			class ACollapsedActor : AActor
			{
				UPROPERTY(Category="MyCategory")
				int Value = 4;
			}

			// AutoExpandCategories - auto-expand specific categories
			UCLASS(AutoExpandCategories=(MyCategory))
			class AAutoExpandActor : AActor
			{
				UPROPERTY(Category="MyCategory")
				int CategoryValue = 5;

				UPROPERTY(Category="OtherCategory")
				int OtherValue = 6;
			}
			)AS"),
			TEXT("AGroupedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("ClassGroup class should compile")));

		UClass* HideCategoriesClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_DisplaySpec.AHideCategoriesActor")));
		ASSERT_THAT(IsNotNull(HideCategoriesClass, TEXT("HideCategories class should compile")));

		UClass* ShowCategoriesClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_DisplaySpec.AShowCategoriesActor")));
		ASSERT_THAT(IsNotNull(ShowCategoriesClass, TEXT("ShowCategories class should compile")));

		UClass* CollapsedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_DisplaySpec.ACollapsedActor")));
		ASSERT_THAT(IsNotNull(CollapsedClass, TEXT("CollapseCategories class should compile")));

		UClass* AutoExpandClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_DisplaySpec.AAutoExpandActor")));
		ASSERT_THAT(IsNotNull(AutoExpandClass, TEXT("AutoExpandCategories class should compile")));
	}

	// -------------------------------------------------------------------------
	// Metadata: DisplayName, ToolTip, ShortTooltip
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassMetaData)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_MetaData"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassMetaData.as"),
			ASTEST_AS(R"AS(
			// DisplayName - custom display name in editor
			UCLASS(meta=(DisplayName="My Custom Actor"))
			class ADisplayNameActor : AActor
			{
				UPROPERTY()
				int Value = 1;
			}

			// ToolTip - full tooltip text
			UCLASS(meta=(ToolTip="This is a detailed tooltip for the actor"))
			class AToolTipActor : AActor
			{
				UPROPERTY()
				int Value = 2;
			}

			// ShortTooltip - brief tooltip
			UCLASS(meta=(ShortTooltip="Brief description"))
			class AShortTooltipActor : AActor
			{
				UPROPERTY()
				int Value = 3;
			}

			// Multiple metadata entries
			UCLASS(meta=(DisplayName="Combined Meta Actor", ToolTip="Actor with multiple metadata entries"))
			class ACombinedMetaActor : AActor
			{
				UPROPERTY()
				int Value = 4;
			}
			)AS"),
			TEXT("ADisplayNameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DisplayName class should compile")));

		// Verify DisplayName metadata
		FString DisplayName = ScriptClass->GetMetaData(TEXT("DisplayName"));
		ASSERT_THAT(AreEqual(DisplayName, FString(TEXT("My Custom Actor")), TEXT("DisplayName metadata should be set")));

		UClass* ToolTipClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_MetaData.AToolTipActor")));
		ASSERT_THAT(IsNotNull(ToolTipClass, TEXT("ToolTip class should compile")));
		FString ToolTip = ToolTipClass->GetMetaData(TEXT("ToolTip"));
		ASSERT_THAT(AreEqual(ToolTip, FString(TEXT("This is a detailed tooltip for the actor")), TEXT("ToolTip metadata should be set")));

		UClass* ShortTooltipClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_MetaData.AShortTooltipActor")));
		ASSERT_THAT(IsNotNull(ShortTooltipClass, TEXT("ShortTooltip class should compile")));

		UClass* CombinedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_MetaData.ACombinedMetaActor")));
		ASSERT_THAT(IsNotNull(CombinedClass, TEXT("Combined metadata class should compile")));
		FString CombinedDisplayName = CombinedClass->GetMetaData(TEXT("DisplayName"));
		FString CombinedToolTip = CombinedClass->GetMetaData(TEXT("ToolTip"));
		ASSERT_THAT(AreEqual(CombinedDisplayName, FString(TEXT("Combined Meta Actor")), TEXT("Combined DisplayName should be set")));
		ASSERT_THAT(AreEqual(CombinedToolTip, FString(TEXT("Actor with multiple metadata entries")), TEXT("Combined ToolTip should be set")));
	}

	// -------------------------------------------------------------------------
	// Specifier combinations: common patterns
	// -------------------------------------------------------------------------
	TEST_METHOD(UClassSpecifierCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_Combinations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUClassCombinations.as"),
			ASTEST_AS(R"AS(
			// Common pattern: Abstract base that's blueprintable
			UCLASS(Abstract, Blueprintable)
			class AGameplayBase : AActor
			{
				UPROPERTY()
				int BaseHealth = 100;
			}

			// Concrete implementation
			UCLASS()
			class AGameplayDerived : AGameplayBase
			{
				UPROPERTY()
				int DerivedArmor = 50;
			}

			// BlueprintType but NotBlueprintable - can use as variable but not subclass
			UCLASS(NotBlueprintable, BlueprintType)
			class ADataOnlyActor : AActor
			{
				UPROPERTY()
				int DataValue = 200;
			}

			// Config + DefaultConfig combination
			UCLASS(Config=Game, DefaultConfig)
			class AConfigurableActor : AActor
			{
				UPROPERTY(Config)
				int ConfigHealth = 150;

				UPROPERTY(Config)
				float ConfigSpeed = 600.0f;
			}

			// Complex combination: multiple specifiers and metadata
			UCLASS(Blueprintable, BlueprintType, HideCategories=(Rendering), meta=(DisplayName="Complex Actor", ToolTip="Combines multiple specifiers"))
			class AComplexActor : AActor
			{
				UPROPERTY()
				int ComplexValue = 999;
			}
			)AS"),
			TEXT("AGameplayBase"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Abstract + Blueprintable should compile")));
		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Should be abstract")));

		UClass* DerivedClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_Combinations.AGameplayDerived")));
		ASSERT_THAT(IsNotNull(DerivedClass, TEXT("Concrete derived should compile")));
		ASSERT_THAT(IsFalse(DerivedClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Derived should not be abstract")));

		UClass* DataOnlyClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_Combinations.ADataOnlyActor")));
		ASSERT_THAT(IsNotNull(DataOnlyClass, TEXT("NotBlueprintable + BlueprintType should compile")));

		UClass* ConfigClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_Combinations.AConfigurableActor")));
		ASSERT_THAT(IsNotNull(ConfigClass, TEXT("Config + DefaultConfig should compile")));
		ASSERT_THAT(IsTrue(ConfigClass->ClassConfigName == NAME_Game, TEXT("Should use Game config")));

		UClass* ComplexClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, TEXT("/Script/ASCoverageUClass_Combinations.AComplexActor")));
		ASSERT_THAT(IsNotNull(ComplexClass, TEXT("Complex combination should compile")));
		FString ComplexDisplayName = ComplexClass->GetMetaData(TEXT("DisplayName"));
		ASSERT_THAT(AreEqual(ComplexDisplayName, FString(TEXT("Complex Actor")), TEXT("Complex metadata should be set")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
