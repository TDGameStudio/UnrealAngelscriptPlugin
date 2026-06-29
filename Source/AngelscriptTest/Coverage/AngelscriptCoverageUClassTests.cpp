#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/ScopeExit.h"
#include "Subsystem/ScriptGameInstanceSubsystem.h"
#include "Subsystem/ScriptLocalPlayerSubsystem.h"
#include "Subsystem/ScriptWorldSubsystem.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUClassTest,
	"Angelscript.TestModule.Coverage.UClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CompileUClassFixture(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName, const FString& Filename, const FString& ScriptSource)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptSource),
			*FString::Printf(TEXT("UCLASS coverage module '%s' should compile"), *ModuleName.ToString()));
	}

	static bool CompileUClassFixtureShouldFail(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName, const FString& Filename, const FString& ScriptSource, TArrayView<const FString> ExpectedDiagnosticFragments)
	{
		FNoDiscardAsserter LocalAssert(Test);

		FAngelscriptCompileTraceSummary Summary;
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			Filename,
			ScriptSource,
			true,
			Summary,
			true);

		bool bPassed = LocalAssert.IsFalse(
			Summary.bCompileSucceeded,
			*FString::Printf(TEXT("UCLASS coverage module '%s' should fail compilation"), *ModuleName.ToString()));
		bPassed &= LocalAssert.AreEqual(
			ECompileResult::Error,
			Summary.CompileResult,
			*FString::Printf(TEXT("UCLASS coverage module '%s' should report compile error"), *ModuleName.ToString()));

		for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
		{
			bool bFoundFragment = false;
			for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
			{
				if (Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragment))
				{
					bFoundFragment = true;
					break;
				}
			}

			bPassed &= LocalAssert.IsTrue(
				bFoundFragment,
				*FString::Printf(TEXT("UCLASS coverage module '%s' diagnostics should contain '%s'"),
					*ModuleName.ToString(),
					*ExpectedFragment));
		}

		Engine.DiscardModule(*ModuleName.ToString());
		return bPassed;
	}

	static int32 CountActorComponentsByClass(const AActor* Actor, const UClass* ComponentClass)
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

	static int32 CountNonReturnParameters(const UFunction* Function)
	{
		if (Function == nullptr)
		{
			return 0;
		}

		int32 Count = 0;
		for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
		{
			if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				++Count;
			}
		}
		return Count;
	}

	static UActorComponent* FindActorComponentByName(const AActor* Actor, FName ComponentName)
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

	TEST_METHOD(UClassBaseTypeDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_BaseTypeDeclarations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassBaseObject : UObject
			{
			}

			UCLASS()
			class ACoverageUClassBaseActor : AActor
			{
			}

			UCLASS()
			class UCoverageUClassBaseActorComponent : UActorComponent
			{
			}

			UCLASS()
			class UCoverageUClassBaseSceneComponent : USceneComponent
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassBaseTypeDeclarations.as"), ScriptSource)));

		UClass* ObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassBaseObject"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassBaseActor"));
		UClass* ActorComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassBaseActorComponent"));
		UClass* SceneComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassBaseSceneComponent"));
		ASSERT_THAT(IsNotNull(ObjectClass, TEXT("UObject-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("AActor-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(ActorComponentClass, TEXT("UActorComponent-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(SceneComponentClass, TEXT("USceneComponent-derived UCLASS should be generated")));
		if (ObjectClass == nullptr || ActorClass == nullptr || ActorComponentClass == nullptr || SceneComponentClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ObjectClass->IsChildOf(UObject::StaticClass()), TEXT("Generated object should inherit UObject")));
		ASSERT_THAT(IsTrue(ActorClass->IsChildOf(AActor::StaticClass()), TEXT("Generated actor should inherit AActor")));
		ASSERT_THAT(IsTrue(ActorComponentClass->IsChildOf(UActorComponent::StaticClass()), TEXT("Generated component should inherit UActorComponent")));
		ASSERT_THAT(IsTrue(SceneComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Generated scene component should inherit USceneComponent")));
	}

	TEST_METHOD(UClassCommonEngineBaseDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_CommonEngineBaseDeclarations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassPawn : APawn
			{
			}

			UCLASS()
			class ACoverageUClassCharacter : ACharacter
			{
			}

			UCLASS()
			class ACoverageUClassPlayerController : APlayerController
			{
			}

			UCLASS()
			class ACoverageUClassGameMode : AGameModeBase
			{
			}

			UCLASS()
			class ACoverageUClassGameState : AGameStateBase
			{
			}

			UCLASS()
			class ACoverageUClassPlayerState : APlayerState
			{
			}

			UCLASS()
			class ACoverageUClassHUD : AHUD
			{
			}

			UCLASS()
			class UCoverageUClassUserWidget : UUserWidget
			{
			}

			UCLASS()
			class UCoverageUClassWorldSubsystem : UScriptWorldSubsystem
			{
			}

			UCLASS()
			class UCoverageUClassGameInstanceSubsystem : UScriptGameInstanceSubsystem
			{
			}

			UCLASS()
			class UCoverageUClassLocalPlayerSubsystem : UScriptLocalPlayerSubsystem
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassCommonEngineBaseDeclarations.as"), ScriptSource)));

		UClass* PawnClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPawn"));
		UClass* CharacterClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassCharacter"));
		UClass* PlayerControllerClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPlayerController"));
		UClass* GameModeClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassGameMode"));
		UClass* GameStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassGameState"));
		UClass* PlayerStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPlayerState"));
		UClass* HUDClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassHUD"));
		UClass* UserWidgetClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassUserWidget"));
		UClass* WorldSubsystemClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassWorldSubsystem"));
		UClass* GameInstanceSubsystemClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassGameInstanceSubsystem"));
		UClass* LocalPlayerSubsystemClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassLocalPlayerSubsystem"));
		ASSERT_THAT(IsNotNull(PawnClass, TEXT("APawn-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(CharacterClass, TEXT("ACharacter-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(PlayerControllerClass, TEXT("APlayerController-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(GameModeClass, TEXT("AGameModeBase-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(GameStateClass, TEXT("AGameStateBase-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(PlayerStateClass, TEXT("APlayerState-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(HUDClass, TEXT("AHUD-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(UserWidgetClass, TEXT("UUserWidget-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(WorldSubsystemClass, TEXT("UScriptWorldSubsystem-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(GameInstanceSubsystemClass, TEXT("UScriptGameInstanceSubsystem-derived UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(LocalPlayerSubsystemClass, TEXT("UScriptLocalPlayerSubsystem-derived UCLASS should be generated")));
		if (PawnClass == nullptr || CharacterClass == nullptr || PlayerControllerClass == nullptr || GameModeClass == nullptr
			|| GameStateClass == nullptr || PlayerStateClass == nullptr || HUDClass == nullptr || UserWidgetClass == nullptr
			|| WorldSubsystemClass == nullptr || GameInstanceSubsystemClass == nullptr || LocalPlayerSubsystemClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(PawnClass->IsChildOf(APawn::StaticClass()), TEXT("Generated pawn should inherit APawn")));
		ASSERT_THAT(IsTrue(CharacterClass->IsChildOf(ACharacter::StaticClass()), TEXT("Generated character should inherit ACharacter")));
		ASSERT_THAT(IsTrue(PlayerControllerClass->IsChildOf(APlayerController::StaticClass()), TEXT("Generated player controller should inherit APlayerController")));
		ASSERT_THAT(IsTrue(GameModeClass->IsChildOf(AGameModeBase::StaticClass()), TEXT("Generated game mode should inherit AGameModeBase")));
		ASSERT_THAT(IsTrue(GameStateClass->IsChildOf(AGameStateBase::StaticClass()), TEXT("Generated game state should inherit AGameStateBase")));
		ASSERT_THAT(IsTrue(PlayerStateClass->IsChildOf(APlayerState::StaticClass()), TEXT("Generated player state should inherit APlayerState")));
		ASSERT_THAT(IsTrue(HUDClass->IsChildOf(AHUD::StaticClass()), TEXT("Generated HUD should inherit AHUD")));
		ASSERT_THAT(IsTrue(UserWidgetClass->IsChildOf(UUserWidget::StaticClass()), TEXT("Generated widget should inherit UUserWidget")));
		ASSERT_THAT(IsTrue(WorldSubsystemClass->IsChildOf(UScriptWorldSubsystem::StaticClass()), TEXT("Generated world subsystem should inherit UScriptWorldSubsystem")));
		ASSERT_THAT(IsTrue(GameInstanceSubsystemClass->IsChildOf(UScriptGameInstanceSubsystem::StaticClass()), TEXT("Generated game-instance subsystem should inherit UScriptGameInstanceSubsystem")));
		ASSERT_THAT(IsTrue(LocalPlayerSubsystemClass->IsChildOf(UScriptLocalPlayerSubsystem::StaticClass()), TEXT("Generated local-player subsystem should inherit UScriptLocalPlayerSubsystem")));
	}

	TEST_METHOD(UClassGameFrameworkReferenceSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_GameFrameworkReferenceSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassFrameworkCharacter : ACharacter
			{
				UPROPERTY()
				int Health = 44;

				UFUNCTION()
				int ReadHealth()
				{
					return Health;
				}
			}

			UCLASS()
			class ACoverageUClassFrameworkPlayerState : APlayerState
			{
				UPROPERTY()
				FString PublicName = "CoveragePlayer";

				UFUNCTION()
				FString BuildPlayerLabel(const FString&in Suffix)
				{
					return PublicName + "_" + Suffix;
				}
			}

			UCLASS()
			class ACoverageUClassFrameworkController : APlayerController
			{
				UPROPERTY()
				ACoverageUClassFrameworkCharacter CharacterRef;

				UPROPERTY()
				APlayerState PlayerStateRef;

				UPROPERTY()
				TSubclassOf<ACharacter> CharacterClass = ACoverageUClassFrameworkCharacter::StaticClass();

				UFUNCTION()
				void AssignRefs(ACoverageUClassFrameworkCharacter InCharacter, APlayerState InPlayerState)
				{
					CharacterRef = InCharacter;
					PlayerStateRef = InPlayerState;
				}
			}

			UCLASS()
			class ACoverageUClassFrameworkGameMode : AGameModeBase
			{
				UPROPERTY()
				TSubclassOf<APawn> PawnClassRef = ACoverageUClassFrameworkCharacter::StaticClass();

				UFUNCTION()
				int RoutePlayers(APlayerController NewPlayer, AController LeavingController)
				{
					return (NewPlayer != nullptr ? 10 : 0) + (LeavingController != nullptr ? 1 : 0);
				}
			}

			UCLASS()
			class ACoverageUClassFrameworkGameState : AGameStateBase
			{
				UPROPERTY()
				TArray<APlayerState> ScriptPlayerStates;

				UFUNCTION()
				void AddScriptPlayerState(APlayerState State)
				{
					ScriptPlayerStates.Add(State);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassGameFrameworkReferenceSurface.as"), ScriptSource)));

		UClass* CharacterClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassFrameworkCharacter"));
		UClass* PlayerStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassFrameworkPlayerState"));
		UClass* ControllerClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassFrameworkController"));
		UClass* GameModeClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassFrameworkGameMode"));
		UClass* GameStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassFrameworkGameState"));
		ASSERT_THAT(IsNotNull(CharacterClass, TEXT("Character reference class should be generated")));
		ASSERT_THAT(IsNotNull(PlayerStateClass, TEXT("PlayerState reference class should be generated")));
		ASSERT_THAT(IsNotNull(ControllerClass, TEXT("PlayerController reference class should be generated")));
		ASSERT_THAT(IsNotNull(GameModeClass, TEXT("GameMode reference class should be generated")));
		ASSERT_THAT(IsNotNull(GameStateClass, TEXT("GameState reference class should be generated")));
		if (CharacterClass == nullptr || PlayerStateClass == nullptr || ControllerClass == nullptr || GameModeClass == nullptr || GameStateClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CharacterClass->IsChildOf(ACharacter::StaticClass()), TEXT("Framework character should inherit ACharacter")));
		ASSERT_THAT(IsTrue(PlayerStateClass->IsChildOf(APlayerState::StaticClass()), TEXT("Framework player state should inherit APlayerState")));
		ASSERT_THAT(IsTrue(ControllerClass->IsChildOf(APlayerController::StaticClass()), TEXT("Framework controller should inherit APlayerController")));
		ASSERT_THAT(IsTrue(GameModeClass->IsChildOf(AGameModeBase::StaticClass()), TEXT("Framework game mode should inherit AGameModeBase")));
		ASSERT_THAT(IsTrue(GameStateClass->IsChildOf(AGameStateBase::StaticClass()), TEXT("Framework game state should inherit AGameStateBase")));

		FObjectPropertyBase* CharacterRefProperty = FindFProperty<FObjectPropertyBase>(ControllerClass, TEXT("CharacterRef"));
		FObjectPropertyBase* PlayerStateRefProperty = FindFProperty<FObjectPropertyBase>(ControllerClass, TEXT("PlayerStateRef"));
		FClassProperty* CharacterClassProperty = FindFProperty<FClassProperty>(ControllerClass, TEXT("CharacterClass"));
		ASSERT_THAT(IsNotNull(CharacterRefProperty, TEXT("Controller should reflect script character references")));
		ASSERT_THAT(IsNotNull(PlayerStateRefProperty, TEXT("Controller should reflect native player-state references")));
		ASSERT_THAT(IsNotNull(CharacterClassProperty, TEXT("Controller should reflect TSubclassOf<ACharacter>")));
		if (CharacterRefProperty == nullptr || PlayerStateRefProperty == nullptr || CharacterClassProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(CharacterClass, CharacterRefProperty->PropertyClass, TEXT("CharacterRef should preserve the script character class")));
		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), PlayerStateRefProperty->PropertyClass, TEXT("PlayerStateRef should target native APlayerState")));
		ASSERT_THAT(IsTrue(CharacterClassProperty->MetaClass != nullptr && CharacterClassProperty->MetaClass->IsChildOf(ACharacter::StaticClass()), TEXT("CharacterClass should constrain TSubclassOf to ACharacter")));
		UObject* ControllerDefaultObject = ControllerClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ControllerDefaultObject, TEXT("Controller CDO should exist")));
		if (ControllerDefaultObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(CharacterClass, CharacterClassProperty->GetPropertyValue_InContainer(ControllerDefaultObject), TEXT("TSubclassOf default should store the script character class")));

		UFunction* AssignRefsFunction = ControllerClass->FindFunctionByName(TEXT("AssignRefs"));
		ASSERT_THAT(IsNotNull(AssignRefsFunction, TEXT("Controller AssignRefs UFUNCTION should be generated")));
		if (AssignRefsFunction == nullptr)
		{
			return;
		}
		FObjectPropertyBase* InCharacterParam = FindFProperty<FObjectPropertyBase>(AssignRefsFunction, TEXT("InCharacter"));
		FObjectPropertyBase* InPlayerStateParam = FindFProperty<FObjectPropertyBase>(AssignRefsFunction, TEXT("InPlayerState"));
		ASSERT_THAT(IsNotNull(InCharacterParam, TEXT("AssignRefs should expose InCharacter")));
		ASSERT_THAT(IsNotNull(InPlayerStateParam, TEXT("AssignRefs should expose InPlayerState")));
		if (InCharacterParam == nullptr || InPlayerStateParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(CharacterClass, InCharacterParam->PropertyClass, TEXT("AssignRefs should preserve script character parameter type")));
		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), InPlayerStateParam->PropertyClass, TEXT("AssignRefs should preserve native player-state parameter type")));
		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(AssignRefsFunction), TEXT("AssignRefs should expose two object parameters")));

		FClassProperty* PawnClassProperty = FindFProperty<FClassProperty>(GameModeClass, TEXT("PawnClassRef"));
		UFunction* RoutePlayersFunction = GameModeClass->FindFunctionByName(TEXT("RoutePlayers"));
		ASSERT_THAT(IsNotNull(PawnClassProperty, TEXT("GameMode should reflect TSubclassOf<APawn>")));
		ASSERT_THAT(IsNotNull(RoutePlayersFunction, TEXT("GameMode RoutePlayers UFUNCTION should be generated")));
		if (PawnClassProperty == nullptr || RoutePlayersFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(PawnClassProperty->MetaClass != nullptr && PawnClassProperty->MetaClass->IsChildOf(APawn::StaticClass()), TEXT("PawnClassRef should constrain TSubclassOf to APawn")));
		ASSERT_THAT(AreEqual(CharacterClass, PawnClassProperty->GetPropertyValue_InContainer(GameModeClass->GetDefaultObject()), TEXT("PawnClassRef default should accept a script ACharacter class")));
		FObjectPropertyBase* NewPlayerParam = FindFProperty<FObjectPropertyBase>(RoutePlayersFunction, TEXT("NewPlayer"));
		FObjectPropertyBase* LeavingControllerParam = FindFProperty<FObjectPropertyBase>(RoutePlayersFunction, TEXT("LeavingController"));
		ASSERT_THAT(IsNotNull(NewPlayerParam, TEXT("RoutePlayers should expose NewPlayer")));
		ASSERT_THAT(IsNotNull(LeavingControllerParam, TEXT("RoutePlayers should expose LeavingController")));
		if (NewPlayerParam == nullptr || LeavingControllerParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), NewPlayerParam->PropertyClass, TEXT("RoutePlayers should preserve APlayerController parameter type")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), LeavingControllerParam->PropertyClass, TEXT("RoutePlayers should preserve AController parameter type")));

		FArrayProperty* PlayerStatesProperty = FindFProperty<FArrayProperty>(GameStateClass, TEXT("ScriptPlayerStates"));
		UFunction* AddScriptPlayerStateFunction = GameStateClass->FindFunctionByName(TEXT("AddScriptPlayerState"));
		ASSERT_THAT(IsNotNull(PlayerStatesProperty, TEXT("GameState should reflect TArray<APlayerState>")));
		ASSERT_THAT(IsNotNull(AddScriptPlayerStateFunction, TEXT("GameState AddScriptPlayerState UFUNCTION should be generated")));
		if (PlayerStatesProperty == nullptr || AddScriptPlayerStateFunction == nullptr)
		{
			return;
		}
		FObjectPropertyBase* PlayerStateInnerProperty = CastField<FObjectPropertyBase>(PlayerStatesProperty->Inner);
		ASSERT_THAT(IsNotNull(PlayerStateInnerProperty, TEXT("ScriptPlayerStates inner should be an object property")));
		if (PlayerStateInnerProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), PlayerStateInnerProperty->PropertyClass, TEXT("ScriptPlayerStates should contain APlayerState entries")));
		FObjectPropertyBase* AddStateParam = FindFProperty<FObjectPropertyBase>(AddScriptPlayerStateFunction, TEXT("State"));
		ASSERT_THAT(IsNotNull(AddStateParam, TEXT("AddScriptPlayerState should expose State")));
		if (AddStateParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), AddStateParam->PropertyClass, TEXT("AddScriptPlayerState should preserve APlayerState parameter type")));
	}

	TEST_METHOD(UClassBlueprintAndAbstractSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_BlueprintAndAbstractSpecifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, Abstract)
			class ACoverageUClassAbstractBlueprintableActor : AActor
			{
			}

			UCLASS(NotBlueprintable, BlueprintType)
			class UCoverageUClassBlueprintVariableOnlyObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassBlueprintAndAbstractSpecifiers.as"), ScriptSource)));

		UClass* AbstractBlueprintableClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassAbstractBlueprintableActor"));
		UClass* BlueprintVariableOnlyClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassBlueprintVariableOnlyObject"));
		ASSERT_THAT(IsNotNull(AbstractBlueprintableClass, TEXT("Blueprintable Abstract class should be generated")));
		ASSERT_THAT(IsNotNull(BlueprintVariableOnlyClass, TEXT("NotBlueprintable BlueprintType class should be generated")));
		if (AbstractBlueprintableClass == nullptr || BlueprintVariableOnlyClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AbstractBlueprintableClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Abstract should set CLASS_Abstract")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AbstractBlueprintableClass->GetMetaData(TEXT("Blueprintable")), TEXT("Blueprintable metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AbstractBlueprintableClass->GetMetaData(TEXT("BlueprintType")), TEXT("Generated non-statics classes should remain BlueprintType")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AbstractBlueprintableClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("Blueprintable Abstract class should remain a Blueprint base")));

		ASSERT_THAT(AreEqual(FString(TEXT("true")), BlueprintVariableOnlyClass->GetMetaData(TEXT("BlueprintType")), TEXT("BlueprintType metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), BlueprintVariableOnlyClass->GetMetaData(TEXT("NotBlueprintable")), TEXT("NotBlueprintable metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), BlueprintVariableOnlyClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("NotBlueprintable should clear Blueprint base metadata")));
	}

	TEST_METHOD(UClassConfigAndInlineCreationSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_ConfigAndInlineCreationSpecifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Config=Game, DefaultConfig, DefaultToInstanced, EditInlineNew, HideDropdown)
			class UCoverageUClassConfigInlineObject : UObject
			{
				UPROPERTY(Config)
				int ConfigValue = 7;
			}

			UCLASS(Config=Editor)
			class UCoverageUClassEditorConfigObject : UObject
			{
				UPROPERTY(Config)
				int EditorValue = 11;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassConfigAndInlineCreationSpecifiers.as"), ScriptSource)));

		UClass* ConfigInlineClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassConfigInlineObject"));
		UClass* EditorConfigClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassEditorConfigObject"));
		ASSERT_THAT(IsNotNull(ConfigInlineClass, TEXT("Config inline object class should be generated")));
		ASSERT_THAT(IsNotNull(EditorConfigClass, TEXT("Editor config object class should be generated")));
		if (ConfigInlineClass == nullptr || EditorConfigClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ConfigInlineClass->HasAnyClassFlags(CLASS_Config), TEXT("Config=Game should set CLASS_Config")));
		ASSERT_THAT(IsTrue(ConfigInlineClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig should set CLASS_DefaultConfig on config classes")));
		ASSERT_THAT(IsTrue(ConfigInlineClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("DefaultToInstanced should set CLASS_DefaultToInstanced")));
		ASSERT_THAT(IsTrue(ConfigInlineClass->HasAnyClassFlags(CLASS_EditInlineNew), TEXT("EditInlineNew should set CLASS_EditInlineNew")));
		ASSERT_THAT(IsTrue(ConfigInlineClass->HasAnyClassFlags(CLASS_HideDropDown), TEXT("HideDropdown should set CLASS_HideDropDown")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), ConfigInlineClass->ClassConfigName, TEXT("Config=Game should set ClassConfigName")));

		FProperty* ConfigValueProperty = ConfigInlineClass->FindPropertyByName(TEXT("ConfigValue"));
		ASSERT_THAT(IsNotNull(ConfigValueProperty, TEXT("ConfigValue property should be generated")));
		if (ConfigValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConfigValueProperty->HasAnyPropertyFlags(CPF_Config), TEXT("UPROPERTY(Config) should set CPF_Config")));

		ASSERT_THAT(IsTrue(EditorConfigClass->HasAnyClassFlags(CLASS_Config), TEXT("Config=Editor should set CLASS_Config")));
		ASSERT_THAT(AreEqual(FName(TEXT("Editor")), EditorConfigClass->ClassConfigName, TEXT("Config=Editor should set ClassConfigName")));
	}

	TEST_METHOD(UClassDisplayAndCategoryMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DisplayAndCategoryMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(ClassGroup="Coverage", HideCategories="Rendering", meta=(DisplayName="Coverage Metadata Object", ToolTip="Full class tooltip", ShortTooltip="Short class tooltip", IsBlueprintBase="true", ChildCanTick, IgnoreCategoryKeywordsInSubclasses))
			class UCoverageUClassDisplayMetadataObject : UObject
			{
			}

			UCLASS(HideCategories="Rendering")
			class UCoverageUClassHiddenCategoryBaseObject : UObject
			{
			}

			UCLASS(HideCategories="Rendering", meta=(ShowCategories="Rendering", AutoExpandCategories="Coverage", AutoCollapseCategories="Advanced"))
			class UCoverageUClassShownCategoryObject : UCoverageUClassHiddenCategoryBaseObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDisplayAndCategoryMetadata.as"), ScriptSource)));

		UClass* DisplayMetadataClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDisplayMetadataObject"));
		UClass* HiddenCategoryClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassHiddenCategoryBaseObject"));
		UClass* ShownCategoryClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassShownCategoryObject"));
		ASSERT_THAT(IsNotNull(DisplayMetadataClass, TEXT("Display metadata class should be generated")));
		ASSERT_THAT(IsNotNull(HiddenCategoryClass, TEXT("Hidden category class should be generated")));
		ASSERT_THAT(IsNotNull(ShownCategoryClass, TEXT("Shown category class should be generated")));
		if (DisplayMetadataClass == nullptr || HiddenCategoryClass == nullptr || ShownCategoryClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Coverage")), DisplayMetadataClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("ClassGroup should emit ClassGroupNames metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), DisplayMetadataClass->GetMetaData(TEXT("HideCategories")), TEXT("HideCategories should emit metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Metadata Object")), DisplayMetadataClass->GetMetaData(TEXT("DisplayName")), TEXT("DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Full class tooltip")), DisplayMetadataClass->GetMetaData(TEXT("ToolTip")), TEXT("ToolTip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Short class tooltip")), DisplayMetadataClass->GetMetaData(TEXT("ShortTooltip")), TEXT("ShortTooltip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), DisplayMetadataClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("IsBlueprintBase metadata should round-trip")));
		ASSERT_THAT(IsTrue(DisplayMetadataClass->HasMetaData(TEXT("ChildCanTick")), TEXT("ChildCanTick metadata should be present")));
		ASSERT_THAT(IsTrue(DisplayMetadataClass->HasMetaData(TEXT("IgnoreCategoryKeywordsInSubclasses")), TEXT("IgnoreCategoryKeywordsInSubclasses metadata should be present")));

		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), HiddenCategoryClass->GetMetaData(TEXT("HideCategories")), TEXT("Base HideCategories metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), ShownCategoryClass->GetMetaData(TEXT("ShowCategories")), TEXT("ShowCategories metadata should round-trip through meta")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage")), ShownCategoryClass->GetMetaData(TEXT("AutoExpandCategories")), TEXT("AutoExpandCategories metadata should round-trip through meta")));
		ASSERT_THAT(AreEqual(FString(TEXT("Advanced")), ShownCategoryClass->GetMetaData(TEXT("AutoCollapseCategories")), TEXT("AutoCollapseCategories metadata should round-trip through meta")));
	}

	TEST_METHOD(UClassBehaviorFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_BehaviorFlags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Transient, Deprecated)
			class UCoverageUClassTransientDeprecatedObject : UObject
			{
			}

			UCLASS(NotPlaceable)
			class ACoverageUClassNotPlaceableActor : AActor
			{
			}

			UCLASS()
			class ACoverageUClassDefaultPlaceableActor : AActor
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassBehaviorFlags.as"), ScriptSource)));

		UClass* TransientDeprecatedClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassTransientDeprecatedObject"));
		UClass* NotPlaceableActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassNotPlaceableActor"));
		UClass* DefaultPlaceableActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultPlaceableActor"));
		ASSERT_THAT(IsNotNull(TransientDeprecatedClass, TEXT("Transient Deprecated class should be generated")));
		ASSERT_THAT(IsNotNull(NotPlaceableActorClass, TEXT("NotPlaceable actor class should be generated")));
		ASSERT_THAT(IsNotNull(DefaultPlaceableActorClass, TEXT("Default placeable actor class should be generated")));
		if (TransientDeprecatedClass == nullptr || NotPlaceableActorClass == nullptr || DefaultPlaceableActorClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(TransientDeprecatedClass->HasAnyClassFlags(CLASS_Transient), TEXT("Transient should set CLASS_Transient")));
		ASSERT_THAT(IsTrue(TransientDeprecatedClass->HasAnyClassFlags(CLASS_Deprecated), TEXT("Deprecated should set CLASS_Deprecated")));
		ASSERT_THAT(IsTrue(NotPlaceableActorClass->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("NotPlaceable should set CLASS_NotPlaceable")));
		ASSERT_THAT(IsFalse(DefaultPlaceableActorClass->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("Actor classes should be placeable by default")));
	}

	TEST_METHOD(UClassSpecialAndInheritedMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_SpecialAndInheritedMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(ComponentWrapperClass, meta=(ConversionRoot, HideFunctions="CoverageHiddenFunction", SparseClassDataTypes="CoverageSparseData", AutoExpandCategories="CoverageExpanded", CollapseCategories, DontCollapseCategories))
			class UCoverageUClassSpecialMetadataBaseObject : UObject
			{
			}

			UCLASS()
			class UCoverageUClassInheritedMetadataObject : UCoverageUClassSpecialMetadataBaseObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassSpecialAndInheritedMetadata.as"), ScriptSource)));

		UClass* SpecialMetadataClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassSpecialMetadataBaseObject"));
		UClass* InheritedMetadataClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassInheritedMetadataObject"));
		ASSERT_THAT(IsNotNull(SpecialMetadataClass, TEXT("Special metadata class should be generated")));
		ASSERT_THAT(IsNotNull(InheritedMetadataClass, TEXT("Inherited metadata class should be generated")));
		if (SpecialMetadataClass == nullptr || InheritedMetadataClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SpecialMetadataClass->HasMetaData(TEXT("ComponentWrapperClass")), TEXT("ComponentWrapperClass should emit metadata")));
		ASSERT_THAT(IsTrue(SpecialMetadataClass->HasMetaData(TEXT("ConversionRoot")), TEXT("ConversionRoot metadata should be present")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageHiddenFunction")), SpecialMetadataClass->GetMetaData(TEXT("HideFunctions")), TEXT("HideFunctions metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageSparseData")), SpecialMetadataClass->GetMetaData(TEXT("SparseClassDataTypes")), TEXT("SparseClassDataTypes metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageExpanded")), SpecialMetadataClass->GetMetaData(TEXT("AutoExpandCategories")), TEXT("AutoExpandCategories metadata should round-trip")));
		ASSERT_THAT(IsTrue(SpecialMetadataClass->HasMetaData(TEXT("CollapseCategories")), TEXT("CollapseCategories metadata should be present")));
		ASSERT_THAT(IsTrue(SpecialMetadataClass->HasMetaData(TEXT("DontCollapseCategories")), TEXT("DontCollapseCategories metadata should be present")));

		ASSERT_THAT(AreEqual(FString(TEXT("CoverageHiddenFunction")), InheritedMetadataClass->GetMetaData(TEXT("HideFunctions")), TEXT("HideFunctions should be copied to subclasses")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageSparseData")), InheritedMetadataClass->GetMetaData(TEXT("SparseClassDataTypes")), TEXT("SparseClassDataTypes should be copied to subclasses")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageExpanded")), InheritedMetadataClass->GetMetaData(TEXT("AutoExpandCategories")), TEXT("AutoExpandCategories should be copied to subclasses")));
	}

	TEST_METHOD(UClassScriptOnlyAndDisplayNameMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_ScriptOnlyAndDisplayNameMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			class FCoverageUClassPlainScriptState
			{
				int Value = 5;
			}

			UCLASS()
			class UCoverageUClassDefaultDisplayNameObject : UObject
			{
			}

			UCLASS(meta=(DisplayName="Coverage Explicit Display Object"))
			class UCoverageUClassExplicitDisplayNameObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassScriptOnlyAndDisplayNameMetadata.as"), ScriptSource)));

		UClass* PlainScriptClass = FindGeneratedClass(&Engine, TEXT("FCoverageUClassPlainScriptState"));
		UClass* DefaultDisplayNameClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDefaultDisplayNameObject"));
		UClass* ExplicitDisplayNameClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassExplicitDisplayNameObject"));
		ASSERT_THAT(IsNull(PlainScriptClass, TEXT("Script-only classes without UCLASS should not generate UClass reflection")));
		ASSERT_THAT(IsNotNull(DefaultDisplayNameClass, TEXT("Default display-name UCLASS should be generated")));
		ASSERT_THAT(IsNotNull(ExplicitDisplayNameClass, TEXT("Explicit display-name UCLASS should be generated")));
		if (DefaultDisplayNameClass == nullptr || ExplicitDisplayNameClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DefaultDisplayNameClass->HasMetaData(TEXT("DisplayName")), TEXT("Generated UCLASS should receive fallback DisplayName metadata")));
		ASSERT_THAT(IsFalse(DefaultDisplayNameClass->GetMetaData(TEXT("DisplayName")).IsEmpty(), TEXT("Fallback DisplayName metadata should not be empty")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Explicit Display Object")), ExplicitDisplayNameClass->GetMetaData(TEXT("DisplayName")), TEXT("Explicit DisplayName metadata should override fallback generation")));
	}

	TEST_METHOD(UClassInheritedFlagsAndConfig)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_InheritedFlagsAndConfig"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Transient, Deprecated, DefaultToInstanced, EditInlineNew, Config=Game, DefaultConfig)
			class UCoverageUClassInheritedFlagBaseObject : UObject
			{
				UPROPERTY(Config)
				int BaseConfigValue = 13;
			}

			UCLASS()
			class UCoverageUClassInheritedFlagChildObject : UCoverageUClassInheritedFlagBaseObject
			{
				UPROPERTY(Config)
				int ChildConfigValue = 17;
			}

			UCLASS(HideDropdown)
			class UCoverageUClassHiddenDropdownBaseObject : UObject
			{
			}

			UCLASS()
			class UCoverageUClassVisibleDropdownChildObject : UCoverageUClassHiddenDropdownBaseObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassInheritedFlagsAndConfig.as"), ScriptSource)));

		UClass* InheritedFlagBaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassInheritedFlagBaseObject"));
		UClass* InheritedFlagChildClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassInheritedFlagChildObject"));
		UClass* HiddenDropdownBaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassHiddenDropdownBaseObject"));
		UClass* VisibleDropdownChildClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassVisibleDropdownChildObject"));
		ASSERT_THAT(IsNotNull(InheritedFlagBaseClass, TEXT("Inherited flag base class should be generated")));
		ASSERT_THAT(IsNotNull(InheritedFlagChildClass, TEXT("Inherited flag child class should be generated")));
		ASSERT_THAT(IsNotNull(HiddenDropdownBaseClass, TEXT("HiddenDropdown base class should be generated")));
		ASSERT_THAT(IsNotNull(VisibleDropdownChildClass, TEXT("HiddenDropdown child class should be generated")));
		if (InheritedFlagBaseClass == nullptr || InheritedFlagChildClass == nullptr || HiddenDropdownBaseClass == nullptr || VisibleDropdownChildClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(InheritedFlagBaseClass->HasAnyClassFlags(CLASS_Config), TEXT("Config base should set CLASS_Config")));
		ASSERT_THAT(IsTrue(InheritedFlagBaseClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig base should set CLASS_DefaultConfig")));
		ASSERT_THAT(IsTrue(InheritedFlagBaseClass->HasAnyClassFlags(CLASS_Transient), TEXT("Transient base should set CLASS_Transient")));
		ASSERT_THAT(IsTrue(InheritedFlagBaseClass->HasAnyClassFlags(CLASS_Deprecated), TEXT("Deprecated base should set CLASS_Deprecated")));
		ASSERT_THAT(IsTrue(InheritedFlagBaseClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("DefaultToInstanced base should set CLASS_DefaultToInstanced")));
		ASSERT_THAT(IsTrue(InheritedFlagBaseClass->HasAnyClassFlags(CLASS_EditInlineNew), TEXT("EditInlineNew base should set CLASS_EditInlineNew")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), InheritedFlagBaseClass->ClassConfigName, TEXT("Config base should use Game config")));

		ASSERT_THAT(IsTrue(InheritedFlagChildClass->HasAnyClassFlags(CLASS_Config), TEXT("Config flag should be inherited by script subclasses")));
		ASSERT_THAT(IsFalse(InheritedFlagChildClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig should remain explicit to the declaring config class")));
		ASSERT_THAT(IsTrue(InheritedFlagChildClass->HasAnyClassFlags(CLASS_Transient), TEXT("Transient flag should be inherited by script subclasses")));
		ASSERT_THAT(IsTrue(InheritedFlagChildClass->HasAnyClassFlags(CLASS_Deprecated), TEXT("Deprecated flag should be inherited by script subclasses")));
		ASSERT_THAT(IsTrue(InheritedFlagChildClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("DefaultToInstanced flag should be inherited by script subclasses")));
		ASSERT_THAT(IsTrue(InheritedFlagChildClass->HasAnyClassFlags(CLASS_EditInlineNew), TEXT("EditInlineNew flag should be inherited by script subclasses")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), InheritedFlagChildClass->ClassConfigName, TEXT("Script subclass should inherit the parent ClassConfigName")));

		FProperty* ChildConfigProperty = InheritedFlagChildClass->FindPropertyByName(TEXT("ChildConfigValue"));
		ASSERT_THAT(IsNotNull(ChildConfigProperty, TEXT("Child config property should be generated")));
		if (ChildConfigProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildConfigProperty->HasAnyPropertyFlags(CPF_Config), TEXT("UPROPERTY(Config) should remain config in inherited config classes")));

		ASSERT_THAT(IsTrue(HiddenDropdownBaseClass->HasAnyClassFlags(CLASS_HideDropDown), TEXT("HideDropdown should set CLASS_HideDropDown on the declaring class")));
		ASSERT_THAT(IsFalse(VisibleDropdownChildClass->HasAnyClassFlags(CLASS_HideDropDown), TEXT("HideDropdown should not implicitly propagate to script subclasses")));
	}

	TEST_METHOD(UClassIgnoreCategoryKeywordsInSubclasses)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_IgnoreCategoryKeywordsInSubclasses"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(HideCategories="Rendering", meta=(AutoExpandCategories="Coverage", AutoCollapseCategories="Advanced", IgnoreCategoryKeywordsInSubclasses, HideFunctions="CoverageIgnoredFunction", SparseClassDataTypes="CoverageIgnoredSparseData"))
			class UCoverageUClassIgnoredCategoryBaseObject : UObject
			{
			}

			UCLASS()
			class UCoverageUClassIgnoredCategoryChildObject : UCoverageUClassIgnoredCategoryBaseObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassIgnoreCategoryKeywordsInSubclasses.as"), ScriptSource)));

		UClass* IgnoredCategoryBaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassIgnoredCategoryBaseObject"));
		UClass* IgnoredCategoryChildClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassIgnoredCategoryChildObject"));
		ASSERT_THAT(IsNotNull(IgnoredCategoryBaseClass, TEXT("Ignore-category base class should be generated")));
		ASSERT_THAT(IsNotNull(IgnoredCategoryChildClass, TEXT("Ignore-category child class should be generated")));
		if (IgnoredCategoryBaseClass == nullptr || IgnoredCategoryChildClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), IgnoredCategoryBaseClass->GetMetaData(TEXT("HideCategories")), TEXT("Base HideCategories metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage")), IgnoredCategoryBaseClass->GetMetaData(TEXT("AutoExpandCategories")), TEXT("Base AutoExpandCategories metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Advanced")), IgnoredCategoryBaseClass->GetMetaData(TEXT("AutoCollapseCategories")), TEXT("Base AutoCollapseCategories metadata should round-trip")));
		ASSERT_THAT(IsTrue(IgnoredCategoryBaseClass->HasMetaData(TEXT("IgnoreCategoryKeywordsInSubclasses")), TEXT("Base IgnoreCategoryKeywordsInSubclasses metadata should be present")));

		ASSERT_THAT(IsFalse(IgnoredCategoryChildClass->HasMetaData(TEXT("HideCategories")), TEXT("IgnoreCategoryKeywordsInSubclasses should stop HideCategories inheritance")));
		ASSERT_THAT(IsFalse(IgnoredCategoryChildClass->HasMetaData(TEXT("AutoExpandCategories")), TEXT("IgnoreCategoryKeywordsInSubclasses should stop AutoExpandCategories inheritance")));
		ASSERT_THAT(IsFalse(IgnoredCategoryChildClass->HasMetaData(TEXT("AutoCollapseCategories")), TEXT("IgnoreCategoryKeywordsInSubclasses should stop AutoCollapseCategories inheritance")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageIgnoredFunction")), IgnoredCategoryChildClass->GetMetaData(TEXT("HideFunctions")), TEXT("Non-category inherited metadata should still copy to subclasses")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageIgnoredSparseData")), IgnoredCategoryChildClass->GetMetaData(TEXT("SparseClassDataTypes")), TEXT("SparseClassDataTypes should still copy to subclasses")));
	}

	TEST_METHOD(UClassDefaultInheritancePropertySurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DefaultInheritancePropertySurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultBaseActor : AActor
			{
				default Tags.Add(n"BaseTag");
				default SetReplicates(false);

				UPROPERTY()
				int Health = 100;

				UPROPERTY()
				FName Label = n"Base";
			}

			UCLASS()
			class ACoverageUClassDefaultMidActor : ACoverageUClassDefaultBaseActor
			{
				default Health = 200;
				default Label = n"Mid";
				default Tags.Add(n"MidTag");
			}

			UCLASS()
			class ACoverageUClassDefaultLeafActor : ACoverageUClassDefaultMidActor
			{
				default Health = 300;
				default Tags.Add(n"LeafTag");
				default SetReplicates(true);

				UPROPERTY()
				TSubclassOf<AActor> ActorClass = ACoverageUClassDefaultBaseActor::StaticClass();

				UPROPERTY()
				ACoverageUClassDefaultBaseActor ActorRef;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultInheritancePropertySurface.as"), ScriptSource)));

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultBaseActor"));
		UClass* MidClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultMidActor"));
		UClass* LeafClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultLeafActor"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Default base actor class should be generated")));
		ASSERT_THAT(IsNotNull(MidClass, TEXT("Default mid actor class should be generated")));
		ASSERT_THAT(IsNotNull(LeafClass, TEXT("Default leaf actor class should be generated")));
		if (BaseClass == nullptr || MidClass == nullptr || LeafClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(BaseClass, MidClass->GetSuperClass(), TEXT("Mid class should inherit the script base class directly")));
		ASSERT_THAT(AreEqual(MidClass, LeafClass->GetSuperClass(), TEXT("Leaf class should inherit the script mid class directly")));

		AActor* BaseCDO = Cast<AActor>(BaseClass->GetDefaultObject());
		AActor* MidCDO = Cast<AActor>(MidClass->GetDefaultObject());
		AActor* LeafCDO = Cast<AActor>(LeafClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(BaseCDO, TEXT("Base actor CDO should be available")));
		ASSERT_THAT(IsNotNull(MidCDO, TEXT("Mid actor CDO should be available")));
		ASSERT_THAT(IsNotNull(LeafCDO, TEXT("Leaf actor CDO should be available")));
		if (BaseCDO == nullptr || MidCDO == nullptr || LeafCDO == nullptr)
		{
			return;
		}

		FIntProperty* LeafHealthProperty = FindFProperty<FIntProperty>(LeafClass, TEXT("Health"));
		FNameProperty* LeafLabelProperty = FindFProperty<FNameProperty>(LeafClass, TEXT("Label"));
		FClassProperty* ActorClassProperty = FindFProperty<FClassProperty>(LeafClass, TEXT("ActorClass"));
		FObjectProperty* ActorRefProperty = FindFProperty<FObjectProperty>(LeafClass, TEXT("ActorRef"));
		ASSERT_THAT(IsNotNull(LeafHealthProperty, TEXT("Inherited Health property should remain on the leaf class surface")));
		ASSERT_THAT(IsNotNull(LeafLabelProperty, TEXT("Inherited Label property should remain on the leaf class surface")));
		ASSERT_THAT(IsNotNull(ActorClassProperty, TEXT("TSubclassOf ActorClass property should be reflected")));
		ASSERT_THAT(IsNotNull(ActorRefProperty, TEXT("ActorRef property should be reflected")));
		if (LeafHealthProperty == nullptr || LeafLabelProperty == nullptr || ActorClassProperty == nullptr || ActorRefProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(300, LeafHealthProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("Leaf default should override inherited Health on the CDO")));
		ASSERT_THAT(AreEqual(FName(TEXT("Mid")), LeafLabelProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("Leaf CDO should inherit mid-level Label default")));
		ASSERT_THAT(IsTrue(ActorClassProperty->MetaClass != nullptr && ActorClassProperty->MetaClass->IsChildOf(AActor::StaticClass()), TEXT("TSubclassOf<AActor> should reflect an actor MetaClass")));
		ASSERT_THAT(AreEqual(BaseClass, ActorClassProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("TSubclassOf default should store the script base class")));
		ASSERT_THAT(AreEqual(BaseClass, ActorRefProperty->PropertyClass, TEXT("ActorRef should reflect the script base actor class")));

		ASSERT_THAT(IsFalse(BaseCDO->GetIsReplicated(), TEXT("Base default SetReplicates(false) should keep replication disabled")));
		ASSERT_THAT(IsFalse(MidCDO->GetIsReplicated(), TEXT("Mid class should inherit disabled replication from base defaults")));
		ASSERT_THAT(IsTrue(LeafCDO->GetIsReplicated(), TEXT("Leaf default SetReplicates(true) should override inherited replication state")));
		ASSERT_THAT(IsTrue(BaseCDO->Tags.Contains(FName(TEXT("BaseTag"))), TEXT("Base default Tags.Add should affect the base CDO")));
		ASSERT_THAT(IsTrue(MidCDO->Tags.Contains(FName(TEXT("BaseTag"))) && MidCDO->Tags.Contains(FName(TEXT("MidTag"))), TEXT("Mid CDO should accumulate base and mid default tags")));
		ASSERT_THAT(IsTrue(
			LeafCDO->Tags.Contains(FName(TEXT("BaseTag")))
			&& LeafCDO->Tags.Contains(FName(TEXT("MidTag")))
			&& LeafCDO->Tags.Contains(FName(TEXT("LeafTag"))),
			TEXT("Leaf CDO should accumulate default tags across the inheritance chain")));
	}

	TEST_METHOD(UClassAccessControlCompileFailures)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> PrivateAccessDiagnostics;
		PrivateAccessDiagnostics.Add(TEXT("Illegal access to private property 'SecretValue'"));

		const FString PrivateAccessSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPrivateAccessObject : UObject
			{
				private int SecretValue = 42;
			}

			int ReadPrivateAccess(UCoverageUClassPrivateAccessObject Object)
			{
				return Object.SecretValue;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_PrivateAccessFailure"),
			TEXT("ASCoverageUClassPrivateAccessFailure.as"),
			PrivateAccessSource,
			MakeArrayView(PrivateAccessDiagnostics))));

		TArray<FString> ProtectedAccessDiagnostics;
		ProtectedAccessDiagnostics.Add(TEXT("Illegal access to protected property 'ProtectedValue'"));

		const FString ProtectedAccessSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassProtectedAccessObject : UObject
			{
				protected int ProtectedValue = 23;
			}

			int ReadProtectedAccess(UCoverageUClassProtectedAccessObject Object)
			{
				return Object.ProtectedValue;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_ProtectedAccessFailure"),
			TEXT("ASCoverageUClassProtectedAccessFailure.as"),
			ProtectedAccessSource,
			MakeArrayView(ProtectedAccessDiagnostics))));

		TArray<FString> InheritedPrivateDiagnostics;
		InheritedPrivateDiagnostics.Add(TEXT("Illegal access to inherited private property 'BaseSecret'"));

		const FString InheritedPrivateSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPrivateBaseObject : UObject
			{
				private int BaseSecret = 17;
			}

			UCLASS()
			class UCoverageUClassPrivateDerivedObject : UCoverageUClassPrivateBaseObject
			{
				int ReadBaseSecret()
				{
					return BaseSecret;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_InheritedPrivateAccessFailure"),
			TEXT("ASCoverageUClassInheritedPrivateAccessFailure.as"),
			InheritedPrivateSource,
			MakeArrayView(InheritedPrivateDiagnostics))));
	}

	TEST_METHOD(UClassPrivateAllowPrivateAccessPropertyVisibility)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_PrivateAllowPrivateAccessPropertyVisibility"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPrivatePropertyVisibilityObject : UObject
			{
				UPROPERTY(BlueprintReadWrite, meta=(AllowPrivateAccess))
				private int AllowedPrivateValue = 37;

				UPROPERTY(BlueprintReadWrite)
				private int HiddenPrivateValue = 41;

				UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess))
				private int ReadOnlyPrivateValue = 43;

				UFUNCTION(BlueprintCallable)
				int ReadValues()
				{
					return AllowedPrivateValue + HiddenPrivateValue + ReadOnlyPrivateValue;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPrivateAllowPrivateAccessPropertyVisibility.as"), ScriptSource)));

		UClass* VisibilityClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPrivatePropertyVisibilityObject"));
		ASSERT_THAT(IsNotNull(VisibilityClass, TEXT("Private property visibility class should be generated")));
		if (VisibilityClass == nullptr)
		{
			return;
		}

		FProperty* AllowedPrivateProperty = VisibilityClass->FindPropertyByName(TEXT("AllowedPrivateValue"));
		FProperty* HiddenPrivateProperty = VisibilityClass->FindPropertyByName(TEXT("HiddenPrivateValue"));
		FProperty* ReadOnlyPrivateProperty = VisibilityClass->FindPropertyByName(TEXT("ReadOnlyPrivateValue"));
		ASSERT_THAT(IsNotNull(AllowedPrivateProperty, TEXT("AllowPrivateAccess private property should be generated")));
		ASSERT_THAT(IsNotNull(HiddenPrivateProperty, TEXT("Plain private property should still be generated")));
		ASSERT_THAT(IsNotNull(ReadOnlyPrivateProperty, TEXT("BlueprintReadOnly private property should be generated")));
		if (AllowedPrivateProperty == nullptr || HiddenPrivateProperty == nullptr || ReadOnlyPrivateProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AllowedPrivateProperty->HasMetaData(TEXT("AllowPrivateAccess")), TEXT("AllowPrivateAccess metadata should round-trip on private properties")));
		ASSERT_THAT(IsTrue(AllowedPrivateProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("AllowPrivateAccess should permit BlueprintReadWrite visibility on private properties")));
		ASSERT_THAT(IsFalse(AllowedPrivateProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadWrite private property should remain writable when AllowPrivateAccess is present")));

		ASSERT_THAT(IsFalse(HiddenPrivateProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("Private BlueprintReadWrite property without AllowPrivateAccess should not be Blueprint visible")));
		ASSERT_THAT(IsFalse(HiddenPrivateProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("Hidden private BlueprintReadWrite property should not become Blueprint read-only")));

		ASSERT_THAT(IsTrue(ReadOnlyPrivateProperty->HasMetaData(TEXT("AllowPrivateAccess")), TEXT("AllowPrivateAccess metadata should round-trip on read-only private properties")));
		ASSERT_THAT(IsTrue(ReadOnlyPrivateProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("AllowPrivateAccess should permit BlueprintReadOnly visibility on private properties")));
		ASSERT_THAT(IsTrue(ReadOnlyPrivateProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly private property should remain read-only when AllowPrivateAccess is present")));

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), VisibilityClass, TEXT("CoveragePrivatePropertyVisibilityObject"), RF_Transient);
		ASSERT_THAT(IsNotNull(Instance, TEXT("Private property visibility object should instantiate")));
		if (Instance == nullptr)
		{
			return;
		}

		FFunctionInvoker ReadValuesInvoker(*TestRunner, Instance, TEXT("ReadValues"));
		ASSERT_THAT(IsTrue(ReadValuesInvoker.IsValid(), TEXT("ReadValues should be invokable on the private property visibility object")));
		if (!ReadValuesInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(121, ReadValuesInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Private values should remain script-readable inside the declaring class")));
	}

	TEST_METHOD(UClassAbstractActorSpawnIsRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_AbstractActorSpawnIsRejected"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Abstract)
			class ACoverageUClassUnspawnableAbstractActor : AActor
			{
				UPROPERTY()
				int AbstractValue = 19;
			}

			UCLASS()
			class ACoverageUClassSpawnableConcreteActor : ACoverageUClassUnspawnableAbstractActor
			{
				UPROPERTY()
				int ConcreteValue = 23;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassAbstractActorSpawnIsRejected.as"), ScriptSource)));

		UClass* AbstractActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassUnspawnableAbstractActor"));
		UClass* ConcreteActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassSpawnableConcreteActor"));
		ASSERT_THAT(IsNotNull(AbstractActorClass, TEXT("Abstract actor class should be generated")));
		ASSERT_THAT(IsNotNull(ConcreteActorClass, TEXT("Concrete actor class should be generated")));
		if (AbstractActorClass == nullptr || ConcreteActorClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AbstractActorClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Abstract actor class should carry CLASS_Abstract")));
		ASSERT_THAT(IsFalse(ConcreteActorClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Concrete actor subclass should not carry CLASS_Abstract")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TestRunner->AddExpectedError(TEXT("SpawnActor failed because class"), EAutomationExpectedErrorFlags::Contains, 1);
		AActor* AbstractActor = Spawner.GetWorld().SpawnActor<AActor>(AbstractActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		ASSERT_THAT(IsNull(AbstractActor, TEXT("SpawnActor should reject abstract script actor classes")));

		AActor* ConcreteActor = Spawner.GetWorld().SpawnActor<AActor>(ConcreteActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		ASSERT_THAT(IsNotNull(ConcreteActor, TEXT("Concrete subclass of an abstract script actor should spawn")));
		if (ConcreteActor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ConcreteActor, TEXT("AbstractValue"), 19, TEXT("Concrete actor should inherit abstract base defaults"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ConcreteActor, TEXT("ConcreteValue"), 23, TEXT("Concrete actor should retain its own defaults"))));
	}

	TEST_METHOD(UClassHUDDrawHUDReflectionDispatchBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_HUDDrawHUDReflectionDispatchBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDispatchHUD : AHUD
			{
				UPROPERTY()
				int DrawHUDCount = 0;

				UPROPERTY()
				int DrawHUDMarker = 0;

				UPROPERTY()
				int LastDrawHUDSizeX = 0;

				UPROPERTY()
				int LastDrawHUDSizeY = 0;

				UFUNCTION(BlueprintOverride)
				void DrawHUD(int SizeX, int SizeY)
				{
					DrawHUDCount++;
					DrawHUDMarker = 77;
					LastDrawHUDSizeX = SizeX;
					LastDrawHUDSizeY = SizeY;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassHUDDrawHUDReflectionDispatchBoundary.as"), ScriptSource)));

		UClass* HUDClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDispatchHUD"));
		ASSERT_THAT(IsNotNull(HUDClass, TEXT("HUD DrawHUD class should be generated")));
		if (HUDClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HUDClass->IsChildOf(AHUD::StaticClass()), TEXT("HUD DrawHUD class should inherit AHUD")));

		UFunction* DrawHUDFunction = HUDClass->FindFunctionByName(TEXT("ReceiveDrawHUD"));
		ASSERT_THAT(IsNotNull(DrawHUDFunction, TEXT("DrawHUD override should reflect through the ReceiveDrawHUD event boundary")));
		if (DrawHUDFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DrawHUDFunction->HasAnyFunctionFlags(FUNC_Event), TEXT("ReceiveDrawHUD should remain an event function")));
		ASSERT_THAT(IsTrue(DrawHUDFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent), TEXT("ReceiveDrawHUD should remain a Blueprint event function")));
		ASSERT_THAT(IsNull(DrawHUDFunction->GetReturnProperty(), TEXT("ReceiveDrawHUD should remain void")));
		FIntProperty* SizeXProperty = FindFProperty<FIntProperty>(DrawHUDFunction, TEXT("SizeX"));
		FIntProperty* SizeYProperty = FindFProperty<FIntProperty>(DrawHUDFunction, TEXT("SizeY"));
		ASSERT_THAT(IsNotNull(SizeXProperty, TEXT("ReceiveDrawHUD should expose SizeX as an int parameter")));
		ASSERT_THAT(IsNotNull(SizeYProperty, TEXT("ReceiveDrawHUD should expose SizeY as an int parameter")));
		if (SizeXProperty == nullptr || SizeYProperty == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, HUDClass);
		AHUD* HUD = Cast<AHUD>(Actor);
		ASSERT_THAT(IsNotNull(HUD, TEXT("Generated HUD should spawn as AHUD")));
		if (HUD == nullptr)
		{
			return;
		}

		{
			FAngelscriptEngineScope HUDScope(Engine, HUD);
			HUD->ReceiveDrawHUD(640, 360);
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, HUD, TEXT("DrawHUDCount"), 1, TEXT("ReceiveDrawHUD should dispatch to the AS DrawHUD override once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, HUD, TEXT("DrawHUDMarker"), 77, TEXT("AS DrawHUD override should update HUD state through reflected dispatch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, HUD, TEXT("LastDrawHUDSizeX"), 640, TEXT("ReceiveDrawHUD should pass SizeX through the AS DrawHUD boundary"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, HUD, TEXT("LastDrawHUDSizeY"), 360, TEXT("ReceiveDrawHUD should pass SizeY through the AS DrawHUD boundary"))));
	}

	TEST_METHOD(UClassAbstractInheritanceAndCastingRuntime)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_AbstractInheritanceCastingRuntime"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Abstract, Blueprintable)
			class ACoverageUClassRuntimeAbstractBase : AActor
			{
				UPROPERTY()
				int CallChain = 0;

				void ApplyStep()
				{
					CallChain = CallChain * 10 + 1;
				}
			}

			UCLASS()
			class ACoverageUClassRuntimeMid : ACoverageUClassRuntimeAbstractBase
			{
				void ApplyStep()
				{
					Super::ApplyStep();
					CallChain = CallChain * 10 + 2;
				}
			}

			UCLASS()
			class ACoverageUClassRuntimeLeaf : ACoverageUClassRuntimeMid
			{
				UPROPERTY()
				int UpcastWorked = 0;

				UPROPERTY()
				int DowncastWorked = 0;

				UPROPERTY()
				int InvalidCastFailed = 0;

				void ApplyStep()
				{
					Super::ApplyStep();
					CallChain = CallChain * 10 + 3;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ApplyStep();

					ACoverageUClassRuntimeAbstractBase BaseRef = this;
					if (BaseRef != nullptr && BaseRef.CallChain == 123)
					{
						UpcastWorked = 1;
					}

					ACoverageUClassRuntimeLeaf LeafRef = Cast<ACoverageUClassRuntimeLeaf>(BaseRef);
					if (LeafRef != nullptr)
					{
						DowncastWorked = 1;
					}

					ACoverageUClassRuntimeMid SpawnedMid = Cast<ACoverageUClassRuntimeMid>(SpawnActor(ACoverageUClassRuntimeMid::StaticClass()));
					ACoverageUClassRuntimeLeaf InvalidLeaf = Cast<ACoverageUClassRuntimeLeaf>(SpawnedMid);
					if (SpawnedMid != nullptr && InvalidLeaf == nullptr)
					{
						InvalidCastFailed = 1;
					}
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassAbstractInheritanceCastingRuntime.as"), ScriptSource)));

		UClass* AbstractBaseClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassRuntimeAbstractBase"));
		UClass* MidClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassRuntimeMid"));
		UClass* LeafClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassRuntimeLeaf"));
		ASSERT_THAT(IsNotNull(AbstractBaseClass, TEXT("Runtime abstract base class should be generated")));
		ASSERT_THAT(IsNotNull(MidClass, TEXT("Runtime mid class should be generated")));
		ASSERT_THAT(IsNotNull(LeafClass, TEXT("Runtime leaf class should be generated")));
		if (AbstractBaseClass == nullptr || MidClass == nullptr || LeafClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AbstractBaseClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Abstract runtime base should carry CLASS_Abstract")));
		ASSERT_THAT(IsFalse(MidClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Concrete runtime mid class should be spawnable")));
		ASSERT_THAT(IsFalse(LeafClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Concrete runtime leaf class should be spawnable")));
		ASSERT_THAT(IsTrue(MidClass->IsChildOf(AbstractBaseClass), TEXT("Mid class should inherit the abstract base")));
		ASSERT_THAT(IsTrue(LeafClass->IsChildOf(MidClass), TEXT("Leaf class should inherit the mid class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, LeafClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Runtime leaf actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallChain"), 123, TEXT("Super:: method chain should execute base, mid, and leaf methods"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("UpcastWorked"), 1, TEXT("Implicit upcast to abstract base should preserve state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DowncastWorked"), 1, TEXT("Cast from base reference back to leaf should succeed"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InvalidCastFailed"), 1, TEXT("Cast from mid instance to unrelated leaf instance should fail"))));
	}

	TEST_METHOD(UClassUObjectDefaultObjectAndMethodDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_UObjectDefaultObjectAndMethodDispatch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(BlueprintType)
			class UCoverageUClassPlainDataObject : UObject
			{
				UPROPERTY()
				int Counter = 12;

				UPROPERTY()
				FString Label = "Seed";

				UFUNCTION()
				int AddCounter(int Value)
				{
					Counter += Value;
					return Counter;
				}

				UFUNCTION()
				FString BuildLabel(const FString&in Suffix)
				{
					return Label + "_" + Suffix;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassUObjectDefaultObjectAndMethodDispatch.as"), ScriptSource)));

		UClass* DataObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPlainDataObject"));
		ASSERT_THAT(IsNotNull(DataObjectClass, TEXT("Plain UObject UCLASS should be generated")));
		if (DataObjectClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DataObjectClass->IsChildOf(UObject::StaticClass()), TEXT("Plain data UCLASS should inherit UObject")));
		ASSERT_THAT(IsFalse(DataObjectClass->IsChildOf(AActor::StaticClass()), TEXT("Plain data UCLASS should not be actor-derived")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), DataObjectClass->GetMetaData(TEXT("BlueprintType")), TEXT("BlueprintType should round-trip on pure UObject classes")));

		UObject* DefaultObject = DataObjectClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("Plain UObject UCLASS should expose a CDO")));
		if (DefaultObject == nullptr)
		{
			return;
		}

		FIntProperty* CounterProperty = FindFProperty<FIntProperty>(DataObjectClass, TEXT("Counter"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(DataObjectClass, TEXT("Label"));
		ASSERT_THAT(IsNotNull(CounterProperty, TEXT("Counter property should be reflected on the UObject class")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("Label property should be reflected on the UObject class")));
		if (CounterProperty == nullptr || LabelProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(12, CounterProperty->GetPropertyValue_InContainer(DefaultObject), TEXT("Counter default should propagate to the UObject CDO")));
		ASSERT_THAT(AreEqual(FString(TEXT("Seed")), LabelProperty->GetPropertyValue_InContainer(DefaultObject), TEXT("Label default should propagate to the UObject CDO")));

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), DataObjectClass, TEXT("CoverageUClassPlainDataObject"), RF_Transient);
		ASSERT_THAT(IsNotNull(Instance, TEXT("Plain UObject UCLASS should instantiate through NewObject")));
		if (Instance == nullptr)
		{
			return;
		}

		FFunctionInvoker AddCounterInvoker(*TestRunner, Instance, TEXT("AddCounter"));
		ASSERT_THAT(IsTrue(AddCounterInvoker.IsValid(), TEXT("AddCounter should be invokable on the UObject instance")));
		if (!AddCounterInvoker.IsValid())
		{
			return;
		}
		AddCounterInvoker.AddParam<int32>(8);
		ASSERT_THAT(AreEqual(20, AddCounterInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("UObject UFUNCTION should mutate and return reflected state")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Instance, TEXT("Counter"), 20, TEXT("UObject UFUNCTION should persist the mutated counter"))));

		FFunctionInvoker BuildLabelInvoker(*TestRunner, Instance, TEXT("BuildLabel"));
		ASSERT_THAT(IsTrue(BuildLabelInvoker.IsValid(), TEXT("BuildLabel should be invokable on the UObject instance")));
		if (!BuildLabelInvoker.IsValid())
		{
			return;
		}
		BuildLabelInvoker.AddParam<FString>(FString(TEXT("Done")));
		ASSERT_THAT(AreEqual(FString(TEXT("Seed_Done")), BuildLabelInvoker.CallAndReturn<FString>(FString()), TEXT("UObject FString return should round-trip through reflection")));
	}

	TEST_METHOD(UClassDefaultComponentTreeAndReferenceSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DefaultComponentTreeAndReferenceSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassReferenceObject : UObject
			{
				UPROPERTY()
				int ObjectValue = 31;

				UFUNCTION()
				int ReadValue()
				{
					return ObjectValue;
				}
			}

			UCLASS()
			class UCoverageUClassReferenceComponent : UActorComponent
			{
				UPROPERTY()
				int ComponentValue = 41;

				UFUNCTION()
				int Multiply(int Factor)
				{
					return ComponentValue * Factor;
				}
			}

			UCLASS()
			class UCoverageUClassReferenceSceneComponent : USceneComponent
			{
				UPROPERTY()
				int SceneValue = 53;

				UFUNCTION()
				int ReadSceneValue()
				{
					return SceneValue;
				}
			}

			UCLASS()
			class ACoverageUClassReferenceActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="CoverageSocket")
				USceneComponent Child;

				UPROPERTY(DefaultComponent, Attach=Child, ShowOnActor, EditAnywhere, BlueprintReadOnly)
				UCoverageUClassReferenceSceneComponent VisibleGrandchild;

				UPROPERTY(DefaultComponent)
				UCoverageUClassReferenceComponent Logic;

				UPROPERTY()
				UCoverageUClassReferenceObject MemberObject;

				UPROPERTY()
				AActor ActorRef;

				UPROPERTY()
				UCoverageUClassReferenceComponent ComponentRef;

				UPROPERTY()
				TSubclassOf<AActor> ActorClass;

				UPROPERTY()
				bool RootValid = false;

				UPROPERTY()
				bool ChildAttached = false;

				UPROPERTY()
				bool GrandchildAttached = false;

				UPROPERTY()
				bool GrandchildReadable = false;

				UPROPERTY()
				bool LogicValid = false;

				UPROPERTY()
				bool ObjectReferenceValid = false;

				UPROPERTY()
				bool ActorReferenceValid = false;

				UPROPERTY()
				bool ComponentReferenceValid = false;

				UPROPERTY()
				bool ClassReferenceValid = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MemberObject = Cast<UCoverageUClassReferenceObject>(
						NewObject(this, UCoverageUClassReferenceObject::StaticClass(), n"CoverageUClassReferenceObject"));
					ActorRef = this;
					ComponentRef = Logic;
					ActorClass = ACoverageUClassReferenceActor::StaticClass();

					RootValid = Root != nullptr && Root.GetOwner() == this;
					ChildAttached = Child != nullptr && Root != nullptr &&
						Child.GetAttachParent() == Root &&
						Child.GetAttachSocketName() == n"CoverageSocket";
					GrandchildAttached = VisibleGrandchild != nullptr && Child != nullptr &&
						VisibleGrandchild.GetAttachParent() == Child;
					GrandchildReadable = VisibleGrandchild != nullptr &&
						VisibleGrandchild.GetOwner() == this &&
						VisibleGrandchild.ReadSceneValue() == 53;
					LogicValid = Logic != nullptr &&
						Logic.GetOwner() == this &&
						Logic.ComponentValue == 41;
					ObjectReferenceValid = MemberObject != nullptr &&
						MemberObject.GetOuter() == this &&
						MemberObject.ReadValue() == 31;
					ActorReferenceValid = ActorRef == this;
					ComponentReferenceValid = ComponentRef == Logic &&
						ComponentRef.Multiply(2) == 82;
					ClassReferenceValid = ActorClass.Get() == ACoverageUClassReferenceActor::StaticClass() &&
						ActorClass.IsChildOf(AActor::StaticClass());
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentTreeAndReferenceSurface.as"), ScriptSource)));

		UClass* ReferenceObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassReferenceObject"));
		UClass* ReferenceComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassReferenceComponent"));
		UClass* ReferenceSceneComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassReferenceSceneComponent"));
		UClass* ReferenceActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassReferenceActor"));
		ASSERT_THAT(IsNotNull(ReferenceObjectClass, TEXT("Referenced UObject class should be generated")));
		ASSERT_THAT(IsNotNull(ReferenceComponentClass, TEXT("Referenced component class should be generated")));
		ASSERT_THAT(IsNotNull(ReferenceSceneComponentClass, TEXT("Referenced scene component class should be generated")));
		ASSERT_THAT(IsNotNull(ReferenceActorClass, TEXT("Reference actor class should be generated")));
		if (ReferenceObjectClass == nullptr || ReferenceComponentClass == nullptr || ReferenceSceneComponentClass == nullptr || ReferenceActorClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReferenceObjectClass->IsChildOf(UObject::StaticClass()), TEXT("Referenced object should inherit UObject")));
		ASSERT_THAT(IsTrue(ReferenceComponentClass->IsChildOf(UActorComponent::StaticClass()), TEXT("Referenced component should inherit UActorComponent")));
		ASSERT_THAT(IsTrue(ReferenceSceneComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Referenced scene component should inherit USceneComponent")));
		ASSERT_THAT(IsTrue(ReferenceActorClass->IsChildOf(AActor::StaticClass()), TEXT("Reference actor should inherit AActor")));

		FObjectPropertyBase* RootProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("Root"));
		FObjectPropertyBase* ChildProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("Child"));
		FObjectPropertyBase* VisibleGrandchildProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("VisibleGrandchild"));
		FObjectPropertyBase* LogicProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("Logic"));
		FObjectPropertyBase* MemberObjectProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("MemberObject"));
		FObjectPropertyBase* ActorRefProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("ActorRef"));
		FObjectPropertyBase* ComponentRefProperty = FindFProperty<FObjectPropertyBase>(ReferenceActorClass, TEXT("ComponentRef"));
		FClassProperty* ActorClassProperty = FindFProperty<FClassProperty>(ReferenceActorClass, TEXT("ActorClass"));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Root DefaultComponent property should be reflected")));
		ASSERT_THAT(IsNotNull(ChildProperty, TEXT("Child DefaultComponent property should be reflected")));
		ASSERT_THAT(IsNotNull(VisibleGrandchildProperty, TEXT("VisibleGrandchild DefaultComponent property should be reflected")));
		ASSERT_THAT(IsNotNull(LogicProperty, TEXT("Logic DefaultComponent property should be reflected")));
		ASSERT_THAT(IsNotNull(MemberObjectProperty, TEXT("MemberObject reference property should be reflected")));
		ASSERT_THAT(IsNotNull(ActorRefProperty, TEXT("ActorRef property should be reflected")));
		ASSERT_THAT(IsNotNull(ComponentRefProperty, TEXT("ComponentRef property should be reflected")));
		ASSERT_THAT(IsNotNull(ActorClassProperty, TEXT("ActorClass TSubclassOf property should be reflected")));
		if (RootProperty == nullptr || ChildProperty == nullptr || VisibleGrandchildProperty == nullptr || LogicProperty == nullptr
			|| MemberObjectProperty == nullptr || ActorRefProperty == nullptr || ComponentRefProperty == nullptr || ActorClassProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(RootProperty->PropertyClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Root should be a scene component property")));
		ASSERT_THAT(IsTrue(ChildProperty->PropertyClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Child should be a scene component property")));
		ASSERT_THAT(AreEqual(ReferenceSceneComponentClass, VisibleGrandchildProperty->PropertyClass, TEXT("VisibleGrandchild should preserve the script scene component property class")));
		ASSERT_THAT(IsTrue(VisibleGrandchildProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("ShowOnActor + EditAnywhere should make VisibleGrandchild editable")));
		ASSERT_THAT(IsTrue(VisibleGrandchildProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("ShowOnActor + BlueprintReadOnly should keep VisibleGrandchild Blueprint-visible")));
		ASSERT_THAT(IsTrue(VisibleGrandchildProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly should mark VisibleGrandchild read-only")));
		ASSERT_THAT(IsTrue(VisibleGrandchildProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should add EditInline metadata to VisibleGrandchild")));
		ASSERT_THAT(AreEqual(FString(TEXT("Child")), VisibleGrandchildProperty->GetMetaData(TEXT("Attach")), TEXT("VisibleGrandchild should attach to the Child default component")));
		ASSERT_THAT(AreEqual(ReferenceComponentClass, LogicProperty->PropertyClass, TEXT("Logic should preserve the script component property class")));
		ASSERT_THAT(AreEqual(ReferenceObjectClass, MemberObjectProperty->PropertyClass, TEXT("MemberObject should preserve the script UObject property class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorRefProperty->PropertyClass, TEXT("ActorRef should target AActor")));
		ASSERT_THAT(AreEqual(ReferenceComponentClass, ComponentRefProperty->PropertyClass, TEXT("ComponentRef should target the script component class")));
		ASSERT_THAT(IsTrue(ActorClassProperty->MetaClass != nullptr && ActorClassProperty->MetaClass->IsChildOf(AActor::StaticClass()), TEXT("ActorClass should constrain TSubclassOf to actors")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ReferenceActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Reference actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootValid"), true, TEXT("Root DefaultComponent should be created and owned by the actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildAttached"), true, TEXT("Attach and AttachSocket should materialize on the child component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GrandchildAttached"), true, TEXT("Nested Attach should materialize on the script scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("GrandchildReadable"), true, TEXT("Script scene component method should be callable from the owning actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LogicValid"), true, TEXT("Script component DefaultComponent should be created and owned by the actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectReferenceValid"), true, TEXT("UObject member reference should hold a NewObject instance"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorReferenceValid"), true, TEXT("Actor member reference should assign this"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentReferenceValid"), true, TEXT("Component member reference should point at the script component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ClassReferenceValid"), true, TEXT("TSubclassOf member should hold the generated actor class"))));

		UObject* RootObject = nullptr;
		UObject* ChildObject = nullptr;
		UObject* VisibleGrandchildObject = nullptr;
		UObject* LogicObject = nullptr;
		UObject* MemberObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Root"), RootObject), TEXT("Root property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Child"), ChildObject), TEXT("Child property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("VisibleGrandchild"), VisibleGrandchildObject), TEXT("VisibleGrandchild property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Logic"), LogicObject), TEXT("Logic property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("MemberObject"), MemberObject), TEXT("MemberObject property should be readable")));

		USceneComponent* RootComponent = Cast<USceneComponent>(RootObject);
		USceneComponent* ChildComponent = Cast<USceneComponent>(ChildObject);
		USceneComponent* VisibleGrandchildComponent = Cast<USceneComponent>(VisibleGrandchildObject);
		UActorComponent* LogicComponent = Cast<UActorComponent>(LogicObject);
		ASSERT_THAT(IsNotNull(RootComponent, TEXT("Root property should store a USceneComponent instance")));
		ASSERT_THAT(IsNotNull(ChildComponent, TEXT("Child property should store a USceneComponent instance")));
		ASSERT_THAT(IsNotNull(VisibleGrandchildComponent, TEXT("VisibleGrandchild property should store a script USceneComponent instance")));
		ASSERT_THAT(IsNotNull(LogicComponent, TEXT("Logic property should store a UActorComponent instance")));
		ASSERT_THAT(IsNotNull(MemberObject, TEXT("MemberObject property should store a UObject instance")));
		if (RootComponent == nullptr || ChildComponent == nullptr || VisibleGrandchildComponent == nullptr || LogicComponent == nullptr || MemberObject == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor->GetRootComponent()), static_cast<UObject*>(RootComponent), TEXT("RootComponent specifier should assign the reflected Root as actor root")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(RootComponent), static_cast<UObject*>(ChildComponent->GetAttachParent()), TEXT("Attach specifier should attach Child to Root")));
		ASSERT_THAT(AreEqual(FName(TEXT("CoverageSocket")), ChildComponent->GetAttachSocketName(), TEXT("AttachSocket should persist on the runtime scene attachment")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(ChildComponent), static_cast<UObject*>(VisibleGrandchildComponent->GetAttachParent()), TEXT("Nested Attach specifier should attach VisibleGrandchild to Child")));
		ASSERT_THAT(AreEqual(Actor, LogicComponent->GetOwner(), TEXT("Script component DefaultComponent should be owned by the actor")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), MemberObject->GetOuter(), TEXT("UObject member reference should preserve the actor Outer")));
		ASSERT_THAT(AreEqual(4, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Actor should own exactly the four declared default components")));
	}

	TEST_METHOD(UClassComponentRuntimeOperationSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_ComponentRuntimeOperationSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassRuntimeSceneComponent : USceneComponent
			{
				UPROPERTY()
				int Marker = 17;
			}

			UCLASS()
			class UCoverageUClassRuntimeLogicComponent : UActorComponent
			{
				UPROPERTY()
				int Marker = 23;
			}

			UCLASS()
			class ACoverageUClassComponentRuntimeActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCoverageUClassRuntimeSceneComponent DefaultScene;

				UPROPERTY(DefaultComponent)
				UCoverageUClassRuntimeLogicComponent DefaultLogic;

				UPROPERTY()
				UCoverageUClassRuntimeSceneComponent RuntimeScene;

				UPROPERTY()
				UCoverageUClassRuntimeLogicComponent RuntimeLogic;

				UPROPERTY()
				bool DefaultSceneFoundByClass = false;

				UPROPERTY()
				bool DefaultLogicFoundByClass = false;

				UPROPERTY()
				bool RuntimeSceneCreated = false;

				UPROPERTY()
				bool RuntimeLogicCreated = false;

				UPROPERTY()
				bool RuntimeSceneInitiallyDetached = false;

				UPROPERTY()
				bool RuntimeSceneAttached = false;

				UPROPERTY()
				bool RuntimeSceneDetached = false;

				UPROPERTY()
				bool RuntimeSceneRegistered = false;

				UPROPERTY()
				bool RuntimeSceneFoundByClass = false;

				UPROPERTY()
				bool RuntimeSceneIncludedInAllComponents = false;

				UPROPERTY()
				bool RuntimeLogicRegistered = false;

				UPROPERTY()
				bool RuntimeLogicFoundByClass = false;

				UPROPERTY()
				bool RuntimeLogicIncludedInAllComponents = false;

				UPROPERTY()
				bool RuntimeSceneDestroyed = false;

				UPROPERTY()
				bool RuntimeLogicDestroyed = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DefaultSceneFoundByClass = Cast<UCoverageUClassRuntimeSceneComponent>(
						GetComponentByClass(UCoverageUClassRuntimeSceneComponent::StaticClass())) == DefaultScene;
					DefaultLogicFoundByClass = Cast<UCoverageUClassRuntimeLogicComponent>(
						GetComponentByClass(UCoverageUClassRuntimeLogicComponent::StaticClass())) == DefaultLogic;

					RuntimeScene = Cast<UCoverageUClassRuntimeSceneComponent>(
						NewObject(this, UCoverageUClassRuntimeSceneComponent::StaticClass(), n"CoverageRuntimeScene", true));
					RuntimeLogic = Cast<UCoverageUClassRuntimeLogicComponent>(
						NewObject(this, UCoverageUClassRuntimeLogicComponent::StaticClass(), n"CoverageRuntimeLogic", true));
					RuntimeSceneCreated = RuntimeScene != nullptr && RuntimeScene.GetOwner() == this && RuntimeScene.GetWorld() == GetWorld();
					RuntimeLogicCreated = RuntimeLogic != nullptr && RuntimeLogic.GetOwner() == this && RuntimeLogic.GetWorld() == GetWorld();
					if (RuntimeScene == nullptr || RuntimeLogic == nullptr)
					{
						return;
					}

					RuntimeSceneInitiallyDetached = !RuntimeScene.IsAttachedTo(Root);
					RuntimeScene.AttachToComponent(Root, n"RuntimeSocket",
						EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					RuntimeSceneAttached = RuntimeScene.IsAttachedTo(Root) &&
						RuntimeScene.GetAttachSocketName() == n"RuntimeSocket";

					RuntimeScene.DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
					RuntimeSceneDetached = !RuntimeScene.IsAttachedTo(Root);

					RuntimeScene.AttachToComponent(Root, NAME_None,
						EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					RuntimeScene.RegisterComponent();
					RuntimeSceneRegistered = RuntimeScene.IsRegistered();
					RuntimeSceneFoundByClass = Cast<UCoverageUClassRuntimeSceneComponent>(
						FindComponentByClass(UCoverageUClassRuntimeSceneComponent::StaticClass())) != nullptr;

					TArray<UActorComponent> RuntimeSceneComponents;
					GetComponentsByClass(UCoverageUClassRuntimeSceneComponent::StaticClass(), RuntimeSceneComponents);
					RuntimeSceneIncludedInAllComponents = RuntimeSceneComponents.Contains(RuntimeScene);

					RuntimeLogic.RegisterComponent();
					RuntimeLogicRegistered = RuntimeLogic.IsRegistered();
					RuntimeLogicFoundByClass = Cast<UCoverageUClassRuntimeLogicComponent>(
						FindComponentByClass(UCoverageUClassRuntimeLogicComponent::StaticClass())) != nullptr;

					TArray<UActorComponent> RuntimeLogicComponents;
					GetComponentsByClass(UCoverageUClassRuntimeLogicComponent::StaticClass(), RuntimeLogicComponents);
					RuntimeLogicIncludedInAllComponents = RuntimeLogicComponents.Contains(RuntimeLogic);

					RuntimeScene.DestroyComponent();
					RuntimeLogic.DestroyComponent();
					RuntimeSceneDestroyed = RuntimeScene.IsBeingDestroyed();
					RuntimeLogicDestroyed = RuntimeLogic.IsBeingDestroyed();
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassComponentRuntimeOperationSurface.as"), ScriptSource)));

		UClass* RuntimeSceneClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassRuntimeSceneComponent"));
		UClass* RuntimeLogicClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassRuntimeLogicComponent"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassComponentRuntimeActor"));
		ASSERT_THAT(IsNotNull(RuntimeSceneClass, TEXT("Runtime scene component class should be generated")));
		ASSERT_THAT(IsNotNull(RuntimeLogicClass, TEXT("Runtime logic component class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Component runtime operation actor should be generated")));
		if (RuntimeSceneClass == nullptr || RuntimeLogicClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(RuntimeSceneClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Runtime scene component class should inherit USceneComponent")));
		ASSERT_THAT(IsTrue(RuntimeLogicClass->IsChildOf(UActorComponent::StaticClass()), TEXT("Runtime logic component class should inherit UActorComponent")));

		FObjectPropertyBase* RuntimeSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("RuntimeScene"));
		FObjectPropertyBase* RuntimeLogicProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("RuntimeLogic"));
		ASSERT_THAT(IsNotNull(RuntimeSceneProperty, TEXT("RuntimeScene property should be reflected")));
		ASSERT_THAT(IsNotNull(RuntimeLogicProperty, TEXT("RuntimeLogic property should be reflected")));
		if (RuntimeSceneProperty == nullptr || RuntimeLogicProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(RuntimeSceneClass, RuntimeSceneProperty->PropertyClass, TEXT("RuntimeScene should preserve the script scene component class")));
		ASSERT_THAT(AreEqual(RuntimeLogicClass, RuntimeLogicProperty->PropertyClass, TEXT("RuntimeLogic should preserve the script logic component class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component runtime operation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultSceneFoundByClass"), true, TEXT("GetComponentByClass should find the default scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultLogicFoundByClass"), true, TEXT("GetComponentByClass should find the default logic component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneCreated"), true, TEXT("NewObject should create an actor-owned scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeLogicCreated"), true, TEXT("NewObject should create an actor-owned logic component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneInitiallyDetached"), true, TEXT("NewObject scene component should start detached"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneAttached"), true, TEXT("AttachToComponent should attach the runtime scene component with a socket"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneDetached"), true, TEXT("DetachFromComponent should detach the runtime scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneRegistered"), true, TEXT("RegisterComponent should register the runtime scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneFoundByClass"), true, TEXT("FindComponentByClass should find the registered runtime scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneIncludedInAllComponents"), true, TEXT("GetComponentsByClass should include the registered runtime scene component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeLogicRegistered"), true, TEXT("RegisterComponent should register the runtime logic component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeLogicFoundByClass"), true, TEXT("FindComponentByClass should find the registered runtime logic component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeLogicIncludedInAllComponents"), true, TEXT("GetComponentsByClass should include the registered runtime logic component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeSceneDestroyed"), true, TEXT("DestroyComponent should mark the runtime scene component as destroying"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RuntimeLogicDestroyed"), true, TEXT("DestroyComponent should mark the runtime logic component as destroying"))));

		UObject* RuntimeSceneObject = nullptr;
		UObject* RuntimeLogicObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("RuntimeScene"), RuntimeSceneObject), TEXT("RuntimeScene property should be readable after BeginPlay")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("RuntimeLogic"), RuntimeLogicObject), TEXT("RuntimeLogic property should be readable after BeginPlay")));
		UActorComponent* RuntimeSceneComponent = Cast<UActorComponent>(RuntimeSceneObject);
		UActorComponent* RuntimeLogicComponent = Cast<UActorComponent>(RuntimeLogicObject);
		ASSERT_THAT(IsNotNull(RuntimeSceneComponent, TEXT("RuntimeScene should store a runtime component instance")));
		ASSERT_THAT(IsNotNull(RuntimeLogicComponent, TEXT("RuntimeLogic should store a runtime component instance")));
		if (RuntimeSceneComponent == nullptr || RuntimeLogicComponent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(Actor, RuntimeSceneComponent->GetOwner(), TEXT("RuntimeScene should keep the spawned actor as owner")));
		ASSERT_THAT(AreEqual(Actor, RuntimeLogicComponent->GetOwner(), TEXT("RuntimeLogic should keep the spawned actor as owner")));
		ASSERT_THAT(IsTrue(RuntimeSceneComponent->IsBeingDestroyed(), TEXT("RuntimeScene native component should be marked destroying after DestroyComponent")));
		ASSERT_THAT(IsTrue(RuntimeLogicComponent->IsBeingDestroyed(), TEXT("RuntimeLogic native component should be marked destroying after DestroyComponent")));
	}

	TEST_METHOD(UClassCommonLifecycleFunctionSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_CommonLifecycleFunctionSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassSurfacePawn : APawn
			{
				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
				}

				UFUNCTION(BlueprintOverride)
				void PossessedBy(AController NewController)
				{
				}

				UFUNCTION(BlueprintOverride)
				void UnPossessed()
				{
				}
			}

			UCLASS()
			class ACoverageUClassSurfaceHUD : AHUD
			{
				UFUNCTION(BlueprintOverride)
				void DrawHUD(int SizeX, int SizeY)
				{
				}
			}

			UCLASS()
			class UCoverageUClassSurfaceWidget : UUserWidget
			{
				UFUNCTION(BlueprintOverride)
				void OnInitialized()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Construct()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Destruct()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Tick(FGeometry MyGeometry, float InDeltaTime)
				{
				}
			}

			UCLASS()
			class UCoverageUClassSurfaceComponent : UActorComponent
			{
				UFUNCTION(BlueprintOverride)
				void OnComponentCreated()
				{
				}

				UFUNCTION(BlueprintOverride)
				void InitializeComponent()
				{
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
				}

				UFUNCTION(BlueprintOverride)
				void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
				{
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnComponentDestroyed(bool bDestroyingHierarchy)
				{
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassCommonLifecycleFunctionSurface.as"), ScriptSource)));

		UClass* PawnClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassSurfacePawn"));
		UClass* HUDClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassSurfaceHUD"));
		UClass* WidgetClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassSurfaceWidget"));
		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassSurfaceComponent"));
		ASSERT_THAT(IsNotNull(PawnClass, TEXT("Pawn lifecycle surface class should be generated")));
		ASSERT_THAT(IsNotNull(HUDClass, TEXT("HUD lifecycle surface class should be generated")));
		ASSERT_THAT(IsNotNull(WidgetClass, TEXT("Widget lifecycle surface class should be generated")));
		ASSERT_THAT(IsNotNull(ComponentClass, TEXT("Component lifecycle surface class should be generated")));
		if (PawnClass == nullptr || HUDClass == nullptr || WidgetClass == nullptr || ComponentClass == nullptr)
		{
			return;
		}

		UFunction* SetupInputFunction = PawnClass->FindFunctionByName(TEXT("SetupPlayerInputComponent"));
		UFunction* PossessedByFunction = PawnClass->FindFunctionByName(TEXT("PossessedBy"));
		UFunction* UnPossessedFunction = PawnClass->FindFunctionByName(TEXT("UnPossessed"));
		ASSERT_THAT(IsNotNull(SetupInputFunction, TEXT("APawn SetupPlayerInputComponent override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(PossessedByFunction, TEXT("APawn PossessedBy override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(UnPossessedFunction, TEXT("APawn UnPossessed override should generate a UFunction")));
		if (SetupInputFunction == nullptr || PossessedByFunction == nullptr || UnPossessedFunction == nullptr)
		{
			return;
		}

		FObjectPropertyBase* InputComponentParam = FindFProperty<FObjectPropertyBase>(SetupInputFunction, TEXT("PlayerInputComponent"));
		FObjectPropertyBase* ControllerParam = FindFProperty<FObjectPropertyBase>(PossessedByFunction, TEXT("NewController"));
		ASSERT_THAT(IsNotNull(InputComponentParam, TEXT("SetupPlayerInputComponent should expose PlayerInputComponent")));
		ASSERT_THAT(IsNotNull(ControllerParam, TEXT("PossessedBy should expose NewController")));
		if (InputComponentParam == nullptr || ControllerParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UInputComponent::StaticClass(), InputComponentParam->PropertyClass, TEXT("SetupPlayerInputComponent parameter should target UInputComponent")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), ControllerParam->PropertyClass, TEXT("PossessedBy parameter should target AController")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(UnPossessedFunction), TEXT("UnPossessed should expose no parameters")));

		UFunction* DrawHUDFunction = HUDClass->FindFunctionByName(TEXT("ReceiveDrawHUD"));
		ASSERT_THAT(IsNotNull(DrawHUDFunction, TEXT("AHUD DrawHUD override should route through ReceiveDrawHUD")));
		if (DrawHUDFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(DrawHUDFunction, TEXT("SizeX")), TEXT("ReceiveDrawHUD should expose SizeX")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(DrawHUDFunction, TEXT("SizeY")), TEXT("ReceiveDrawHUD should expose SizeY")));
		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(DrawHUDFunction), TEXT("ReceiveDrawHUD should expose SizeX and SizeY parameters")));

		UFunction* OnInitializedFunction = WidgetClass->FindFunctionByName(TEXT("OnInitialized"));
		UFunction* ConstructFunction = WidgetClass->FindFunctionByName(TEXT("Construct"));
		UFunction* DestructFunction = WidgetClass->FindFunctionByName(TEXT("Destruct"));
		UFunction* WidgetTickFunction = WidgetClass->FindFunctionByName(TEXT("Tick"));
		ASSERT_THAT(IsNotNull(OnInitializedFunction, TEXT("UUserWidget OnInitialized override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(ConstructFunction, TEXT("UUserWidget Construct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(DestructFunction, TEXT("UUserWidget Destruct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(WidgetTickFunction, TEXT("UUserWidget Tick override should generate a UFunction")));
		if (OnInitializedFunction == nullptr || ConstructFunction == nullptr || DestructFunction == nullptr || WidgetTickFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(OnInitializedFunction), TEXT("OnInitialized should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(ConstructFunction), TEXT("Construct should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(DestructFunction), TEXT("Destruct should expose no parameters")));
		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(WidgetTickFunction), TEXT("Widget Tick should expose geometry and delta-time parameters")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(WidgetTickFunction, TEXT("MyGeometry")), TEXT("Widget Tick should expose MyGeometry")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(WidgetTickFunction, TEXT("InDeltaTime")), TEXT("Widget Tick should expose a double-backed float delta-time parameter")));

		UFunction* OnComponentCreatedFunction = ComponentClass->FindFunctionByName(TEXT("OnComponentCreated"));
		UFunction* InitializeComponentFunction = ComponentClass->FindFunctionByName(TEXT("InitializeComponent"));
		UFunction* BeginPlayFunction = ComponentClass->FindFunctionByName(TEXT("BeginPlay"));
		UFunction* TickComponentFunction = ComponentClass->FindFunctionByName(TEXT("TickComponent"));
		UFunction* EndPlayFunction = ComponentClass->FindFunctionByName(TEXT("EndPlay"));
		UFunction* OnComponentDestroyedFunction = ComponentClass->FindFunctionByName(TEXT("OnComponentDestroyed"));
		ASSERT_THAT(IsNotNull(OnComponentCreatedFunction, TEXT("UActorComponent OnComponentCreated override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(InitializeComponentFunction, TEXT("UActorComponent InitializeComponent override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(BeginPlayFunction, TEXT("UActorComponent BeginPlay override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(TickComponentFunction, TEXT("UActorComponent TickComponent override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(EndPlayFunction, TEXT("UActorComponent EndPlay override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(OnComponentDestroyedFunction, TEXT("UActorComponent OnComponentDestroyed override should generate a UFunction")));
		if (OnComponentCreatedFunction == nullptr || InitializeComponentFunction == nullptr || BeginPlayFunction == nullptr
			|| TickComponentFunction == nullptr || EndPlayFunction == nullptr || OnComponentDestroyedFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(OnComponentCreatedFunction), TEXT("OnComponentCreated should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(InitializeComponentFunction), TEXT("InitializeComponent should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(BeginPlayFunction), TEXT("Component BeginPlay should expose no parameters")));
		ASSERT_THAT(AreEqual(3, CountNonReturnParameters(TickComponentFunction), TEXT("TickComponent should expose DeltaTime, TickType, and tick function parameters")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(EndPlayFunction), TEXT("Component EndPlay should expose one reason parameter")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(OnComponentDestroyedFunction), TEXT("OnComponentDestroyed should expose one destroying-hierarchy parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(OnComponentDestroyedFunction, TEXT("bDestroyingHierarchy")), TEXT("OnComponentDestroyed should expose bDestroyingHierarchy")));
	}

	TEST_METHOD(UClassSubsystemFunctionSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_SubsystemFunctionSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassSurfaceWorldSubsystem : UScriptWorldSubsystem
			{
				UFUNCTION(BlueprintOverride)
				bool ShouldCreateSubsystem(UObject Outer) const
				{
					return Outer != nullptr;
				}

				UFUNCTION(BlueprintOverride)
				void Initialize()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Deinitialize()
				{
				}

				UFUNCTION(BlueprintOverride)
				void PostInitialize()
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnWorldBeginPlay()
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnWorldComponentsUpdated()
				{
				}

				UFUNCTION(BlueprintOverride)
				void UpdateStreamingState()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
				}
			}

			UCLASS()
			class UCoverageUClassSurfaceGameInstanceSubsystem : UScriptGameInstanceSubsystem
			{
				UFUNCTION(BlueprintOverride)
				bool ShouldCreateSubsystem(UObject Outer) const
				{
					return Outer != nullptr;
				}

				UFUNCTION(BlueprintOverride)
				void Initialize()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Deinitialize()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
				}
			}

			UCLASS()
			class UCoverageUClassSurfaceLocalPlayerSubsystem : UScriptLocalPlayerSubsystem
			{
				UFUNCTION(BlueprintOverride)
				bool ShouldCreateSubsystem(UObject Outer) const
				{
					return Outer != nullptr;
				}

				UFUNCTION(BlueprintOverride)
				void Initialize()
				{
				}

				UFUNCTION(BlueprintOverride)
				void Deinitialize()
				{
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassSubsystemFunctionSurface.as"), ScriptSource)));

		UClass* WorldSubsystemClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassSurfaceWorldSubsystem"));
		UClass* GameInstanceSubsystemClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassSurfaceGameInstanceSubsystem"));
		UClass* LocalPlayerSubsystemClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassSurfaceLocalPlayerSubsystem"));
		ASSERT_THAT(IsNotNull(WorldSubsystemClass, TEXT("World subsystem function surface class should be generated")));
		ASSERT_THAT(IsNotNull(GameInstanceSubsystemClass, TEXT("Game-instance subsystem function surface class should be generated")));
		ASSERT_THAT(IsNotNull(LocalPlayerSubsystemClass, TEXT("Local-player subsystem function surface class should be generated")));
		if (WorldSubsystemClass == nullptr || GameInstanceSubsystemClass == nullptr || LocalPlayerSubsystemClass == nullptr)
		{
			return;
		}

		UFunction* WorldShouldCreateFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_ShouldCreateSubsystem"));
		UFunction* WorldInitializeFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_Initialize"));
		UFunction* WorldDeinitializeFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_Deinitialize"));
		UFunction* WorldPostInitializeFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_PostInitialize"));
		UFunction* WorldBeginPlayFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_OnWorldBeginPlay"));
		UFunction* WorldComponentsUpdatedFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_OnWorldComponentsUpdated"));
		UFunction* WorldUpdateStreamingFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_UpdateStreamingState"));
		UFunction* WorldTickFunction = WorldSubsystemClass->FindFunctionByName(TEXT("BP_Tick"));
		ASSERT_THAT(IsNotNull(WorldShouldCreateFunction, TEXT("World subsystem ShouldCreateSubsystem should route to BP_ShouldCreateSubsystem")));
		ASSERT_THAT(IsNotNull(WorldInitializeFunction, TEXT("World subsystem Initialize should route to BP_Initialize")));
		ASSERT_THAT(IsNotNull(WorldDeinitializeFunction, TEXT("World subsystem Deinitialize should route to BP_Deinitialize")));
		ASSERT_THAT(IsNotNull(WorldPostInitializeFunction, TEXT("World subsystem PostInitialize should route to BP_PostInitialize")));
		ASSERT_THAT(IsNotNull(WorldBeginPlayFunction, TEXT("World subsystem OnWorldBeginPlay should route to BP_OnWorldBeginPlay")));
		ASSERT_THAT(IsNotNull(WorldComponentsUpdatedFunction, TEXT("World subsystem OnWorldComponentsUpdated should route to BP_OnWorldComponentsUpdated")));
		ASSERT_THAT(IsNotNull(WorldUpdateStreamingFunction, TEXT("World subsystem UpdateStreamingState should route to BP_UpdateStreamingState")));
		ASSERT_THAT(IsNotNull(WorldTickFunction, TEXT("World subsystem Tick should route to BP_Tick")));
		if (WorldShouldCreateFunction == nullptr || WorldInitializeFunction == nullptr || WorldDeinitializeFunction == nullptr
			|| WorldPostInitializeFunction == nullptr || WorldBeginPlayFunction == nullptr || WorldComponentsUpdatedFunction == nullptr
			|| WorldUpdateStreamingFunction == nullptr || WorldTickFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(WorldShouldCreateFunction), TEXT("World ShouldCreateSubsystem should expose Outer")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(WorldShouldCreateFunction, TEXT("ReturnValue")), TEXT("World ShouldCreateSubsystem should return bool")));
		FObjectPropertyBase* WorldOuterParam = FindFProperty<FObjectPropertyBase>(WorldShouldCreateFunction, TEXT("Outer"));
		ASSERT_THAT(IsNotNull(WorldOuterParam, TEXT("World ShouldCreateSubsystem should expose UObject Outer")));
		if (WorldOuterParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), WorldOuterParam->PropertyClass, TEXT("World ShouldCreateSubsystem Outer should target UObject")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(WorldInitializeFunction), TEXT("World Initialize should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(WorldDeinitializeFunction), TEXT("World Deinitialize should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(WorldPostInitializeFunction), TEXT("World PostInitialize should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(WorldBeginPlayFunction), TEXT("World OnWorldBeginPlay should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(WorldComponentsUpdatedFunction), TEXT("World OnWorldComponentsUpdated should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(WorldUpdateStreamingFunction), TEXT("World UpdateStreamingState should expose no parameters")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(WorldTickFunction), TEXT("World Tick should expose DeltaTime")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(WorldTickFunction, TEXT("DeltaTime")), TEXT("World Tick DeltaTime should be double-backed")));

		UFunction* GameInstanceShouldCreateFunction = GameInstanceSubsystemClass->FindFunctionByName(TEXT("BP_ShouldCreateSubsystem"));
		UFunction* GameInstanceInitializeFunction = GameInstanceSubsystemClass->FindFunctionByName(TEXT("BP_Initialize"));
		UFunction* GameInstanceDeinitializeFunction = GameInstanceSubsystemClass->FindFunctionByName(TEXT("BP_Deinitialize"));
		UFunction* GameInstanceTickFunction = GameInstanceSubsystemClass->FindFunctionByName(TEXT("BP_Tick"));
		ASSERT_THAT(IsNotNull(GameInstanceShouldCreateFunction, TEXT("Game-instance subsystem ShouldCreateSubsystem should route to BP_ShouldCreateSubsystem")));
		ASSERT_THAT(IsNotNull(GameInstanceInitializeFunction, TEXT("Game-instance subsystem Initialize should route to BP_Initialize")));
		ASSERT_THAT(IsNotNull(GameInstanceDeinitializeFunction, TEXT("Game-instance subsystem Deinitialize should route to BP_Deinitialize")));
		ASSERT_THAT(IsNotNull(GameInstanceTickFunction, TEXT("Game-instance subsystem Tick should route to BP_Tick")));
		if (GameInstanceShouldCreateFunction == nullptr || GameInstanceInitializeFunction == nullptr
			|| GameInstanceDeinitializeFunction == nullptr || GameInstanceTickFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(GameInstanceShouldCreateFunction), TEXT("Game-instance ShouldCreateSubsystem should expose Outer")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(GameInstanceShouldCreateFunction, TEXT("ReturnValue")), TEXT("Game-instance ShouldCreateSubsystem should return bool")));
		FObjectPropertyBase* GameInstanceOuterParam = FindFProperty<FObjectPropertyBase>(GameInstanceShouldCreateFunction, TEXT("Outer"));
		ASSERT_THAT(IsNotNull(GameInstanceOuterParam, TEXT("Game-instance ShouldCreateSubsystem should expose UObject Outer")));
		if (GameInstanceOuterParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), GameInstanceOuterParam->PropertyClass, TEXT("Game-instance ShouldCreateSubsystem Outer should target UObject")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(GameInstanceInitializeFunction), TEXT("Game-instance Initialize should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(GameInstanceDeinitializeFunction), TEXT("Game-instance Deinitialize should expose no parameters")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(GameInstanceTickFunction), TEXT("Game-instance Tick should expose DeltaTime")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(GameInstanceTickFunction, TEXT("DeltaTime")), TEXT("Game-instance Tick DeltaTime should be double-backed")));

		UFunction* LocalPlayerShouldCreateFunction = LocalPlayerSubsystemClass->FindFunctionByName(TEXT("BP_ShouldCreateSubsystem"));
		UFunction* LocalPlayerInitializeFunction = LocalPlayerSubsystemClass->FindFunctionByName(TEXT("BP_Initialize"));
		UFunction* LocalPlayerDeinitializeFunction = LocalPlayerSubsystemClass->FindFunctionByName(TEXT("BP_Deinitialize"));
		ASSERT_THAT(IsNotNull(LocalPlayerShouldCreateFunction, TEXT("Local-player subsystem ShouldCreateSubsystem should route to BP_ShouldCreateSubsystem")));
		ASSERT_THAT(IsNotNull(LocalPlayerInitializeFunction, TEXT("Local-player subsystem Initialize should route to BP_Initialize")));
		ASSERT_THAT(IsNotNull(LocalPlayerDeinitializeFunction, TEXT("Local-player subsystem Deinitialize should route to BP_Deinitialize")));
		if (LocalPlayerShouldCreateFunction == nullptr || LocalPlayerInitializeFunction == nullptr || LocalPlayerDeinitializeFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(LocalPlayerShouldCreateFunction), TEXT("Local-player ShouldCreateSubsystem should expose Outer")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(LocalPlayerShouldCreateFunction, TEXT("ReturnValue")), TEXT("Local-player ShouldCreateSubsystem should return bool")));
		FObjectPropertyBase* LocalPlayerOuterParam = FindFProperty<FObjectPropertyBase>(LocalPlayerShouldCreateFunction, TEXT("Outer"));
		ASSERT_THAT(IsNotNull(LocalPlayerOuterParam, TEXT("Local-player ShouldCreateSubsystem should expose UObject Outer")));
		if (LocalPlayerOuterParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), LocalPlayerOuterParam->PropertyClass, TEXT("Local-player ShouldCreateSubsystem Outer should target UObject")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(LocalPlayerInitializeFunction), TEXT("Local-player Initialize should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(LocalPlayerDeinitializeFunction), TEXT("Local-player Deinitialize should expose no parameters")));
	}

	TEST_METHOD(UClassGameFrameworkEventFunctionSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_GameFrameworkEventFunctionSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassEventCharacter : ACharacter
			{
				UFUNCTION(BlueprintOverride)
				void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnMovementModeChanged(EMovementMode PrevMovementMode, EMovementMode NewMovementMode, uint8 PrevCustomMode, uint8 NewCustomMode)
				{
				}

				UFUNCTION(BlueprintOverride)
				void UpdateCustomMovement(float DeltaTime)
				{
				}
			}

			UCLASS()
			class ACoverageUClassEventGameMode : AGameModeBase
			{
				UFUNCTION(BlueprintOverride)
				void OnPostLogin(APlayerController NewPlayer)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnLogout(AController ExitingController)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnChangeName(AController Other, const FString&in NewName, bool bNameChange)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnRestartPlayer(AController NewPlayer)
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnSwapPlayerControllers(APlayerController OldPC, APlayerController NewPC)
				{
				}
			}

			UCLASS()
			class ACoverageUClassEventPlayerState : APlayerState
			{
				UFUNCTION(BlueprintOverride)
				void OverrideWith(APlayerState OldPlayerState)
				{
				}

				UFUNCTION(BlueprintOverride)
				void CopyProperties(APlayerState NewPlayerState)
				{
				}
			}

			UCLASS()
			class ACoverageUClassEventGameState : AGameStateBase
			{
				UPROPERTY(ReplicatedUsing=OnRep_CoverageFlag)
				bool bCoverageFlag = false;

				UFUNCTION()
				void OnRep_CoverageFlag()
				{
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassGameFrameworkEventFunctionSurface.as"), ScriptSource)));

		UClass* CharacterClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassEventCharacter"));
		UClass* GameModeClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassEventGameMode"));
		UClass* PlayerStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassEventPlayerState"));
		UClass* GameStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassEventGameState"));
		ASSERT_THAT(IsNotNull(CharacterClass, TEXT("Character event surface class should be generated")));
		ASSERT_THAT(IsNotNull(GameModeClass, TEXT("GameMode event surface class should be generated")));
		ASSERT_THAT(IsNotNull(PlayerStateClass, TEXT("PlayerState event surface class should be generated")));
		ASSERT_THAT(IsNotNull(GameStateClass, TEXT("GameState event surface class should be generated")));
		if (CharacterClass == nullptr || GameModeClass == nullptr || PlayerStateClass == nullptr || GameStateClass == nullptr)
		{
			return;
		}

		UFunction* OnStartCrouchFunction = CharacterClass->FindFunctionByName(TEXT("K2_OnStartCrouch"));
		UFunction* OnEndCrouchFunction = CharacterClass->FindFunctionByName(TEXT("K2_OnEndCrouch"));
		UFunction* MovementModeFunction = CharacterClass->FindFunctionByName(TEXT("K2_OnMovementModeChanged"));
		UFunction* UpdateCustomMovementFunction = CharacterClass->FindFunctionByName(TEXT("K2_UpdateCustomMovement"));
		ASSERT_THAT(IsNotNull(OnStartCrouchFunction, TEXT("ACharacter OnStartCrouch ScriptName override should route through K2_OnStartCrouch")));
		ASSERT_THAT(IsNotNull(OnEndCrouchFunction, TEXT("ACharacter OnEndCrouch ScriptName override should route through K2_OnEndCrouch")));
		ASSERT_THAT(IsNotNull(MovementModeFunction, TEXT("ACharacter OnMovementModeChanged ScriptName override should route through K2_OnMovementModeChanged")));
		ASSERT_THAT(IsNotNull(UpdateCustomMovementFunction, TEXT("ACharacter UpdateCustomMovement ScriptName override should route through K2_UpdateCustomMovement")));
		if (OnStartCrouchFunction == nullptr || OnEndCrouchFunction == nullptr || MovementModeFunction == nullptr || UpdateCustomMovementFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(OnStartCrouchFunction), TEXT("OnStartCrouch should expose two float parameters")));
		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(OnEndCrouchFunction), TEXT("OnEndCrouch should expose two float parameters")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(OnStartCrouchFunction, TEXT("HalfHeightAdjust")), TEXT("OnStartCrouch should expose HalfHeightAdjust")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(OnStartCrouchFunction, TEXT("ScaledHalfHeightAdjust")), TEXT("OnStartCrouch should expose ScaledHalfHeightAdjust")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(OnEndCrouchFunction, TEXT("HalfHeightAdjust")), TEXT("OnEndCrouch should expose HalfHeightAdjust")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(OnEndCrouchFunction, TEXT("ScaledHalfHeightAdjust")), TEXT("OnEndCrouch should expose ScaledHalfHeightAdjust")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(UpdateCustomMovementFunction), TEXT("UpdateCustomMovement should expose one delta-time parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(UpdateCustomMovementFunction, TEXT("DeltaTime")), TEXT("UpdateCustomMovement should expose DeltaTime")));

		FByteProperty* PrevMovementModeParam = FindFProperty<FByteProperty>(MovementModeFunction, TEXT("PrevMovementMode"));
		FByteProperty* NewMovementModeParam = FindFProperty<FByteProperty>(MovementModeFunction, TEXT("NewMovementMode"));
		FByteProperty* PrevCustomModeParam = FindFProperty<FByteProperty>(MovementModeFunction, TEXT("PrevCustomMode"));
		FByteProperty* NewCustomModeParam = FindFProperty<FByteProperty>(MovementModeFunction, TEXT("NewCustomMode"));
		ASSERT_THAT(IsNotNull(PrevMovementModeParam, TEXT("OnMovementModeChanged should expose PrevMovementMode")));
		ASSERT_THAT(IsNotNull(NewMovementModeParam, TEXT("OnMovementModeChanged should expose NewMovementMode")));
		ASSERT_THAT(IsNotNull(PrevCustomModeParam, TEXT("OnMovementModeChanged should expose PrevCustomMode")));
		ASSERT_THAT(IsNotNull(NewCustomModeParam, TEXT("OnMovementModeChanged should expose NewCustomMode")));
		if (PrevMovementModeParam == nullptr || NewMovementModeParam == nullptr || PrevCustomModeParam == nullptr || NewCustomModeParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(4, CountNonReturnParameters(MovementModeFunction), TEXT("OnMovementModeChanged should expose movement mode and custom mode parameters")));
		ASSERT_THAT(AreEqual(StaticEnum<EMovementMode>(), PrevMovementModeParam->Enum, TEXT("PrevMovementMode should retain the EMovementMode enum")));
		ASSERT_THAT(AreEqual(StaticEnum<EMovementMode>(), NewMovementModeParam->Enum, TEXT("NewMovementMode should retain the EMovementMode enum")));
		ASSERT_THAT(IsNull(PrevCustomModeParam->Enum, TEXT("PrevCustomMode should remain a raw uint8 byte parameter")));
		ASSERT_THAT(IsNull(NewCustomModeParam->Enum, TEXT("NewCustomMode should remain a raw uint8 byte parameter")));

		UFunction* PostLoginFunction = GameModeClass->FindFunctionByName(TEXT("K2_PostLogin"));
		UFunction* LogoutFunction = GameModeClass->FindFunctionByName(TEXT("K2_OnLogout"));
		UFunction* ChangeNameFunction = GameModeClass->FindFunctionByName(TEXT("K2_OnChangeName"));
		UFunction* RestartPlayerFunction = GameModeClass->FindFunctionByName(TEXT("K2_OnRestartPlayer"));
		UFunction* SwapPlayerControllersFunction = GameModeClass->FindFunctionByName(TEXT("K2_OnSwapPlayerControllers"));
		ASSERT_THAT(IsNotNull(PostLoginFunction, TEXT("AGameModeBase OnPostLogin ScriptName override should route through K2_PostLogin")));
		ASSERT_THAT(IsNotNull(LogoutFunction, TEXT("AGameModeBase OnLogout ScriptName override should route through K2_OnLogout")));
		ASSERT_THAT(IsNotNull(ChangeNameFunction, TEXT("AGameModeBase OnChangeName ScriptName override should route through K2_OnChangeName")));
		ASSERT_THAT(IsNotNull(RestartPlayerFunction, TEXT("AGameModeBase OnRestartPlayer ScriptName override should route through K2_OnRestartPlayer")));
		ASSERT_THAT(IsNotNull(SwapPlayerControllersFunction, TEXT("AGameModeBase OnSwapPlayerControllers ScriptName override should route through K2_OnSwapPlayerControllers")));
		if (PostLoginFunction == nullptr || LogoutFunction == nullptr || ChangeNameFunction == nullptr || RestartPlayerFunction == nullptr || SwapPlayerControllersFunction == nullptr)
		{
			return;
		}

		FObjectPropertyBase* PostLoginPlayerParam = FindFProperty<FObjectPropertyBase>(PostLoginFunction, TEXT("NewPlayer"));
		FObjectPropertyBase* LogoutControllerParam = FindFProperty<FObjectPropertyBase>(LogoutFunction, TEXT("ExitingController"));
		FObjectPropertyBase* ChangeNameControllerParam = FindFProperty<FObjectPropertyBase>(ChangeNameFunction, TEXT("Other"));
		FStrProperty* ChangeNameParam = FindFProperty<FStrProperty>(ChangeNameFunction, TEXT("NewName"));
		FBoolProperty* NameChangeParam = FindFProperty<FBoolProperty>(ChangeNameFunction, TEXT("bNameChange"));
		FObjectPropertyBase* RestartControllerParam = FindFProperty<FObjectPropertyBase>(RestartPlayerFunction, TEXT("NewPlayer"));
		FObjectPropertyBase* SwapOldControllerParam = FindFProperty<FObjectPropertyBase>(SwapPlayerControllersFunction, TEXT("OldPC"));
		FObjectPropertyBase* SwapNewControllerParam = FindFProperty<FObjectPropertyBase>(SwapPlayerControllersFunction, TEXT("NewPC"));
		ASSERT_THAT(IsNotNull(PostLoginPlayerParam, TEXT("OnPostLogin should expose NewPlayer")));
		ASSERT_THAT(IsNotNull(LogoutControllerParam, TEXT("OnLogout should expose ExitingController")));
		ASSERT_THAT(IsNotNull(ChangeNameControllerParam, TEXT("OnChangeName should expose Other")));
		ASSERT_THAT(IsNotNull(ChangeNameParam, TEXT("OnChangeName should expose NewName")));
		ASSERT_THAT(IsNotNull(NameChangeParam, TEXT("OnChangeName should expose bNameChange")));
		ASSERT_THAT(IsNotNull(RestartControllerParam, TEXT("OnRestartPlayer should expose NewPlayer")));
		ASSERT_THAT(IsNotNull(SwapOldControllerParam, TEXT("OnSwapPlayerControllers should expose OldPC")));
		ASSERT_THAT(IsNotNull(SwapNewControllerParam, TEXT("OnSwapPlayerControllers should expose NewPC")));
		if (PostLoginPlayerParam == nullptr || LogoutControllerParam == nullptr || ChangeNameControllerParam == nullptr || ChangeNameParam == nullptr
			|| NameChangeParam == nullptr || RestartControllerParam == nullptr || SwapOldControllerParam == nullptr || SwapNewControllerParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), PostLoginPlayerParam->PropertyClass, TEXT("OnPostLogin should preserve APlayerController parameter type")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), LogoutControllerParam->PropertyClass, TEXT("OnLogout should preserve AController parameter type")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), ChangeNameControllerParam->PropertyClass, TEXT("OnChangeName should preserve AController parameter type")));
		ASSERT_THAT(IsTrue(ChangeNameParam->HasAnyPropertyFlags(CPF_ConstParm), TEXT("OnChangeName NewName should remain const")));
		ASSERT_THAT(IsTrue(ChangeNameParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("OnChangeName NewName should remain an input reference parameter")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), RestartControllerParam->PropertyClass, TEXT("OnRestartPlayer should preserve AController parameter type")));
		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), SwapOldControllerParam->PropertyClass, TEXT("OnSwapPlayerControllers should preserve OldPC type")));
		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), SwapNewControllerParam->PropertyClass, TEXT("OnSwapPlayerControllers should preserve NewPC type")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(PostLoginFunction), TEXT("OnPostLogin should expose one parameter")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(LogoutFunction), TEXT("OnLogout should expose one parameter")));
		ASSERT_THAT(AreEqual(3, CountNonReturnParameters(ChangeNameFunction), TEXT("OnChangeName should expose controller, name, and flag")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(RestartPlayerFunction), TEXT("OnRestartPlayer should expose one controller parameter")));
		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(SwapPlayerControllersFunction), TEXT("OnSwapPlayerControllers should expose old and new controller parameters")));

		UFunction* OverrideWithFunction = PlayerStateClass->FindFunctionByName(TEXT("ReceiveOverrideWith"));
		UFunction* CopyPropertiesFunction = PlayerStateClass->FindFunctionByName(TEXT("ReceiveCopyProperties"));
		ASSERT_THAT(IsNotNull(OverrideWithFunction, TEXT("APlayerState OverrideWith display-name override should route through ReceiveOverrideWith")));
		ASSERT_THAT(IsNotNull(CopyPropertiesFunction, TEXT("APlayerState CopyProperties display-name override should route through ReceiveCopyProperties")));
		if (OverrideWithFunction == nullptr || CopyPropertiesFunction == nullptr)
		{
			return;
		}

		FObjectPropertyBase* OverrideWithParam = FindFProperty<FObjectPropertyBase>(OverrideWithFunction, TEXT("OldPlayerState"));
		FObjectPropertyBase* CopyPropertiesParam = FindFProperty<FObjectPropertyBase>(CopyPropertiesFunction, TEXT("NewPlayerState"));
		ASSERT_THAT(IsNotNull(OverrideWithParam, TEXT("OverrideWith should expose OldPlayerState")));
		ASSERT_THAT(IsNotNull(CopyPropertiesParam, TEXT("CopyProperties should expose NewPlayerState")));
		if (OverrideWithParam == nullptr || CopyPropertiesParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), OverrideWithParam->PropertyClass, TEXT("OverrideWith should preserve APlayerState parameter type")));
		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), CopyPropertiesParam->PropertyClass, TEXT("CopyProperties should preserve APlayerState parameter type")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(OverrideWithFunction), TEXT("OverrideWith should expose one player-state parameter")));
		ASSERT_THAT(AreEqual(1, CountNonReturnParameters(CopyPropertiesFunction), TEXT("CopyProperties should expose one player-state parameter")));

		FBoolProperty* CoverageFlagProperty = FindFProperty<FBoolProperty>(GameStateClass, TEXT("bCoverageFlag"));
		UFunction* RepNotifyFunction = GameStateClass->FindFunctionByName(TEXT("OnRep_CoverageFlag"));
		ASSERT_THAT(IsNotNull(CoverageFlagProperty, TEXT("GameState should reflect ReplicatedUsing flag property")));
		ASSERT_THAT(IsNotNull(RepNotifyFunction, TEXT("GameState should reflect RepNotify function")));
		if (CoverageFlagProperty == nullptr || RepNotifyFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CoverageFlagProperty->HasAnyPropertyFlags(CPF_Net), TEXT("ReplicatedUsing should set CPF_Net on the GameState property")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_CoverageFlag")), CoverageFlagProperty->RepNotifyFunc, TEXT("ReplicatedUsing should bind the GameState notify function")));
		ASSERT_THAT(AreEqual(0, CountNonReturnParameters(RepNotifyFunction), TEXT("RepNotify function should expose no parameters")));
	}

	TEST_METHOD(UClassSpecifierCrossProductMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_SpecifierCrossProductMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, BlueprintType, Abstract, Transient, Deprecated, NotPlaceable, Config=Game, DefaultConfig, DefaultToInstanced, EditInlineNew, HideDropdown, ClassGroup="CoverageGroup", HideCategories="Rendering", ComponentWrapperClass, meta=(DisplayName="Coverage All Specifiers", ToolTip="All supported specifiers", ShortTooltip="All specifiers", ConversionRoot, HideFunctions="HiddenA,HiddenB", SparseClassDataTypes="SparseData", AutoExpandCategories="Expanded", AutoCollapseCategories="Collapsed", CollapseCategories, DontCollapseCategories, ChildCanTick))
			class ACoverageUClassAllSupportedSpecifiersActor : AActor
			{
				UPROPERTY(Config)
				int ConfigValue = 101;
			}

			UCLASS(NotBlueprintable, BlueprintType, Config=Editor, DefaultToInstanced)
			class UCoverageUClassVariableOnlyConfigObject : UObject
			{
				UPROPERTY(Config)
				int EditorValue = 202;
			}

			UCLASS(Transient, Deprecated, DefaultToInstanced, EditInlineNew, Config=Game, DefaultConfig)
			class UCoverageUClassCombinationBaseObject : UObject
			{
			}

			UCLASS(Blueprintable)
			class UCoverageUClassCombinationChildObject : UCoverageUClassCombinationBaseObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassSpecifierCrossProductMatrix.as"), ScriptSource)));

		UClass* AllSpecifiersClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassAllSupportedSpecifiersActor"));
		UClass* VariableOnlyConfigClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassVariableOnlyConfigObject"));
		UClass* CombinationBaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassCombinationBaseObject"));
		UClass* CombinationChildClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassCombinationChildObject"));
		ASSERT_THAT(IsNotNull(AllSpecifiersClass, TEXT("All-supported-specifiers class should be generated")));
		ASSERT_THAT(IsNotNull(VariableOnlyConfigClass, TEXT("Variable-only config class should be generated")));
		ASSERT_THAT(IsNotNull(CombinationBaseClass, TEXT("Combination base class should be generated")));
		ASSERT_THAT(IsNotNull(CombinationChildClass, TEXT("Combination child class should be generated")));
		if (AllSpecifiersClass == nullptr || VariableOnlyConfigClass == nullptr || CombinationBaseClass == nullptr || CombinationChildClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AllSpecifiersClass->IsChildOf(AActor::StaticClass()), TEXT("All-supported-specifiers class should remain actor-derived")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Abstract should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_Transient), TEXT("Transient should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_Deprecated), TEXT("Deprecated should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("NotPlaceable should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_Config), TEXT("Config should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("DefaultToInstanced should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_EditInlineNew), TEXT("EditInlineNew should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasAnyClassFlags(CLASS_HideDropDown), TEXT("HideDropdown should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), AllSpecifiersClass->ClassConfigName, TEXT("Config=Game should survive the full specifier cross product")));

		ASSERT_THAT(AreEqual(FString(TEXT("true")), AllSpecifiersClass->GetMetaData(TEXT("BlueprintType")), TEXT("BlueprintType metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AllSpecifiersClass->GetMetaData(TEXT("Blueprintable")), TEXT("Blueprintable metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AllSpecifiersClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("Blueprintable should leave the class as a Blueprint base")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageGroup")), AllSpecifiersClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("ClassGroup metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage All Specifiers")), AllSpecifiersClass->GetMetaData(TEXT("DisplayName")), TEXT("DisplayName metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("All supported specifiers")), AllSpecifiersClass->GetMetaData(TEXT("ToolTip")), TEXT("ToolTip metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("All specifiers")), AllSpecifiersClass->GetMetaData(TEXT("ShortTooltip")), TEXT("ShortTooltip metadata should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->GetMetaData(TEXT("HideCategories")).Contains(TEXT("Rendering")), TEXT("HideCategories should preserve explicit categories")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->GetMetaData(TEXT("HideCategories")).Contains(TEXT("DefaultComponents")), TEXT("Actor UCLASS should append DefaultComponents hidden category")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasMetaData(TEXT("ComponentWrapperClass")), TEXT("ComponentWrapperClass metadata should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasMetaData(TEXT("ConversionRoot")), TEXT("ConversionRoot metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("HiddenA,HiddenB")), AllSpecifiersClass->GetMetaData(TEXT("HideFunctions")), TEXT("HideFunctions metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("SparseData")), AllSpecifiersClass->GetMetaData(TEXT("SparseClassDataTypes")), TEXT("SparseClassDataTypes metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("Expanded")), AllSpecifiersClass->GetMetaData(TEXT("AutoExpandCategories")), TEXT("AutoExpandCategories metadata should survive the full specifier cross product")));
		ASSERT_THAT(AreEqual(FString(TEXT("Collapsed")), AllSpecifiersClass->GetMetaData(TEXT("AutoCollapseCategories")), TEXT("AutoCollapseCategories metadata should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasMetaData(TEXT("CollapseCategories")), TEXT("CollapseCategories metadata should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasMetaData(TEXT("DontCollapseCategories")), TEXT("DontCollapseCategories metadata should survive the full specifier cross product")));
		ASSERT_THAT(IsTrue(AllSpecifiersClass->HasMetaData(TEXT("ChildCanTick")), TEXT("ChildCanTick metadata should survive the full specifier cross product")));

		FProperty* AllSpecifiersConfigProperty = AllSpecifiersClass->FindPropertyByName(TEXT("ConfigValue"));
		ASSERT_THAT(IsNotNull(AllSpecifiersConfigProperty, TEXT("ConfigValue should be generated on the all-specifiers class")));
		if (AllSpecifiersConfigProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AllSpecifiersConfigProperty->HasAnyPropertyFlags(CPF_Config), TEXT("Config property flag should survive the full specifier cross product")));

		ASSERT_THAT(IsTrue(VariableOnlyConfigClass->HasAnyClassFlags(CLASS_Config), TEXT("Variable-only config class should set CLASS_Config")));
		ASSERT_THAT(IsTrue(VariableOnlyConfigClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("Variable-only config class should set CLASS_DefaultToInstanced")));
		ASSERT_THAT(IsFalse(VariableOnlyConfigClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Variable-only config class should not inherit unrelated behavior flags")));
		ASSERT_THAT(AreEqual(FName(TEXT("Editor")), VariableOnlyConfigClass->ClassConfigName, TEXT("Config=Editor should set the editor config name")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), VariableOnlyConfigClass->GetMetaData(TEXT("BlueprintType")), TEXT("NotBlueprintable + BlueprintType should keep variable type metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), VariableOnlyConfigClass->GetMetaData(TEXT("NotBlueprintable")), TEXT("NotBlueprintable metadata should be present")));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), VariableOnlyConfigClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("NotBlueprintable should win over default Blueprint base metadata")));
		ASSERT_THAT(IsFalse(VariableOnlyConfigClass->HasMetaData(TEXT("Blueprintable")), TEXT("NotBlueprintable should remove Blueprintable metadata")));

		ASSERT_THAT(IsTrue(CombinationChildClass->HasAnyClassFlags(CLASS_Config), TEXT("Script child should inherit CLASS_Config from its config parent")));
		ASSERT_THAT(IsTrue(CombinationChildClass->HasAnyClassFlags(CLASS_Transient), TEXT("Script child should inherit CLASS_Transient from its parent")));
		ASSERT_THAT(IsTrue(CombinationChildClass->HasAnyClassFlags(CLASS_Deprecated), TEXT("Script child should inherit CLASS_Deprecated from its parent")));
		ASSERT_THAT(IsTrue(CombinationChildClass->HasAnyClassFlags(CLASS_DefaultToInstanced), TEXT("Script child should inherit CLASS_DefaultToInstanced from its parent")));
		ASSERT_THAT(IsFalse(CombinationChildClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("Script child should not inherit explicit DefaultConfig")));
		ASSERT_THAT(IsTrue(CombinationChildClass->HasAnyClassFlags(CLASS_EditInlineNew), TEXT("Script child should inherit CLASS_EditInlineNew from its parent on initial generation")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), CombinationChildClass->ClassConfigName, TEXT("Script child should inherit parent config name")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), CombinationChildClass->GetMetaData(TEXT("Blueprintable")), TEXT("Script child should apply its own Blueprintable metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), CombinationChildClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("Script child Blueprintable should keep it as a Blueprint base")));
	}

	TEST_METHOD(UClassSpecifierOrderAndBoundaryCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_SpecifierOrderAndBoundaryCombinations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, NotBlueprintable, BlueprintType)
			class UCoverageUClassBlueprintOrderNotBaseObject : UObject
			{
			}

			UCLASS(NotBlueprintable, Blueprintable, BlueprintType)
			class UCoverageUClassBlueprintOrderBaseObject : UObject
			{
			}

			UCLASS(DefaultConfig)
			class UCoverageUClassDefaultConfigWithoutConfigObject : UObject
			{
			}

			UCLASS(NotPlaceable, Abstract, Blueprintable)
			class ACoverageUClassAbstractNotPlaceableActor : AActor
			{
			}

			UCLASS(HideCategories="Rendering", meta=(ShowCategories="Rendering", HideCategories="Input", ClassGroupNames="MetaGroup"))
			class ACoverageUClassMetaOverridesActor : AActor
			{
			}

			UCLASS(ClassGroup="FirstGroup", meta=(ClassGroupNames="SecondGroup"))
			class UCoverageUClassClassGroupOverrideObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassSpecifierOrderAndBoundaryCombinations.as"), ScriptSource)));

		UClass* BlueprintOrderNotBaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassBlueprintOrderNotBaseObject"));
		UClass* BlueprintOrderBaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassBlueprintOrderBaseObject"));
		UClass* DefaultConfigWithoutConfigClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDefaultConfigWithoutConfigObject"));
		UClass* AbstractNotPlaceableClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassAbstractNotPlaceableActor"));
		UClass* MetaOverridesClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassMetaOverridesActor"));
		UClass* ClassGroupOverrideClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassClassGroupOverrideObject"));
		ASSERT_THAT(IsNotNull(BlueprintOrderNotBaseClass, TEXT("Blueprintable then NotBlueprintable class should be generated")));
		ASSERT_THAT(IsNotNull(BlueprintOrderBaseClass, TEXT("NotBlueprintable then Blueprintable class should be generated")));
		ASSERT_THAT(IsNotNull(DefaultConfigWithoutConfigClass, TEXT("DefaultConfig-without-Config class should be generated")));
		ASSERT_THAT(IsNotNull(AbstractNotPlaceableClass, TEXT("Abstract NotPlaceable actor should be generated")));
		ASSERT_THAT(IsNotNull(MetaOverridesClass, TEXT("Metadata override actor should be generated")));
		ASSERT_THAT(IsNotNull(ClassGroupOverrideClass, TEXT("ClassGroup override class should be generated")));
		if (BlueprintOrderNotBaseClass == nullptr || BlueprintOrderBaseClass == nullptr || DefaultConfigWithoutConfigClass == nullptr
			|| AbstractNotPlaceableClass == nullptr || MetaOverridesClass == nullptr || ClassGroupOverrideClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("true")), BlueprintOrderNotBaseClass->GetMetaData(TEXT("BlueprintType")), TEXT("BlueprintType should survive Blueprintable/NotBlueprintable ordering")));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), BlueprintOrderNotBaseClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("NotBlueprintable should win when it appears after Blueprintable")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), BlueprintOrderNotBaseClass->GetMetaData(TEXT("NotBlueprintable")), TEXT("NotBlueprintable metadata should remain when it wins ordering")));
		ASSERT_THAT(IsFalse(BlueprintOrderNotBaseClass->HasMetaData(TEXT("Blueprintable")), TEXT("NotBlueprintable should remove Blueprintable metadata when it wins ordering")));

		ASSERT_THAT(AreEqual(FString(TEXT("true")), BlueprintOrderBaseClass->GetMetaData(TEXT("BlueprintType")), TEXT("BlueprintType should survive NotBlueprintable/Blueprintable ordering")));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), BlueprintOrderBaseClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("NotBlueprintable should win even when Blueprintable appears later")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), BlueprintOrderBaseClass->GetMetaData(TEXT("NotBlueprintable")), TEXT("NotBlueprintable metadata should remain when both Blueprint base specifiers are present")));
		ASSERT_THAT(IsFalse(BlueprintOrderBaseClass->HasMetaData(TEXT("Blueprintable")), TEXT("NotBlueprintable should remove Blueprintable metadata even when Blueprintable appears later")));

		ASSERT_THAT(IsFalse(DefaultConfigWithoutConfigClass->HasAnyClassFlags(CLASS_Config), TEXT("DefaultConfig without Config should not set CLASS_Config")));
		ASSERT_THAT(IsFalse(DefaultConfigWithoutConfigClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig without Config should not set CLASS_DefaultConfig")));
		ASSERT_THAT(IsTrue(DefaultConfigWithoutConfigClass->HasMetaData(TEXT("DefaultConfig")), TEXT("DefaultConfig metadata should still be present when Config is omitted")));

		ASSERT_THAT(IsTrue(AbstractNotPlaceableClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Abstract should combine with NotPlaceable")));
		ASSERT_THAT(IsTrue(AbstractNotPlaceableClass->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("NotPlaceable should combine with Abstract")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AbstractNotPlaceableClass->GetMetaData(TEXT("Blueprintable")), TEXT("Blueprintable should combine with Abstract and NotPlaceable")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), AbstractNotPlaceableClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("Abstract NotPlaceable Blueprintable actor should remain a Blueprint base")));

		ASSERT_THAT(IsTrue(MetaOverridesClass->GetMetaData(TEXT("HideCategories")).Contains(TEXT("Input")), TEXT("meta HideCategories should override the top-level HideCategories value before actor defaults append")));
		ASSERT_THAT(IsFalse(MetaOverridesClass->GetMetaData(TEXT("HideCategories")).Contains(TEXT("Rendering")), TEXT("Overridden HideCategories should no longer contain the top-level category")));
		ASSERT_THAT(IsTrue(MetaOverridesClass->GetMetaData(TEXT("HideCategories")).Contains(TEXT("DefaultComponents")), TEXT("Actor metadata override should still receive the DefaultComponents hidden category")));
		ASSERT_THAT(AreEqual(FString(TEXT("Rendering")), MetaOverridesClass->GetMetaData(TEXT("ShowCategories")), TEXT("meta ShowCategories should round-trip when paired with HideCategories")));
		ASSERT_THAT(AreEqual(FString(TEXT("MetaGroup")), MetaOverridesClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("meta ClassGroupNames should round-trip on actors")));
		ASSERT_THAT(AreEqual(FString(TEXT("SecondGroup")), ClassGroupOverrideClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("meta ClassGroupNames should override ClassGroup in the same macro")));
	}

	TEST_METHOD(UClassSpecifierDuplicateOrderingMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_SpecifierDuplicateOrderingMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Config=Game, Config=Editor, DefaultConfig)
			class UCoverageUClassConfigLastWinsEditorObject : UObject
			{
				UPROPERTY(Config)
				int EditorConfigValue = 31;
			}

			UCLASS(Config=Editor, Config=Game, DefaultConfig)
			class UCoverageUClassConfigLastWinsGameObject : UObject
			{
				UPROPERTY(Config)
				int GameConfigValue = 37;
			}

			UCLASS(ClassGroup="FirstGroup", ClassGroup="SecondGroup", HideCategories="Rendering", HideCategories="Input", meta=(DisplayName="First Display", DisplayName="Second Display", ShortTooltip="First Short", ShortTooltip="Second Short", ToolTip="First ToolTip", ToolTip="Second ToolTip"))
			class UCoverageUClassDuplicateMetadataObject : UObject
			{
			}

			UCLASS(Blueprintable, NotBlueprintable, Blueprintable, BlueprintType)
			class UCoverageUClassRepeatedBlueprintSpecifiersObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassSpecifierDuplicateOrderingMatrix.as"), ScriptSource)));

		UClass* ConfigLastWinsEditorClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassConfigLastWinsEditorObject"));
		UClass* ConfigLastWinsGameClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassConfigLastWinsGameObject"));
		UClass* DuplicateMetadataClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDuplicateMetadataObject"));
		UClass* RepeatedBlueprintSpecifiersClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassRepeatedBlueprintSpecifiersObject"));
		ASSERT_THAT(IsNotNull(ConfigLastWinsEditorClass, TEXT("Config duplicate ordering class ending in Editor should be generated")));
		ASSERT_THAT(IsNotNull(ConfigLastWinsGameClass, TEXT("Config duplicate ordering class ending in Game should be generated")));
		ASSERT_THAT(IsNotNull(DuplicateMetadataClass, TEXT("Duplicate metadata class should be generated")));
		ASSERT_THAT(IsNotNull(RepeatedBlueprintSpecifiersClass, TEXT("Repeated Blueprint specifier class should be generated")));
		if (ConfigLastWinsEditorClass == nullptr || ConfigLastWinsGameClass == nullptr || DuplicateMetadataClass == nullptr || RepeatedBlueprintSpecifiersClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ConfigLastWinsEditorClass->HasAnyClassFlags(CLASS_Config), TEXT("Repeated Config specifiers should still set CLASS_Config")));
		ASSERT_THAT(IsTrue(ConfigLastWinsEditorClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig should still apply after repeated Config specifiers")));
		ASSERT_THAT(AreEqual(FName(TEXT("Editor")), ConfigLastWinsEditorClass->ClassConfigName, TEXT("Later Config=Editor should override earlier Config=Game")));
		FProperty* EditorConfigValueProperty = ConfigLastWinsEditorClass->FindPropertyByName(TEXT("EditorConfigValue"));
		ASSERT_THAT(IsNotNull(EditorConfigValueProperty, TEXT("EditorConfigValue should be generated")));
		if (EditorConfigValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EditorConfigValueProperty->HasAnyPropertyFlags(CPF_Config), TEXT("Config property should remain config when class config is overwritten")));

		ASSERT_THAT(IsTrue(ConfigLastWinsGameClass->HasAnyClassFlags(CLASS_Config), TEXT("Repeated Config specifiers should set CLASS_Config regardless of order")));
		ASSERT_THAT(IsTrue(ConfigLastWinsGameClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig should apply when repeated Config ends in Game")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), ConfigLastWinsGameClass->ClassConfigName, TEXT("Later Config=Game should override earlier Config=Editor")));
		FProperty* GameConfigValueProperty = ConfigLastWinsGameClass->FindPropertyByName(TEXT("GameConfigValue"));
		ASSERT_THAT(IsNotNull(GameConfigValueProperty, TEXT("GameConfigValue should be generated")));
		if (GameConfigValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(GameConfigValueProperty->HasAnyPropertyFlags(CPF_Config), TEXT("Config property should remain config when class config order is reversed")));

		ASSERT_THAT(AreEqual(FString(TEXT("SecondGroup")), DuplicateMetadataClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("Later ClassGroup should override earlier ClassGroup")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input")), DuplicateMetadataClass->GetMetaData(TEXT("HideCategories")), TEXT("Later HideCategories should override earlier HideCategories")));
		ASSERT_THAT(AreEqual(FString(TEXT("Second Display")), DuplicateMetadataClass->GetMetaData(TEXT("DisplayName")), TEXT("Later DisplayName metadata should override earlier DisplayName")));
		ASSERT_THAT(AreEqual(FString(TEXT("Second Short")), DuplicateMetadataClass->GetMetaData(TEXT("ShortTooltip")), TEXT("Later ShortTooltip metadata should override earlier ShortTooltip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Second ToolTip")), DuplicateMetadataClass->GetMetaData(TEXT("ToolTip")), TEXT("Later ToolTip metadata should override earlier ToolTip")));

		ASSERT_THAT(AreEqual(FString(TEXT("true")), RepeatedBlueprintSpecifiersClass->GetMetaData(TEXT("BlueprintType")), TEXT("BlueprintType should survive repeated Blueprint base specifiers")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), RepeatedBlueprintSpecifiersClass->GetMetaData(TEXT("NotBlueprintable")), TEXT("NotBlueprintable metadata should remain when repeated with Blueprintable")));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), RepeatedBlueprintSpecifiersClass->GetMetaData(TEXT("IsBlueprintBase")), TEXT("NotBlueprintable should remain the winning Blueprint base state after repeated Blueprintable")));
		ASSERT_THAT(IsFalse(RepeatedBlueprintSpecifiersClass->HasMetaData(TEXT("Blueprintable")), TEXT("NotBlueprintable should suppress Blueprintable metadata after duplicate ordering normalization")));
	}

	TEST_METHOD(UClassSpecifierListSyntaxBoundaryMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_SpecifierListSyntaxBoundaryMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(ClassGroup=(ListGroup), HideCategories=(Rendering,Input), DefaultConfig=(Ignored), ComponentWrapperClass=(Ignored), meta=(DisplayName=("List Display"), ShortTooltip=("List Short"), ToolTip=("List ToolTip"), ShowCategories=(Rendering,Input), AutoExpandCategories=(Coverage,Advanced), AutoCollapseCategories=(Collapsed), HideFunctions=(HiddenA,HiddenB), SparseClassDataTypes=(SparseA,SparseB), ConversionRoot=(Ignored), ChildCanTick=(Ignored), CollapseCategories=(Ignored), DontCollapseCategories=(Ignored)))
			class UCoverageUClassListSyntaxObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassSpecifierListSyntaxBoundaryMatrix.as"), ScriptSource)));

		UClass* ListSyntaxClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassListSyntaxObject"));
		ASSERT_THAT(IsNotNull(ListSyntaxClass, TEXT("List-syntax UCLASS boundary class should be generated")));
		if (ListSyntaxClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(ListSyntaxClass->HasAnyClassFlags(CLASS_DefaultConfig), TEXT("DefaultConfig list syntax without Config should not set CLASS_DefaultConfig")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("ClassGroupNames")), TEXT("ClassGroup list syntax currently does not serialize list entries into ClassGroupNames")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("HideCategories")), TEXT("HideCategories list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("DisplayName")), TEXT("DisplayName list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("ShortTooltip")), TEXT("ShortTooltip list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("ToolTip")), TEXT("ToolTip list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("ShowCategories")), TEXT("ShowCategories list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("AutoExpandCategories")), TEXT("AutoExpandCategories list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("AutoCollapseCategories")), TEXT("AutoCollapseCategories list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("HideFunctions")), TEXT("HideFunctions list syntax currently does not serialize list entries")));
		ASSERT_THAT(AreEqual(FString(), ListSyntaxClass->GetMetaData(TEXT("SparseClassDataTypes")), TEXT("SparseClassDataTypes list syntax currently does not serialize list entries")));
	}

	TEST_METHOD(UClassNonActorComponentSpecifierMetadataBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_NonActorComponentSpecifierMetadataBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassNonActorSpecifierComponent : UActorComponent
			{
			}

			UCLASS()
			class UCoverageUClassNonActorComponentSpecifierBaseOwner : UObject
			{
				UPROPERTY(DefaultComponent, ShowOnActor)
				UCoverageUClassNonActorSpecifierComponent Logic;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassNonActorComponentSpecifierMetadataBoundary.as"), ScriptSource)));

		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassNonActorSpecifierComponent"));
		UClass* BaseOwnerClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassNonActorComponentSpecifierBaseOwner"));
		ASSERT_THAT(IsNotNull(ComponentClass, TEXT("Non-actor component specifier component class should be generated")));
		ASSERT_THAT(IsNotNull(BaseOwnerClass, TEXT("Non-actor component specifier base owner class should be generated")));
		if (ComponentClass == nullptr || BaseOwnerClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(BaseOwnerClass->IsChildOf(AActor::StaticClass()), TEXT("Non-actor base owner should remain outside actor finalization")));
		ASSERT_THAT(IsTrue(BaseOwnerClass->HasAnyClassFlags(CLASS_HasInstancedReference), TEXT("DefaultComponent-style object references should mark the non-actor base owner class")));

		FObjectPropertyBase* LogicProperty = FindFProperty<FObjectPropertyBase>(BaseOwnerClass, TEXT("Logic"));
		ASSERT_THAT(IsNotNull(LogicProperty, TEXT("DefaultComponent metadata property should be generated on non-actor owner")));
		if (LogicProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ComponentClass, LogicProperty->PropertyClass, TEXT("Logic should preserve the component property type")));
		ASSERT_THAT(IsTrue(LogicProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("DefaultComponent specifier should remain visible as property metadata")));
		ASSERT_THAT(IsTrue(LogicProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should emit EditInline metadata on non-actor properties")));
		ASSERT_THAT(IsTrue(LogicProperty->HasAnyPropertyFlags(CPF_InstancedReference), TEXT("DefaultComponent should still set instanced-reference property flags")));
		ASSERT_THAT(IsTrue(LogicProperty->HasAnyPropertyFlags(CPF_ExportObject), TEXT("DefaultComponent should still set export-object property flags")));
	}

	TEST_METHOD(UClassComponentDerivedTypeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_ComponentDerivedTypeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDerivedStaticMeshComponent : UStaticMeshComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedSkeletalMeshComponent : USkeletalMeshComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedCapsuleComponent : UCapsuleComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedBoxComponent : UBoxComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedSphereComponent : USphereComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedSpringArmComponent : USpringArmComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedCameraComponent : UCameraComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedPointLightComponent : UPointLightComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedArrowComponent : UArrowComponent
			{
			}

			UCLASS()
			class UCoverageUClassDerivedCharacterMovementComponent : UCharacterMovementComponent
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassComponentDerivedTypeMatrix.as"), ScriptSource)));

		UClass* StaticMeshComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedStaticMeshComponent"));
		UClass* SkeletalMeshComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedSkeletalMeshComponent"));
		UClass* CapsuleComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedCapsuleComponent"));
		UClass* BoxComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedBoxComponent"));
		UClass* SphereComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedSphereComponent"));
		UClass* SpringArmComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedSpringArmComponent"));
		UClass* CameraComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedCameraComponent"));
		UClass* PointLightComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedPointLightComponent"));
		UClass* ArrowComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedArrowComponent"));
		UClass* CharacterMovementComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDerivedCharacterMovementComponent"));
		ASSERT_THAT(IsNotNull(StaticMeshComponentClass, TEXT("Static mesh component subclass should be generated")));
		ASSERT_THAT(IsNotNull(SkeletalMeshComponentClass, TEXT("Skeletal mesh component subclass should be generated")));
		ASSERT_THAT(IsNotNull(CapsuleComponentClass, TEXT("Capsule component subclass should be generated")));
		ASSERT_THAT(IsNotNull(BoxComponentClass, TEXT("Box component subclass should be generated")));
		ASSERT_THAT(IsNotNull(SphereComponentClass, TEXT("Sphere component subclass should be generated")));
		ASSERT_THAT(IsNotNull(SpringArmComponentClass, TEXT("Spring arm component subclass should be generated")));
		ASSERT_THAT(IsNotNull(CameraComponentClass, TEXT("Camera component subclass should be generated")));
		ASSERT_THAT(IsNotNull(PointLightComponentClass, TEXT("Point light component subclass should be generated")));
		ASSERT_THAT(IsNotNull(ArrowComponentClass, TEXT("Arrow component subclass should be generated")));
		ASSERT_THAT(IsNotNull(CharacterMovementComponentClass, TEXT("Character movement component subclass should be generated")));
		if (StaticMeshComponentClass == nullptr || SkeletalMeshComponentClass == nullptr || CapsuleComponentClass == nullptr
			|| BoxComponentClass == nullptr || SphereComponentClass == nullptr || SpringArmComponentClass == nullptr
			|| CameraComponentClass == nullptr || PointLightComponentClass == nullptr || ArrowComponentClass == nullptr
			|| CharacterMovementComponentClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(StaticMeshComponentClass->IsChildOf(UStaticMeshComponent::StaticClass()), TEXT("Static mesh component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(SkeletalMeshComponentClass->IsChildOf(USkeletalMeshComponent::StaticClass()), TEXT("Skeletal mesh component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(CapsuleComponentClass->IsChildOf(UCapsuleComponent::StaticClass()), TEXT("Capsule component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(BoxComponentClass->IsChildOf(UBoxComponent::StaticClass()), TEXT("Box component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(SphereComponentClass->IsChildOf(USphereComponent::StaticClass()), TEXT("Sphere component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(SpringArmComponentClass->IsChildOf(USpringArmComponent::StaticClass()), TEXT("Spring arm component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(CameraComponentClass->IsChildOf(UCameraComponent::StaticClass()), TEXT("Camera component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(PointLightComponentClass->IsChildOf(UPointLightComponent::StaticClass()), TEXT("Point light component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(ArrowComponentClass->IsChildOf(UArrowComponent::StaticClass()), TEXT("Arrow component subclass should preserve its native base")));
		ASSERT_THAT(IsTrue(CharacterMovementComponentClass->IsChildOf(UCharacterMovementComponent::StaticClass()), TEXT("Character movement component subclass should preserve its native base")));

		ASSERT_THAT(IsTrue(StaticMeshComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Static mesh component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(SkeletalMeshComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Skeletal mesh component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(CapsuleComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Capsule component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(BoxComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Box component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(SphereComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Sphere component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(SpringArmComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Spring arm component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(CameraComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Camera component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(PointLightComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Point light component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(ArrowComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Arrow component subclass should still be a scene component")));
		ASSERT_THAT(IsTrue(CharacterMovementComponentClass->IsChildOf(UActorComponent::StaticClass()), TEXT("Character movement component subclass should still be an actor component")));
		ASSERT_THAT(IsFalse(CharacterMovementComponentClass->IsChildOf(USceneComponent::StaticClass()), TEXT("Character movement component subclass should cover the non-scene component branch")));
	}

	TEST_METHOD(UClassDefaultComponentSpecifierPermutationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DefaultComponentSpecifierPermutationMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPermutationSceneComponent : USceneComponent
			{
				UPROPERTY()
				int SceneMarker = 5;
			}

			UCLASS()
			class UCoverageUClassPermutationLogicComponent : UActorComponent
			{
				UPROPERTY()
				int LogicMarker = 7;
			}

			UCLASS()
			class ACoverageUClassDefaultComponentPermutationsActor : AActor
			{
				UPROPERTY(ShowOnActor, DefaultComponent, RootComponent, EditAnywhere, BlueprintReadOnly)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="MeshSocket")
				UStaticMeshComponent Mesh;

				UPROPERTY(DefaultComponent, Attach=Mesh)
				USkeletalMeshComponent Skeletal;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCapsuleComponent Capsule;

				UPROPERTY(DefaultComponent, Attach=Capsule)
				UBoxComponent Box;

				UPROPERTY(DefaultComponent, Attach=Root)
				USphereComponent Sphere;

				UPROPERTY(DefaultComponent, Attach=Root)
				USpringArmComponent SpringArm;

				UPROPERTY(DefaultComponent, Attach=SpringArm)
				UCameraComponent Camera;

				UPROPERTY(DefaultComponent, Attach=Root)
				UPointLightComponent Light;

				UPROPERTY(DefaultComponent, Attach=Root)
				UArrowComponent Arrow;

				UPROPERTY(DefaultComponent)
				UCharacterMovementComponent Movement;

				UPROPERTY(DefaultComponent)
				UCoverageUClassPermutationLogicComponent Logic;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCoverageUClassPermutationSceneComponent ScriptScene;

				UPROPERTY()
				bool AllComponentsCreated = false;

				UPROPERTY()
				bool AttachmentPermutationValid = false;

				UPROPERTY()
				bool NonSceneComponentsValid = false;

				UPROPERTY()
				bool ScriptComponentsValid = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (Root == nullptr ||
						Mesh == nullptr ||
						Skeletal == nullptr ||
						Capsule == nullptr ||
						Box == nullptr ||
						Sphere == nullptr ||
						SpringArm == nullptr ||
						Camera == nullptr ||
						Light == nullptr ||
						Arrow == nullptr ||
						Movement == nullptr ||
						Logic == nullptr ||
						ScriptScene == nullptr)
					{
						return;
					}

					AllComponentsCreated =
						true;

					AttachmentPermutationValid =
						Mesh.GetAttachParent() == Root &&
						Mesh.GetAttachSocketName() == n"MeshSocket" &&
						Skeletal.GetAttachParent() == Mesh &&
						Capsule.GetAttachParent() == Root &&
						Box.GetAttachParent() == Capsule &&
						Sphere.GetAttachParent() == Root &&
						SpringArm.GetAttachParent() == Root &&
						Camera.GetAttachParent() == SpringArm &&
						Light.GetAttachParent() == Root &&
						Arrow.GetAttachParent() == Root &&
						ScriptScene.GetAttachParent() == Root;

					NonSceneComponentsValid =
						Movement.GetOwner() == this &&
						Logic.GetOwner() == this &&
						Logic.LogicMarker == 7;

					ScriptComponentsValid =
						ScriptScene.GetOwner() == this &&
						ScriptScene.SceneMarker == 5;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentSpecifierPermutationMatrix.as"), ScriptSource)));

		UClass* SceneComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPermutationSceneComponent"));
		UClass* LogicComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPermutationLogicComponent"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentPermutationsActor"));
		ASSERT_THAT(IsNotNull(SceneComponentClass, TEXT("Script scene component permutation class should be generated")));
		ASSERT_THAT(IsNotNull(LogicComponentClass, TEXT("Script logic component permutation class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Default component permutation actor should be generated")));
		if (SceneComponentClass == nullptr || LogicComponentClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* RootProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Root"));
		FObjectPropertyBase* MeshProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Mesh"));
		FObjectPropertyBase* SkeletalProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Skeletal"));
		FObjectPropertyBase* CapsuleProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Capsule"));
		FObjectPropertyBase* BoxProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Box"));
		FObjectPropertyBase* SphereProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Sphere"));
		FObjectPropertyBase* SpringArmProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SpringArm"));
		FObjectPropertyBase* CameraProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Camera"));
		FObjectPropertyBase* LightProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Light"));
		FObjectPropertyBase* ArrowProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Arrow"));
		FObjectPropertyBase* MovementProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Movement"));
		FObjectPropertyBase* LogicProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Logic"));
		FObjectPropertyBase* ScriptSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ScriptScene"));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Root default component property should be reflected")));
		ASSERT_THAT(IsNotNull(MeshProperty, TEXT("Mesh default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SkeletalProperty, TEXT("Skeletal default component property should be reflected")));
		ASSERT_THAT(IsNotNull(CapsuleProperty, TEXT("Capsule default component property should be reflected")));
		ASSERT_THAT(IsNotNull(BoxProperty, TEXT("Box default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SphereProperty, TEXT("Sphere default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SpringArmProperty, TEXT("SpringArm default component property should be reflected")));
		ASSERT_THAT(IsNotNull(CameraProperty, TEXT("Camera default component property should be reflected")));
		ASSERT_THAT(IsNotNull(LightProperty, TEXT("Light default component property should be reflected")));
		ASSERT_THAT(IsNotNull(ArrowProperty, TEXT("Arrow default component property should be reflected")));
		ASSERT_THAT(IsNotNull(MovementProperty, TEXT("Movement default component property should be reflected")));
		ASSERT_THAT(IsNotNull(LogicProperty, TEXT("Logic default component property should be reflected")));
		ASSERT_THAT(IsNotNull(ScriptSceneProperty, TEXT("ScriptScene default component property should be reflected")));
		if (RootProperty == nullptr || MeshProperty == nullptr || SkeletalProperty == nullptr || CapsuleProperty == nullptr || BoxProperty == nullptr
			|| SphereProperty == nullptr || SpringArmProperty == nullptr || CameraProperty == nullptr || LightProperty == nullptr || ArrowProperty == nullptr
			|| MovementProperty == nullptr || LogicProperty == nullptr || ScriptSceneProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(USceneComponent::StaticClass(), RootProperty->PropertyClass, TEXT("Root should reflect a native scene component")));
		ASSERT_THAT(AreEqual(UStaticMeshComponent::StaticClass(), MeshProperty->PropertyClass, TEXT("Mesh should reflect UStaticMeshComponent")));
		ASSERT_THAT(AreEqual(USkeletalMeshComponent::StaticClass(), SkeletalProperty->PropertyClass, TEXT("Skeletal should reflect USkeletalMeshComponent")));
		ASSERT_THAT(AreEqual(UCapsuleComponent::StaticClass(), CapsuleProperty->PropertyClass, TEXT("Capsule should reflect UCapsuleComponent")));
		ASSERT_THAT(AreEqual(UBoxComponent::StaticClass(), BoxProperty->PropertyClass, TEXT("Box should reflect UBoxComponent")));
		ASSERT_THAT(AreEqual(USphereComponent::StaticClass(), SphereProperty->PropertyClass, TEXT("Sphere should reflect USphereComponent")));
		ASSERT_THAT(AreEqual(USpringArmComponent::StaticClass(), SpringArmProperty->PropertyClass, TEXT("SpringArm should reflect USpringArmComponent")));
		ASSERT_THAT(AreEqual(UCameraComponent::StaticClass(), CameraProperty->PropertyClass, TEXT("Camera should reflect UCameraComponent")));
		ASSERT_THAT(AreEqual(UPointLightComponent::StaticClass(), LightProperty->PropertyClass, TEXT("Light should reflect UPointLightComponent")));
		ASSERT_THAT(AreEqual(UArrowComponent::StaticClass(), ArrowProperty->PropertyClass, TEXT("Arrow should reflect UArrowComponent")));
		ASSERT_THAT(AreEqual(UCharacterMovementComponent::StaticClass(), MovementProperty->PropertyClass, TEXT("Movement should reflect UCharacterMovementComponent")));
		ASSERT_THAT(AreEqual(LogicComponentClass, LogicProperty->PropertyClass, TEXT("Logic should preserve the script component class")));
		ASSERT_THAT(AreEqual(SceneComponentClass, ScriptSceneProperty->PropertyClass, TEXT("ScriptScene should preserve the script scene component class")));

		ASSERT_THAT(IsTrue(RootProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("ShowOnActor before DefaultComponent should keep Root editable")));
		ASSERT_THAT(IsTrue(RootProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("DefaultComponent + BlueprintReadOnly should keep Root Blueprint-visible")));
		ASSERT_THAT(IsTrue(RootProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly should mark Root read-only")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("Root should keep DefaultComponent metadata despite specifier order")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("RootComponent")), TEXT("Root should keep RootComponent metadata")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should add EditInline metadata even when it precedes DefaultComponent")));
		ASSERT_THAT(AreEqual(FString(TEXT("Root")), MeshProperty->GetMetaData(TEXT("Attach")), TEXT("Mesh should attach to Root in metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("MeshSocket")), MeshProperty->GetMetaData(TEXT("AttachSocket")), TEXT("Mesh should preserve AttachSocket metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Mesh")), SkeletalProperty->GetMetaData(TEXT("Attach")), TEXT("Skeletal should attach to Mesh in metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default component permutation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllComponentsCreated"), true, TEXT("All declared default component permutations should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AttachmentPermutationValid"), true, TEXT("DefaultComponent Attach and AttachSocket permutations should materialize"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NonSceneComponentsValid"), true, TEXT("Non-scene DefaultComponents should be owned by the actor without attachment metadata"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ScriptComponentsValid"), true, TEXT("Script-derived component default components should keep defaults and ownership"))));
		ASSERT_THAT(AreEqual(13, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Actor should own exactly the declared native and script default components")));
	}

	TEST_METHOD(UClassDefaultComponentImplicitRootPermutation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_DefaultComponentImplicitRootPermutation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassImplicitDefaultComponentActor : AActor
			{
				UPROPERTY(DefaultComponent)
				USceneComponent FirstScene;

				UPROPERTY(DefaultComponent)
				USceneComponent SecondScene;

				UPROPERTY(DefaultComponent)
				UActorComponent Logic;

				UPROPERTY(DefaultComponent, Attach=SecondScene, AttachSocket="DelayedSocket")
				USceneComponent DelayedChild;

				UPROPERTY()
				bool FirstSceneBecameRoot = false;

				UPROPERTY()
				bool SecondSceneAttachedToRoot = false;

				UPROPERTY()
				bool LogicHasNoSceneAttachment = false;

				UPROPERTY()
				bool DelayedAttachResolved = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (FirstScene == nullptr ||
						SecondScene == nullptr ||
						Logic == nullptr ||
						DelayedChild == nullptr)
					{
						return;
					}

					FirstSceneBecameRoot =
						FirstScene.GetAttachParent() == nullptr;
					SecondSceneAttachedToRoot =
						SecondScene.GetAttachParent() == FirstScene;
					LogicHasNoSceneAttachment =
						Logic.GetOwner() == this;
					DelayedAttachResolved =
						DelayedChild.GetAttachParent() == SecondScene &&
						DelayedChild.GetAttachSocketName() == n"DelayedSocket";
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentImplicitRootPermutation.as"), ScriptSource)));

		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassImplicitDefaultComponentActor"));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Implicit default component actor should be generated")));
		if (ActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* FirstSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("FirstScene"));
		FObjectPropertyBase* SecondSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SecondScene"));
		FObjectPropertyBase* LogicProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Logic"));
		FObjectPropertyBase* DelayedChildProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("DelayedChild"));
		ASSERT_THAT(IsNotNull(FirstSceneProperty, TEXT("FirstScene property should be reflected")));
		ASSERT_THAT(IsNotNull(SecondSceneProperty, TEXT("SecondScene property should be reflected")));
		ASSERT_THAT(IsNotNull(LogicProperty, TEXT("Logic property should be reflected")));
		ASSERT_THAT(IsNotNull(DelayedChildProperty, TEXT("DelayedChild property should be reflected")));
		if (FirstSceneProperty == nullptr || SecondSceneProperty == nullptr || LogicProperty == nullptr || DelayedChildProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FirstSceneProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("FirstScene should keep DefaultComponent metadata")));
		ASSERT_THAT(IsFalse(FirstSceneProperty->HasMetaData(TEXT("RootComponent")), TEXT("FirstScene should cover implicit root without RootComponent metadata")));
		ASSERT_THAT(IsFalse(SecondSceneProperty->HasMetaData(TEXT("Attach")), TEXT("SecondScene should cover implicit attachment without Attach metadata")));
		ASSERT_THAT(IsFalse(LogicProperty->HasMetaData(TEXT("Attach")), TEXT("Non-scene default component should carry no implicit attachment metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("SecondScene")), DelayedChildProperty->GetMetaData(TEXT("Attach")), TEXT("DelayedChild should preserve a forward attach target")));
		ASSERT_THAT(AreEqual(FString(TEXT("DelayedSocket")), DelayedChildProperty->GetMetaData(TEXT("AttachSocket")), TEXT("DelayedChild should preserve AttachSocket metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Implicit component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FirstSceneBecameRoot"), true, TEXT("First DefaultComponent scene should become root when no root is declared"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SecondSceneAttachedToRoot"), true, TEXT("Second DefaultComponent scene should attach to the implicit root"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LogicHasNoSceneAttachment"), true, TEXT("Non-scene DefaultComponent should still be actor-owned"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DelayedAttachResolved"), true, TEXT("Delayed Attach target declared later should resolve at runtime"))));
		UObject* FirstSceneObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("FirstScene"), FirstSceneObject), TEXT("FirstScene property should be readable")));
		ASSERT_THAT(AreEqual(FirstSceneObject, static_cast<UObject*>(Actor->GetRootComponent()), TEXT("First DefaultComponent scene should be actor root in C++")));
		ASSERT_THAT(AreEqual(4, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Implicit component actor should own exactly four declared default components")));
	}

	TEST_METHOD(UClassOverrideComponentSpecifierMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_OverrideComponentSpecifierMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassOverrideRootComponent : USceneComponent
			{
			}

			UCLASS()
			class UCoverageUClassOverrideBaseSceneComponent : USceneComponent
			{
			}

			UCLASS()
			class UCoverageUClassOverrideDerivedSceneComponent : UCoverageUClassOverrideBaseSceneComponent
			{
			}

			UCLASS()
			class UCoverageUClassOverrideBaseLogicComponent : UActorComponent
			{
			}

			UCLASS()
			class UCoverageUClassOverrideDerivedLogicComponent : UCoverageUClassOverrideBaseLogicComponent
			{
			}

			UCLASS()
			class ACoverageUClassOverrideBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCoverageUClassOverrideRootComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="BaseSocket")
				UCoverageUClassOverrideBaseSceneComponent BaseScene;

				UPROPERTY(DefaultComponent)
				UCoverageUClassOverrideBaseLogicComponent BaseLogic;
			}

			UCLASS()
			class ACoverageUClassOverrideChildActor : ACoverageUClassOverrideBaseActor
			{
				UPROPERTY(OverrideComponent=BaseScene)
				UCoverageUClassOverrideDerivedSceneComponent ReplacementScene;

				UPROPERTY(OverrideComponent=BaseLogic)
				UCoverageUClassOverrideDerivedLogicComponent ReplacementLogic;

				UPROPERTY(DefaultComponent)
				UCameraComponent Camera;

				UPROPERTY()
				bool ReplacementPropertiesAssigned = false;

				UPROPERTY()
				bool ReplacementAttachmentPreserved = false;

				UPROPERTY()
				bool ReplacementLogicAssigned = false;

				UPROPERTY()
				bool ChildDefaultAttachedToInheritedRoot = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (ReplacementScene == nullptr ||
						ReplacementLogic == nullptr ||
						Camera == nullptr ||
						Root == nullptr)
					{
						return;
					}

					ReplacementPropertiesAssigned =
						true;
					ReplacementAttachmentPreserved =
						ReplacementScene.GetAttachParent() == Root &&
						ReplacementScene.GetAttachSocketName() == n"BaseSocket";
					ReplacementLogicAssigned =
						ReplacementLogic.GetOwner() == this;
					ChildDefaultAttachedToInheritedRoot =
						Camera.GetAttachParent() == Root;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassOverrideComponentSpecifierMatrix.as"), ScriptSource)));

		UClass* BaseSceneClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassOverrideBaseSceneComponent"));
		UClass* DerivedSceneClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassOverrideDerivedSceneComponent"));
		UClass* BaseLogicClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassOverrideBaseLogicComponent"));
		UClass* DerivedLogicClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassOverrideDerivedLogicComponent"));
		UClass* ChildActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassOverrideChildActor"));
		ASSERT_THAT(IsNotNull(BaseSceneClass, TEXT("Override base scene component class should be generated")));
		ASSERT_THAT(IsNotNull(DerivedSceneClass, TEXT("Override derived scene component class should be generated")));
		ASSERT_THAT(IsNotNull(BaseLogicClass, TEXT("Override base logic component class should be generated")));
		ASSERT_THAT(IsNotNull(DerivedLogicClass, TEXT("Override derived logic component class should be generated")));
		ASSERT_THAT(IsNotNull(ChildActorClass, TEXT("Override child actor class should be generated")));
		if (BaseSceneClass == nullptr || DerivedSceneClass == nullptr || BaseLogicClass == nullptr || DerivedLogicClass == nullptr || ChildActorClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DerivedSceneClass->IsChildOf(BaseSceneClass), TEXT("Scene replacement class should inherit the base scene component class")));
		ASSERT_THAT(IsTrue(DerivedLogicClass->IsChildOf(BaseLogicClass), TEXT("Logic replacement class should inherit the base logic component class")));

		FObjectPropertyBase* ReplacementSceneProperty = FindFProperty<FObjectPropertyBase>(ChildActorClass, TEXT("ReplacementScene"));
		FObjectPropertyBase* ReplacementLogicProperty = FindFProperty<FObjectPropertyBase>(ChildActorClass, TEXT("ReplacementLogic"));
		FObjectPropertyBase* CameraProperty = FindFProperty<FObjectPropertyBase>(ChildActorClass, TEXT("Camera"));
		ASSERT_THAT(IsNotNull(ReplacementSceneProperty, TEXT("ReplacementScene override component property should be reflected")));
		ASSERT_THAT(IsNotNull(ReplacementLogicProperty, TEXT("ReplacementLogic override component property should be reflected")));
		ASSERT_THAT(IsNotNull(CameraProperty, TEXT("Camera child default component property should be reflected")));
		if (ReplacementSceneProperty == nullptr || ReplacementLogicProperty == nullptr || CameraProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(DerivedSceneClass, ReplacementSceneProperty->PropertyClass, TEXT("ReplacementScene should preserve the derived script scene component type")));
		ASSERT_THAT(AreEqual(DerivedLogicClass, ReplacementLogicProperty->PropertyClass, TEXT("ReplacementLogic should preserve the derived script logic component type")));
		ASSERT_THAT(AreEqual(FString(TEXT("BaseScene")), ReplacementSceneProperty->GetMetaData(TEXT("OverrideComponent")), TEXT("ReplacementScene should target the base scene component by name")));
		ASSERT_THAT(AreEqual(FString(TEXT("BaseLogic")), ReplacementLogicProperty->GetMetaData(TEXT("OverrideComponent")), TEXT("ReplacementLogic should target the base logic component by name")));
		ASSERT_THAT(IsFalse(CameraProperty->HasMetaData(TEXT("Attach")), TEXT("New child default components should not need explicit inherited attach metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ChildActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Override component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReplacementPropertiesAssigned"), true, TEXT("OverrideComponent properties should point at created replacement instances"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReplacementAttachmentPreserved"), true, TEXT("OverrideComponent should preserve the base component attachment metadata"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReplacementLogicAssigned"), true, TEXT("Non-scene OverrideComponent should be owned by the actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildDefaultAttachedToInheritedRoot"), true, TEXT("DefaultComponent should attach to an inherited default component while overrides are present"))));

		UObject* ReplacementSceneObject = nullptr;
		UObject* ReplacementLogicObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ReplacementScene"), ReplacementSceneObject), TEXT("ReplacementScene property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ReplacementLogic"), ReplacementLogicObject), TEXT("ReplacementLogic property should be readable")));
		UActorComponent* BaseSceneComponent = FindActorComponentByName(Actor, TEXT("BaseScene"));
		UActorComponent* BaseLogicComponent = FindActorComponentByName(Actor, TEXT("BaseLogic"));
		ASSERT_THAT(IsNotNull(BaseSceneComponent, TEXT("Overridden scene component should keep the base component object name")));
		ASSERT_THAT(IsNotNull(BaseLogicComponent, TEXT("Overridden logic component should keep the base component object name")));
		if (ReplacementSceneObject == nullptr || ReplacementLogicObject == nullptr || BaseSceneComponent == nullptr || BaseLogicComponent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<UObject*>(BaseSceneComponent), ReplacementSceneObject, TEXT("ReplacementScene property should point at the overridden BaseScene object")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(BaseLogicComponent), ReplacementLogicObject, TEXT("ReplacementLogic property should point at the overridden BaseLogic object")));
		ASSERT_THAT(IsTrue(BaseSceneComponent->IsA(DerivedSceneClass), TEXT("BaseScene object should materialize as the derived replacement scene component")));
		ASSERT_THAT(IsTrue(BaseLogicComponent->IsA(DerivedLogicClass), TEXT("BaseLogic object should materialize as the derived replacement logic component")));
		ASSERT_THAT(AreEqual(4, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Override actor should own root, two replacements, and the extra camera component")));
	}

	TEST_METHOD(UClassNativeParentOverrideComponentMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClass_NativeParentOverrideComponentMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassOverrideCapsuleComponent : UCapsuleComponent
			{
			}

			UCLASS()
			class UCoverageUClassOverrideMeshComponent : USkeletalMeshComponent
			{
			}

			UCLASS()
			class ACoverageUClassNativeOverrideCharacter : ACharacter
			{
				UPROPERTY(OverrideComponent=CollisionCylinder)
				UCoverageUClassOverrideCapsuleComponent ReplacementCapsule;

				UPROPERTY(OverrideComponent=CharacterMesh0)
				UCoverageUClassOverrideMeshComponent ReplacementMesh;

				UPROPERTY()
				bool NativeOverridePropertiesAssigned = false;

				UPROPERTY()
				bool NativeOverrideTypesMaterialized = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					NativeOverridePropertiesAssigned =
						ReplacementCapsule != nullptr &&
						ReplacementMesh != nullptr;
					NativeOverrideTypesMaterialized =
						NativeOverridePropertiesAssigned;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassNativeParentOverrideComponentMatrix.as"), ScriptSource)));

		UClass* CapsuleClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassOverrideCapsuleComponent"));
		UClass* MeshClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassOverrideMeshComponent"));
		UClass* CharacterClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassNativeOverrideCharacter"));
		ASSERT_THAT(IsNotNull(CapsuleClass, TEXT("Native override capsule replacement class should be generated")));
		ASSERT_THAT(IsNotNull(MeshClass, TEXT("Native override mesh replacement class should be generated")));
		ASSERT_THAT(IsNotNull(CharacterClass, TEXT("Native override character class should be generated")));
		if (CapsuleClass == nullptr || MeshClass == nullptr || CharacterClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* ReplacementCapsuleProperty = FindFProperty<FObjectPropertyBase>(CharacterClass, TEXT("ReplacementCapsule"));
		FObjectPropertyBase* ReplacementMeshProperty = FindFProperty<FObjectPropertyBase>(CharacterClass, TEXT("ReplacementMesh"));
		ASSERT_THAT(IsNotNull(ReplacementCapsuleProperty, TEXT("ReplacementCapsule property should be reflected")));
		ASSERT_THAT(IsNotNull(ReplacementMeshProperty, TEXT("ReplacementMesh property should be reflected")));
		if (ReplacementCapsuleProperty == nullptr || ReplacementMeshProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(CapsuleClass, ReplacementCapsuleProperty->PropertyClass, TEXT("ReplacementCapsule should preserve the script capsule subclass")));
		ASSERT_THAT(AreEqual(MeshClass, ReplacementMeshProperty->PropertyClass, TEXT("ReplacementMesh should preserve the script mesh subclass")));
		ASSERT_THAT(AreEqual(ACharacter::CapsuleComponentName.ToString(), ReplacementCapsuleProperty->GetMetaData(TEXT("OverrideComponent")), TEXT("ReplacementCapsule should target ACharacter's native capsule component")));
		ASSERT_THAT(AreEqual(ACharacter::MeshComponentName.ToString(), ReplacementMeshProperty->GetMetaData(TEXT("OverrideComponent")), TEXT("ReplacementMesh should target ACharacter's native mesh component")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, CharacterClass);
		ACharacter* Character = Cast<ACharacter>(Actor);
		ASSERT_THAT(IsNotNull(Character, TEXT("Native override character should spawn as ACharacter")));
		if (Character == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Character);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Character, TEXT("NativeOverridePropertiesAssigned"), true, TEXT("OverrideComponent properties should be assigned for native parent components"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Character, TEXT("NativeOverrideTypesMaterialized"), true, TEXT("Native parent component accessors should see the script replacement classes"))));
		ASSERT_THAT(IsTrue(Character->GetCapsuleComponent()->IsA(CapsuleClass), TEXT("ACharacter capsule component should materialize as the script replacement class")));
		ASSERT_THAT(IsTrue(Character->GetMesh()->IsA(MeshClass), TEXT("ACharacter mesh component should materialize as the script replacement class")));
	}

	TEST_METHOD(UClassComponentSpecifierInvalidCombinationBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> AttachWithoutDefaultDiagnostics;
		AttachWithoutDefaultDiagnostics.Add(TEXT("Attachments can only be specified on DefaultComponents"));

		const FString AttachWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassAttachWithoutDefaultActor : AActor
			{
				UPROPERTY(Attach=Root)
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_AttachWithoutDefault"),
			TEXT("ASCoverageUClassAttachWithoutDefault.as"),
			AttachWithoutDefaultSource,
			MakeArrayView(AttachWithoutDefaultDiagnostics))));

		TArray<FString> AttachSocketWithoutDefaultDiagnostics;
		AttachSocketWithoutDefaultDiagnostics.Add(TEXT("Attachments can only be specified on DefaultComponents"));

		const FString AttachSocketWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassAttachSocketWithoutDefaultActor : AActor
			{
				UPROPERTY(AttachSocket="LooseSocket")
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_AttachSocketWithoutDefault"),
			TEXT("ASCoverageUClassAttachSocketWithoutDefault.as"),
			AttachSocketWithoutDefaultSource,
			MakeArrayView(AttachSocketWithoutDefaultDiagnostics))));

		TArray<FString> RootWithoutDefaultDiagnostics;
		RootWithoutDefaultDiagnostics.Add(TEXT("RootComponent can only be specified on DefaultComponents"));

		const FString RootWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassRootWithoutDefaultActor : AActor
			{
				UPROPERTY(RootComponent)
				USceneComponent Root;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_RootWithoutDefault"),
			TEXT("ASCoverageUClassRootWithoutDefault.as"),
			RootWithoutDefaultSource,
			MakeArrayView(RootWithoutDefaultDiagnostics))));

		TArray<FString> ShowOnActorWithoutDefaultDiagnostics;
		ShowOnActorWithoutDefaultDiagnostics.Add(TEXT("ShowOnActor can only be used on default components in actors"));

		const FString ShowOnActorWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassShowOnActorWithoutDefaultActor : AActor
			{
				UPROPERTY(ShowOnActor)
				USceneComponent VisibleChild;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_ShowOnActorWithoutDefault"),
			TEXT("ASCoverageUClassShowOnActorWithoutDefault.as"),
			ShowOnActorWithoutDefaultSource,
			MakeArrayView(ShowOnActorWithoutDefaultDiagnostics))));

		TArray<FString> ShowOnActorWithInstancedObjectDiagnostics;
		ShowOnActorWithInstancedObjectDiagnostics.Add(TEXT("does not derive from UActorComponent"));

		const FString ShowOnActorWithInstancedObjectSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassShowOnActorPlainObject : UObject
			{
			}

			UCLASS()
			class ACoverageUClassShowOnActorInstancedObjectActor : AActor
			{
				UPROPERTY(DefaultComponent, ShowOnActor)
				UCoverageUClassShowOnActorPlainObject PlainObject;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_ShowOnActorWithInstancedObject"),
			TEXT("ASCoverageUClassShowOnActorWithInstancedObject.as"),
			ShowOnActorWithInstancedObjectSource,
			MakeArrayView(ShowOnActorWithInstancedObjectDiagnostics))));

		TArray<FString> OverrideAndDefaultDiagnostics;
		OverrideAndDefaultDiagnostics.Add(TEXT("OverrideComponent and DefaultComponent should not be used simultaneously"));

		const FString OverrideAndDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassOverrideAndDefaultActor : AActor
			{
				UPROPERTY(DefaultComponent, OverrideComponent=Root)
				USceneComponent Root;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_OverrideAndDefault"),
			TEXT("ASCoverageUClassOverrideAndDefault.as"),
			OverrideAndDefaultSource,
			MakeArrayView(OverrideAndDefaultDiagnostics))));

		TArray<FString> OverrideWithRootDiagnostics;
		OverrideWithRootDiagnostics.Add(TEXT("RootComponent can only be specified on DefaultComponents"));

		const FString OverrideWithRootSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassOverrideWithRootBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;
			}

			UCLASS()
			class ACoverageUClassOverrideWithRootChildActor : ACoverageUClassOverrideWithRootBaseActor
			{
				UPROPERTY(OverrideComponent=Root, RootComponent)
				USceneComponent Replacement;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_OverrideWithRoot"),
			TEXT("ASCoverageUClassOverrideWithRoot.as"),
			OverrideWithRootSource,
			MakeArrayView(OverrideWithRootDiagnostics))));

		TArray<FString> OverrideWithAttachDiagnostics;
		OverrideWithAttachDiagnostics.Add(TEXT("Attachments can only be specified on DefaultComponents"));

		const FString OverrideWithAttachSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassOverrideWithAttachBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child;
			}

			UCLASS()
			class ACoverageUClassOverrideWithAttachChildActor : ACoverageUClassOverrideWithAttachBaseActor
			{
				UPROPERTY(OverrideComponent=Child, Attach=Root)
				USceneComponent Replacement;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_OverrideWithAttach"),
			TEXT("ASCoverageUClassOverrideWithAttach.as"),
			OverrideWithAttachSource,
			MakeArrayView(OverrideWithAttachDiagnostics))));

		TArray<FString> NonComponentDefaultDiagnostics;
		NonComponentDefaultDiagnostics.Add(TEXT("does not derive from UActorComponent"));

		const FString NonComponentDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPlainDefaultObject : UObject
			{
			}

			UCLASS()
			class ACoverageUClassNonComponentDefaultActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUClassPlainDefaultObject PlainObject;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_NonComponentDefault"),
			TEXT("ASCoverageUClassNonComponentDefault.as"),
			NonComponentDefaultSource,
			MakeArrayView(NonComponentDefaultDiagnostics))));

		TArray<FString> NonSceneRootDiagnostics;
		NonSceneRootDiagnostics.Add(TEXT("has RootComponent set, but is not a type of scene component"));

		const FString NonSceneRootSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPlainRootLogicComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassNonSceneRootActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCoverageUClassPlainRootLogicComponent RootLogic;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_NonSceneRoot"),
			TEXT("ASCoverageUClassNonSceneRoot.as"),
			NonSceneRootSource,
			MakeArrayView(NonSceneRootDiagnostics))));

		TArray<FString> NonSceneAttachDiagnostics;
		NonSceneAttachDiagnostics.Add(TEXT("has a component attach set, but is not a type of scene component"));

		const FString NonSceneAttachSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPlainAttachLogicComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassNonSceneAttachActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCoverageUClassPlainAttachLogicComponent Logic;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_NonSceneAttach"),
			TEXT("ASCoverageUClassNonSceneAttach.as"),
			NonSceneAttachSource,
			MakeArrayView(NonSceneAttachDiagnostics))));

		TArray<FString> MissingAttachParentDiagnostics;
		MissingAttachParentDiagnostics.Add(TEXT("Attach parent MissingParent does not exist for DefaultComponent Child"));

		const FString MissingAttachParentSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassMissingAttachParentActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=MissingParent)
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_MissingAttachParent"),
			TEXT("ASCoverageUClassMissingAttachParent.as"),
			MissingAttachParentSource,
			MakeArrayView(MissingAttachParentDiagnostics))));

		TArray<FString> DuplicateRootDiagnostics;
		DuplicateRootDiagnostics.Add(TEXT("is RootComponent, but the actor already has root component Root"));

		const FString DuplicateRootSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDuplicateRootActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent OtherRoot;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_DuplicateRoot"),
			TEXT("ASCoverageUClassDuplicateRoot.as"),
			DuplicateRootSource,
			MakeArrayView(DuplicateRootDiagnostics))));

		TArray<FString> MissingOverrideTargetDiagnostics;
		MissingOverrideTargetDiagnostics.Add(TEXT("could not find component MissingScene in base class to override"));

		const FString MissingOverrideTargetSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassMissingOverrideBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;
			}

			UCLASS()
			class ACoverageUClassMissingOverrideChildActor : ACoverageUClassMissingOverrideBaseActor
			{
				UPROPERTY(OverrideComponent=MissingScene)
				USceneComponent Replacement;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_MissingOverrideTarget"),
			TEXT("ASCoverageUClassMissingOverrideTarget.as"),
			MissingOverrideTargetSource,
			MakeArrayView(MissingOverrideTargetDiagnostics))));

		TArray<FString> OverrideWrongTypeDiagnostics;
		OverrideWrongTypeDiagnostics.Add(TEXT("type does not inherit from the base class's"));

		const FString OverrideWrongTypeSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassOverrideWrongTypeBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UStaticMeshComponent Mesh;
			}

			UCLASS()
			class ACoverageUClassOverrideWrongTypeChildActor : ACoverageUClassOverrideWrongTypeBaseActor
			{
				UPROPERTY(OverrideComponent=Mesh)
				USceneComponent Replacement;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_OverrideWrongType"),
			TEXT("ASCoverageUClassOverrideWrongType.as"),
			OverrideWrongTypeSource,
			MakeArrayView(OverrideWrongTypeDiagnostics))));

		TArray<FString> NonComponentOverrideDiagnostics;
		NonComponentOverrideDiagnostics.Add(TEXT("was marked as OverrideComponent, but is not a type of component"));

		const FString NonComponentOverrideSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPlainOverrideObject : UObject
			{
			}

			UCLASS()
			class ACoverageUClassNonComponentOverrideBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;
			}

			UCLASS()
			class ACoverageUClassNonComponentOverrideChildActor : ACoverageUClassNonComponentOverrideBaseActor
			{
				UPROPERTY(OverrideComponent=Root)
				UCoverageUClassPlainOverrideObject Replacement;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_NonComponentOverride"),
			TEXT("ASCoverageUClassNonComponentOverride.as"),
			NonComponentOverrideSource,
			MakeArrayView(NonComponentOverrideDiagnostics))));

		TArray<FString> AbstractOverrideComponentDiagnostics;
		AbstractOverrideComponentDiagnostics.Add(TEXT("was marked as OverrideComponent, but the component class is abstract and cannot be used"));

		const FString AbstractOverrideComponentSource = ASTEST_AS(R"AS(
			UCLASS(Abstract)
			class UCoverageUClassAbstractOverrideSceneComponent : USceneComponent
			{
			}

			UCLASS()
			class ACoverageUClassAbstractOverrideBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;
			}

			UCLASS()
			class ACoverageUClassAbstractOverrideChildActor : ACoverageUClassAbstractOverrideBaseActor
			{
				UPROPERTY(OverrideComponent=Root)
				UCoverageUClassAbstractOverrideSceneComponent Replacement;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_AbstractOverrideComponent"),
			TEXT("ASCoverageUClassAbstractOverrideComponent.as"),
			AbstractOverrideComponentSource,
			MakeArrayView(AbstractOverrideComponentDiagnostics))));

		TArray<FString> AbstractDefaultComponentDiagnostics;
		AbstractDefaultComponentDiagnostics.Add(TEXT("was marked as DefaultComponent, but the component class is abstract and cannot be added"));

		const FString AbstractDefaultComponentSource = ASTEST_AS(R"AS(
			UCLASS(Abstract)
			class UCoverageUClassAbstractDefaultSceneComponent : USceneComponent
			{
			}

			UCLASS()
			class ACoverageUClassAbstractDefaultComponentActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUClassAbstractDefaultSceneComponent AbstractComponent;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_AbstractDefaultComponent"),
			TEXT("ASCoverageUClassAbstractDefaultComponent.as"),
			AbstractDefaultComponentSource,
			MakeArrayView(AbstractDefaultComponentDiagnostics))));
	}

	TEST_METHOD(UClassUnsupportedSpecifierBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> NonTransientDiagnostics;
		NonTransientDiagnostics.Add(TEXT("Unknown class specifier NonTransient"));

		const FString NonTransientSource = ASTEST_AS(R"AS(
			UCLASS(NonTransient)
			class UCoverageUClassUnsupportedNonTransientObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedNonTransient"),
			TEXT("ASCoverageUClassUnsupportedNonTransient.as"),
			NonTransientSource,
			MakeArrayView(NonTransientDiagnostics))));

		TArray<FString> NotBlueprintTypeDiagnostics;
		NotBlueprintTypeDiagnostics.Add(TEXT("Unknown class specifier NotBlueprintType"));

		const FString NotBlueprintTypeSource = ASTEST_AS(R"AS(
			UCLASS(NotBlueprintType)
			class UCoverageUClassUnsupportedNotBlueprintTypeObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedNotBlueprintType"),
			TEXT("ASCoverageUClassUnsupportedNotBlueprintType.as"),
			NotBlueprintTypeSource,
			MakeArrayView(NotBlueprintTypeDiagnostics))));

		TArray<FString> ExplicitPlaceableDiagnostics;
		ExplicitPlaceableDiagnostics.Add(TEXT("Unknown class specifier Placeable"));

		const FString ExplicitPlaceableSource = ASTEST_AS(R"AS(
			UCLASS(Placeable)
			class ACoverageUClassUnsupportedExplicitPlaceableActor : AActor
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedExplicitPlaceable"),
			TEXT("ASCoverageUClassUnsupportedExplicitPlaceable.as"),
			ExplicitPlaceableSource,
			MakeArrayView(ExplicitPlaceableDiagnostics))));

		TArray<FString> GlobalUserConfigDiagnostics;
		GlobalUserConfigDiagnostics.Add(TEXT("Unknown class specifier GlobalUserConfig"));

		const FString GlobalUserConfigSource = ASTEST_AS(R"AS(
			UCLASS(GlobalUserConfig)
			class UCoverageUClassUnsupportedGlobalUserConfigObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedGlobalUserConfig"),
			TEXT("ASCoverageUClassUnsupportedGlobalUserConfig.as"),
			GlobalUserConfigSource,
			MakeArrayView(GlobalUserConfigDiagnostics))));

		TArray<FString> ProjectUserConfigDiagnostics;
		ProjectUserConfigDiagnostics.Add(TEXT("Unknown class specifier ProjectUserConfig"));

		const FString ProjectUserConfigSource = ASTEST_AS(R"AS(
			UCLASS(ProjectUserConfig)
			class UCoverageUClassUnsupportedProjectUserConfigObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedProjectUserConfig"),
			TEXT("ASCoverageUClassUnsupportedProjectUserConfig.as"),
			ProjectUserConfigSource,
			MakeArrayView(ProjectUserConfigDiagnostics))));

		TArray<FString> TopLevelShowCategoriesDiagnostics;
		TopLevelShowCategoriesDiagnostics.Add(TEXT("Unknown class specifier ShowCategories"));

		const FString TopLevelShowCategoriesSource = ASTEST_AS(R"AS(
			UCLASS(ShowCategories="Rendering")
			class UCoverageUClassUnsupportedShowCategoriesObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelShowCategories"),
			TEXT("ASCoverageUClassUnsupportedTopLevelShowCategories.as"),
			TopLevelShowCategoriesSource,
			MakeArrayView(TopLevelShowCategoriesDiagnostics))));

		TArray<FString> TopLevelCollapseCategoriesDiagnostics;
		TopLevelCollapseCategoriesDiagnostics.Add(TEXT("Unknown class specifier CollapseCategories"));

		const FString TopLevelCollapseCategoriesSource = ASTEST_AS(R"AS(
			UCLASS(CollapseCategories)
			class UCoverageUClassUnsupportedCollapseCategoriesObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelCollapseCategories"),
			TEXT("ASCoverageUClassUnsupportedCollapseCategories.as"),
			TopLevelCollapseCategoriesSource,
			MakeArrayView(TopLevelCollapseCategoriesDiagnostics))));

		TArray<FString> TopLevelDontCollapseCategoriesDiagnostics;
		TopLevelDontCollapseCategoriesDiagnostics.Add(TEXT("Unknown class specifier DontCollapseCategories"));

		const FString TopLevelDontCollapseCategoriesSource = ASTEST_AS(R"AS(
			UCLASS(DontCollapseCategories)
			class UCoverageUClassUnsupportedDontCollapseCategoriesObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelDontCollapseCategories"),
			TEXT("ASCoverageUClassUnsupportedDontCollapseCategories.as"),
			TopLevelDontCollapseCategoriesSource,
			MakeArrayView(TopLevelDontCollapseCategoriesDiagnostics))));

		TArray<FString> TopLevelAutoExpandCategoriesDiagnostics;
		TopLevelAutoExpandCategoriesDiagnostics.Add(TEXT("Unknown class specifier AutoExpandCategories"));

		const FString TopLevelAutoExpandCategoriesSource = ASTEST_AS(R"AS(
			UCLASS(AutoExpandCategories="Coverage")
			class UCoverageUClassUnsupportedAutoExpandCategoriesObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelAutoExpandCategories"),
			TEXT("ASCoverageUClassUnsupportedAutoExpandCategories.as"),
			TopLevelAutoExpandCategoriesSource,
			MakeArrayView(TopLevelAutoExpandCategoriesDiagnostics))));

		TArray<FString> TopLevelAutoCollapseCategoriesDiagnostics;
		TopLevelAutoCollapseCategoriesDiagnostics.Add(TEXT("Unknown class specifier AutoCollapseCategories"));

		const FString TopLevelAutoCollapseCategoriesSource = ASTEST_AS(R"AS(
			UCLASS(AutoCollapseCategories="Advanced")
			class UCoverageUClassUnsupportedAutoCollapseCategoriesObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelAutoCollapseCategories"),
			TEXT("ASCoverageUClassUnsupportedAutoCollapseCategories.as"),
			TopLevelAutoCollapseCategoriesSource,
			MakeArrayView(TopLevelAutoCollapseCategoriesDiagnostics))));

		TArray<FString> TopLevelConversionRootDiagnostics;
		TopLevelConversionRootDiagnostics.Add(TEXT("Unknown class specifier ConversionRoot"));

		const FString TopLevelConversionRootSource = ASTEST_AS(R"AS(
			UCLASS(ConversionRoot)
			class UCoverageUClassUnsupportedConversionRootObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelConversionRoot"),
			TEXT("ASCoverageUClassUnsupportedConversionRoot.as"),
			TopLevelConversionRootSource,
			MakeArrayView(TopLevelConversionRootDiagnostics))));

		TArray<FString> TopLevelHideFunctionsDiagnostics;
		TopLevelHideFunctionsDiagnostics.Add(TEXT("Unknown class specifier HideFunctions"));

		const FString TopLevelHideFunctionsSource = ASTEST_AS(R"AS(
			UCLASS(HideFunctions="HiddenA")
			class UCoverageUClassUnsupportedHideFunctionsObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelHideFunctions"),
			TEXT("ASCoverageUClassUnsupportedHideFunctions.as"),
			TopLevelHideFunctionsSource,
			MakeArrayView(TopLevelHideFunctionsDiagnostics))));

		TArray<FString> TopLevelSparseClassDataTypesDiagnostics;
		TopLevelSparseClassDataTypesDiagnostics.Add(TEXT("Unknown class specifier SparseClassDataTypes"));

		const FString TopLevelSparseClassDataTypesSource = ASTEST_AS(R"AS(
			UCLASS(SparseClassDataTypes="SparseData")
			class UCoverageUClassUnsupportedSparseClassDataTypesObject : UObject
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClass_UnsupportedTopLevelSparseClassDataTypes"),
			TEXT("ASCoverageUClassUnsupportedSparseClassDataTypes.as"),
			TopLevelSparseClassDataTypesSource,
			MakeArrayView(TopLevelSparseClassDataTypesDiagnostics))));
	}
};

#endif
