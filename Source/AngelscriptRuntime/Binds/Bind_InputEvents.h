#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Components/SlateWrapperTypes.h"

class UWidget;

struct FAngelscriptInputEventsBinds
{
	static void ConstructKey(FKey* Key, const FName& KeyName);
	static FKey ConvertNameToKey(const FName& Name);
	static bool AreKeysEqual(const FKey& Left, const FKey& Right);
	static void AppendKeyToString(void* Value, FString& OutString);
	static void ConstructInputChord(FInputChord* InputChord, const FKey& Key);
	static void ConstructInputChordWithModifiers(FInputChord* InputChord, const FKey& Key, bool bShift, bool bCtrl, bool bAlt, bool bCmd);

	static FVector2D GetScreenSpacePosition(FPointerEvent* Event);
	static FVector2D GetLastScreenSpacePosition(FPointerEvent* Event);
	static FVector2D GetCursorDelta(FPointerEvent* Event);
	static FVector2D GetGestureDelta(FPointerEvent* Event);
	static uint32 GetTouchpadIndex(const FPointerEvent* Event);

	static FEventReply* PreventThrottling(FEventReply* Reply);
	static FEventReply* SetUserFocus(FEventReply* Reply, UWidget* FocusWidget, EFocusCause Cause, bool bInAllUsers);
	static FEventReply* ClearUserFocus(FEventReply* Reply, bool bInAllUsers);
	static FEventReply* CaptureMouse(FEventReply* Reply, UWidget* CaptureWidget);
	static FEventReply* UseHighPrecisionMouseMovement(FEventReply* Reply, UWidget* CaptureWidget);
	static FEventReply* ReleaseMouseCapture(FEventReply* Reply);
	static FEventReply* LockMouseToWidget(FEventReply* Reply, UWidget* CaptureWidget);
	static FEventReply* ReleaseMouseLock(FEventReply* Reply);
	static FEventReply* SetMousePosition(FEventReply* Reply, const FIntPoint& NewMousePos);
	static FEventReply* SetNavigation(FEventReply* Reply, EUINavigation NavigationType, ENavigationGenesis Genesis, ENavigationSource Source);
	static FEventReply* SetNavigationDestination(FEventReply* Reply, UWidget* NavigationDestination, ENavigationGenesis Genesis, ENavigationSource Source);
	static FEventReply MakeHandledReply();
	static FEventReply MakeUnhandledReply();

	static FString GetCharacterString(const FCharacterEvent& Event);
};
