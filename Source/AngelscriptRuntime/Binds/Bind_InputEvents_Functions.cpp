#include "Bind_InputEvents.h"

#include "Components/Widget.h"

void FAngelscriptInputEventsBinds::ConstructKey(FKey* Key, const FName& KeyName)
{
	new (Key) FKey(KeyName);
}

FKey FAngelscriptInputEventsBinds::ConvertNameToKey(const FName& Name)
{
	return FKey(Name);
}

bool FAngelscriptInputEventsBinds::AreKeysEqual(const FKey& Left, const FKey& Right)
{
	return Left == Right;
}

void FAngelscriptInputEventsBinds::AppendKeyToString(void* Value, FString& OutString)
{
	OutString += static_cast<FKey*>(Value)->ToString();
}

void FAngelscriptInputEventsBinds::ConstructInputChord(FInputChord* InputChord, const FKey& Key)
{
	new (InputChord) FInputChord(Key);
}

void FAngelscriptInputEventsBinds::ConstructInputChordWithModifiers(
	FInputChord* InputChord,
	const FKey& Key,
	bool bShift,
	bool bCtrl,
	bool bAlt,
	bool bCmd)
{
	new (InputChord) FInputChord(Key, bShift, bCtrl, bAlt, bCmd);
}

FVector2D FAngelscriptInputEventsBinds::GetScreenSpacePosition(FPointerEvent* Event)
{
	return FVector2D(Event->GetScreenSpacePosition());
}

FVector2D FAngelscriptInputEventsBinds::GetLastScreenSpacePosition(FPointerEvent* Event)
{
	return FVector2D(Event->GetLastScreenSpacePosition());
}

FVector2D FAngelscriptInputEventsBinds::GetCursorDelta(FPointerEvent* Event)
{
	return FVector2D(Event->GetCursorDelta());
}

FVector2D FAngelscriptInputEventsBinds::GetGestureDelta(FPointerEvent* Event)
{
	return FVector2D(Event->GetGestureDelta());
}

uint32 FAngelscriptInputEventsBinds::GetTouchpadIndex(const FPointerEvent*)
{
	return 0;
}

FEventReply* FAngelscriptInputEventsBinds::PreventThrottling(FEventReply* Reply)
{
	Reply->NativeReply.PreventThrottling();
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::SetUserFocus(FEventReply* Reply, UWidget* FocusWidget, EFocusCause Cause, bool bInAllUsers)
{
	TSharedPtr<SWidget> CapturingSlateWidget = FocusWidget->GetCachedWidget();
	if (CapturingSlateWidget.IsValid())
	{
		Reply->NativeReply = Reply->NativeReply.SetUserFocus(CapturingSlateWidget.ToSharedRef(), Cause, bInAllUsers);
	}
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::ClearUserFocus(FEventReply* Reply, bool bInAllUsers)
{
	Reply->NativeReply = Reply->NativeReply.ClearUserFocus(bInAllUsers);
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::CaptureMouse(FEventReply* Reply, UWidget* CaptureWidget)
{
	TSharedPtr<SWidget> CapturingSlateWidget = CaptureWidget->GetCachedWidget();
	if (CapturingSlateWidget.IsValid())
	{
		Reply->NativeReply.CaptureMouse(CapturingSlateWidget.ToSharedRef());
	}
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::UseHighPrecisionMouseMovement(FEventReply* Reply, UWidget* CaptureWidget)
{
	TSharedPtr<SWidget> CapturingSlateWidget = CaptureWidget->GetCachedWidget();
	if (CapturingSlateWidget.IsValid())
	{
		Reply->NativeReply.UseHighPrecisionMouseMovement(CapturingSlateWidget.ToSharedRef());
	}
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::ReleaseMouseCapture(FEventReply* Reply)
{
	Reply->NativeReply.ReleaseMouseCapture();
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::LockMouseToWidget(FEventReply* Reply, UWidget* CaptureWidget)
{
	TSharedPtr<SWidget> CapturingSlateWidget = CaptureWidget->GetCachedWidget();
	if (CapturingSlateWidget.IsValid())
	{
		Reply->NativeReply.LockMouseToWidget(CapturingSlateWidget.ToSharedRef());
	}
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::ReleaseMouseLock(FEventReply* Reply)
{
	Reply->NativeReply.ReleaseMouseLock();
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::SetMousePosition(FEventReply* Reply, const FIntPoint& NewMousePos)
{
	Reply->NativeReply.SetMousePos(NewMousePos);
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::SetNavigation(FEventReply* Reply, EUINavigation NavigationType, ENavigationGenesis Genesis, ENavigationSource Source)
{
	Reply->NativeReply.SetNavigation(NavigationType, Genesis, Source);
	return Reply;
}

FEventReply* FAngelscriptInputEventsBinds::SetNavigationDestination(
	FEventReply* Reply,
	UWidget* NavigationDestination,
	ENavigationGenesis Genesis,
	ENavigationSource Source)
{
	TSharedPtr<SWidget> CapturingSlateWidget = NavigationDestination->GetCachedWidget();
	if (CapturingSlateWidget.IsValid())
	{
		Reply->NativeReply.SetNavigation(CapturingSlateWidget.ToSharedRef(), Genesis, Source);
	}
	return Reply;
}

FEventReply FAngelscriptInputEventsBinds::MakeHandledReply()
{
	return FEventReply(true);
}

FEventReply FAngelscriptInputEventsBinds::MakeUnhandledReply()
{
	return FEventReply(false);
}

FString FAngelscriptInputEventsBinds::GetCharacterString(const FCharacterEvent& Event)
{
	TCHAR Character = Event.GetCharacter();
	return FString(1, &Character);
}
