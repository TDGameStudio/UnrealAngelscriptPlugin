#include "Bind_Subsystems.h"

#include "AngelscriptEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "Subsystems/WorldSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorSubsystem.h"
#endif

namespace
{
	template<typename TSubsystem>
	TSubclassOf<TSubsystem> MakeSubsystemClass(UClass* Class)
	{
		if (Class == nullptr || !Class->IsChildOf(TSubsystem::StaticClass()))
		{
			return TSubclassOf<TSubsystem>();
		}

		return TSubclassOf<TSubsystem>(Class);
	}

	UClass* GetBoundSubsystemClass()
	{
		return FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
	}

	UWorld* GetCurrentWorld()
	{
		return GEngine->GetWorldFromContextObject(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			EGetWorldErrorMode::ReturnNull);
	}
}

UObject* FAngelscriptSubsystemsBinds::GetEngineSubsystem(UClass* Class)
{
	const TSubclassOf<UEngineSubsystem> SubsystemClass = MakeSubsystemClass<UEngineSubsystem>(Class);
	return SubsystemClass ? USubsystemBlueprintLibrary::GetEngineSubsystem(SubsystemClass) : nullptr;
}

UObject* FAngelscriptSubsystemsBinds::GetGameInstanceSubsystem(UClass* Class)
{
	const TSubclassOf<UGameInstanceSubsystem> SubsystemClass = MakeSubsystemClass<UGameInstanceSubsystem>(Class);
	return SubsystemClass
		? USubsystemBlueprintLibrary::GetGameInstanceSubsystem(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			SubsystemClass)
		: nullptr;
}

UObject* FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystem(UClass* Class)
{
	const TSubclassOf<ULocalPlayerSubsystem> SubsystemClass = MakeSubsystemClass<ULocalPlayerSubsystem>(Class);
	return SubsystemClass
		? USubsystemBlueprintLibrary::GetLocalPlayerSubsystem(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			SubsystemClass)
		: nullptr;
}

UObject* FAngelscriptSubsystemsBinds::GetWorldSubsystem(UClass* Class)
{
	const TSubclassOf<UWorldSubsystem> SubsystemClass = MakeSubsystemClass<UWorldSubsystem>(Class);
	return SubsystemClass
		? USubsystemBlueprintLibrary::GetWorldSubsystem(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			SubsystemClass)
		: nullptr;
}

UObject* FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemFromLocalPlayer(
	ULocalPlayer* LocalPlayer,
	UClass* Class)
{
	const TSubclassOf<ULocalPlayerSubsystem> SubsystemClass = MakeSubsystemClass<ULocalPlayerSubsystem>(Class);
	return LocalPlayer != nullptr && SubsystemClass
		? LocalPlayer->GetSubsystemBase(SubsystemClass)
		: nullptr;
}

UObject* FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemFromPlayerController(
	APlayerController* PlayerController,
	UClass* Class)
{
	const TSubclassOf<ULocalPlayerSubsystem> SubsystemClass = MakeSubsystemClass<ULocalPlayerSubsystem>(Class);
	return PlayerController != nullptr && SubsystemClass
		? USubsystemBlueprintLibrary::GetLocalPlayerSubSystemFromPlayerController(PlayerController, SubsystemClass)
		: nullptr;
}

#if WITH_EDITOR
UEditorSubsystem* FAngelscriptSubsystemsBinds::GetEditorSubsystemForClass()
{
	return GEditor->GetEditorSubsystemBase(GetBoundSubsystemClass());
}
#endif

UEngineSubsystem* FAngelscriptSubsystemsBinds::GetEngineSubsystemForClass()
{
	return GEngine->GetEngineSubsystemBase(GetBoundSubsystemClass());
}

UGameInstanceSubsystem* FAngelscriptSubsystemsBinds::GetGameInstanceSubsystemForClass()
{
	UWorld* World = GetCurrentWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	const UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance != nullptr
		? GameInstance->GetSubsystemBase(GetBoundSubsystemClass())
		: nullptr;
}

UWorldSubsystem* FAngelscriptSubsystemsBinds::GetWorldSubsystemForClass()
{
	UWorld* World = GetCurrentWorld();
	return World != nullptr
		? World->GetSubsystemBase(GetBoundSubsystemClass())
		: nullptr;
}

ULocalPlayerSubsystem* FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemForLocalPlayer(ULocalPlayer* LocalPlayer)
{
	return LocalPlayer != nullptr
		? LocalPlayer->GetSubsystemBase(GetBoundSubsystemClass())
		: nullptr;
}

ULocalPlayerSubsystem* FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemForPlayerController(
	APlayerController* PlayerController)
{
	if (PlayerController == nullptr)
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
	return LocalPlayer != nullptr
		? LocalPlayer->GetSubsystemBase(GetBoundSubsystemClass())
		: nullptr;
}
