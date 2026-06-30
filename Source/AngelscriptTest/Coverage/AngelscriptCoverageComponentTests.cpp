#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestWorld.h"

#include "Components/ActorTestSpawner.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Sound/AudioBus.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundSourceBus.h"
#include "Sound/SoundSubmix.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageComponentTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UActorComponent basics and lifecycle, corresponding
// to OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md sections 1-4 and 8-10.
//
// Axes covered here:
//   * ComponentDeclaration      - DefaultComponent, RootComponent, Attach specifiers
//   * ComponentLifecycle        - OnComponentCreated, BeginPlay, Tick, EndPlay
//   * ComponentTickControl      - bCanEverTick, TickInterval, SetComponentTickEnabled
//   * ComponentActivation       - Activate, Deactivate, IsActive
//   * ComponentFinding          - GetComponentByClass, GetComponentsByClass
//   * ComponentTags             - ComponentTags, ComponentHasTag
//   * CustomScriptComponent     - Script-derived component classes
//   * AudioComponentSurface     - Audio playback, parameters, routing reflection
//
// Pattern D (script execution): compile AS actors with components, spawn them,
// drive component operations through lifecycle, verify state via properties.
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageComponentTest,
	"Angelscript.TestModule.Coverage.Component",
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

	static bool ReadObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject*& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetObjectByPath(Test, Object, Path, OutValue), Message);
	}

	static bool ReadIntByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetByPath<FIntProperty, int32>(Test, Object, Path, OutValue), Message);
	}

	static bool ReadFloatByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, double& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);

		FPropertyBindingPathIndirection Leaf;
		if (!ResolvePathOnObject(Test, Object, Path, Leaf))
		{
			return false;
		}

		const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Leaf.GetProperty());
		if (!LocalAssert.IsNotNull(
				NumericProperty,
				*FString::Printf(TEXT("Property '%.*s' should be numeric"), Path.Len(), Path.GetData())))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(
				NumericProperty->IsFloatingPoint(),
				*FString::Printf(TEXT("Property '%.*s' should be floating point"), Path.Len(), Path.GetData())))
		{
			return false;
		}

		OutValue = NumericProperty->GetFloatingPointPropertyValue(Leaf.GetPropertyAddress());
		return true;
	}

	template <typename StructType>
	static bool ReadStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, StructType& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetStructByPath<StructType>(Test, Object, Path, OutValue), Message);
	}

	static UFunction* FindAudioFunction(FName FunctionName)
	{
		return UAudioComponent::StaticClass()->FindFunctionByName(FunctionName);
	}

	static FProperty* FindFunctionParameter(UFunction* Function, FName ParameterName)
	{
		return Function != nullptr ? Function->FindPropertyByName(ParameterName) : nullptr;
	}

	static bool HasObjectParameterChildOf(UFunction* Function, FName ParameterName, const UClass* ExpectedClass)
	{
		const FObjectPropertyBase* ObjectParameter = CastField<FObjectPropertyBase>(FindFunctionParameter(Function, ParameterName));
		return ObjectParameter != nullptr
			&& ObjectParameter->PropertyClass != nullptr
			&& ExpectedClass != nullptr
			&& ObjectParameter->PropertyClass->IsChildOf(ExpectedClass);
	}

	static bool HasStructParameter(UFunction* Function, FName ParameterName, const UScriptStruct* ExpectedStruct)
	{
		const FStructProperty* StructParameter = CastField<FStructProperty>(FindFunctionParameter(Function, ParameterName));
		return StructParameter != nullptr
			&& StructParameter->Struct == ExpectedStruct;
	}

	static bool HasFloatParameter(UFunction* Function, FName ParameterName)
	{
		const FNumericProperty* Parameter = CastField<FNumericProperty>(FindFunctionParameter(Function, ParameterName));
		return Parameter != nullptr && Parameter->IsFloatingPoint();
	}

	static bool HasAudioPlayStateParameter(UFunction* Function, FName ParameterName)
	{
		const FProperty* Parameter = FindFunctionParameter(Function, ParameterName);
		if (const FEnumProperty* EnumParameter = CastField<FEnumProperty>(Parameter))
		{
			return EnumParameter->GetEnum() != nullptr
				&& EnumParameter->GetEnum()->GetFName() == FName(TEXT("EAudioComponentPlayState"));
		}

		const FByteProperty* ByteParameter = CastField<FByteProperty>(Parameter);
		return ByteParameter != nullptr
			&& ByteParameter->Enum != nullptr
			&& ByteParameter->Enum->GetFName() == FName(TEXT("EAudioComponentPlayState"));
	}

	static int32 CountRegisteredComponentsByClass(const AActor* Actor, const UClass* ComponentClass)
	{
		if (Actor == nullptr || ComponentClass == nullptr)
		{
			return 0;
		}

		int32 Count = 0;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsRegistered() && Component->IsA(ComponentClass))
			{
				++Count;
			}
		}

		return Count;
	}

	static bool ExpectComponentRegisteredByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, bool bExpectedRegistered, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);

		UObject* ComponentObject = nullptr;
		if (!ReadObjectByPath(Test, Object, Path, ComponentObject, Message))
		{
			return false;
		}

		UActorComponent* Component = Cast<UActorComponent>(ComponentObject);
		if (!LocalAssert.IsNotNull(Component, *FString::Printf(TEXT("Property '%.*s' should contain an actor component"), Path.Len(), Path.GetData())))
		{
			return false;
		}

		return LocalAssert.AreEqual(bExpectedRegistered, Component->IsRegistered(), Message);
	}

	static bool ExpectUnsupportedActorComponentRegistrationSurface(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ModuleName)
	{
		const TArray<FString> ExpectedDiagnostics = {
			TEXT("No matching signatures to 'UCoverageUnsupportedRegistrationProbe::IsRegistered()'"),
			TEXT("No matching signatures to 'UCoverageUnsupportedRegistrationProbe::RegisterComponent()'"),
			TEXT("No matching signatures to 'UCoverageUnsupportedRegistrationProbe::UnregisterComponent()'")
		};
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUnsupportedRegistrationProbe : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUnsupportedComponentRegistrationSurface : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUnsupportedRegistrationProbe Probe;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					bool bRegistered = Probe.IsRegistered();
					Probe.UnregisterComponent();
					Probe.RegisterComponent();
				}
			}
			)AS"),
			TEXT("direct component registration APIs should remain explicit AS binding boundaries"),
			MakeArrayView(ExpectedDiagnostics));
	}

	static bool ExpectUnsupportedComponentTickSurface(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ModuleName)
	{
		const TArray<FString> ExpectedDiagnostics = {
			TEXT("Identifier 'ELevelTick' is not a data type in global namespace"),
			TEXT("'bCanEverTick' is not a member of 'FActorComponentTickFunction'")
		};
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUnsupportedTickSurfaceComponent : UActorComponent
			{
				default PrimaryComponentTick.bCanEverTick = true;

				UFUNCTION(BlueprintOverride)
				void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
				{
				}
			}
			)AS"),
			TEXT("direct TickComponent override and PrimaryComponentTick flag defaults should remain explicit AS binding boundaries"),
			MakeArrayView(ExpectedDiagnostics));
	}

	static bool ExpectUnsupportedNativeComponentCallbacks(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ModuleName)
	{
		const TArray<FString> ExpectedDiagnostics = {
			TEXT("BlueprintOverride method OnComponentCreated"),
			TEXT("BlueprintOverride method InitializeComponent"),
			TEXT("BlueprintOverride method UninitializeComponent"),
			TEXT("BlueprintOverride method OnComponentDestroyed")
		};
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUnsupportedNativeComponentCallbacks : UActorComponent
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
				void UninitializeComponent()
				{
				}

				UFUNCTION(BlueprintOverride)
				void OnComponentDestroyed(bool bDestroyingHierarchy)
				{
				}
			}
			)AS"),
			TEXT("native-only component callbacks should remain explicit BlueprintOverride boundaries"),
			MakeArrayView(ExpectedDiagnostics));
	}

	static bool ExpectUnsupportedFindComponentByClassSurface(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ModuleName)
	{
		const TArray<FString> ExpectedDiagnostics = {
			TEXT("No matching signatures to 'FindComponentByClass(UClass)'")
		};
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUnsupportedFindComponentProbe : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUnsupportedFindComponentByClassActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUnsupportedFindComponentProbe Probe;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UActorComponent Found = FindComponentByClass(UCoverageUnsupportedFindComponentProbe::StaticClass());
				}
			}
			)AS"),
			TEXT("FindComponentByClass should remain an explicit AS binding boundary; supported script lookups use GetComponent/GetAllComponents"),
			MakeArrayView(ExpectedDiagnostics));
	}

	static bool ExpectUnsupportedSceneWorldLocationSurface(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ModuleName)
	{
		const TArray<FString> ExpectedDiagnostics = {
			TEXT("No matching signatures to 'USceneComponent::SetWorldLocation(FVector)'"),
			TEXT("No matching signatures to 'USceneComponent::GetComponentLocation()'")
		};
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUnsupportedSceneWorldLocationActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				USceneComponent Child;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Child.SetWorldLocation(FVector(25.0f, 35.0f, 45.0f));
					FVector Location = Child.GetComponentLocation();
				}
			}
			)AS"),
			TEXT("scene component world-location helpers should remain explicit AS binding boundaries"),
			MakeArrayView(ExpectedDiagnostics));
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
	// Component declaration: DefaultComponent, RootComponent, Attach specifiers
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_BasicDeclaration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentBasicDeclaration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageBasicLogicComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageComponentBasicActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child;

				UPROPERTY(DefaultComponent)
				UCoverageBasicLogicComponent LogicComponent;

				UPROPERTY()
				bool RootIsValid = false;

				UPROPERTY()
				bool ChildIsValid = false;

				UPROPERTY()
				bool ChildIsAttached = false;

				UPROPERTY()
				bool LogicComponentIsValid = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RootIsValid = (Root != nullptr);
					ChildIsValid = (Child != nullptr);
					LogicComponentIsValid = (LogicComponent != nullptr);

					if (Child != nullptr && Root != nullptr)
					{
						ChildIsAttached = Child.IsAttachedTo(Root);
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component basic declaration actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component basic declaration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootIsValid"), true, TEXT("Root component should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildIsValid"), true, TEXT("Child component should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LogicComponentIsValid"), true, TEXT("Logic component should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ChildIsAttached"), true, TEXT("Child should be attached to Root"))));

		UObject* RootObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("Root"), RootObject, TEXT("Root property should be readable"))));
		USceneComponent* RootComponent = Cast<USceneComponent>(RootObject);
		ASSERT_THAT(IsNotNull(RootComponent, TEXT("Root property should contain a scene component")));
		if (RootComponent == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor->GetRootComponent()), static_cast<UObject*>(RootComponent), TEXT("RootComponent specifier should assign the AS Root property as the actor root")));
	}

	// -------------------------------------------------------------------------
	// Component type declarations: Arrow, Audio, and Input component coverage
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentSpecialTypeDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_SpecialTypeDeclarations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentSpecialTypeDeclarations.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentSpecialTypeActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UArrowComponent Arrow;

				UPROPERTY(DefaultComponent, Attach=Root)
				UAudioComponent Audio;

				UPROPERTY(DefaultComponent)
				UInputComponent Input;

				UPROPERTY()
				bool ArrowValid = false;

				UPROPERTY()
				bool AudioValid = false;

				UPROPERTY()
				bool InputValid = false;

				UPROPERTY()
				bool SceneTypesAttached = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ArrowValid = Arrow != nullptr;
					AudioValid = Audio != nullptr;
					InputValid = Input != nullptr;
					SceneTypesAttached = ArrowValid && AudioValid && Arrow.IsAttachedTo(Root) && Audio.IsAttachedTo(Root);
				}
			}
			)AS"),
			TEXT("ACoverageComponentSpecialTypeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Special component type actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FObjectProperty* ArrowProperty = CastField<FObjectProperty>(ScriptClass->FindPropertyByName(TEXT("Arrow")));
		FObjectProperty* AudioProperty = CastField<FObjectProperty>(ScriptClass->FindPropertyByName(TEXT("Audio")));
		FObjectProperty* InputProperty = CastField<FObjectProperty>(ScriptClass->FindPropertyByName(TEXT("Input")));
		ASSERT_THAT(IsNotNull(ArrowProperty, TEXT("Arrow property should exist")));
		ASSERT_THAT(IsNotNull(AudioProperty, TEXT("Audio property should exist")));
		ASSERT_THAT(IsNotNull(InputProperty, TEXT("Input property should exist")));
		if (ArrowProperty == nullptr || AudioProperty == nullptr || InputProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrowProperty->PropertyClass->IsChildOf(UArrowComponent::StaticClass()), TEXT("Arrow property should use UArrowComponent")));
		ASSERT_THAT(IsTrue(AudioProperty->PropertyClass->IsChildOf(UAudioComponent::StaticClass()), TEXT("Audio property should use UAudioComponent")));
		ASSERT_THAT(IsTrue(InputProperty->PropertyClass->IsChildOf(UInputComponent::StaticClass()), TEXT("Input property should use UInputComponent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Special component type actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ArrowValid"), true, TEXT("UArrowComponent default component should be created"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("AudioValid"), true, TEXT("UAudioComponent default component should be created"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InputValid"), true, TEXT("UInputComponent default component should be created"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("SceneTypesAttached"), true, TEXT("Arrow and Audio scene components should attach to Root"))));
	}

	// -------------------------------------------------------------------------
	// Audio component controls: declaration, playback, and parameter APIs
	// -------------------------------------------------------------------------
	TEST_METHOD(AudioComponentDeclarationAndControls)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageAudio_ComponentControls"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageAudioComponentControls.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageAudioComponentActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UAudioComponent Audio;

				UPROPERTY()
				bool bAudioComponentValid = false;

				UPROPERTY()
				bool bPlaybackControlsCallable = false;

				UPROPERTY()
				bool bParameterControlsCallable = false;

				UPROPERTY()
				bool bAudioRemainsStoppedWithoutSound = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					bAudioComponentValid = Audio != nullptr;
					if (Audio == nullptr)
					{
						return;
					}

					Audio.SetVolumeMultiplier(0.25f);
					Audio.SetPitchMultiplier(1.50f);
					Audio.SetUISound(true);
					Audio.SetPaused(true);
					Audio.SetPaused(false);
					Audio.Play(0.0f);
					Audio.Stop();
					bPlaybackControlsCallable = true;

					Audio.SetBoolParameter(n"CoverageBool", true);
					Audio.SetFloatParameter(n"CoverageFloat", 0.75f);
					Audio.SetIntParameter(n"CoverageInt", 12);
					bParameterControlsCallable = true;

					bAudioRemainsStoppedWithoutSound = !Audio.IsPlaying();
				}
			}
			)AS"),
			TEXT("ACoverageAudioComponentActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Audio component controls actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FObjectProperty* AudioProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("Audio"));
		ASSERT_THAT(IsNotNull(AudioProperty, TEXT("Audio component property should be generated")));
		if (AudioProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AudioProperty->PropertyClass != nullptr && AudioProperty->PropertyClass->IsChildOf(UAudioComponent::StaticClass()),
			TEXT("Audio property should reference UAudioComponent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Audio coverage actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bAudioComponentValid"), true,
			TEXT("UAudioComponent DefaultComponent should be created"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bPlaybackControlsCallable"), true,
			TEXT("Audio playback controls should be callable from AS"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bParameterControlsCallable"), true,
			TEXT("Audio parameter controls should be callable from AS"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bAudioRemainsStoppedWithoutSound"), true,
			TEXT("Audio component without a sound should remain stopped after Play/Stop"))));
	}

	// -------------------------------------------------------------------------
	// Audio component controls: fade and filter APIs
	// -------------------------------------------------------------------------
	TEST_METHOD(AudioComponentFadeAndFilterControls)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageAudio_FadeAndFilterControls"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageAudioFadeAndFilterControls.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageAudioFadeActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UAudioComponent Audio;

				UPROPERTY()
				bool bFadeControlsCallable = false;

				UPROPERTY()
				bool bFilterControlsCallable = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (Audio == nullptr)
					{
						return;
					}

					Audio.FadeIn(0.01f, 0.5f, 0.0f);
					Audio.AdjustVolume(0.01f, 0.25f);
					Audio.FadeOut(0.01f, 0.0f);
					bFadeControlsCallable = true;

					Audio.SetLowPassFilterEnabled(true);
					Audio.SetLowPassFilterFrequency(1200.0f);
					Audio.SetHighPassFilterEnabled(true);
					Audio.SetHighPassFilterFrequency(300.0f);
					bFilterControlsCallable = true;
				}
			}
			)AS"),
			TEXT("ACoverageAudioFadeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Audio fade/filter coverage actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Audio fade/filter coverage actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bFadeControlsCallable"), true,
			TEXT("Audio fade controls should be callable from AS"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bFilterControlsCallable"), true,
			TEXT("Audio filter controls should be callable from AS"))));
	}

	// -------------------------------------------------------------------------
	// Audio component routing and delegate reflection surface
	// -------------------------------------------------------------------------
	TEST_METHOD(AudioComponentRoutingAndReflectionSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageAudio_RoutingAndReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageAudioRoutingAndReflection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageAudioRoutingActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UAudioComponent Audio;

				UPROPERTY()
				bool bRoutingSurfaceCallable = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (Audio == nullptr)
					{
						return;
					}

					Audio.SetSound(nullptr);
					Audio.SetAttenuationSettings(nullptr);
					bRoutingSurfaceCallable = true;
				}
			}
			)AS"),
			TEXT("ACoverageAudioRoutingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Audio routing/reflection coverage actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FObjectPropertyBase* SoundProperty = FindFProperty<FObjectPropertyBase>(UAudioComponent::StaticClass(), TEXT("Sound"));
		const FObjectPropertyBase* AttenuationSettingsProperty = FindFProperty<FObjectPropertyBase>(UAudioComponent::StaticClass(), TEXT("AttenuationSettings"));
		const FStructProperty* AttenuationOverridesProperty = FindFProperty<FStructProperty>(UAudioComponent::StaticClass(), TEXT("AttenuationOverrides"));
		const FSetProperty* ConcurrencySetProperty = FindFProperty<FSetProperty>(UAudioComponent::StaticClass(), TEXT("ConcurrencySet"));
		ASSERT_THAT(IsNotNull(SoundProperty, TEXT("UAudioComponent.Sound should be reflected")));
		ASSERT_THAT(IsNotNull(AttenuationSettingsProperty, TEXT("UAudioComponent.AttenuationSettings should be reflected")));
		ASSERT_THAT(IsNotNull(AttenuationOverridesProperty, TEXT("UAudioComponent.AttenuationOverrides should be reflected")));
		ASSERT_THAT(IsNotNull(ConcurrencySetProperty, TEXT("UAudioComponent.ConcurrencySet should be reflected")));
		if (SoundProperty == nullptr || AttenuationSettingsProperty == nullptr || AttenuationOverridesProperty == nullptr || ConcurrencySetProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SoundProperty->PropertyClass != nullptr && SoundProperty->PropertyClass->IsChildOf(USoundBase::StaticClass()),
			TEXT("Sound property should reference USoundBase")));
		ASSERT_THAT(IsTrue(AttenuationSettingsProperty->PropertyClass != nullptr && AttenuationSettingsProperty->PropertyClass->IsChildOf(USoundAttenuation::StaticClass()),
			TEXT("AttenuationSettings property should reference USoundAttenuation")));
		ASSERT_THAT(IsTrue(AttenuationOverridesProperty->Struct == FSoundAttenuationSettings::StaticStruct(),
			TEXT("AttenuationOverrides property should use FSoundAttenuationSettings")));

		const FObjectPropertyBase* ConcurrencyElementProperty = CastField<FObjectPropertyBase>(ConcurrencySetProperty->ElementProp);
		ASSERT_THAT(IsNotNull(ConcurrencyElementProperty, TEXT("ConcurrencySet should have an object element property")));
		if (ConcurrencyElementProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConcurrencyElementProperty->PropertyClass != nullptr && ConcurrencyElementProperty->PropertyClass->IsChildOf(USoundConcurrency::StaticClass()),
			TEXT("ConcurrencySet element should reference USoundConcurrency")));

		UFunction* SetSoundFunction = FindAudioFunction(TEXT("SetSound"));
		UFunction* AdjustAttenuationFunction = FindAudioFunction(TEXT("AdjustAttenuation"));
		UFunction* SetSubmixSendFunction = FindAudioFunction(TEXT("SetSubmixSend"));
		UFunction* SetSourceBusSendPreEffectFunction = FindAudioFunction(TEXT("SetSourceBusSendPreEffect"));
		UFunction* SetAudioBusSendPreEffectFunction = FindAudioFunction(TEXT("SetAudioBusSendPreEffect"));
		ASSERT_THAT(IsNotNull(SetSoundFunction, TEXT("SetSound should be reflected as an audio callable")));
		ASSERT_THAT(IsNotNull(AdjustAttenuationFunction, TEXT("AdjustAttenuation should be reflected as an audio callable")));
		ASSERT_THAT(IsNotNull(SetSubmixSendFunction, TEXT("SetSubmixSend should be reflected as an audio callable")));
		ASSERT_THAT(IsNotNull(SetSourceBusSendPreEffectFunction, TEXT("SetSourceBusSendPreEffect should be reflected as an audio callable")));
		ASSERT_THAT(IsNotNull(SetAudioBusSendPreEffectFunction, TEXT("SetAudioBusSendPreEffect should be reflected as an audio callable")));
		if (SetSoundFunction == nullptr || AdjustAttenuationFunction == nullptr || SetSubmixSendFunction == nullptr || SetSourceBusSendPreEffectFunction == nullptr || SetAudioBusSendPreEffectFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasObjectParameterChildOf(SetSoundFunction, TEXT("NewSound"), USoundBase::StaticClass()),
			TEXT("SetSound should accept USoundBase")));
		ASSERT_THAT(IsTrue(HasStructParameter(AdjustAttenuationFunction, TEXT("InAttenuationSettings"), FSoundAttenuationSettings::StaticStruct()),
			TEXT("AdjustAttenuation should accept FSoundAttenuationSettings")));
		ASSERT_THAT(IsTrue(HasObjectParameterChildOf(SetSubmixSendFunction, TEXT("Submix"), USoundSubmixBase::StaticClass()),
			TEXT("SetSubmixSend should accept USoundSubmixBase")));
		ASSERT_THAT(IsTrue(HasFloatParameter(SetSubmixSendFunction, TEXT("SendLevel")),
			TEXT("SetSubmixSend should expose a send-level float")));
		ASSERT_THAT(IsTrue(HasObjectParameterChildOf(SetSourceBusSendPreEffectFunction, TEXT("SoundSourceBus"), USoundSourceBus::StaticClass()),
			TEXT("SetSourceBusSendPreEffect should accept USoundSourceBus")));
		ASSERT_THAT(IsTrue(HasFloatParameter(SetSourceBusSendPreEffectFunction, TEXT("SourceBusSendLevel")),
			TEXT("SetSourceBusSendPreEffect should expose a send-level float")));
		ASSERT_THAT(IsTrue(HasObjectParameterChildOf(SetAudioBusSendPreEffectFunction, TEXT("AudioBus"), UAudioBus::StaticClass()),
			TEXT("SetAudioBusSendPreEffect should accept UAudioBus")));
		ASSERT_THAT(IsTrue(HasFloatParameter(SetAudioBusSendPreEffectFunction, TEXT("AudioBusSendLevel")),
			TEXT("SetAudioBusSendPreEffect should expose a send-level float")));

		const FMulticastDelegateProperty* OnAudioFinishedProperty = FindFProperty<FMulticastDelegateProperty>(UAudioComponent::StaticClass(), TEXT("OnAudioFinished"));
		const FMulticastDelegateProperty* OnAudioPlayStateChangedProperty = FindFProperty<FMulticastDelegateProperty>(UAudioComponent::StaticClass(), TEXT("OnAudioPlayStateChanged"));
		const FMulticastDelegateProperty* OnAudioPlaybackPercentProperty = FindFProperty<FMulticastDelegateProperty>(UAudioComponent::StaticClass(), TEXT("OnAudioPlaybackPercent"));
		ASSERT_THAT(IsNotNull(OnAudioFinishedProperty, TEXT("OnAudioFinished delegate should be reflected")));
		ASSERT_THAT(IsNotNull(OnAudioPlayStateChangedProperty, TEXT("OnAudioPlayStateChanged delegate should be reflected")));
		ASSERT_THAT(IsNotNull(OnAudioPlaybackPercentProperty, TEXT("OnAudioPlaybackPercent delegate should be reflected")));
		if (OnAudioFinishedProperty == nullptr || OnAudioPlayStateChangedProperty == nullptr || OnAudioPlaybackPercentProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(OnAudioFinishedProperty->SignatureFunction, TEXT("OnAudioFinished should expose a signature function")));
		ASSERT_THAT(IsTrue(HasAudioPlayStateParameter(OnAudioPlayStateChangedProperty->SignatureFunction, TEXT("PlayState")),
			TEXT("OnAudioPlayStateChanged signature should expose PlayState")));
		ASSERT_THAT(IsNotNull(FindFProperty<FObjectPropertyBase>(OnAudioPlaybackPercentProperty->SignatureFunction, TEXT("PlayingSoundWave")),
			TEXT("OnAudioPlaybackPercent signature should expose PlayingSoundWave")));
		ASSERT_THAT(IsTrue(HasFloatParameter(OnAudioPlaybackPercentProperty->SignatureFunction, TEXT("PlaybackPercent")),
			TEXT("OnAudioPlaybackPercent signature should expose PlaybackPercent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Audio routing/reflection coverage actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("bRoutingSurfaceCallable"), true,
			TEXT("Audio routing callable surface should be callable from AS without a sound asset"))));
	}

	// -------------------------------------------------------------------------
	// Component UPROPERTY specifiers: EditAnywhere, BlueprintReadOnly, Instanced
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentPropertySpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_PropertySpecifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageInstancedLogicObject : UObject
			{
				UPROPERTY()
				int Value = 19;
			}

			UCLASS()
			class ACoverageComponentPropertySpecifierActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, ShowOnActor, EditAnywhere, BlueprintReadOnly)
				USceneComponent VisibleChild;

				UPROPERTY(Instanced)
				UCoverageInstancedLogicObject InlineObject;

				UPROPERTY()
				bool VisibleChildValid = false;

				UPROPERTY()
				bool InlineObjectAssigned = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VisibleChildValid = VisibleChild != nullptr;
					InlineObject = Cast<UCoverageInstancedLogicObject>(NewObject(this, UCoverageInstancedLogicObject::StaticClass()));
					InlineObjectAssigned = InlineObject != nullptr && InlineObject.Value == 19;
				}
			}
			)AS"),
			TEXT("ACoverageComponentPropertySpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component property specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* VisibleChildProperty = ScriptClass->FindPropertyByName(TEXT("VisibleChild"));
		ASSERT_THAT(IsNotNull(VisibleChildProperty, TEXT("Default component property should exist")));
		if (VisibleChildProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere/ShowOnActor should make the component property editable")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly should make the component property Blueprint visible")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly should set CPF_BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("DefaultComponent should be an instanced exported reference")));
		ASSERT_THAT(IsTrue(VisibleChildProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should add EditInline metadata")));

		FProperty* InlineObjectProperty = ScriptClass->FindPropertyByName(TEXT("InlineObject"));
		ASSERT_THAT(IsNotNull(InlineObjectProperty, TEXT("Instanced object property should exist")));
		if (InlineObjectProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(InlineObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance), TEXT("UPROPERTY(Instanced) should set persistent instance flags")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component property specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("VisibleChildValid"), true, TEXT("Default component specifier property should create a component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InlineObjectAssigned"), true, TEXT("Instanced property should accept a runtime inline object"))));
	}

	// -------------------------------------------------------------------------
	// Component lifecycle: OnComponentCreated -> BeginPlay -> Tick -> EndPlay
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

		ASSERT_THAT(IsTrue(ExpectUnsupportedComponentTickSurface(
			*TestRunner,
			Engine,
			TEXT("ASCoverageComponentLifecycleUnsupportedTickSurface"))));
		ASSERT_THAT(IsTrue(ExpectUnsupportedNativeComponentCallbacks(
			*TestRunner,
			Engine,
			TEXT("ASCoverageComponentLifecycleUnsupportedNativeCallbacks"))));

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentLifecycle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ULifecycleTestComponent : UActorComponent
			{
				UPROPERTY()
				int LifecycleStage = 0;

				UPROPERTY()
				int TickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					LifecycleStage = 2;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					if (LifecycleStage == 2)
					{
						TickCount++;
					}
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					if (LifecycleStage == 2)
					{
						LifecycleStage = 3;
					}
				}
			}

			UCLASS()
			class ACoverageComponentLifecycleActor : AActor
			{
				UPROPERTY(DefaultComponent)
				ULifecycleTestComponent TestComp;
			}
			)AS"),
			TEXT("ACoverageComponentLifecycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component lifecycle actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component lifecycle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UActorComponent* TestComp = Actor->GetComponentByClass(UActorComponent::StaticClass());
		ASSERT_THAT(IsNotNull(TestComp, TEXT("TestComp should exist")));
		if (TestComp == nullptr)
		{
			return;
		}

		TestComp->PrimaryComponentTick.bCanEverTick = true;
		TestComp->SetComponentTickEnabled(true);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, TestComp, TEXT("LifecycleStage"), 2, TEXT("Lifecycle should reach BeginPlay stage"))));

		FAngelscriptTestWorld::DispatchComponentTick(Engine, *TestComp, 0.1f, 2);

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, TestComp, TEXT("TickCount"), TickCount, TEXT("TickCount should be readable"))));
		ASSERT_THAT(AreEqual(2, TickCount, TEXT("Component Tick override should be dispatched exactly twice")));
	}

	// -------------------------------------------------------------------------
	// Component lifecycle ordering: create, initialize, begin play, uninitialize, destroy
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentLifecycleOrdering)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_LifecycleOrdering"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentLifecycleOrdering.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageLifecycleOrderComponent : UActorComponent
			{
				UPROPERTY()
				int NextOrder = 0;

				UPROPERTY()
				int BeginPlayOrder = 0;

				UPROPERTY()
				int EndPlayOrder = 0;

				UPROPERTY()
				bool bSawOwnerDuringBeginPlay = false;

				int ClaimOrder()
				{
					NextOrder++;
					return NextOrder;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayOrder = ClaimOrder();
					bSawOwnerDuringBeginPlay = GetOwner() != nullptr;
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					EndPlayOrder = ClaimOrder();
				}
			}

			UCLASS()
			class ACoverageComponentLifecycleOrderingActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				UCoverageLifecycleOrderComponent Probe;

				UPROPERTY()
				int ActorBeginPlayOrder = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ActorBeginPlayOrder = 1;
					Tags.Add(n"ActorBeginPlayRan");
				}
			}
			)AS"),
			TEXT("ACoverageComponentLifecycleOrderingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component lifecycle ordering actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FAngelscriptTestWorld World(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(World.IsValid(), TEXT("Lifecycle ordering world should be valid")));
		if (!World.IsValid())
		{
			return;
		}
		AActor* Actor = World.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component lifecycle ordering actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		World.BeginPlay(*Actor);

		UObject* ProbeObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("Probe"), ProbeObject, TEXT("Probe component should be readable"))));
		UActorComponent* Probe = Cast<UActorComponent>(ProbeObject);
		ASSERT_THAT(IsNotNull(Probe, TEXT("Probe should be an actor component")));
		if (Probe == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Probe, TEXT("bSawOwnerDuringBeginPlay"), true, TEXT("Component BeginPlay should observe its owner"))));
		ASSERT_THAT(IsTrue(Probe->IsRegistered(), TEXT("Default component should be registered before explicit unregister")));

		World.DestroyAndDrain(*Actor);

		int32 BeginPlayOrder = 0;
		int32 EndPlayOrder = 0;
		ASSERT_THAT(IsTrue(
			ReadIntByPath(*TestRunner, Probe, TEXT("BeginPlayOrder"), BeginPlayOrder, TEXT("BeginPlayOrder should be readable"))
			&& ReadIntByPath(*TestRunner, Probe, TEXT("EndPlayOrder"), EndPlayOrder, TEXT("EndPlayOrder should be readable")),
			TEXT("Lifecycle order values should be readable after DestroyAndDrain")));

		ASSERT_THAT(IsTrue(BeginPlayOrder > 0, TEXT("BeginPlay should be recorded")));
		ASSERT_THAT(IsTrue(EndPlayOrder > 0, TEXT("EndPlay should be recorded")));

		ASSERT_THAT(IsTrue(BeginPlayOrder < EndPlayOrder, TEXT("BeginPlay should precede EndPlay")));
	}

	// -------------------------------------------------------------------------
	// Component lifecycle ordering across actor, multiple default components, and runtime components
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentActorMultiAndDynamicLifecycleOrdering)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_MultiDynamicLifecycleOrdering"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentMultiDynamicLifecycleOrdering.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageRootLifecycleComponent : USceneComponent
			{
				UPROPERTY()
				int BeginPlayOrder = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor Owner = GetOwner();
					if (Owner != nullptr)
					{
						Owner.Tags.Add(n"RootBeginPlay");
						BeginPlayOrder = Owner.Tags.Num();
					}
				}
			}

			UCLASS()
			class UCoverageChildLifecycleComponent : USceneComponent
			{
				UPROPERTY()
				int BeginPlayOrder = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor Owner = GetOwner();
					if (Owner != nullptr)
					{
						Owner.Tags.Add(n"ChildBeginPlay");
						BeginPlayOrder = Owner.Tags.Num();
					}
				}
			}

			UCLASS()
			class UCoverageLaterLifecycleComponent : UActorComponent
			{
				UPROPERTY()
				int BeginPlayOrder = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor Owner = GetOwner();
					if (Owner != nullptr)
					{
						Owner.Tags.Add(n"LaterBeginPlay");
						BeginPlayOrder = Owner.Tags.Num();
					}
				}
			}

			UCLASS()
			class UCoverageDynamicLifecycleComponent : UActorComponent
			{
				UPROPERTY()
				int BeginPlayOrder = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor Owner = GetOwner();
					if (Owner != nullptr)
					{
						Owner.Tags.Add(n"DynamicBeginPlay");
						BeginPlayOrder = Owner.Tags.Num();
					}
				}
			}

			UCLASS()
			class ACoverageComponentMultiDynamicLifecycleActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCoverageRootLifecycleComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCoverageChildLifecycleComponent ChildProbe;

				UPROPERTY(DefaultComponent)
				UCoverageLaterLifecycleComponent LaterProbe;

				UPROPERTY()
				UCoverageDynamicLifecycleComponent DynamicProbe;

				UPROPERTY()
				int ActorBeginPlayOrder = 0;

				UPROPERTY()
				int RootBeginPlayOrder = 0;

				UPROPERTY()
				int ChildBeginPlayOrder = 0;

				UPROPERTY()
				int LaterBeginPlayOrder = 0;

				UPROPERTY()
				int DynamicBeginPlayOrder = 0;

				UPROPERTY()
				bool DynamicCreated = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Tags.Add(n"ActorBeginPlay");
					ActorBeginPlayOrder = Tags.Num();
				}

				UFUNCTION()
				void CreateRuntimeProbe()
				{
					DynamicProbe = UCoverageDynamicLifecycleComponent::Create(this, n"DynamicProbe");
					if (DynamicProbe == nullptr)
					{
						return;
					}

					DynamicCreated = true;
					DynamicBeginPlayOrder = DynamicProbe.BeginPlayOrder;
				}

				UFUNCTION()
				void CaptureDefaultOrders()
				{
					RootBeginPlayOrder = Root.BeginPlayOrder;
					ChildBeginPlayOrder = ChildProbe.BeginPlayOrder;
					LaterBeginPlayOrder = LaterProbe.BeginPlayOrder;
				}
			}
			)AS"),
			TEXT("ACoverageComponentMultiDynamicLifecycleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component multi/dynamic lifecycle actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component multi/dynamic lifecycle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker CaptureInvoker(*TestRunner, Actor, FName(TEXT("CaptureDefaultOrders")));
		ASSERT_THAT(IsTrue(CaptureInvoker.IsValid(), TEXT("CaptureDefaultOrders should be invokable")));
		ASSERT_THAT(IsTrue(CaptureInvoker.Call(), TEXT("CaptureDefaultOrders should execute")));

		int32 ActorBeginPlayOrder = 0;
		int32 RootBeginPlayOrder = 0;
		int32 ChildBeginPlayOrder = 0;
		int32 LaterBeginPlayOrder = 0;
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, Actor, TEXT("ActorBeginPlayOrder"), ActorBeginPlayOrder, TEXT("ActorBeginPlayOrder should be readable"))));
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, Actor, TEXT("RootBeginPlayOrder"), RootBeginPlayOrder, TEXT("RootBeginPlayOrder should be readable"))));
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, Actor, TEXT("ChildBeginPlayOrder"), ChildBeginPlayOrder, TEXT("ChildBeginPlayOrder should be readable"))));
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, Actor, TEXT("LaterBeginPlayOrder"), LaterBeginPlayOrder, TEXT("LaterBeginPlayOrder should be readable"))));
		ASSERT_THAT(IsTrue(ActorBeginPlayOrder > 0, TEXT("Actor BeginPlay should record an order")));
		ASSERT_THAT(IsTrue(RootBeginPlayOrder > 0, TEXT("Root component BeginPlay should record an order")));
		ASSERT_THAT(IsTrue(ChildBeginPlayOrder > 0, TEXT("Attached child component BeginPlay should record an order")));
		ASSERT_THAT(IsTrue(LaterBeginPlayOrder > 0, TEXT("Later default component BeginPlay should record an order")));
		ASSERT_THAT(IsTrue(RootBeginPlayOrder < ActorBeginPlayOrder, TEXT("Root default component BeginPlay should run before the AS actor BeginPlay body")));
		ASSERT_THAT(IsTrue(ChildBeginPlayOrder < ActorBeginPlayOrder, TEXT("Attached child default component BeginPlay should run before the AS actor BeginPlay body")));
		ASSERT_THAT(IsTrue(LaterBeginPlayOrder < ActorBeginPlayOrder, TEXT("Later default component BeginPlay should run before the AS actor BeginPlay body")));
		ASSERT_THAT(AreNotEqual(RootBeginPlayOrder, ChildBeginPlayOrder, TEXT("Root and child components should record distinct BeginPlay observations")));
		ASSERT_THAT(AreNotEqual(ChildBeginPlayOrder, LaterBeginPlayOrder, TEXT("Child and later components should record distinct BeginPlay observations")));

		FFunctionInvoker DynamicInvoker(*TestRunner, Actor, FName(TEXT("CreateRuntimeProbe")));
		ASSERT_THAT(IsTrue(DynamicInvoker.IsValid(), TEXT("CreateRuntimeProbe should be invokable")));
		ASSERT_THAT(IsTrue(DynamicInvoker.Call(), TEXT("CreateRuntimeProbe should execute")));

		int32 DynamicBeginPlayOrder = 0;
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("DynamicCreated"), true, TEXT("Runtime-created component should be created"))));
		ASSERT_THAT(IsTrue(ExpectComponentRegisteredByPath(*TestRunner, Actor, TEXT("DynamicProbe"), true, TEXT("Runtime-created component should register"))));
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, Actor, TEXT("DynamicBeginPlayOrder"), DynamicBeginPlayOrder, TEXT("DynamicBeginPlayOrder should be readable"))));
		ASSERT_THAT(IsTrue(DynamicBeginPlayOrder > ActorBeginPlayOrder, TEXT("Runtime component BeginPlay should happen after actor BeginPlay")));
	}

	// -------------------------------------------------------------------------
	// Custom component lifecycle Super:: calls across BeginPlay and TickComponent
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomComponentLifecycleSuperCalls)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_CustomLifecycleSuperCalls"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentCustomLifecycleSuperCalls.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageBaseLifecycleSuperComponent : UActorComponent
			{
				UPROPERTY()
				int BaseBeginPlayCount = 0;

				UPROPERTY()
				int BaseTickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BaseBeginPlayCount++;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					BaseTickCount++;
				}
			}

			UCLASS()
			class UCoverageDerivedLifecycleSuperComponent : UCoverageBaseLifecycleSuperComponent
			{
				UPROPERTY()
				int DerivedBeginPlayCount = 0;

				UPROPERTY()
				int DerivedTickCount = 0;

				UPROPERTY()
				int LastDeltaMillis = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Super::BeginPlay();
					DerivedBeginPlayCount++;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					Super::Tick(DeltaTime);
					DerivedTickCount++;
					LastDeltaMillis = int(DeltaTime * 1000.0f);
				}
			}

			UCLASS()
			class ACoverageComponentLifecycleSuperActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageDerivedLifecycleSuperComponent SuperProbe;
			}
			)AS"),
			TEXT("ACoverageComponentLifecycleSuperActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Custom component lifecycle Super:: actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Custom component lifecycle Super:: actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UObject* ProbeObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("SuperProbe"), ProbeObject, TEXT("SuperProbe component should be readable"))));
		UActorComponent* Probe = Cast<UActorComponent>(ProbeObject);
		ASSERT_THAT(IsNotNull(Probe, TEXT("SuperProbe should be an actor component")));
		if (Probe == nullptr)
		{
			return;
		}

		Probe->PrimaryComponentTick.bCanEverTick = true;
		Probe->SetComponentTickEnabled(true);
		FAngelscriptTestWorld::DispatchComponentTick(Engine, *Probe, 0.025f, 2);

		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Probe, TEXT("BaseBeginPlayCount"), 1, TEXT("Super::BeginPlay should execute the base component override once"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Probe, TEXT("DerivedBeginPlayCount"), 1, TEXT("Derived component BeginPlay override should execute once"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Probe, TEXT("BaseTickCount"), 2, TEXT("Super::Tick should execute the base component override for each dispatched tick"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Probe, TEXT("DerivedTickCount"), 2, TEXT("Derived component Tick override should execute for each dispatched tick"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Probe, TEXT("LastDeltaMillis"), 25, TEXT("Derived component Tick should receive DeltaTime after calling Super::Tick"))));
	}

	// -------------------------------------------------------------------------
	// Component tick control: bCanEverTick, TickInterval, SetComponentTickEnabled
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTickControl)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_TickControl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentTickControl.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UTickControlComponent : UActorComponent
			{
				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				float AccumulatedTime = 0.0f;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					TickCount++;
					AccumulatedTime += DeltaTime;
				}
			}

			UCLASS()
			class ACoverageComponentTickControlActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UTickControlComponent TestComp;

				UPROPERTY()
				int DisableTickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (TestComp.IsComponentTickEnabled())
					{
						DisableTickCount = 1;
					}
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					if (TestComp.TickCount >= 3 && DisableTickCount == 1)
					{
						TestComp.SetComponentTickEnabled(false);
						DisableTickCount = 2;
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentTickControlActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component tick control actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tick control actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UObject* ComponentObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("TestComp"), ComponentObject, TEXT("TestComp should be readable"))));
		UActorComponent* Component = Cast<UActorComponent>(ComponentObject);
		ASSERT_THAT(IsNotNull(Component, TEXT("TestComp should be an actor component")));
		if (Component == nullptr)
		{
			return;
		}
		Component->PrimaryComponentTick.bCanEverTick = true;
		Component->SetComponentTickEnabled(true);
		BeginPlayActor(Engine, *Actor);

		for (int32 TickIndex = 0; TickIndex < 3; ++TickIndex)
		{
			FAngelscriptTestWorld::DispatchComponentTick(Engine, *Component, 0.05f, 1);
			TickWorld(Engine, Spawner.GetWorld(), 0.05f, 1);
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DisableTickCount"), 2, TEXT("Tick should be disabled after reaching count"))));
	}

	// -------------------------------------------------------------------------
	// Component tick configuration: start enabled, interval, tick group and prerequisites
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTickConfigurationAndPrerequisites)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_TickConfiguration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentTickConfiguration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageTickConfigComponent : UActorComponent
			{
				UPROPERTY()
				int TickCount = 0;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					TickCount++;
				}
			}

			UCLASS()
			class UCoverageTickDisabledComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageComponentTickConfigurationActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageTickConfigComponent TickComp;

				UPROPERTY(DefaultComponent)
				UCoverageTickDisabledComponent DisabledComp;

				UPROPERTY()
				bool StartTickEnabled = false;

				UPROPERTY()
				bool ComponentPrereqAdded = false;

				UPROPERTY()
				bool ActorPrereqAdded = false;

				UPROPERTY()
				bool ComponentPrereqRemoved = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StartTickEnabled = TickComp.IsComponentTickEnabled();

					TickComp.AddTickPrerequisiteComponent(DisabledComp);
					ComponentPrereqAdded = true;

					TickComp.AddTickPrerequisiteActor(this);
					ActorPrereqAdded = true;

					TickComp.RemoveTickPrerequisiteComponent(DisabledComp);
					ComponentPrereqRemoved = true;
				}
			}
			)AS"),
			TEXT("ACoverageComponentTickConfigurationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component tick configuration actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tick configuration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UObject* TickCompObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("TickComp"), TickCompObject, TEXT("TickComp should be readable"))));
		UActorComponent* TickComp = Cast<UActorComponent>(TickCompObject);
		ASSERT_THAT(IsNotNull(TickComp, TEXT("TickComp should be an actor component")));
		if (TickComp == nullptr)
		{
			return;
		}
		TickComp->PrimaryComponentTick.bCanEverTick = true;
		TickComp->PrimaryComponentTick.TickInterval = 0.5f;
		TickComp->PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;
		TickComp->SetComponentTickEnabled(true);

		ASSERT_THAT(IsTrue(TickComp->PrimaryComponentTick.bCanEverTick, TEXT("C++ can enable bCanEverTick for script component tick dispatch")));
		ASSERT_THAT(IsTrue(TickComp->IsComponentTickEnabled(), TEXT("C++ can start component tick enabled")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TickComp->PrimaryComponentTick.TickInterval, 0.5f, 0.01f), TEXT("C++ can configure component tick interval")));
		ASSERT_THAT(AreEqual(ETickingGroup::TG_PrePhysics, TickComp->PrimaryComponentTick.TickGroup, TEXT("C++ can configure component tick group")));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("StartTickEnabled"), true, TEXT("Script component tick override should start enabled when the component can tick"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ComponentPrereqAdded"), true, TEXT("AddTickPrerequisiteComponent should be callable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ActorPrereqAdded"), true, TEXT("AddTickPrerequisiteActor should be callable"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ComponentPrereqRemoved"), true, TEXT("RemoveTickPrerequisiteComponent should be callable"))));
	}

	// -------------------------------------------------------------------------
	// Component activation: Activate, Deactivate, IsActive
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentActivation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Activation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentActivation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageActivationComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageComponentActivationActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageActivationComponent TestComp;

				UPROPERTY()
				bool InitiallyActive = false;

				UPROPERTY()
				bool AfterDeactivate = true;

				UPROPERTY()
				bool AfterReactivate = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitiallyActive = TestComp.IsActive();

					TestComp.Deactivate();
					AfterDeactivate = TestComp.IsActive();

					TestComp.Activate(true);
					AfterReactivate = TestComp.IsActive();
				}
			}
			)AS"),
			TEXT("ACoverageComponentActivationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component activation actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component activation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InitiallyActive"), false, TEXT("Script actor component should start inactive until explicitly activated"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterDeactivate"), false, TEXT("Component should be inactive after Deactivate"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AfterReactivate"), true, TEXT("Component should be active after Activate"))));
	}

	// -------------------------------------------------------------------------
	// Component registration and activation APIs: Register, Unregister, Activate
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentRegistrationAndActivation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_RegistrationActivation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		ASSERT_THAT(IsTrue(ExpectUnsupportedActorComponentRegistrationSurface(
			*TestRunner,
			Engine,
			TEXT("ASCoverageComponentRegistrationActivationUnsupportedRegistration"))));

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentRegistrationActivation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageRuntimeLogicComponent : UActorComponent
			{
				UPROPERTY()
				int Value = 17;
			}

			UCLASS()
			class ACoverageComponentRegistrationActivationActor : AActor
			{
				UPROPERTY()
				UCoverageRuntimeLogicComponent RuntimeComp;

				UPROPERTY()
				bool RegisteredAfterCreate = false;

				UPROPERTY()
				bool ActiveAfterActivate = false;

				UPROPERTY()
				bool InactiveAfterDeactivate = false;

				UPROPERTY()
				bool OwnerMatched = false;

				UPROPERTY()
				bool WorldMatched = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RuntimeComp = UCoverageRuntimeLogicComponent::Create(this, n"RuntimeComp");
					RegisteredAfterCreate = RuntimeComp != nullptr;

					RuntimeComp.Activate(true);
					ActiveAfterActivate = RuntimeComp.IsActive();

					RuntimeComp.Deactivate();
					InactiveAfterDeactivate = !RuntimeComp.IsActive();

					OwnerMatched = RuntimeComp.GetOwner() == this;
					WorldMatched = RuntimeComp.GetWorld() == GetWorld();
				}
			}
			)AS"),
			TEXT("ACoverageComponentRegistrationActivationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component registration/activation actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component registration/activation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("RegisteredAfterCreate"), true, TEXT("Create should return a component"))));
		ASSERT_THAT(IsTrue(ExpectComponentRegisteredByPath(*TestRunner, Actor, TEXT("RuntimeComp"), true, TEXT("Create should register the runtime component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ActiveAfterActivate"), true, TEXT("Activate should set component active"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InactiveAfterDeactivate"), true, TEXT("Deactivate should clear component active"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("OwnerMatched"), true, TEXT("GetOwner should return the owning actor"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("WorldMatched"), true, TEXT("GetWorld should match actor world"))));
	}

	// -------------------------------------------------------------------------
	// Component finding: GetComponentByClass, GetComponentsByClass
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentFinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Finding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		ASSERT_THAT(IsTrue(ExpectUnsupportedFindComponentByClassSurface(
			*TestRunner,
			Engine,
			TEXT("ASCoverageComponentFindingByClassUnsupportedFindComponentByClass"))));

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentFinding.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageFindingLogicComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageComponentFindingActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child1;

				UPROPERTY(DefaultComponent, Attach=Root)
				USceneComponent Child2;

				UPROPERTY(DefaultComponent)
				UCoverageFindingLogicComponent LogicComp;

				UPROPERTY()
				bool FoundSingleComponent = false;

				UPROPERTY()
				int SceneComponentCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UActorComponent FoundComp = GetComponentByClass(UActorComponent::StaticClass());
					FoundSingleComponent = (FoundComp != nullptr);

					TArray<USceneComponent> SceneComps;
					GetComponentsByClass(USceneComponent::StaticClass(), SceneComps);
					SceneComponentCount = SceneComps.Num();
				}
			}
			)AS"),
			TEXT("ACoverageComponentFindingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component finding actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component finding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FoundSingleComponent"), true, TEXT("GetComponentByClass should find a component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SceneComponentCount"), 3, TEXT("Should find 3 scene components"))));
	}

	// -------------------------------------------------------------------------
	// Component finding APIs: GetComponentByClass, FindComponentByClass and tag filtering
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentFindingByClassAndTag)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_FindingByClassAndTag"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentFindingByClassAndTag.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageFindBaseComponent : UActorComponent
			{
			}

			UCLASS()
			class UCoverageFindDerivedComponent : UCoverageFindBaseComponent
			{
			}

			UCLASS()
			class ACoverageComponentFindingByClassAndTagActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageFindDerivedComponent DerivedA;

				UPROPERTY(DefaultComponent)
				UCoverageFindDerivedComponent DerivedB;

				UPROPERTY()
				bool GetComponentByClassFound = false;

				UPROPERTY()
				int TaggedComponentCount = 0;

				UPROPERTY()
				int TaggedQueryCount = 0;

				UPROPERTY()
				int DerivedComponentCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DerivedA.ComponentTags.Add(n"CoverageTag");
					DerivedB.ComponentTags.Add(n"CoverageTag");

					UActorComponent FoundBase = GetComponentByClass(UCoverageFindBaseComponent::StaticClass());
					GetComponentByClassFound = FoundBase != nullptr;

					TArray<UActorComponent> AllComponents;
					GetComponentsByClass(UActorComponent::StaticClass(), AllComponents);
					for (UActorComponent Component : AllComponents)
					{
						if (Component.ComponentHasTag(n"CoverageTag"))
						{
							TaggedComponentCount++;
						}
					}

					TArray<UActorComponent> TaggedComponents = GetComponentsByTag(UActorComponent::StaticClass(), n"CoverageTag");
					TaggedQueryCount = TaggedComponents.Num();

					TArray<UCoverageFindDerivedComponent> DerivedComponents;
					GetComponentsByClass(UCoverageFindDerivedComponent::StaticClass(), DerivedComponents);
					DerivedComponentCount = DerivedComponents.Num();
				}
			}
			)AS"),
			TEXT("ACoverageComponentFindingByClassAndTagActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component class/tag finding actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component class/tag finding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("GetComponentByClassFound"), true, TEXT("GetComponentByClass should find a derived component through base class"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("TaggedComponentCount"), 2, TEXT("ComponentHasTag should identify both tagged components after class lookup"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("TaggedQueryCount"), 2, TEXT("GetComponentsByTag should return both tagged components"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("DerivedComponentCount"), 2, TEXT("GetComponentsByClass should find both derived components"))));
	}

	// -------------------------------------------------------------------------
	// Component tags: ComponentTags, ComponentHasTag
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentTags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Tags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentTags.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageTagsComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageComponentTagsActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageTagsComponent TestComp;

				UPROPERTY()
				bool HasTestTag = false;

				UPROPERTY()
				bool HasOtherTag = false;

				UPROPERTY()
				int TagCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestComp.ComponentTags.Add(n"TestTag");
					TestComp.ComponentTags.Add(n"AnotherTag");

					HasTestTag = TestComp.ComponentHasTag(n"TestTag");
					HasOtherTag = TestComp.ComponentHasTag(n"OtherTag");
					TagCount = TestComp.ComponentTags.Num();
				}
			}
			)AS"),
			TEXT("ACoverageComponentTagsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component tags actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component tags actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasTestTag"), true, TEXT("Should have TestTag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasOtherTag"), false, TEXT("Should not have OtherTag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TagCount"), 2, TEXT("Should have 2 tags"))));
	}

	// -------------------------------------------------------------------------
	// Custom script component: script-derived UActorComponent with custom properties
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomScriptComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_CustomScript"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentCustomScript.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCustomLogicComponent : UActorComponent
			{
				UPROPERTY()
				int CustomValue = 42;

				UPROPERTY()
				FString CustomName = "TestComponent";

				UFUNCTION()
				int GetDoubledValue()
				{
					return CustomValue * 2;
				}
			}

			UCLASS()
			class ACoverageComponentCustomScriptActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCustomLogicComponent CustomComp;

				UPROPERTY()
				int RetrievedValue = 0;

				UPROPERTY()
				FString RetrievedName;

				UPROPERTY()
				int DoubledValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (CustomComp != nullptr)
					{
						RetrievedValue = CustomComp.CustomValue;
						RetrievedName = CustomComp.CustomName;
						DoubledValue = CustomComp.GetDoubledValue();
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentCustomScriptActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Custom script component actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Custom script component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RetrievedValue"), 42, TEXT("Should retrieve custom value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("RetrievedName"), FString(TEXT("TestComponent")), TEXT("Should retrieve custom name"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DoubledValue"), 84, TEXT("Should calculate doubled value"))));
	}

	// -------------------------------------------------------------------------
	// Custom component reuse: multiple actors, inheritance chain, and Create()
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomComponentReuseInheritanceAndInstantiation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_CustomReuseInheritance"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentCustomReuseInheritance.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageReusableBaseComponent : UActorComponent
			{
				UPROPERTY()
				int BaseValue = 10;

				UFUNCTION()
				int ComputeValue()
				{
					return BaseValue;
				}
			}

			UCLASS()
			class UCoverageReusableDerivedComponent : UCoverageReusableBaseComponent
			{
				UPROPERTY()
				int DerivedValue = 5;

				UFUNCTION()
				int ComputeDerivedValue()
				{
					return ComputeValue() + DerivedValue;
				}
			}

			UCLASS()
			class ACoverageComponentReusableActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageReusableDerivedComponent DefaultReusable;

				UPROPERTY()
				UCoverageReusableDerivedComponent DynamicReusable;

				UPROPERTY()
				bool DefaultComponentValid = false;

				UPROPERTY()
				bool DynamicComponentValid = false;

				UPROPERTY()
				int CombinedValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DynamicReusable = UCoverageReusableDerivedComponent::Create(this, n"DynamicReusable");

					DefaultComponentValid = DefaultReusable != nullptr;
					DynamicComponentValid = DynamicReusable != nullptr && DynamicReusable != DefaultReusable;

					if (DefaultReusable != nullptr && DynamicReusable != nullptr)
					{
						DefaultReusable.DerivedValue = 5;
						DynamicReusable.DerivedValue = 5;
						CombinedValue = DefaultReusable.ComputeDerivedValue() + DynamicReusable.ComputeDerivedValue();
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentReusableActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Reusable component actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* FirstActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		AActor* SecondActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(FirstActor, TEXT("First reusable component actor should spawn")));
		ASSERT_THAT(IsNotNull(SecondActor, TEXT("Second reusable component actor should spawn")));
		if (FirstActor == nullptr || SecondActor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *FirstActor);
		BeginPlayActor(Engine, *SecondActor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, FirstActor, TEXT("DefaultComponentValid"), true, TEXT("First actor should receive reusable default component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, FirstActor, TEXT("DynamicComponentValid"), true, TEXT("First actor should create a distinct reusable runtime component"))));
		ASSERT_THAT(IsTrue(ExpectComponentRegisteredByPath(*TestRunner, FirstActor, TEXT("DynamicReusable"), true, TEXT("First actor runtime component should register"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, FirstActor, TEXT("CombinedValue"), 30, TEXT("Component inheritance methods should work on first actor"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, SecondActor, TEXT("DefaultComponentValid"), true, TEXT("Second actor should receive reusable default component"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, SecondActor, TEXT("CombinedValue"), 30, TEXT("Component class should be reusable across actor instances"))));

		UObject* FirstDefaultObject = nullptr;
		UObject* SecondDefaultObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, FirstActor, TEXT("DefaultReusable"), FirstDefaultObject, TEXT("First default component should be readable"))));
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, SecondActor, TEXT("DefaultReusable"), SecondDefaultObject, TEXT("Second default component should be readable"))));
		ASSERT_THAT(IsTrue(FirstDefaultObject != nullptr && SecondDefaultObject != nullptr && FirstDefaultObject != SecondDefaultObject, TEXT("Reusable component instances should be distinct per actor")));
		if (FirstDefaultObject == nullptr || SecondDefaultObject == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FirstDefaultObject->GetClass() == SecondDefaultObject->GetClass(), TEXT("Reusable component instances should share the generated component class")));
	}

	// -------------------------------------------------------------------------
	// Dynamic component creation through NewObject plus explicit registration
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentManualNewObjectRegistration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_ManualNewObjectRegistration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentManualNewObjectRegistration.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageManualNewObjectComponent : UActorComponent
			{
				UPROPERTY()
				int BaseValue = 29;

				UFUNCTION()
				int AddValue(int ExtraValue)
				{
					return BaseValue + ExtraValue;
				}
			}

			UCLASS()
			class ACoverageComponentManualNewObjectActor : AActor
			{
				UPROPERTY()
				UCoverageManualNewObjectComponent ManualComp;

				UPROPERTY()
				bool NewObjectCreated = false;

				UPROPERTY()
				bool OwnerBeforeRegisterMatched = false;

				UPROPERTY()
				bool WorldBeforeRegisterMatched = false;

				UPROPERTY()
				bool TaggedAfterRegister = false;

				UPROPERTY()
				bool ActiveAfterActivate = false;

				UPROPERTY()
				bool InactiveAfterDeactivate = false;

				UPROPERTY()
				int CustomMethodValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ManualComp = Cast<UCoverageManualNewObjectComponent>(NewObject(this, UCoverageManualNewObjectComponent::StaticClass(), n"ManualNewObjectComp", true));
					NewObjectCreated = ManualComp != nullptr;
					if (ManualComp == nullptr)
					{
						return;
					}

					OwnerBeforeRegisterMatched = ManualComp.GetOwner() == this;
					WorldBeforeRegisterMatched = ManualComp.GetWorld() == GetWorld();
					ManualComp.ComponentTags.Add(n"ManualNewObject");

					TaggedAfterRegister = ManualComp.ComponentHasTag(n"ManualNewObject");

					ManualComp.Activate(true);
					ActiveAfterActivate = ManualComp.IsActive();

					ManualComp.Deactivate();
					InactiveAfterDeactivate = !ManualComp.IsActive();

					CustomMethodValue = ManualComp.AddValue(13);
				}
			}
			)AS"),
			TEXT("ACoverageComponentManualNewObjectActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Manual NewObject component actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Manual NewObject component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("NewObjectCreated"), true, TEXT("NewObject should create a script component instance"))));
		ASSERT_THAT(IsTrue(ExpectComponentRegisteredByPath(*TestRunner, Actor, TEXT("ManualComp"), false, TEXT("NewObject component should start unregistered before native registration"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("OwnerBeforeRegisterMatched"), true, TEXT("NewObject component should resolve its actor owner before registration"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("WorldBeforeRegisterMatched"), true, TEXT("NewObject component should resolve owner world before registration"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("TaggedAfterRegister"), true, TEXT("ComponentTags should work on the manually registered component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ActiveAfterActivate"), true, TEXT("Activate should set the manually registered component active"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InactiveAfterDeactivate"), true, TEXT("Deactivate should clear active state on the manually registered component"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Actor, TEXT("CustomMethodValue"), 42, TEXT("Script component custom method should execute on the NewObject instance"))));

		UObject* ManualComponentObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("ManualComp"), ManualComponentObject, TEXT("ManualComp should be readable"))));
		UActorComponent* ManualComponent = Cast<UActorComponent>(ManualComponentObject);
		ASSERT_THAT(IsNotNull(ManualComponent, TEXT("ManualComp should be an actor component")));
		if (ManualComponent == nullptr)
		{
			return;
		}

		ManualComponent->RegisterComponent();
		ASSERT_THAT(IsTrue(ManualComponent->IsRegistered(), TEXT("Native RegisterComponent should register the NewObject component")));
		ASSERT_THAT(AreEqual(1, CountRegisteredComponentsByClass(Actor, ManualComponent->GetClass()), TEXT("Native component lookup should find the manually registered component")));
		ManualComponent->UnregisterComponent();
		ASSERT_THAT(IsFalse(ManualComponent->IsRegistered(), TEXT("Native UnregisterComponent should unregister the NewObject component")));
		ManualComponent->RegisterComponent();
		ASSERT_THAT(IsTrue(ManualComponent->IsRegistered(), TEXT("Native RegisterComponent should allow the component to register again")));
	}

	// -------------------------------------------------------------------------
	// Runtime tick interval and tick enabled state changes
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentRuntimeTickIntervalControl)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_RuntimeTickIntervalControl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentRuntimeTickIntervalControl.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageRuntimeTickIntervalComponent : UActorComponent
			{
				UPROPERTY()
				int TickCount = 0;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					TickCount++;
				}
			}

			UCLASS()
			class ACoverageComponentRuntimeTickIntervalActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageRuntimeTickIntervalComponent TickComp;

				UPROPERTY()
				bool InitiallyDisabled = false;

				UPROPERTY()
				bool EnabledAfterToggle = false;

				UPROPERTY()
				bool DisabledAfterToggle = false;

				UPROPERTY()
				float InitialInterval = 0.0f;

				UPROPERTY()
				float UpdatedInterval = 0.0f;

				UPROPERTY()
				float SecondUpdatedInterval = 0.0f;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					InitiallyDisabled = !TickComp.IsComponentTickEnabled();
					InitialInterval = TickComp.GetComponentTickInterval();

					TickComp.SetComponentTickInterval(0.25f);
					UpdatedInterval = TickComp.GetComponentTickInterval();

					TickComp.SetComponentTickInterval(0.05f);
					SecondUpdatedInterval = TickComp.GetComponentTickInterval();

					TickComp.SetComponentTickEnabled(true);
					EnabledAfterToggle = TickComp.IsComponentTickEnabled();
				}

				UFUNCTION()
				void DisableRuntimeTick()
				{
					TickComp.SetComponentTickEnabled(false);
					DisabledAfterToggle = !TickComp.IsComponentTickEnabled();
				}
			}
			)AS"),
			TEXT("ACoverageComponentRuntimeTickIntervalActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Runtime tick interval actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Runtime tick interval actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UObject* TickComponentObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("TickComp"), TickComponentObject, TEXT("TickComp should be readable"))));
		UActorComponent* TickComponent = Cast<UActorComponent>(TickComponentObject);
		ASSERT_THAT(IsNotNull(TickComponent, TEXT("TickComp should be an actor component")));
		if (TickComponent == nullptr)
		{
			return;
		}
		TickComponent->PrimaryComponentTick.bCanEverTick = true;
		TickComponent->PrimaryComponentTick.TickInterval = 0.125f;
		TickComponent->SetComponentTickEnabled(false);
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("InitiallyDisabled"), false, TEXT("Script component tick override should remain enabled during BeginPlay"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("EnabledAfterToggle"), true, TEXT("SetComponentTickEnabled(true) should enable runtime ticking"))));

		double InitialInterval = 0.0;
		double UpdatedInterval = 0.0;
		double SecondUpdatedInterval = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("InitialInterval"), InitialInterval), TEXT("InitialInterval should be readable")));
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("UpdatedInterval"), UpdatedInterval), TEXT("UpdatedInterval should be readable")));
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("SecondUpdatedInterval"), SecondUpdatedInterval), TEXT("SecondUpdatedInterval should be readable")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(InitialInterval, 0.125, 0.01), TEXT("Initial tick interval should come from the default tick config")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(UpdatedInterval, 0.25, 0.01), TEXT("SetComponentTickInterval should update the readable interval")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SecondUpdatedInterval, 0.05, 0.01), TEXT("SetComponentTickInterval should support repeated interval updates")));

		TickWorld(Engine, Spawner.GetWorld(), 0.05f, 2);

		int32 TickCountAfterEnabled = 0;
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, TickComponent, TEXT("TickCount"), TickCountAfterEnabled, TEXT("Tick count should be readable after enabling tick"))));
		ASSERT_THAT(IsTrue(TickCountAfterEnabled > 0, TEXT("Enabled component tick should execute after world ticks")));

		FFunctionInvoker DisableInvoker(*TestRunner, Actor, FName(TEXT("DisableRuntimeTick")));
		ASSERT_THAT(IsTrue(DisableInvoker.IsValid(), TEXT("DisableRuntimeTick should be invokable")));
		if (!DisableInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(DisableInvoker.Call(), TEXT("DisableRuntimeTick should execute")));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("DisabledAfterToggle"), true, TEXT("SetComponentTickEnabled(false) should disable runtime ticking"))));

		TickWorld(Engine, Spawner.GetWorld(), 0.05f, 2);

		int32 TickCountAfterDisabled = 0;
		ASSERT_THAT(IsTrue(ReadIntByPath(*TestRunner, TickComponent, TEXT("TickCount"), TickCountAfterDisabled, TEXT("Tick count should be readable after disabling tick"))));
		ASSERT_THAT(AreEqual(TickCountAfterEnabled, TickCountAfterDisabled, TEXT("Disabled component tick should not run during later world ticks")));
	}

	// -------------------------------------------------------------------------
	// Component destruction: DestroyComponent, IsBeingDestroyed
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentDestruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_Destruction"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentDestruction.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageDestructionComponent : UActorComponent
			{
			}

			UCLASS()
			class ACoverageComponentDestructionActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageDestructionComponent TestComp;

				UPROPERTY()
				bool WasDestroyed = false;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					if (TestComp != nullptr && !TestComp.IsBeingDestroyed())
					{
						TestComp.DestroyComponent();
						WasDestroyed = true;
					}
				}
			}
			)AS"),
			TEXT("ACoverageComponentDestructionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component destruction actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component destruction actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		TickWorld(Engine, Spawner.GetWorld(), 0.1f, 1);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasDestroyed"), true, TEXT("Component should be destroyed"))));
	}

	// -------------------------------------------------------------------------
	// Scene component runtime attachment rules: KeepWorld, KeepRelative, SnapToTarget
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentSceneAttachmentRuleTransforms)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_SceneAttachmentRuleTransforms"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		ASSERT_THAT(IsTrue(ExpectUnsupportedSceneWorldLocationSurface(
			*TestRunner,
			Engine,
			TEXT("ASCoverageComponentSceneUnsupportedWorldLocationSurface"))));

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentSceneAttachmentRuleTransforms.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageComponentSceneAttachmentRulesActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent)
				USceneComponent KeepWorldChild;

				UPROPERTY(DefaultComponent)
				USceneComponent KeepRelativeChild;

				UPROPERTY(DefaultComponent)
				USceneComponent SnapChild;

				UPROPERTY()
				bool KeepWorldAttached = false;

				UPROPERTY()
				bool KeepRelativeAttached = false;

				UPROPERTY()
				bool SnapAttached = false;

				UPROPERTY()
				FVector KeepWorldRelativeLocation;

				UPROPERTY()
				FVector KeepRelativeRelativeLocation;

				UPROPERTY()
				FVector SnapRelativeLocation;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Root.SetRelativeLocation(FVector(100.0f, 200.0f, 300.0f));

					KeepWorldChild.DetachFromComponent(
						EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					KeepWorldChild.SetRelativeLocation(FVector(25.0f, 35.0f, 45.0f));
					KeepWorldChild.AttachToComponent(Root, NAME_None,
						EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
					KeepWorldAttached = KeepWorldChild.IsAttachedTo(Root);
					KeepWorldRelativeLocation = KeepWorldChild.RelativeLocation;

					KeepRelativeChild.DetachFromComponent(
						EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					KeepRelativeChild.SetRelativeLocation(FVector(5.0f, 6.0f, 7.0f));
					KeepRelativeChild.AttachToComponent(Root, NAME_None,
						EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
					KeepRelativeAttached = KeepRelativeChild.IsAttachedTo(Root);
					KeepRelativeRelativeLocation = KeepRelativeChild.RelativeLocation;

					SnapChild.DetachFromComponent(
						EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					SnapChild.SetRelativeLocation(FVector(400.0f, 500.0f, 600.0f));
					SnapChild.AttachToComponent(Root, NAME_None,
						EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, false);
					SnapAttached = SnapChild.IsAttachedTo(Root);
					SnapRelativeLocation = SnapChild.RelativeLocation;
				}
			}
			)AS"),
			TEXT("ACoverageComponentSceneAttachmentRulesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component scene attachment rules actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component scene attachment rules actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("KeepWorldAttached"), true, TEXT("KeepWorld child should attach to Root"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("KeepRelativeAttached"), true, TEXT("KeepRelative child should attach to Root"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("SnapAttached"), true, TEXT("SnapToTarget child should attach to Root"))));

		FVector KeepWorldRelativeLocation;
		FVector KeepRelativeRelativeLocation;
		FVector SnapRelativeLocation;
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("KeepWorldRelativeLocation"), KeepWorldRelativeLocation, TEXT("KeepWorldRelativeLocation should be readable"))));
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("KeepRelativeRelativeLocation"), KeepRelativeRelativeLocation, TEXT("KeepRelativeRelativeLocation should be readable"))));
		ASSERT_THAT(IsTrue(ReadStructByPath(*TestRunner, Actor, TEXT("SnapRelativeLocation"), SnapRelativeLocation, TEXT("SnapRelativeLocation should be readable"))));

		ASSERT_THAT(IsTrue(KeepWorldRelativeLocation.Equals(FVector(-75.0f, -165.0f, -255.0f), 0.01f), TEXT("KeepWorld should recompute relative location against Root")));
		ASSERT_THAT(IsTrue(KeepRelativeRelativeLocation.Equals(FVector(5.0f, 6.0f, 7.0f), 0.01f), TEXT("KeepRelative should keep the relative location unchanged")));
		ASSERT_THAT(IsTrue(SnapRelativeLocation.Equals(FVector::ZeroVector, 0.01f), TEXT("SnapToTarget should clear relative location")));
	}

	// -------------------------------------------------------------------------
	// Component destruction state: callbacks, unregistering, and IsBeingDestroyed
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentDestructionCallbacksAndState)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageComponent_DestructionCallbacksState"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentDestructionCallbacksState.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageDestroyStateComponent : UActorComponent
			{
				UPROPERTY()
				int EndPlayCount = 0;

				UPROPERTY()
				bool DestroyingDuringEndPlay = false;

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					EndPlayCount++;
					DestroyingDuringEndPlay = IsBeingDestroyed();
				}
			}

			UCLASS()
			class ACoverageComponentDestructionStateActor : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageDestroyStateComponent DestroyProbe;

				UPROPERTY()
				bool DestroyCallCompleted = false;

				UPROPERTY()
				bool BeingDestroyedAfterCall = false;

				UFUNCTION()
				void DestroyProbeComponent()
				{
					if (DestroyProbe == nullptr)
					{
						return;
					}

					DestroyProbe.DestroyComponent();
					DestroyCallCompleted = true;
					BeingDestroyedAfterCall = DestroyProbe.IsBeingDestroyed();
				}
			}
			)AS"),
			TEXT("ACoverageComponentDestructionStateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Component destruction callback/state actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Component destruction callback/state actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UObject* ProbeObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("DestroyProbe"), ProbeObject, TEXT("DestroyProbe should be readable before destruction"))));
		UActorComponent* Probe = Cast<UActorComponent>(ProbeObject);
		ASSERT_THAT(IsNotNull(Probe, TEXT("DestroyProbe should be an actor component")));
		if (Probe == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Probe->IsRegistered(), TEXT("DestroyProbe should start registered")));

		FFunctionInvoker DestroyInvoker(*TestRunner, Actor, FName(TEXT("DestroyProbeComponent")));
		ASSERT_THAT(IsTrue(DestroyInvoker.IsValid(), TEXT("DestroyProbeComponent should be invokable")));
		if (!DestroyInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(DestroyInvoker.Call(), TEXT("DestroyProbeComponent should execute")));

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("DestroyCallCompleted"), true, TEXT("DestroyComponent should return to script after destroying the component"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("BeingDestroyedAfterCall"), true, TEXT("DestroyComponent should mark the component as being destroyed"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, Probe, TEXT("EndPlayCount"), 1, TEXT("DestroyComponent should call EndPlay once after BeginPlay"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Probe, TEXT("DestroyingDuringEndPlay"), true, TEXT("IsBeingDestroyed should be true during EndPlay"))));
		ASSERT_THAT(IsTrue(Probe->IsBeingDestroyed(), TEXT("Native IsBeingDestroyed should remain true after DestroyComponent")));
		ASSERT_THAT(IsFalse(Probe->IsRegistered(), TEXT("Native IsRegistered should be false after DestroyComponent")));
		ASSERT_THAT(AreEqual(0, CountRegisteredComponentsByClass(Actor, Probe->GetClass()), TEXT("Native registered component lookup should not return the destroyed component")));
	}

	// -------------------------------------------------------------------------
	// Component destruction with explicit promote-children flag and K2 metadata
	// -------------------------------------------------------------------------
	TEST_METHOD(ComponentDestroyComponentPromoteChildrenAndK2Metadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UFunction* K2DestroyFunction = UActorComponent::StaticClass()->FindFunctionByName(TEXT("K2_DestroyComponent"));
		ASSERT_THAT(IsNotNull(K2DestroyFunction, TEXT("UActorComponent should expose native K2_DestroyComponent")));
		if (K2DestroyFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("DestroyComponent")), K2DestroyFunction->GetMetaData(TEXT("ScriptName")), TEXT("K2_DestroyComponent should advertise DestroyComponent as its script name")));

		FObjectProperty* ObjectParameter = FindFProperty<FObjectProperty>(K2DestroyFunction, TEXT("Object"));
		ASSERT_THAT(IsNotNull(ObjectParameter, TEXT("K2_DestroyComponent should expose the Object parameter")));
		if (ObjectParameter == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectParameter->PropertyClass, TEXT("K2_DestroyComponent Object parameter should be UObject")));

		static const FName ModuleName(TEXT("ASCoverageComponent_DestroyPromoteChildren"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageComponentDestroyPromoteChildren.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageDestroyPromoteChildComponent : USceneComponent
			{
				UPROPERTY()
				int EndPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					EndPlayCount++;
				}
			}

			UCLASS()
			class ACoverageComponentDestroyPromoteActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCoverageDestroyPromoteChildComponent ParentProbe;

				UPROPERTY(DefaultComponent, Attach=ParentProbe)
				UCoverageDestroyPromoteChildComponent ChildProbe;

				UPROPERTY()
				bool DestroyReturned = false;

				UPROPERTY()
				bool ParentBeingDestroyedAfterCall = false;

				UPROPERTY()
				bool ChildReattachedToRoot = false;

				UFUNCTION()
				void DestroyParentWithPromotedChild()
				{
					if (ParentProbe == nullptr || ChildProbe == nullptr || Root == nullptr)
					{
						return;
					}

					ParentProbe.DestroyComponent(true);
					DestroyReturned = true;
					ParentBeingDestroyedAfterCall = ParentProbe.IsBeingDestroyed();
					ChildReattachedToRoot = ChildProbe.GetAttachParent() == Root;
				}
			}
			)AS"),
			TEXT("ACoverageComponentDestroyPromoteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DestroyComponent promote-children actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DestroyComponent promote-children actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		UObject* ParentProbeObject = nullptr;
		const bool bReadParentProbe = ReadObjectByPath(*TestRunner, Actor, TEXT("ParentProbe"), ParentProbeObject, TEXT("ParentProbe should be readable before destruction"));
		ASSERT_THAT(IsTrue(bReadParentProbe, TEXT("ParentProbe should be readable before destruction")));
		if (!bReadParentProbe)
		{
			return;
		}
		USceneComponent* ParentProbe = Cast<USceneComponent>(ParentProbeObject);
		ASSERT_THAT(IsNotNull(ParentProbe, TEXT("ParentProbe should be a scene component")));
		if (ParentProbe == nullptr)
		{
			return;
		}

		UObject* ChildProbeObject = nullptr;
		const bool bReadChildProbe = ReadObjectByPath(*TestRunner, Actor, TEXT("ChildProbe"), ChildProbeObject, TEXT("ChildProbe should be readable before destruction"));
		ASSERT_THAT(IsTrue(bReadChildProbe, TEXT("ChildProbe should be readable before destruction")));
		if (!bReadChildProbe)
		{
			return;
		}
		USceneComponent* ChildProbe = Cast<USceneComponent>(ChildProbeObject);
		ASSERT_THAT(IsNotNull(ChildProbe, TEXT("ChildProbe should be a scene component")));
		if (ChildProbe == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ParentProbe->IsRegistered(), TEXT("ParentProbe should start registered")));
		ASSERT_THAT(IsTrue(ChildProbe->IsRegistered(), TEXT("ChildProbe should start registered")));

		FFunctionInvoker DestroyInvoker(*TestRunner, Actor, FName(TEXT("DestroyParentWithPromotedChild")));
		ASSERT_THAT(IsTrue(DestroyInvoker.IsValid(), TEXT("DestroyParentWithPromotedChild should be invokable")));
		if (!DestroyInvoker.IsValid())
		{
			return;
		}
		const bool bDestroyCallSucceeded = DestroyInvoker.Call();
		ASSERT_THAT(IsTrue(bDestroyCallSucceeded, TEXT("DestroyParentWithPromotedChild should execute")));
		if (!bDestroyCallSucceeded)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("DestroyReturned"), true, TEXT("DestroyComponent(true) should return to script"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ParentBeingDestroyedAfterCall"), true, TEXT("DestroyComponent(true) should mark the parent component as being destroyed"))));
		ASSERT_THAT(IsTrue(ExpectBoolByPath(*TestRunner, Actor, TEXT("ChildReattachedToRoot"), true, TEXT("DestroyComponent(true) should promote children to the destroyed component parent"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, ParentProbe, TEXT("EndPlayCount"), 1, TEXT("DestroyComponent(true) should end play on the destroyed parent once"))));
		ASSERT_THAT(IsTrue(ExpectIntByPath(*TestRunner, ChildProbe, TEXT("EndPlayCount"), 0, TEXT("DestroyComponent(true) should not destroy promoted children"))));
		ASSERT_THAT(IsTrue(ParentProbe->IsBeingDestroyed(), TEXT("Native IsBeingDestroyed should remain true after DestroyComponent(true)")));
		ASSERT_THAT(IsFalse(ParentProbe->IsRegistered(), TEXT("Native IsRegistered should remain false after DestroyComponent(true)")));
		ASSERT_THAT(IsFalse(ChildProbe->IsBeingDestroyed(), TEXT("Native child component should not be marked destroyed after promotion")));
		ASSERT_THAT(IsTrue(ChildProbe->IsRegistered(), TEXT("Native child component should remain registered after promotion")));
	}

};

#endif // WITH_DEV_AUTOMATION_TESTS
