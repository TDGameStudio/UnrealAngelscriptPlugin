#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UClass;
class UEngineSubsystem;
class UGameInstanceSubsystem;
class ULocalPlayer;
class ULocalPlayerSubsystem;
class UObject;
class UWorldSubsystem;

#if WITH_EDITOR
class UEditorSubsystem;
#endif

struct FAngelscriptSubsystemsBinds
{
	static UObject* GetEngineSubsystem(UClass* Class);
	static UObject* GetGameInstanceSubsystem(UClass* Class);
	static UObject* GetLocalPlayerSubsystem(UClass* Class);
	static UObject* GetWorldSubsystem(UClass* Class);
	static UObject* GetLocalPlayerSubsystemFromLocalPlayer(ULocalPlayer* LocalPlayer, UClass* Class);
	static UObject* GetLocalPlayerSubsystemFromPlayerController(APlayerController* PlayerController, UClass* Class);

#if WITH_EDITOR
	static UEditorSubsystem* GetEditorSubsystemForClass();
#endif
	static UEngineSubsystem* GetEngineSubsystemForClass();
	static UGameInstanceSubsystem* GetGameInstanceSubsystemForClass();
	static UWorldSubsystem* GetWorldSubsystemForClass();
	static ULocalPlayerSubsystem* GetLocalPlayerSubsystemForLocalPlayer(ULocalPlayer* LocalPlayer);
	static ULocalPlayerSubsystem* GetLocalPlayerSubsystemForPlayerController(APlayerController* PlayerController);
};
