#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageAudioComponentTests
// -----------------------------------------------------------------------------
// Coverage for the high-priority AudioComponent slice from:
//
//   Documents/Coverage/Coverage_Audio.md
//
// This deliberately avoids requiring a real audio device or sound asset in
// headless automation. It verifies that AS can declare an audio component,
// call the exposed playback/control/parameter APIs, and observe stable state.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageAudioComponentTest,
	"Angelscript.TestModule.Coverage.AudioComponent",
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FObjectProperty* AudioProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("Audio"));
		ASSERT_THAT(IsNotNull(AudioProperty, TEXT("Audio component property should be generated")));
		ASSERT_THAT(IsTrue(AudioProperty->PropertyClass != nullptr && AudioProperty->PropertyClass->IsChildOf(UAudioComponent::StaticClass()),
			TEXT("Audio property should reference UAudioComponent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Audio coverage actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAudioComponentValid"), true,
			TEXT("UAudioComponent DefaultComponent should be created"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPlaybackControlsCallable"), true,
			TEXT("Audio playback controls should be callable from AS"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bParameterControlsCallable"), true,
			TEXT("Audio parameter controls should be callable from AS"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAudioRemainsStoppedWithoutSound"), true,
			TEXT("Audio component without a sound should remain stopped after Play/Stop"));
	}

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Audio fade/filter coverage actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFadeControlsCallable"), true,
			TEXT("Audio fade controls should be callable from AS"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFilterControlsCallable"), true,
			TEXT("Audio filter controls should be callable from AS"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
