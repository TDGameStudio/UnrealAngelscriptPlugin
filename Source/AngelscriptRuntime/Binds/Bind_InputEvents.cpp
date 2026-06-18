#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Engine/EngineTypes.h"
#include "Helper_ToString.h"

#include "InputCoreTypes.h"
#include "Input/Events.h"

#include "Components/Widget.h"

#include "Components/SlateWrapperTypes.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_InputEvents(FAngelscriptBinds::EOrder::Late, []
{
	auto FKey_ = FAngelscriptBinds::ExistingClass("FKey");
	FKey_.Constructor("void f(const FName& KeyName)", [](FKey* Key, const FName& KeyName)
	{
		new(Key) FKey(KeyName);
	});

	auto FName_ = FAngelscriptBinds::ExistingClass("FName");
	FName_.Method("FKey opImplConv() const", [](const FName& Name) -> FKey
	{
		return FKey(Name);
	});

	FKey_.Method("bool IsValid() const", &FKey::IsValid);
	FKey_.Method("bool IsModifierKey() const", &FKey::IsModifierKey);
	FKey_.Method("bool IsGamepadKey() const", &FKey::IsGamepadKey);
	FKey_.Method("bool IsTouch() const", &FKey::IsTouch);
	FKey_.Method("bool IsMouseButton() const", &FKey::IsMouseButton);
	FKey_.Method("bool IsAxis1D() const", &FKey::IsAxis1D);
	FKey_.Method("bool IsAxis2D() const", &FKey::IsAxis2D);
	FKey_.Method("bool IsAxis3D() const", &FKey::IsAxis3D);
	FKey_.Method("FText GetDisplayName(bool bLongDisplayName = true) const", &FKey::GetDisplayName);
	FKey_.Method("FName GetKeyName() const", &FKey::GetFName);

	FKey_.Method("bool opEquals(const FKey& Other) const",
	[](const FKey& A, const FKey& B) -> bool
	{
		return A == B;
	});

	FToStringHelper::Register(TEXT("FKey"), [](void* Ptr, FString& Str)
	{
		Str += ((FKey*)Ptr)->ToString();
	});

	auto FInputChord_ = FAngelscriptBinds::ExistingClass("FInputChord");
	FInputChord_.Constructor("void f(const FKey& Key)", [](FInputChord* InputChord, const FKey& Key)
	{
		new(InputChord) FInputChord(Key);
	});

	FInputChord_.Constructor("void f(const FKey& Key, bool bShift, bool bCtrl, bool bAlt, bool bCmd)", [](FInputChord* InputChord, const FKey& Key, bool bShift, bool bCtrl, bool bAlt, bool bCmd)
	{
		new(InputChord) FInputChord(Key, bShift, bCtrl, bAlt, bCmd);
	});

	auto FKeyEvent_ = FAngelscriptBinds::ExistingClass("FKeyEvent");
	FKeyEvent_.Method("bool IsRepeat() const", &FKeyEvent::IsRepeat);
	FKeyEvent_.Method("bool IsShiftDown() const", &FKeyEvent::IsShiftDown);
	FKeyEvent_.Method("bool IsLeftShiftDown() const", &FKeyEvent::IsLeftShiftDown);
	FKeyEvent_.Method("bool IsRightShiftDown() const", &FKeyEvent::IsRightShiftDown);
	FKeyEvent_.Method("bool IsControlDown() const", &FKeyEvent::IsControlDown);
	FKeyEvent_.Method("bool IsLeftControlDown() const", &FKeyEvent::IsLeftControlDown);
	FKeyEvent_.Method("bool IsRightControlDown() const", &FKeyEvent::IsRightControlDown);
	FKeyEvent_.Method("bool IsAltDown() const", &FKeyEvent::IsAltDown);
	FKeyEvent_.Method("bool IsLeftAltDown() const", &FKeyEvent::IsLeftAltDown);
	FKeyEvent_.Method("bool IsRightAltDown() const", &FKeyEvent::IsRightAltDown);
	FKeyEvent_.Method("bool IsCommandDown() const", &FKeyEvent::IsCommandDown);
	FKeyEvent_.Method("bool IsLeftCommandDown() const", &FKeyEvent::IsLeftCommandDown);
	FKeyEvent_.Method("bool IsRightCommandDown() const", &FKeyEvent::IsRightCommandDown);
	FKeyEvent_.Method("bool AreCapsLocked() const", &FKeyEvent::AreCapsLocked);
	FKeyEvent_.Method("uint32 GetUserIndex() const", &FKeyEvent::GetUserIndex);
	FKeyEvent_.Method("FPlatformUserId GetPlatformUserid() const", &FKeyEvent::GetPlatformUserId);
	FKeyEvent_.Method("FInputDeviceId GetInputDeviceId() const", &FKeyEvent::GetInputDeviceId);

	FKeyEvent_.Method("FKey GetKey() const", &FKeyEvent::GetKey);
	FKeyEvent_.Method("uint32 GetCharacter() const", &FKeyEvent::GetCharacter);
	FKeyEvent_.Method("uint32 GetKeyCode() const", &FKeyEvent::GetKeyCode);

	auto FPointerEvent_ = FAngelscriptBinds::ExistingClass("FPointerEvent");
	FPointerEvent_.Method("bool IsRepeat() const", &FPointerEvent::IsRepeat);
	FPointerEvent_.Method("bool IsShiftDown() const", &FPointerEvent::IsShiftDown);
	FPointerEvent_.Method("bool IsLeftShiftDown() const", &FPointerEvent::IsLeftShiftDown);
	FPointerEvent_.Method("bool IsRightShiftDown() const", &FPointerEvent::IsRightShiftDown);
	FPointerEvent_.Method("bool IsControlDown() const", &FPointerEvent::IsControlDown);
	FPointerEvent_.Method("bool IsLeftControlDown() const", &FPointerEvent::IsLeftControlDown);
	FPointerEvent_.Method("bool IsRightControlDown() const", &FPointerEvent::IsRightControlDown);
	FPointerEvent_.Method("bool IsAltDown() const", &FPointerEvent::IsAltDown);
	FPointerEvent_.Method("bool IsLeftAltDown() const", &FPointerEvent::IsLeftAltDown);
	FPointerEvent_.Method("bool IsRightAltDown() const", &FPointerEvent::IsRightAltDown);
	FPointerEvent_.Method("bool IsCommandDown() const", &FPointerEvent::IsCommandDown);
	FPointerEvent_.Method("bool IsLeftCommandDown() const", &FPointerEvent::IsLeftCommandDown);
	FPointerEvent_.Method("bool IsRightCommandDown() const", &FPointerEvent::IsRightCommandDown);
	FPointerEvent_.Method("bool AreCapsLocked() const", &FPointerEvent::AreCapsLocked);
	FPointerEvent_.Method("uint32 GetUserIndex() const", &FPointerEvent::GetUserIndex);
	FPointerEvent_.Method("FPlatformUserId GetPlatformUserid() const", &FPointerEvent::GetPlatformUserId);
	FPointerEvent_.Method("FInputDeviceId GetInputDeviceId() const", &FPointerEvent::GetInputDeviceId);

	FPointerEvent_.Method("FVector2D GetScreenSpacePosition() const", [](FPointerEvent* Event) -> FVector2D { return FVector2D(Event->GetScreenSpacePosition()); });
	FPointerEvent_.Method("FVector2D GetLastScreenSpacePosition() const", [](FPointerEvent* Event) -> FVector2D { return FVector2D(Event->GetLastScreenSpacePosition()); });
	FPointerEvent_.Method("FVector2D GetCursorDelta() const", [](FPointerEvent* Event) -> FVector2D { return FVector2D(Event->GetCursorDelta()); });
	FPointerEvent_.Method("FVector2D GetGestureDelta() const", [](FPointerEvent* Event) -> FVector2D { return FVector2D(Event->GetGestureDelta()); });

	FPointerEvent_.Method("bool IsMouseButtonDown(FKey MouseButton) const", &FPointerEvent::IsMouseButtonDown);
	FPointerEvent_.Method("FKey GetEffectingButton() const", &FPointerEvent::GetEffectingButton);
	FPointerEvent_.Method("float32 GetWheelDelta() const", &FPointerEvent::GetWheelDelta);
	FPointerEvent_.Method("uint32 GetPointerIndex() const", &FPointerEvent::GetPointerIndex);
	FPointerEvent_.Method("uint32 GetTouchpadIndex() const", [](const FPointerEvent*) -> uint32 { return 0; });
	FPointerEvent_.Method("float32 GetTouchForce() const", &FPointerEvent::GetTouchForce);
	FPointerEvent_.Method("bool IsTouchEvent() const", &FPointerEvent::IsTouchEvent);
	FPointerEvent_.Method("bool IsTouchForceChangedEvent() const", &FPointerEvent::IsTouchForceChangedEvent);
	FPointerEvent_.Method("bool IsTouchFirstMoveEvent() const", &FPointerEvent::IsTouchFirstMoveEvent);
	FPointerEvent_.Method("bool IsDirectionInvertedFromDevice() const", &FPointerEvent::IsDirectionInvertedFromDevice);

	auto FEventReply_ = FAngelscriptBinds::ExistingClass("FEventReply");
	FEventReply_.Method("FEventReply& PreventThrottling() accept_temporary_this", [](FEventReply& Event) -> FEventReply&
	{
		Event.NativeReply.PreventThrottling();
		return Event;
	});

	FEventReply_.Method("FEventReply& SetUserFocus(UWidget Widget, EFocusCause FocusCause = EFocusCause::SetDirectly, bool bAllUsers = false) accept_temporary_this",
		[](FEventReply& Reply, UWidget* FocusWidget, EFocusCause Cause, bool bInAllUsers) -> FEventReply&
		{
			TSharedPtr<SWidget> CapturingSlateWidget = FocusWidget->GetCachedWidget();
			if (CapturingSlateWidget.IsValid())
			{
				Reply.NativeReply = Reply.NativeReply.SetUserFocus(CapturingSlateWidget.ToSharedRef(), Cause, bInAllUsers);
			}
			return Reply;
		});

	FEventReply_.Method("FEventReply& ClearUserFocus(bool bAllUsers = false) accept_temporary_this",
		[](FEventReply& Reply, bool bInAllUsers) -> FEventReply&
		{
			Reply.NativeReply = Reply.NativeReply.ClearUserFocus(bInAllUsers);
			return Reply;
		});

	FEventReply_.Method("FEventReply& CaptureMouse(UWidget Widget) accept_temporary_this",
		[](FEventReply& Reply, UWidget* CaptureWidget) -> FEventReply&
		{
			TSharedPtr<SWidget> CapturingSlateWidget = CaptureWidget->GetCachedWidget();
			if (CapturingSlateWidget.IsValid())
			{
				Reply.NativeReply.CaptureMouse(CapturingSlateWidget.ToSharedRef());
			}
			return Reply;
		});

	FEventReply_.Method("FEventReply& UseHighPrecisionMouseMovement(UWidget Widget) accept_temporary_this",
		[](FEventReply& Reply, UWidget* CaptureWidget) -> FEventReply&
		{
			TSharedPtr<SWidget> CapturingSlateWidget = CaptureWidget->GetCachedWidget();
			if (CapturingSlateWidget.IsValid())
			{
				Reply.NativeReply.UseHighPrecisionMouseMovement(CapturingSlateWidget.ToSharedRef());
			}
			return Reply;
		});

	FEventReply_.Method("FEventReply& ReleaseMouseCapture() accept_temporary_this",
		[](FEventReply& Reply) -> FEventReply&
		{
			Reply.NativeReply.ReleaseMouseCapture();
			return Reply;
		});

	FEventReply_.Method("FEventReply& LockMouseToWidget(UWidget Widget) accept_temporary_this",
		[](FEventReply& Reply, UWidget* CaptureWidget) -> FEventReply&
		{
			TSharedPtr<SWidget> CapturingSlateWidget = CaptureWidget->GetCachedWidget();
			if (CapturingSlateWidget.IsValid())
			{
				Reply.NativeReply.LockMouseToWidget(CapturingSlateWidget.ToSharedRef());
			}
			return Reply;
		});

	FEventReply_.Method("FEventReply& ReleaseMouseLock() accept_temporary_this",
		[](FEventReply& Reply) -> FEventReply&
		{
			Reply.NativeReply.ReleaseMouseLock();
			return Reply;
		});

	FEventReply_.Method("FEventReply& SetMousePos(const FIntPoint& NewMousePos) accept_temporary_this",
		[](FEventReply& Reply, const FIntPoint& NewMousePos) -> FEventReply&
		{
			Reply.NativeReply.SetMousePos(NewMousePos);
			return Reply;
		});

	FEventReply_.Method("FEventReply& SetNavigation(EUINavigation NavigationType, ENavigationGenesis Genesis, ENavigationSource Source = ENavigationSource::FocusedWidget) accept_temporary_this",
		[](FEventReply& Reply, EUINavigation NavigationType, ENavigationGenesis Genesis, ENavigationSource Source) -> FEventReply&
		{
			Reply.NativeReply.SetNavigation(NavigationType, Genesis, Source);
			return Reply;
		});

	FEventReply_.Method("FEventReply& SetNavigation(UWidget NavigationDestination, ENavigationGenesis Genesis, ENavigationSource Source = ENavigationSource::FocusedWidget) accept_temporary_this",
		[](FEventReply& Reply, UWidget* NavigationDestination, ENavigationGenesis Genesis, ENavigationSource Source) -> FEventReply&
		{
			TSharedPtr<SWidget> CapturingSlateWidget = NavigationDestination->GetCachedWidget();
			if (CapturingSlateWidget.IsValid())
			{
				Reply.NativeReply.SetNavigation(CapturingSlateWidget.ToSharedRef(), Genesis, Source);
			}
			return Reply;
		});

	{
		FAngelscriptBinds::FNamespace ns("FEventReply");

		FAngelscriptBinds::BindGlobalFunction("FEventReply Handled()", []() -> FEventReply
		{
			return FEventReply(true);
		});

		FAngelscriptBinds::BindGlobalFunction("FEventReply Unhandled()", []() -> FEventReply
		{
			return FEventReply(false);
		});
	}

	auto FNavigationEvent_ = FAngelscriptBinds::ExistingClass("FNavigationEvent");
	FNavigationEvent_.Method("bool IsRepeat() const", &FNavigationEvent::IsRepeat);
	FNavigationEvent_.Method("bool IsShiftDown() const", &FNavigationEvent::IsShiftDown);
	FNavigationEvent_.Method("bool IsLeftShiftDown() const", &FNavigationEvent::IsLeftShiftDown);
	FNavigationEvent_.Method("bool IsRightShiftDown() const", &FNavigationEvent::IsRightShiftDown);
	FNavigationEvent_.Method("bool IsControlDown() const", &FNavigationEvent::IsControlDown);
	FNavigationEvent_.Method("bool IsLeftControlDown() const", &FNavigationEvent::IsLeftControlDown);
	FNavigationEvent_.Method("bool IsRightControlDown() const", &FNavigationEvent::IsRightControlDown);
	FNavigationEvent_.Method("bool IsAltDown() const", &FNavigationEvent::IsAltDown);
	FNavigationEvent_.Method("bool IsLeftAltDown() const", &FNavigationEvent::IsLeftAltDown);
	FNavigationEvent_.Method("bool IsRightAltDown() const", &FNavigationEvent::IsRightAltDown);
	FNavigationEvent_.Method("bool IsCommandDown() const", &FNavigationEvent::IsCommandDown);
	FNavigationEvent_.Method("bool IsLeftCommandDown() const", &FNavigationEvent::IsLeftCommandDown);
	FNavigationEvent_.Method("bool IsRightCommandDown() const", &FNavigationEvent::IsRightCommandDown);
	FNavigationEvent_.Method("bool AreCapsLocked() const", &FNavigationEvent::AreCapsLocked);
	FNavigationEvent_.Method("uint32 GetUserIndex() const", &FNavigationEvent::GetUserIndex);
	FNavigationEvent_.Method("FPlatformUserId GetPlatformUserid() const", &FNavigationEvent::GetPlatformUserId);
	FNavigationEvent_.Method("FInputDeviceId GetInputDeviceId() const", &FNavigationEvent::GetInputDeviceId);

	FNavigationEvent_.Method("EUINavigation GetNavigationType() const", &FNavigationEvent::GetNavigationType);
	FNavigationEvent_.Method("ENavigationGenesis GetNavigationGenesis() const", &FNavigationEvent::GetNavigationGenesis);

	auto FAnalogInputEvent_ = FAngelscriptBinds::ExistingClass("FAnalogInputEvent");
	FAnalogInputEvent_.Method("float32 GetAnalogValue() const", &FAnalogInputEvent::GetAnalogValue);
	FAnalogInputEvent_.Method("uint32 GetUserIndex() const", &FAnalogInputEvent::GetUserIndex);
	FAnalogInputEvent_.Method("FPlatformUserId GetPlatformUserid() const", &FAnalogInputEvent::GetPlatformUserId);
	FAnalogInputEvent_.Method("FInputDeviceId GetInputDeviceId() const", &FAnalogInputEvent::GetInputDeviceId);
	FAnalogInputEvent_.Method("FKey GetKey() const", &FAnalogInputEvent::GetKey);
	FAnalogInputEvent_.Method("uint32 GetKeyCode() const", &FAnalogInputEvent::GetKeyCode);

	auto FFocusEvent_ = FAngelscriptBinds::ExistingClass("FFocusEvent");
	FFocusEvent_.Method("EFocusCause GetCause() const", &FFocusEvent::GetCause);
	FFocusEvent_.Method("uint32 GetUser() const", &FFocusEvent::GetUser);

	auto FCharacterEvent_ = FAngelscriptBinds::ExistingClass("FCharacterEvent");
	FCharacterEvent_.Method("bool IsRepeat() const", &FCharacterEvent::IsRepeat);
	FCharacterEvent_.Method("bool IsShiftDown() const", &FCharacterEvent::IsShiftDown);
	FCharacterEvent_.Method("bool IsLeftShiftDown() const", &FCharacterEvent::IsLeftShiftDown);
	FCharacterEvent_.Method("bool IsRightShiftDown() const", &FCharacterEvent::IsRightShiftDown);
	FCharacterEvent_.Method("bool IsControlDown() const", &FCharacterEvent::IsControlDown);
	FCharacterEvent_.Method("bool IsLeftControlDown() const", &FCharacterEvent::IsLeftControlDown);
	FCharacterEvent_.Method("bool IsRightControlDown() const", &FCharacterEvent::IsRightControlDown);
	FCharacterEvent_.Method("bool IsAltDown() const", &FCharacterEvent::IsAltDown);
	FCharacterEvent_.Method("bool IsLeftAltDown() const", &FCharacterEvent::IsLeftAltDown);
	FCharacterEvent_.Method("bool IsRightAltDown() const", &FCharacterEvent::IsRightAltDown);
	FCharacterEvent_.Method("bool IsCommandDown() const", &FCharacterEvent::IsCommandDown);
	FCharacterEvent_.Method("bool IsLeftCommandDown() const", &FCharacterEvent::IsLeftCommandDown);
	FCharacterEvent_.Method("bool IsRightCommandDown() const", &FCharacterEvent::IsRightCommandDown);
	FCharacterEvent_.Method("bool AreCapsLocked() const", &FCharacterEvent::AreCapsLocked);
	FCharacterEvent_.Method("uint32 GetUserIndex() const", &FCharacterEvent::GetUserIndex);
	FCharacterEvent_.Method("FPlatformUserId GetPlatformUserid() const", &FCharacterEvent::GetPlatformUserId);
	FCharacterEvent_.Method("FInputDeviceId GetInputDeviceId() const", &FCharacterEvent::GetInputDeviceId);

	FCharacterEvent_.Method("uint16 GetCharacter() const", &FCharacterEvent::GetCharacter);
	FCharacterEvent_.Method("FString GetString() const", [](const FCharacterEvent& Event) -> FString
	{
		TCHAR Character = Event.GetCharacter();
		return FString(1, &Character);
	});

	{
		FAngelscriptBinds::FNamespace ns("EKeys");

#define BIND_EKEYS(Key) FAngelscriptBinds::BindGlobalVariable("const FKey " #Key, &EKeys::Key);

		BIND_EKEYS(AnyKey);

		BIND_EKEYS(MouseX);
		BIND_EKEYS(MouseY);
		BIND_EKEYS(Mouse2D);
		BIND_EKEYS(MouseScrollUp);
		BIND_EKEYS(MouseScrollDown);
		BIND_EKEYS(MouseWheelAxis);

		BIND_EKEYS(LeftMouseButton);
		BIND_EKEYS(RightMouseButton);
		BIND_EKEYS(MiddleMouseButton);
		BIND_EKEYS(ThumbMouseButton);
		BIND_EKEYS(ThumbMouseButton2);

		BIND_EKEYS(BackSpace);
		BIND_EKEYS(Tab);
		BIND_EKEYS(Enter);
		BIND_EKEYS(Pause);

		BIND_EKEYS(CapsLock);
		BIND_EKEYS(Escape);
		BIND_EKEYS(SpaceBar);
		BIND_EKEYS(PageUp);
		BIND_EKEYS(PageDown);
		BIND_EKEYS(End);
		BIND_EKEYS(Home);

		BIND_EKEYS(Left);
		BIND_EKEYS(Up);
		BIND_EKEYS(Right);
		BIND_EKEYS(Down);

		BIND_EKEYS(Insert);
		BIND_EKEYS(Delete);

		BIND_EKEYS(Zero);
		BIND_EKEYS(One);
		BIND_EKEYS(Two);
		BIND_EKEYS(Three);
		BIND_EKEYS(Four);
		BIND_EKEYS(Five);
		BIND_EKEYS(Six);
		BIND_EKEYS(Seven);
		BIND_EKEYS(Eight);
		BIND_EKEYS(Nine);

		BIND_EKEYS(A);
		BIND_EKEYS(B);
		BIND_EKEYS(C);
		BIND_EKEYS(D);
		BIND_EKEYS(E);
		BIND_EKEYS(F);
		BIND_EKEYS(G);
		BIND_EKEYS(H);
		BIND_EKEYS(I);
		BIND_EKEYS(J);
		BIND_EKEYS(K);
		BIND_EKEYS(L);
		BIND_EKEYS(M);
		BIND_EKEYS(N);
		BIND_EKEYS(O);
		BIND_EKEYS(P);
		BIND_EKEYS(Q);
		BIND_EKEYS(R);
		BIND_EKEYS(S);
		BIND_EKEYS(T);
		BIND_EKEYS(U);
		BIND_EKEYS(V);
		BIND_EKEYS(W);
		BIND_EKEYS(X);
		BIND_EKEYS(Y);
		BIND_EKEYS(Z);

		BIND_EKEYS(NumPadZero);
		BIND_EKEYS(NumPadOne);
		BIND_EKEYS(NumPadTwo);
		BIND_EKEYS(NumPadThree);
		BIND_EKEYS(NumPadFour);
		BIND_EKEYS(NumPadFive);
		BIND_EKEYS(NumPadSix);
		BIND_EKEYS(NumPadSeven);
		BIND_EKEYS(NumPadEight);
		BIND_EKEYS(NumPadNine);

		BIND_EKEYS(Multiply);
		BIND_EKEYS(Add);
		BIND_EKEYS(Subtract);
		BIND_EKEYS(Decimal);
		BIND_EKEYS(Divide);

		BIND_EKEYS(F1);
		BIND_EKEYS(F2);
		BIND_EKEYS(F3);
		BIND_EKEYS(F4);
		BIND_EKEYS(F5);
		BIND_EKEYS(F6);
		BIND_EKEYS(F7);
		BIND_EKEYS(F8);
		BIND_EKEYS(F9);
		BIND_EKEYS(F10);
		BIND_EKEYS(F11);
		BIND_EKEYS(F12);

		BIND_EKEYS(NumLock);

		BIND_EKEYS(ScrollLock);

		BIND_EKEYS(LeftShift);
		BIND_EKEYS(RightShift);
		BIND_EKEYS(LeftControl);
		BIND_EKEYS(RightControl);
		BIND_EKEYS(LeftAlt);
		BIND_EKEYS(RightAlt);
		BIND_EKEYS(LeftCommand);
		BIND_EKEYS(RightCommand);

		BIND_EKEYS(Semicolon);
		BIND_EKEYS(Equals);
		BIND_EKEYS(Comma);
		BIND_EKEYS(Underscore);
		BIND_EKEYS(Hyphen);
		BIND_EKEYS(Period);
		BIND_EKEYS(Slash);
		BIND_EKEYS(Tilde);
		BIND_EKEYS(LeftBracket);
		BIND_EKEYS(Backslash);
		BIND_EKEYS(RightBracket);
		BIND_EKEYS(Apostrophe);

		BIND_EKEYS(Ampersand);
		BIND_EKEYS(Asterix);
		BIND_EKEYS(Caret);
		BIND_EKEYS(Colon);
		BIND_EKEYS(Dollar);
		BIND_EKEYS(Exclamation);
		BIND_EKEYS(LeftParantheses);
		BIND_EKEYS(RightParantheses);
		BIND_EKEYS(Quote);

		BIND_EKEYS(A_AccentGrave);
		BIND_EKEYS(E_AccentGrave);
		BIND_EKEYS(E_AccentAigu);
		BIND_EKEYS(C_Cedille);
		BIND_EKEYS(Section);

		// Platform Keys
		// These keys platform specific versions of keys that go by different names.
		// The delete key is a good example, on Windows Delete is the virtual key for Delete.
		// On Macs, the Delete key is the virtual key for BackSpace.
		BIND_EKEYS(Platform_Delete);

		// Gamepad Keys
		BIND_EKEYS(Gamepad_Left2D);
		BIND_EKEYS(Gamepad_LeftX);
		BIND_EKEYS(Gamepad_LeftY);
		BIND_EKEYS(Gamepad_Right2D);
		BIND_EKEYS(Gamepad_RightX);
		BIND_EKEYS(Gamepad_RightY);
		BIND_EKEYS(Gamepad_LeftTriggerAxis);
		BIND_EKEYS(Gamepad_RightTriggerAxis);

		BIND_EKEYS(Gamepad_LeftThumbstick);
		BIND_EKEYS(Gamepad_RightThumbstick);
		BIND_EKEYS(Gamepad_Special_Left);
		BIND_EKEYS(Gamepad_Special_Left_X);
		BIND_EKEYS(Gamepad_Special_Left_Y);
		BIND_EKEYS(Gamepad_Special_Right);
		BIND_EKEYS(Gamepad_FaceButton_Bottom);
		BIND_EKEYS(Gamepad_FaceButton_Right);
		BIND_EKEYS(Gamepad_FaceButton_Left);
		BIND_EKEYS(Gamepad_FaceButton_Top);
		BIND_EKEYS(Gamepad_LeftShoulder);
		BIND_EKEYS(Gamepad_RightShoulder);
		BIND_EKEYS(Gamepad_LeftTrigger);
		BIND_EKEYS(Gamepad_RightTrigger);
		BIND_EKEYS(Gamepad_DPad_Up);
		BIND_EKEYS(Gamepad_DPad_Down);
		BIND_EKEYS(Gamepad_DPad_Right);
		BIND_EKEYS(Gamepad_DPad_Left);

		// Virtual key codes used for input axis button press/release emulation
		BIND_EKEYS(Gamepad_LeftStick_Up);
		BIND_EKEYS(Gamepad_LeftStick_Down);
		BIND_EKEYS(Gamepad_LeftStick_Right);
		BIND_EKEYS(Gamepad_LeftStick_Left);

		BIND_EKEYS(Gamepad_RightStick_Up);
		BIND_EKEYS(Gamepad_RightStick_Down);
		BIND_EKEYS(Gamepad_RightStick_Right);
		BIND_EKEYS(Gamepad_RightStick_Left);

		BIND_EKEYS(Tilt);
		BIND_EKEYS(RotationRate);
		BIND_EKEYS(Gravity);
		BIND_EKEYS(Acceleration);

		// Gestures
		BIND_EKEYS(Gesture_Pinch);
		BIND_EKEYS(Gesture_Flick);
		BIND_EKEYS(Gesture_Rotate);

		// Steam Controller Specific
		BIND_EKEYS(Steam_Touch_0);
		BIND_EKEYS(Steam_Touch_1);
		BIND_EKEYS(Steam_Touch_2);
		BIND_EKEYS(Steam_Touch_3);
		BIND_EKEYS(Steam_Back_Left);
		BIND_EKEYS(Steam_Back_Right);

		// Xbox One global speech commands
		BIND_EKEYS(Global_Menu);
		BIND_EKEYS(Global_View);
		BIND_EKEYS(Global_Pause);
		BIND_EKEYS(Global_Play);
		BIND_EKEYS(Global_Back);

		// Android-specific
		BIND_EKEYS(Android_Back);
		BIND_EKEYS(Android_Volume_Up);
		BIND_EKEYS(Android_Volume_Down);
		BIND_EKEYS(Android_Menu);

		// Virtual buttons that use other buttons depending on the platform
		static const FKey VirtualAcceptKey = EKeys::Virtual_Gamepad_Accept.GetVirtualKey();
		static const FKey VirtualBackKey = EKeys::Virtual_Gamepad_Back.GetVirtualKey();
		FAngelscriptBinds::BindGlobalVariable("const FKey Virtual_Accept", &VirtualAcceptKey);
		FAngelscriptBinds::BindGlobalVariable("const FKey Virtual_Back", &VirtualBackKey);

		BIND_EKEYS(Invalid);

#undef BIND_EKEYS
	}
});
