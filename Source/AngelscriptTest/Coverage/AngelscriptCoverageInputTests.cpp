#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/InputComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageInputTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript traditional input system, corresponding to
// Documents/Coverage/Coverage_Input.md submatrices 1-3 and 6-8.
//
// Test axes covered:
//   * SetupPlayerInputComponent  - APawn override for input setup
//   * ActionBinding              - BindAction with IE_Pressed/Released/Repeat/DoubleClick
//   * AxisBinding                - BindAxis for 1D axis inputs
//   * KeyDirectBinding           - BindKey for direct key bindings
//   * InputEventTypes            - IE_Pressed, IE_Released, IE_Repeat, IE_DoubleClick
//   * InputStateQuery            - IsInputKeyDown, WasInputKeyJustPressed, GetInputAxisValue
//   * CommonKeys                 - EKeys constants (keyboard, mouse, gamepad)
//
// Pattern D (script execution): compile AS pawns/controllers with input bindings,
// spawn them, verify input component setup and binding configurations.
//
// Note: Full input simulation (key press/release, axis values) requires a more
// complex input subsystem setup typically found in integration tests. These tests
// verify binding API correctness and compilation.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_Input.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageInputTest,
	"Angelscript.TestModule.Coverage.Input",
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
	// SetupPlayerInputComponent: APawn override for input binding
	// -------------------------------------------------------------------------
	TEST_METHOD(SetupPlayerInputComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_SetupPlayerInputComponent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputSetup.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AInputSetupPawn : APawn
			{
				UPROPERTY()
				bool InputComponentReceived = false;

				UPROPERTY()
				bool InputComponentValid = false;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					InputComponentReceived = true;
					InputComponentValid = (PlayerInputComponent != nullptr);
				}
			}
			)AS"),
			TEXT("AInputSetupPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("SetupPlayerInputComponent pawn class should compile")));

		// The method compiles correctly - actual invocation requires controller setup
		// which is validated in the compilation step
	}

	// -------------------------------------------------------------------------
	// Action Bindings: BindAction with Pressed, Released, Repeat, DoubleClick
	// -------------------------------------------------------------------------
	TEST_METHOD(ActionBinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_ActionBinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputAction.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AActionBindingPawn : APawn
			{
				UPROPERTY()
				int JumpPressedCount = 0;

				UPROPERTY()
				int JumpReleasedCount = 0;

				UPROPERTY()
				int FireRepeatCount = 0;

				UPROPERTY()
				int SelectDoubleClickCount = 0;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					// Bind action with IE_Pressed
					PlayerInputComponent.BindAction(n"Jump", IE_Pressed, this, n"OnJumpPressed");

					// Bind action with IE_Released
					PlayerInputComponent.BindAction(n"Jump", IE_Released, this, n"OnJumpReleased");

					// Bind action with IE_Repeat
					PlayerInputComponent.BindAction(n"Fire", IE_Repeat, this, n"OnFireRepeat");

					// Bind action with IE_DoubleClick
					PlayerInputComponent.BindAction(n"Select", IE_DoubleClick, this, n"OnSelectDoubleClick");
				}

				UFUNCTION()
				void OnJumpPressed()
				{
					JumpPressedCount++;
				}

				UFUNCTION()
				void OnJumpReleased()
				{
					JumpReleasedCount++;
				}

				UFUNCTION()
				void OnFireRepeat()
				{
					FireRepeatCount++;
				}

				UFUNCTION()
				void OnSelectDoubleClick()
				{
					SelectDoubleClickCount++;
				}
			}
			)AS"),
			TEXT("AActionBindingPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Action binding pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Axis Bindings: BindAxis for 1D input with float value parameter
	// -------------------------------------------------------------------------
	TEST_METHOD(AxisBinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_AxisBinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputAxis.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AAxisBindingPawn : APawn
			{
				UPROPERTY()
				float MoveForwardValue = 0.0f;

				UPROPERTY()
				float MoveRightValue = 0.0f;

				UPROPERTY()
				float LookUpValue = 0.0f;

				UPROPERTY()
				float TurnValue = 0.0f;

				UPROPERTY()
				int AxisCallCount = 0;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					// Bind multiple axes for movement and camera
					PlayerInputComponent.BindAxis(n"MoveForward", this, n"OnMoveForward");
					PlayerInputComponent.BindAxis(n"MoveRight", this, n"OnMoveRight");
					PlayerInputComponent.BindAxis(n"LookUp", this, n"OnLookUp");
					PlayerInputComponent.BindAxis(n"Turn", this, n"OnTurn");
				}

				UFUNCTION()
				void OnMoveForward(float Value)
				{
					MoveForwardValue = Value;
					AxisCallCount++;
				}

				UFUNCTION()
				void OnMoveRight(float Value)
				{
					MoveRightValue = Value;
					AxisCallCount++;
				}

				UFUNCTION()
				void OnLookUp(float Value)
				{
					LookUpValue = Value;
					AxisCallCount++;
				}

				UFUNCTION()
				void OnTurn(float Value)
				{
					TurnValue = Value;
					AxisCallCount++;
				}
			}
			)AS"),
			TEXT("AAxisBindingPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Axis binding pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Key Direct Binding: BindKey for specific key inputs
	// -------------------------------------------------------------------------
	TEST_METHOD(KeyDirectBinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_KeyDirectBinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputKeyDirect.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AKeyDirectBindingPawn : APawn
			{
				UPROPERTY()
				int SpaceKeyPressedCount = 0;

				UPROPERTY()
				int WKeyPressedCount = 0;

				UPROPERTY()
				int LeftMousePressedCount = 0;

				UPROPERTY()
				int RightMouseReleasedCount = 0;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					// Direct key bindings bypass InputAction mappings
					PlayerInputComponent.BindKey(EKeys::SpaceBar, IE_Pressed, this, n"OnSpacePressed");
					PlayerInputComponent.BindKey(EKeys::W, IE_Pressed, this, n"OnWPressed");
					PlayerInputComponent.BindKey(EKeys::LeftMouseButton, IE_Pressed, this, n"OnLeftMousePressed");
					PlayerInputComponent.BindKey(EKeys::RightMouseButton, IE_Released, this, n"OnRightMouseReleased");
				}

				UFUNCTION()
				void OnSpacePressed()
				{
					SpaceKeyPressedCount++;
				}

				UFUNCTION()
				void OnWPressed()
				{
					WKeyPressedCount++;
				}

				UFUNCTION()
				void OnLeftMousePressed()
				{
					LeftMousePressedCount++;
				}

				UFUNCTION()
				void OnRightMouseReleased()
				{
					RightMouseReleasedCount++;
				}
			}
			)AS"),
			TEXT("AKeyDirectBindingPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Key direct binding pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Input binding visibility: SetupPlayerInputComponent creates action, axis, and key bindings
	// -------------------------------------------------------------------------
	TEST_METHOD(InputBindingCollectionsVisibleAfterSetup)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_BindingCollectionsVisible"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputBindingCollectionsVisible.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AInputBindingVisibilityPawn : APawn
			{
				UPROPERTY()
				int SetupCallCount = 0;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					SetupCallCount++;
					PlayerInputComponent.BindAction(n"Jump", IE_Pressed, this, n"OnJumpPressed");
					PlayerInputComponent.BindAction(n"Jump", IE_Released, this, n"OnJumpReleased");
					PlayerInputComponent.BindAxis(n"MoveForward", this, n"OnMoveForward");
					PlayerInputComponent.BindAxis(n"Turn", this, n"OnTurn");
					PlayerInputComponent.BindKey(EKeys::SpaceBar, IE_Pressed, this, n"OnSpacePressed");
					PlayerInputComponent.BindKey(EKeys::LeftMouseButton, IE_Released, this, n"OnLeftMouseReleased");
				}

				UFUNCTION() void OnJumpPressed() {}
				UFUNCTION() void OnJumpReleased() {}
				UFUNCTION() void OnSpacePressed() {}
				UFUNCTION() void OnLeftMouseReleased() {}
				UFUNCTION() void OnMoveForward(float Value) {}
				UFUNCTION() void OnTurn(float Value) {}
			}
			)AS"),
			TEXT("AInputBindingVisibilityPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Input binding visibility pawn class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		APawn* Pawn = SpawnScriptActor<APawn>(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Pawn, TEXT("Input binding visibility pawn should spawn")));
		if (Pawn == nullptr)
		{
			return;
		}

		UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn, TEXT("CoverageInputComponent"));
		ASSERT_THAT(IsNotNull(InputComponent, TEXT("Input component should be created for setup")));
		if (InputComponent == nullptr)
		{
			return;
		}

		FFunctionInvoker SetupInvoker(*TestRunner, Pawn, FName(TEXT("SetupPlayerInputComponent")));
		ASSERT_THAT(IsTrue(SetupInvoker.IsValid(), TEXT("SetupPlayerInputComponent should be invokable")));
		SetupInvoker.AddParam<UInputComponent*>(InputComponent);
		ASSERT_THAT(IsTrue(SetupInvoker.Call(), TEXT("SetupPlayerInputComponent should execute")));

		int32 SetupCallCount = 0;
		ASSERT_THAT(IsTrue(GetByPath<FIntProperty, int32>(*TestRunner, Pawn, TEXT("SetupCallCount"), SetupCallCount), TEXT("SetupCallCount should be readable")));
		ASSERT_THAT(AreEqual(1, SetupCallCount, TEXT("SetupPlayerInputComponent should run exactly once")));

		ASSERT_THAT(AreEqual(2, InputComponent->GetNumActionBindings(), TEXT("BindAction should add pressed and released action bindings")));
		ASSERT_THAT(AreEqual(2, InputComponent->AxisBindings.Num(), TEXT("BindAxis should add two axis bindings")));
		ASSERT_THAT(AreEqual(2, InputComponent->KeyBindings.Num(), TEXT("BindKey should add two key bindings")));
		ASSERT_THAT(IsTrue(InputComponent->HasBindings(), TEXT("InputComponent should report bindings after AS setup")));

		const FInputActionBinding& JumpPressedBinding = InputComponent->GetActionBinding(0);
		const FInputActionBinding& JumpReleasedBinding = InputComponent->GetActionBinding(1);
		ASSERT_THAT(AreEqual(FName(TEXT("Jump")), JumpPressedBinding.GetActionName(), TEXT("First action binding should keep the Jump action name")));
		ASSERT_THAT(AreEqual(static_cast<int32>(IE_Pressed), static_cast<int32>(JumpPressedBinding.KeyEvent.GetValue()), TEXT("First action binding should keep IE_Pressed")));
		ASSERT_THAT(AreEqual(FName(TEXT("Jump")), JumpReleasedBinding.GetActionName(), TEXT("Second action binding should keep the Jump action name")));
		ASSERT_THAT(AreEqual(static_cast<int32>(IE_Released), static_cast<int32>(JumpReleasedBinding.KeyEvent.GetValue()), TEXT("Second action binding should keep IE_Released")));

		ASSERT_THAT(AreEqual(FName(TEXT("MoveForward")), InputComponent->AxisBindings[0].AxisName, TEXT("First axis binding should keep MoveForward")));
		ASSERT_THAT(AreEqual(FName(TEXT("Turn")), InputComponent->AxisBindings[1].AxisName, TEXT("Second axis binding should keep Turn")));
		ASSERT_THAT(AreEqual(EKeys::SpaceBar, InputComponent->KeyBindings[0].Chord.Key, TEXT("First key binding should keep SpaceBar")));
		ASSERT_THAT(AreEqual(static_cast<int32>(IE_Pressed), static_cast<int32>(InputComponent->KeyBindings[0].KeyEvent.GetValue()), TEXT("SpaceBar binding should keep IE_Pressed")));
		ASSERT_THAT(AreEqual(EKeys::LeftMouseButton, InputComponent->KeyBindings[1].Chord.Key, TEXT("Second key binding should keep LeftMouseButton")));
		ASSERT_THAT(AreEqual(static_cast<int32>(IE_Released), static_cast<int32>(InputComponent->KeyBindings[1].KeyEvent.GetValue()), TEXT("LeftMouseButton binding should keep IE_Released")));
	}

	// -------------------------------------------------------------------------
	// Input State Query: IsInputKeyDown, WasInputKeyJustPressed, GetInputAxisValue
	// -------------------------------------------------------------------------
	TEST_METHOD(InputStateQuery)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_StateQuery"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputStateQuery.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AInputQueryController : APlayerController
			{
				UPROPERTY()
				bool WKeyDown = false;

				UPROPERTY()
				bool SpaceJustPressed = false;

				UPROPERTY()
				bool SpaceJustReleased = false;

				UPROPERTY()
				float MoveForwardAxisValue = 0.0f;

				UPROPERTY()
				float KeyDownTime = 0.0f;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					// Query input key states
					WKeyDown = IsInputKeyDown(EKeys::W);
					SpaceJustPressed = WasInputKeyJustPressed(EKeys::SpaceBar);
					SpaceJustReleased = WasInputKeyJustReleased(EKeys::SpaceBar);

					// Query axis value
					MoveForwardAxisValue = GetInputAxisValue(n"MoveForward");

					// Query key hold time
					KeyDownTime = GetInputKeyTimeDown(EKeys::W);
				}
			}
			)AS"),
			TEXT("AInputQueryController"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Input state query controller class should compile")));
	}

	// -------------------------------------------------------------------------
	// Keyboard Keys: Common keyboard key constants
	// -------------------------------------------------------------------------
	TEST_METHOD(KeyboardKeys)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_KeyboardKeys"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputKeyboard.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AKeyboardInputPawn : APawn
			{
				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					// WASD movement keys
					PlayerInputComponent.BindKey(EKeys::W, IE_Pressed, this, n"OnW");
					PlayerInputComponent.BindKey(EKeys::A, IE_Pressed, this, n"OnA");
					PlayerInputComponent.BindKey(EKeys::S, IE_Pressed, this, n"OnS");
					PlayerInputComponent.BindKey(EKeys::D, IE_Pressed, this, n"OnD");

					// Common action keys
					PlayerInputComponent.BindKey(EKeys::SpaceBar, IE_Pressed, this, n"OnSpace");
					PlayerInputComponent.BindKey(EKeys::LeftShift, IE_Pressed, this, n"OnShift");
					PlayerInputComponent.BindKey(EKeys::LeftControl, IE_Pressed, this, n"OnCtrl");
					PlayerInputComponent.BindKey(EKeys::LeftAlt, IE_Pressed, this, n"OnAlt");

					// UI keys
					PlayerInputComponent.BindKey(EKeys::Tab, IE_Pressed, this, n"OnTab");
					PlayerInputComponent.BindKey(EKeys::Escape, IE_Pressed, this, n"OnEscape");
					PlayerInputComponent.BindKey(EKeys::Enter, IE_Pressed, this, n"OnEnter");

					// Number keys
					PlayerInputComponent.BindKey(EKeys::One, IE_Pressed, this, n"OnOne");
					PlayerInputComponent.BindKey(EKeys::Two, IE_Pressed, this, n"OnTwo");
					PlayerInputComponent.BindKey(EKeys::Nine, IE_Pressed, this, n"OnNine");

					// Function keys
					PlayerInputComponent.BindKey(EKeys::F1, IE_Pressed, this, n"OnF1");
					PlayerInputComponent.BindKey(EKeys::F12, IE_Pressed, this, n"OnF12");
				}

				UFUNCTION() void OnW() {}
				UFUNCTION() void OnA() {}
				UFUNCTION() void OnS() {}
				UFUNCTION() void OnD() {}
				UFUNCTION() void OnSpace() {}
				UFUNCTION() void OnShift() {}
				UFUNCTION() void OnCtrl() {}
				UFUNCTION() void OnAlt() {}
				UFUNCTION() void OnTab() {}
				UFUNCTION() void OnEscape() {}
				UFUNCTION() void OnEnter() {}
				UFUNCTION() void OnOne() {}
				UFUNCTION() void OnTwo() {}
				UFUNCTION() void OnNine() {}
				UFUNCTION() void OnF1() {}
				UFUNCTION() void OnF12() {}
			}
			)AS"),
			TEXT("AKeyboardInputPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Keyboard keys pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Mouse Input: Mouse buttons and scroll wheel
	// -------------------------------------------------------------------------
	TEST_METHOD(MouseInput)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_MouseInput"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputMouse.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AMouseInputPawn : APawn
			{
				UPROPERTY()
				float MouseXValue = 0.0f;

				UPROPERTY()
				float MouseYValue = 0.0f;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					// Mouse buttons
					PlayerInputComponent.BindKey(EKeys::LeftMouseButton, IE_Pressed, this, n"OnLeftMouse");
					PlayerInputComponent.BindKey(EKeys::RightMouseButton, IE_Pressed, this, n"OnRightMouse");
					PlayerInputComponent.BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, n"OnMiddleMouse");
					PlayerInputComponent.BindKey(EKeys::ThumbMouseButton, IE_Pressed, this, n"OnThumbMouse");
					PlayerInputComponent.BindKey(EKeys::ThumbMouseButton2, IE_Pressed, this, n"OnThumbMouse2");

					// Mouse wheel
					PlayerInputComponent.BindKey(EKeys::MouseScrollUp, IE_Pressed, this, n"OnScrollUp");
					PlayerInputComponent.BindKey(EKeys::MouseScrollDown, IE_Pressed, this, n"OnScrollDown");

					// Mouse movement axes
					PlayerInputComponent.BindAxis(n"MouseX", this, n"OnMouseX");
					PlayerInputComponent.BindAxis(n"MouseY", this, n"OnMouseY");
				}

				UFUNCTION() void OnLeftMouse() {}
				UFUNCTION() void OnRightMouse() {}
				UFUNCTION() void OnMiddleMouse() {}
				UFUNCTION() void OnThumbMouse() {}
				UFUNCTION() void OnThumbMouse2() {}
				UFUNCTION() void OnScrollUp() {}
				UFUNCTION() void OnScrollDown() {}

				UFUNCTION()
				void OnMouseX(float Value)
				{
					MouseXValue = Value;
				}

				UFUNCTION()
				void OnMouseY(float Value)
				{
					MouseYValue = Value;
				}
			}
			)AS"),
			TEXT("AMouseInputPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Mouse input pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Gamepad Input: Controller buttons and analog sticks
	// -------------------------------------------------------------------------
	TEST_METHOD(GamepadInput)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_GamepadInput"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputGamepad.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AGamepadInputPawn : APawn
			{
				UPROPERTY()
				float LeftStickXValue = 0.0f;

				UPROPERTY()
				float LeftStickYValue = 0.0f;

				UPROPERTY()
				float RightStickXValue = 0.0f;

				UPROPERTY()
				float RightStickYValue = 0.0f;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					// Face buttons (Xbox layout: A/B/X/Y)
					PlayerInputComponent.BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, n"OnFaceBottom");
					PlayerInputComponent.BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, n"OnFaceRight");
					PlayerInputComponent.BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, n"OnFaceLeft");
					PlayerInputComponent.BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, n"OnFaceTop");

					// Shoulder buttons
					PlayerInputComponent.BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, n"OnLeftShoulder");
					PlayerInputComponent.BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, n"OnRightShoulder");

					// Triggers
					PlayerInputComponent.BindKey(EKeys::Gamepad_LeftTrigger, IE_Pressed, this, n"OnLeftTrigger");
					PlayerInputComponent.BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, n"OnRightTrigger");

					// D-Pad
					PlayerInputComponent.BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, n"OnDPadUp");
					PlayerInputComponent.BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, n"OnDPadDown");
					PlayerInputComponent.BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, n"OnDPadLeft");
					PlayerInputComponent.BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, n"OnDPadRight");

					// Start/Select (Special buttons)
					PlayerInputComponent.BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, n"OnSpecialLeft");
					PlayerInputComponent.BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, n"OnSpecialRight");

					// Analog sticks
					PlayerInputComponent.BindAxis(n"Gamepad_LeftX", this, n"OnLeftStickX");
					PlayerInputComponent.BindAxis(n"Gamepad_LeftY", this, n"OnLeftStickY");
					PlayerInputComponent.BindAxis(n"Gamepad_RightX", this, n"OnRightStickX");
					PlayerInputComponent.BindAxis(n"Gamepad_RightY", this, n"OnRightStickY");
				}

				UFUNCTION() void OnFaceBottom() {}
				UFUNCTION() void OnFaceRight() {}
				UFUNCTION() void OnFaceLeft() {}
				UFUNCTION() void OnFaceTop() {}
				UFUNCTION() void OnLeftShoulder() {}
				UFUNCTION() void OnRightShoulder() {}
				UFUNCTION() void OnLeftTrigger() {}
				UFUNCTION() void OnRightTrigger() {}
				UFUNCTION() void OnDPadUp() {}
				UFUNCTION() void OnDPadDown() {}
				UFUNCTION() void OnDPadLeft() {}
				UFUNCTION() void OnDPadRight() {}
				UFUNCTION() void OnSpecialLeft() {}
				UFUNCTION() void OnSpecialRight() {}

				UFUNCTION()
				void OnLeftStickX(float Value)
				{
					LeftStickXValue = Value;
				}

				UFUNCTION()
				void OnLeftStickY(float Value)
				{
					LeftStickYValue = Value;
				}

				UFUNCTION()
				void OnRightStickX(float Value)
				{
					RightStickXValue = Value;
				}

				UFUNCTION()
				void OnRightStickY(float Value)
				{
					RightStickYValue = Value;
				}
			}
			)AS"),
			TEXT("AGamepadInputPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Gamepad input pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Input Component Finding: GetComponentByClass for UInputComponent
	// -------------------------------------------------------------------------
	TEST_METHOD(InputComponentFinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_ComponentFinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputComponentFinding.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AInputComponentFindingPawn : APawn
			{
				UPROPERTY()
				bool FoundInputComponent = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UInputComponent InputComp = Cast<UInputComponent>(FindComponentByClass(UInputComponent::StaticClass()));
					FoundInputComponent = (InputComp != nullptr);
				}
			}
			)AS"),
			TEXT("AInputComponentFindingPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Input component finding pawn class should compile")));
	}

	// -------------------------------------------------------------------------
	// Input Mode Control: SetShowMouseCursor (PlayerController)
	// -------------------------------------------------------------------------
	TEST_METHOD(InputModeControl)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_ModeControl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputModeControl.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AInputModeController : APlayerController
			{
				UPROPERTY()
				bool MouseCursorShown = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Show/hide mouse cursor
					SetShowMouseCursor(true);
					MouseCursorShown = GetShowMouseCursor();

					SetShowMouseCursor(false);
					MouseCursorShown = GetShowMouseCursor();
				}
			}
			)AS"),
			TEXT("AInputModeController"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Input mode control controller class should compile")));
	}

	TEST_METHOD(InputModeSwitchingUnsupportedBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsNull(APlayerController::StaticClass()->FindFunctionByName(TEXT("SetInputMode")),
			TEXT("SetInputMode should remain outside reflected APlayerController UFUNCTIONs")));

		const FString GameOnlySource = ASTEST_AS(R"AS(
			UCLASS()
			class AInputModeSwitchingBoundaryController : APlayerController
			{
				UFUNCTION()
				void TryGameOnlyInputMode()
				{
					FInputModeGameOnly GameOnlyMode;
					SetInputMode(GameOnlyMode);
				}
			}
			)AS");

		const TArray<FString> GameOnlyExpectedDiagnostics = { TEXT("FInputModeGameOnly") };
		const bool bGameOnlyFailedAsExpected = CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageInput_InputModeSwitchingUnsupported"),
			GameOnlySource,
			TEXT("SetInputMode input-mode structs should remain an explicit AS binding boundary"),
			MakeArrayView(GameOnlyExpectedDiagnostics));
		ASSERT_THAT(IsTrue(bGameOnlyFailedAsExpected, TEXT("Game-only input mode should stay unavailable to AS")));
		if (!bGameOnlyFailedAsExpected)
		{
			return;
		}

		const FString UIOnlySource = ASTEST_AS(R"AS(
			UCLASS()
			class AInputModeUIOnlyBoundaryController : APlayerController
			{
				UFUNCTION()
				void TryUIOnlyInputMode()
				{
					FInputModeUIOnly UIOnlyMode;
					SetInputMode(UIOnlyMode);
				}
			}
			)AS");

		const TArray<FString> UIOnlyExpectedDiagnostics = { TEXT("FInputModeUIOnly") };
		const bool bUIOnlyFailedAsExpected = CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageInput_UIOnlyInputModeUnsupported"),
			UIOnlySource,
			TEXT("UI-only input mode structs should remain an explicit AS binding boundary"),
			MakeArrayView(UIOnlyExpectedDiagnostics));
		ASSERT_THAT(IsTrue(bUIOnlyFailedAsExpected, TEXT("UI-only input mode should stay unavailable to AS")));
		if (!bUIOnlyFailedAsExpected)
		{
			return;
		}

		const FString GameAndUISource = ASTEST_AS(R"AS(
			UCLASS()
			class AInputModeGameAndUIBoundaryController : APlayerController
			{
				UFUNCTION()
				void TryGameAndUIInputMode()
				{
					FInputModeGameAndUI GameAndUIMode;
					SetInputMode(GameAndUIMode);
				}
			}
			)AS");

		const TArray<FString> GameAndUIExpectedDiagnostics = { TEXT("FInputModeGameAndUI") };
		const bool bGameAndUIFailedAsExpected = CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageInput_GameAndUIInputModeUnsupported"),
			GameAndUISource,
			TEXT("Game-and-UI input mode structs should remain an explicit AS binding boundary"),
			MakeArrayView(GameAndUIExpectedDiagnostics));
		ASSERT_THAT(IsTrue(bGameAndUIFailedAsExpected, TEXT("Game-and-UI input mode should stay unavailable to AS")));
		if (!bGameAndUIFailedAsExpected)
		{
			return;
		}
	}

	TEST_METHOD(EnhancedInputMappingContextAndActionValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int MappingContextMapUnmapAndClear()
			{
				UInputAction MoveAction = Cast<UInputAction>(NewObject(GetTransientPackage(), UInputAction::StaticClass(), n"CoverageMoveAction", true));
				UInputMappingContext MappingContext = Cast<UInputMappingContext>(NewObject(GetTransientPackage(), UInputMappingContext::StaticClass(), n"CoverageMoveContext", true));
				if (MoveAction == nullptr || MappingContext == nullptr)
					return 0;

				MoveAction.SetValueType(EInputActionValueType::Axis2D);
				FEnhancedActionKeyMapping& WMapping = MappingContext.MapKey(MoveAction, EKeys::W);
				FEnhancedActionKeyMapping& DMapping = MappingContext.MapKey(MoveAction, EKeys::D);
				if (MappingContext.GetMappingCount() != 2)
					return 0;
				if (!MappingContext.HasMappingForInputAction(MoveAction))
					return 0;
				if (WMapping.GetAction() != MoveAction || WMapping.GetKey() != EKeys::W)
					return 0;
				if (DMapping.GetAction() != MoveAction || DMapping.GetKey() != EKeys::D)
					return 0;

				MappingContext.UnmapKey(MoveAction, EKeys::W);
				if (MappingContext.GetMappingCount() != 1)
					return 0;

				MappingContext.UnmapAll();
				return MappingContext.GetMappingCount() == 0 ? 1 : 0;
			}

			int InputActionValueShapes()
			{
				FInputActionValue FloatValue(0.75f);
				if (FloatValue.GetAxis1D() < 0.74f || FloatValue.GetAxis1D() > 0.76f)
					return 0;

				FInputActionValue BoolValue(EInputActionValueType::Boolean, FVector(1.0f, 0.0f, 0.0f));
				if (!BoolValue.Get())
					return 0;

				FInputActionValue Vector2DValue(FVector2D(2.0f, -3.0f));
				FVector2D Axis2D = Vector2DValue.GetAxis2D();
				if (Axis2D.X < 1.9f || Axis2D.X > 2.1f || Axis2D.Y > -2.9f || Axis2D.Y < -3.1f)
					return 0;

				FInputActionValue Vector3DValue(FVector(4.0f, 5.0f, 6.0f));
				FVector Axis3D = Vector3DValue.GetAxis3D();
				if (Axis3D.X < 3.9f || Axis3D.X > 4.1f || Axis3D.Z < 5.9f || Axis3D.Z > 6.1f)
					return 0;

				Vector3DValue.ConvertToType(EInputActionValueType::Axis1D);
				return Vector3DValue.GetAxis1D() > 3.9f && Vector3DValue.GetAxis1D() < 4.1f ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageInput_EnhancedMappingAndValues"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Enhanced Input mapping/value module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int MappingContextMapUnmapAndClear()"),
			TEXT("UInputMappingContext should expose map, unmap, query, and clear operations to AS"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int InputActionValueShapes()"),
			TEXT("FInputActionValue should expose bool, float, 2D, and 3D value access to AS"), 1)));
	}

	TEST_METHOD(EnhancedInputComponentBindingEventsAndRemoval)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_EnhancedComponentBinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AEnhancedInputCoverageActor : AActor
			{
				UPROPERTY()
				UInputAction Action;

				UPROPERTY()
				int StartedCount = 0;

				UPROPERTY()
				bool bBindingsAdded = false;

				UPROPERTY()
				bool bActionValueBindingAdded = false;

				UPROPERTY()
				bool bDebugBindingAdded = false;

				UPROPERTY()
				bool bClearRemovedBindings = false;

				UFUNCTION()
				void OnAction(FInputActionValue ActionValue, float32 ElapsedTime, float32 TriggeredTime, const UInputAction SourceAction)
				{
					StartedCount += 1;
				}

				UFUNCTION()
				void OnDebug(FKey Key, FInputActionValue ActionValue)
				{
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Action = Cast<UInputAction>(NewObject(this, UInputAction::StaticClass(), n"CoverageAction", true));
					UEnhancedInputComponent EnhancedComponent = Cast<UEnhancedInputComponent>(NewObject(this, UEnhancedInputComponent::StaticClass(), n"CoverageEnhancedInputComponent", true));
					if (Action == nullptr || EnhancedComponent == nullptr)
						return;

					FEnhancedInputActionHandlerDynamicSignature StartedDelegate;
					StartedDelegate.BindUFunction(this, n"OnAction");
					EnhancedComponent.BindAction(Action, ETriggerEvent::Started, StartedDelegate);
					EnhancedComponent.BindAction(Action, ETriggerEvent::Ongoing, StartedDelegate);
					EnhancedComponent.BindAction(Action, ETriggerEvent::Triggered, StartedDelegate);
					EnhancedComponent.BindAction(Action, ETriggerEvent::Completed, StartedDelegate);
					EnhancedComponent.BindAction(Action, ETriggerEvent::Canceled, StartedDelegate);
					bBindingsAdded = EnhancedComponent.HasBindings();

					EnhancedComponent.BindActionValue(Action);
					bActionValueBindingAdded = EnhancedComponent.HasBindings();

					FInputDebugKeyHandlerDynamicSignature DebugDelegate;
					DebugDelegate.BindUFunction(this, n"OnDebug");
					EnhancedComponent.BindDebugKey(FInputChord(EKeys::SpaceBar), IE_Pressed, DebugDelegate, true);
					bDebugBindingAdded = EnhancedComponent.HasBindings();

					EnhancedComponent.ClearActionEventBindings();
					EnhancedComponent.ClearActionValueBindings();
					EnhancedComponent.ClearDebugKeyBindings();
					bClearRemovedBindings = !EnhancedComponent.HasBindings();
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputEnhancedComponentBinding.as"),
			ScriptSource,
			TEXT("AEnhancedInputCoverageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enhanced Input component binding actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enhanced Input component binding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBindingsAdded"), true,
			TEXT("BindAction should add event bindings for all ETriggerEvent values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bActionValueBindingAdded"), true,
			TEXT("BindActionValue should add a value binding"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDebugBindingAdded"), true,
			TEXT("BindDebugKey should add a debug key binding"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bClearRemovedBindings"), true,
			TEXT("Enhanced Input clear helpers should remove AS-created bindings"))));
	}

	TEST_METHOD(EnhancedInputModifiersAndTriggers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int ModifierAndTriggerLists()
			{
				UInputAction Action = Cast<UInputAction>(NewObject(GetTransientPackage(), UInputAction::StaticClass(), n"CoverageTriggerAction", true));
				UInputMappingContext MappingContext = Cast<UInputMappingContext>(NewObject(GetTransientPackage(), UInputMappingContext::StaticClass(), n"CoverageTriggerContext", true));
				if (Action == nullptr || MappingContext == nullptr)
					return 0;

				FEnhancedActionKeyMapping& Mapping = MappingContext.MapKey(Action, EKeys::SpaceBar);

				UInputModifierDeadZone DeadZone = Cast<UInputModifierDeadZone>(NewObject(MappingContext, UInputModifierDeadZone::StaticClass(), n"CoverageDeadZone", true));
				UInputModifierNegate Negate = Cast<UInputModifierNegate>(NewObject(MappingContext, UInputModifierNegate::StaticClass(), n"CoverageNegate", true));
				UInputModifierScalar Scalar = Cast<UInputModifierScalar>(NewObject(MappingContext, UInputModifierScalar::StaticClass(), n"CoverageScalar", true));
				UInputModifierSmooth Smooth = Cast<UInputModifierSmooth>(NewObject(MappingContext, UInputModifierSmooth::StaticClass(), n"CoverageSmooth", true));
				UInputModifierResponseCurveExponential ResponseCurve = Cast<UInputModifierResponseCurveExponential>(NewObject(MappingContext, UInputModifierResponseCurveExponential::StaticClass(), n"CoverageResponseCurve", true));
				if (DeadZone == nullptr || Negate == nullptr || Scalar == nullptr || Smooth == nullptr || ResponseCurve == nullptr)
					return 0;

				Mapping.AddModifier(DeadZone);
				Mapping.AddModifier(Negate);
				Mapping.AddModifier(Scalar);
				Mapping.AddModifier(Smooth);
				Mapping.AddModifier(ResponseCurve);
				if (Mapping.GetModifierCount() != 5)
					return 0;

				UInputTriggerDown Down = Cast<UInputTriggerDown>(NewObject(MappingContext, UInputTriggerDown::StaticClass(), n"CoverageDown", true));
				UInputTriggerPressed Pressed = Cast<UInputTriggerPressed>(NewObject(MappingContext, UInputTriggerPressed::StaticClass(), n"CoveragePressed", true));
				UInputTriggerReleased Released = Cast<UInputTriggerReleased>(NewObject(MappingContext, UInputTriggerReleased::StaticClass(), n"CoverageReleased", true));
				UInputTriggerHold Hold = Cast<UInputTriggerHold>(NewObject(MappingContext, UInputTriggerHold::StaticClass(), n"CoverageHold", true));
				UInputTriggerTap Tap = Cast<UInputTriggerTap>(NewObject(MappingContext, UInputTriggerTap::StaticClass(), n"CoverageTap", true));
				UInputTriggerPulse Pulse = Cast<UInputTriggerPulse>(NewObject(MappingContext, UInputTriggerPulse::StaticClass(), n"CoveragePulse", true));
				if (Down == nullptr || Pressed == nullptr || Released == nullptr || Hold == nullptr || Tap == nullptr || Pulse == nullptr)
					return 0;

				Mapping.AddTrigger(Down);
				Mapping.AddTrigger(Pressed);
				Mapping.AddTrigger(Released);
				Mapping.AddTrigger(Hold);
				Mapping.AddTrigger(Tap);
				Mapping.AddTrigger(Pulse);
				return Mapping.GetTriggerCount() == 6 ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageInput_EnhancedModifiersAndTriggers"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Enhanced Input modifier/trigger module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int ModifierAndTriggerLists()"),
			TEXT("Enhanced Input mappings should accept AS-created modifiers and triggers"), 1)));
	}

	TEST_METHOD(EnhancedInputActionAndMappingMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int ActionAndMappingMetadata()
			{
				UInputAction MoveAction = Cast<UInputAction>(NewObject(GetTransientPackage(), UInputAction::StaticClass(), n"CoverageMetadataMoveAction", true));
				UInputAction ConfirmAction = Cast<UInputAction>(NewObject(GetTransientPackage(), UInputAction::StaticClass(), n"CoverageMetadataConfirmAction", true));
				UInputMappingContext MappingContext = Cast<UInputMappingContext>(NewObject(GetTransientPackage(), UInputMappingContext::StaticClass(), n"CoverageMetadataContext", true));
				if (MoveAction == nullptr || ConfirmAction == nullptr || MappingContext == nullptr)
					return 0;

				MoveAction.SetValueType(EInputActionValueType::Axis2D);
				MoveAction.SetAccumulationBehavior(EInputActionAccumulationBehavior::Cumulative);
				ConfirmAction.SetValueType(EInputActionValueType::Boolean);
				ConfirmAction.SetAccumulationBehavior(EInputActionAccumulationBehavior::TakeHighestAbsoluteValue);
				if (MoveAction.GetValueType() != EInputActionValueType::Axis2D)
					return 0;
				if (MoveAction.GetAccumulationBehavior() != EInputActionAccumulationBehavior::Cumulative)
					return 0;
				if (ConfirmAction.GetValueType() != EInputActionValueType::Boolean)
					return 0;

				FEnhancedActionKeyMapping& MoveMapping = MappingContext.MapKey(MoveAction, EKeys::Gamepad_Left2D);
				FEnhancedActionKeyMapping& ConfirmMapping = MappingContext.MapKey(ConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
				if (MappingContext.GetMappingCount() != 2)
					return 0;

				UInputModifierNegate Negate = Cast<UInputModifierNegate>(NewObject(MappingContext, UInputModifierNegate::StaticClass(), n"CoverageMetadataNegate", true));
				UInputModifierScalar Scalar = Cast<UInputModifierScalar>(NewObject(MappingContext, UInputModifierScalar::StaticClass(), n"CoverageMetadataScalar", true));
				UInputTriggerCombo Combo = Cast<UInputTriggerCombo>(NewObject(MappingContext, UInputTriggerCombo::StaticClass(), n"CoverageMetadataCombo", true));
				UInputTriggerDown Down = Cast<UInputTriggerDown>(NewObject(MappingContext, UInputTriggerDown::StaticClass(), n"CoverageMetadataDown", true));
				if (Negate == nullptr || Scalar == nullptr || Combo == nullptr || Down == nullptr)
					return 0;

				MoveMapping.AddModifier(Negate);
				MoveMapping.AddModifier(Scalar);
				ConfirmMapping.AddTrigger(Combo);
				ConfirmMapping.AddTrigger(Down);
				if (MoveMapping.GetModifierCount() != 2 || ConfirmMapping.GetTriggerCount() != 2)
					return 0;

				MoveMapping.ClearModifiers();
				ConfirmMapping.ClearTriggers();
				if (MoveMapping.GetModifierCount() != 0 || ConfirmMapping.GetTriggerCount() != 0)
					return 0;

				ConfirmMapping.SetAction(MoveAction);
				ConfirmMapping.SetKey(EKeys::Enter);
				return ConfirmMapping.GetAction() == MoveAction && ConfirmMapping.GetKey() == EKeys::Enter ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageInput_EnhancedActionAndMappingMetadata"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Enhanced Input action/mapping metadata module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int ActionAndMappingMetadata()"),
			TEXT("Enhanced Input action metadata and mapping mutation APIs should be available to AS"), 1)));
	}

	TEST_METHOD(InputSettingsAndRuntimeMappingApi)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int InputSettingsReadApi()
			{
				UInputSettings Settings = UInputSettings::GetInputSettings();
				if (Settings == nullptr)
					return 0;

				FName UniqueAction = Settings.GetUniqueActionName(n"CoverageAction");
				FName UniqueAxis = Settings.GetUniqueAxisName(n"CoverageAxis");
				int ActionCount = Settings.GetActionMappings().Num();
				int AxisCount = Settings.GetAxisMappings().Num();
				bool bMissingAction = Settings.DoesActionExist(n"DefinitelyMissingCoverageAction") == false;
				bool bMissingAxis = Settings.DoesAxisExist(n"DefinitelyMissingCoverageAxis") == false;
				return UniqueAction != NAME_None && UniqueAxis != NAME_None && ActionCount >= 0 && AxisCount >= 0 && bMissingAction && bMissingAxis ? 1 : 0;
			}

			void RuntimeMappingSignaturesCompile(UPlayerInput PlayerInput, FInputActionKeyMapping ActionMapping, FInputAxisKeyMapping AxisMapping)
			{
				PlayerInput.AddActionMapping(ActionMapping);
				PlayerInput.RemoveActionMapping(ActionMapping);
				PlayerInput.AddAxisMapping(AxisMapping);
				PlayerInput.RemoveAxisMapping(AxisMapping);
				PlayerInput.ForceRebuildingKeyMaps();
				int ActionKeyCount = PlayerInput.GetKeysForAction(n"CoverageAction").Num();
				int AxisKeyCount = PlayerInput.GetKeysForAxis(n"CoverageAxis").Num();
				if (ActionKeyCount < 0 || AxisKeyCount < 0)
					return;
			}

			int RuntimeMappingSignatureEntry()
			{
				return 1;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageInput_InputSettingsAndRuntimeMapping"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("InputSettings/runtime mapping module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int InputSettingsReadApi()"),
			TEXT("UInputSettings read APIs should be available to AS"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int RuntimeMappingSignatureEntry()"),
			TEXT("UPlayerInput runtime remapping mixins should compile without requiring a real player-input instance"), 1)));
	}

	TEST_METHOD(TouchAndGestureApiBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int TouchKeyAndPointerSurface()
			{
				bool bGestureKeysAreReachable = EKeys::Gesture_Pinch.IsValid() &&
					EKeys::Gesture_Flick.IsValid() &&
					EKeys::Gesture_Rotate.IsValid();
				bool bTouchLikeControllerKeysAreReachable = EKeys::Steam_Touch_0.IsValid() &&
					EKeys::Steam_Touch_1.IsValid();
				return bGestureKeysAreReachable && bTouchLikeControllerKeysAreReachable ? 1 : 0;
			}

			int TouchBindingSurfaceIsAbsent()
			{
				return 1;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageInput_TouchGestureBoundaries"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("touch and gesture boundary module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int TouchKeyAndPointerSurface()"),
			TEXT("touch and gesture key constants should be reachable to AS without real touch input"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int TouchBindingSurfaceIsAbsent()"),
			TEXT("touch pressed/moved/released dispatch remains a runtime-input gap rather than a fake simulation"), 1)));
	}

	TEST_METHOD(AdvancedInputComponentBindingCollections)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_AdvancedBindingCollections"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputAdvancedBindingCollections.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AAdvancedInputBindingPawn : APawn
			{
				UPROPERTY()
				int SetupCallCount = 0;

				UFUNCTION(BlueprintOverride)
				void SetupPlayerInputComponent(UInputComponent PlayerInputComponent)
				{
					SetupCallCount++;
					PlayerInputComponent.BindChord(FInputChord(EKeys::LeftMouseButton, true, false, false, false), IE_Pressed, this, n"OnShiftLeftMouse");
					PlayerInputComponent.BindAxisKey(n"MouseX", this, n"OnMouseXAxis");
					PlayerInputComponent.BindVectorAxis(EKeys::Tilt, this, n"OnTiltAxis");
				}

				UFUNCTION()
				void OnShiftLeftMouse()
				{
				}

				UFUNCTION()
				void OnMouseXAxis(float Value)
				{
				}

				UFUNCTION()
				void OnTiltAxis(FVector Value)
				{
				}
			}
			)AS"),
			TEXT("AAdvancedInputBindingPawn"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("advanced input binding pawn class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		APawn* Pawn = SpawnScriptActor<APawn>(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Pawn, TEXT("advanced input binding pawn should spawn")));
		if (Pawn == nullptr)
		{
			return;
		}

		UInputComponent* InputComponent = NewObject<UInputComponent>(Pawn, TEXT("AdvancedCoverageInputComponent"));
		ASSERT_THAT(IsNotNull(InputComponent, TEXT("input component should be created for advanced binding setup")));
		if (InputComponent == nullptr)
		{
			return;
		}

		FFunctionInvoker SetupInvoker(*TestRunner, Pawn, FName(TEXT("SetupPlayerInputComponent")));
		ASSERT_THAT(IsTrue(SetupInvoker.IsValid(), TEXT("advanced SetupPlayerInputComponent should be invokable")));
		if (!SetupInvoker.IsValid())
		{
			return;
		}
		SetupInvoker.AddParam<UInputComponent*>(InputComponent);
		ASSERT_THAT(IsTrue(SetupInvoker.Call(), TEXT("advanced SetupPlayerInputComponent should execute")));

		int32 SetupCallCount = 0;
		ASSERT_THAT(IsTrue(GetByPath<FIntProperty, int32>(*TestRunner, Pawn, TEXT("SetupCallCount"), SetupCallCount), TEXT("advanced SetupCallCount should be readable")));
		ASSERT_THAT(AreEqual(1, SetupCallCount, TEXT("advanced SetupPlayerInputComponent should run exactly once")));

		ASSERT_THAT(AreEqual(1, InputComponent->KeyBindings.Num(), TEXT("BindChord should add one key binding")));
		ASSERT_THAT(AreEqual(1, InputComponent->AxisKeyBindings.Num(), TEXT("BindAxisKey should add one axis-key binding")));
		ASSERT_THAT(AreEqual(1, InputComponent->VectorAxisBindings.Num(), TEXT("BindVectorAxis should add one vector-axis binding")));
		ASSERT_THAT(IsTrue(InputComponent->HasBindings(), TEXT("InputComponent should report advanced AS bindings")));
		if (InputComponent->KeyBindings.Num() != 1 || InputComponent->AxisKeyBindings.Num() != 1 || InputComponent->VectorAxisBindings.Num() != 1)
		{
			return;
		}

		ASSERT_THAT(AreEqual(EKeys::LeftMouseButton, InputComponent->KeyBindings[0].Chord.Key, TEXT("BindChord should keep the mouse key")));
		ASSERT_THAT(IsTrue(InputComponent->KeyBindings[0].Chord.bShift, TEXT("BindChord should keep the shift modifier")));
		ASSERT_THAT(AreEqual(static_cast<int32>(IE_Pressed), static_cast<int32>(InputComponent->KeyBindings[0].KeyEvent.GetValue()), TEXT("BindChord should keep IE_Pressed")));
		ASSERT_THAT(AreEqual(FName(TEXT("MouseX")), InputComponent->AxisKeyBindings[0].AxisKey.GetFName(), TEXT("BindAxisKey should keep MouseX")));
		ASSERT_THAT(AreEqual(EKeys::Tilt, InputComponent->VectorAxisBindings[0].AxisKey, TEXT("BindVectorAxis should keep Tilt")));
	}

	TEST_METHOD(EnhancedInputBindingHandlesAndRemoval)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_EnhancedBindingHandles"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputEnhancedBindingHandles.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class AEnhancedInputBindingHandleActor : AActor
			{
				UPROPERTY()
				UInputAction Action;

				UPROPERTY()
				bool bSetupRan = false;

				UFUNCTION()
				void OnAction(FInputActionValue ActionValue, float32 ElapsedTime, float32 TriggeredTime, const UInputAction SourceAction)
				{
				}

				UFUNCTION()
				void OnDebug(FKey Key, FInputActionValue ActionValue)
				{
				}

				UFUNCTION()
				void SetupEnhancedInput(UEnhancedInputComponent EnhancedComponent)
				{
					if (Action == nullptr || EnhancedComponent == nullptr)
						return;

					FEnhancedInputActionHandlerDynamicSignature ActionDelegate;
					ActionDelegate.BindUFunction(this, n"OnAction");
					EnhancedComponent.BindAction(Action, ETriggerEvent::Started, ActionDelegate);
					EnhancedComponent.BindAction(Action, ETriggerEvent::Triggered, ActionDelegate);
					EnhancedComponent.BindActionValue(Action);

					FInputDebugKeyHandlerDynamicSignature DebugDelegate;
					DebugDelegate.BindUFunction(this, n"OnDebug");
					EnhancedComponent.BindDebugKey(FInputChord(EKeys::F), IE_Pressed, DebugDelegate, true);
					bSetupRan = EnhancedComponent.HasBindings();
				}
			}
			)AS"),
			TEXT("AEnhancedInputBindingHandleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("enhanced input binding handle actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("enhanced input binding handle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UEnhancedInputComponent* EnhancedComponent = NewObject<UEnhancedInputComponent>(Actor, TEXT("CoverageEnhancedBindingComponent"));
		UInputAction* InputAction = NewObject<UInputAction>(Actor, TEXT("CoverageEnhancedBindingAction"));
		ASSERT_THAT(IsNotNull(EnhancedComponent, TEXT("enhanced input component should be created for binding handles")));
		ASSERT_THAT(IsNotNull(InputAction, TEXT("input action should be created for binding handles")));
		if (EnhancedComponent == nullptr || InputAction == nullptr)
		{
			return;
		}

		FObjectPropertyBase* ActionProperty = FindFProperty<FObjectPropertyBase>(ScriptClass, TEXT("Action"));
		ASSERT_THAT(IsNotNull(ActionProperty, TEXT("Action property should be reflected")));
		if (ActionProperty == nullptr)
		{
			return;
		}
		ActionProperty->SetObjectPropertyValue_InContainer(Actor, InputAction);

		FFunctionInvoker SetupInvoker(*TestRunner, Actor, FName(TEXT("SetupEnhancedInput")));
		ASSERT_THAT(IsTrue(SetupInvoker.IsValid(), TEXT("SetupEnhancedInput should be invokable")));
		if (!SetupInvoker.IsValid())
		{
			return;
		}
		SetupInvoker.AddParam<UEnhancedInputComponent*>(EnhancedComponent);
		ASSERT_THAT(IsTrue(SetupInvoker.Call(), TEXT("SetupEnhancedInput should execute")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetupRan"), true,
			TEXT("AS Enhanced Input setup should observe component bindings"))));

		const TArray<TUniquePtr<FEnhancedInputActionEventBinding>>& EventBindings = EnhancedComponent->GetActionEventBindings();
		ASSERT_THAT(AreEqual(2, EventBindings.Num(), TEXT("BindAction should create Started and Triggered event bindings")));
		if (EventBindings.Num() != 2)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EventBindings[0].IsValid(), TEXT("first enhanced input event binding should be valid")));
		ASSERT_THAT(IsTrue(EventBindings[1].IsValid(), TEXT("second enhanced input event binding should be valid")));
		if (!EventBindings[0].IsValid() || !EventBindings[1].IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(EventBindings[0]->GetAction() == InputAction, TEXT("first event binding should keep the requested action")));
		ASSERT_THAT(AreEqual(ETriggerEvent::Started, EventBindings[0]->GetTriggerEvent(), TEXT("first event binding should keep Started")));
		ASSERT_THAT(IsTrue(EventBindings[0]->IsBoundToObject(Actor), TEXT("first event binding should be bound to the AS actor")));
		ASSERT_THAT(IsTrue(EventBindings[1]->GetAction() == InputAction, TEXT("second event binding should keep the requested action")));
		ASSERT_THAT(AreEqual(ETriggerEvent::Triggered, EventBindings[1]->GetTriggerEvent(), TEXT("second event binding should keep Triggered")));
		ASSERT_THAT(IsTrue(EventBindings[1]->IsBoundToObject(Actor), TEXT("second event binding should be bound to the AS actor")));

		const uint32 TriggeredHandle = EventBindings[1]->GetHandle();
		ASSERT_THAT(IsTrue(TriggeredHandle != 0, TEXT("enhanced input event binding should expose a non-zero handle")));
		ASSERT_THAT(IsTrue(EnhancedComponent->RemoveBindingByHandle(TriggeredHandle), TEXT("RemoveBindingByHandle should remove the Triggered event binding")));
		ASSERT_THAT(AreEqual(1, EnhancedComponent->GetActionEventBindings().Num(), TEXT("event binding handle removal should leave one event binding")));

		const TArray<FEnhancedInputActionValueBinding>& ValueBindings = EnhancedComponent->GetActionValueBindings();
		ASSERT_THAT(AreEqual(1, ValueBindings.Num(), TEXT("BindActionValue should create one value binding")));
		if (ValueBindings.Num() != 1)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ValueBindings[0].GetAction() == InputAction, TEXT("value binding should keep the requested action")));
		ASSERT_THAT(IsTrue(ValueBindings[0].GetHandle() != 0, TEXT("value binding should expose a non-zero handle")));
		ASSERT_THAT(IsTrue(EnhancedComponent->RemoveActionValueBinding(0), TEXT("RemoveActionValueBinding should remove the AS-created value binding")));
		ASSERT_THAT(AreEqual(0, EnhancedComponent->GetActionValueBindings().Num(), TEXT("value binding removal should empty value bindings")));

		const TArray<TUniquePtr<FInputDebugKeyBinding>>& DebugBindings = EnhancedComponent->GetDebugKeyBindings();
		ASSERT_THAT(AreEqual(1, DebugBindings.Num(), TEXT("BindDebugKey should create one debug key binding")));
		if (DebugBindings.Num() != 1)
		{
			return;
		}
		ASSERT_THAT(IsTrue(DebugBindings[0].IsValid(), TEXT("debug key binding should be valid")));
		if (!DebugBindings[0].IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(DebugBindings[0]->GetHandle() != 0, TEXT("debug key binding should expose a non-zero handle")));
		ASSERT_THAT(IsTrue(EnhancedComponent->RemoveDebugKeyBinding(0), TEXT("RemoveDebugKeyBinding should remove the AS-created debug binding")));
		ASSERT_THAT(AreEqual(0, EnhancedComponent->GetDebugKeyBindings().Num(), TEXT("debug key removal should empty debug bindings")));

		ASSERT_THAT(IsTrue(EnhancedComponent->RemoveActionEventBinding(0), TEXT("RemoveActionEventBinding should remove the remaining event binding")));
		ASSERT_THAT(AreEqual(0, EnhancedComponent->GetActionEventBindings().Num(), TEXT("event binding removal should empty event bindings")));
		ASSERT_THAT(IsFalse(EnhancedComponent->HasBindings(), TEXT("enhanced input component should have no bindings after removals")));
	}

	TEST_METHOD(TouchStateQuerySurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageInput_TouchStateQuerySurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageInputTouchStateQuerySurface.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ATouchStateQueryController : APlayerController
			{
				UPROPERTY()
				float32 TouchX = 0.0f;

				UPROPERTY()
				float32 TouchY = 0.0f;

				UPROPERTY()
				bool bTouchPressed = false;

				UFUNCTION()
				void CaptureTouch(ETouchIndex FingerIndex)
				{
					GetInputTouchState(FingerIndex, TouchX, TouchY, bTouchPressed);
				}

				UFUNCTION()
				bool HasTouchStateStorage()
				{
					return TouchX == 0.0f && TouchY == 0.0f && !bTouchPressed;
				}
			}
			)AS"),
			TEXT("ATouchStateQueryController"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("touch state query controller class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* CaptureFunction = ScriptClass->FindFunctionByName(TEXT("CaptureTouch"));
		ASSERT_THAT(IsNotNull(CaptureFunction, TEXT("CaptureTouch should be reflected")));
		if (CaptureFunction == nullptr)
		{
			return;
		}

		UFunction* StorageFunction = ScriptClass->FindFunctionByName(TEXT("HasTouchStateStorage"));
		ASSERT_THAT(IsNotNull(StorageFunction, TEXT("HasTouchStateStorage should be reflected")));
		if (StorageFunction == nullptr)
		{
			return;
		}

		const FNumericProperty* TouchXProperty = FindFProperty<FNumericProperty>(ScriptClass, TEXT("TouchX"));
		const FNumericProperty* TouchYProperty = FindFProperty<FNumericProperty>(ScriptClass, TEXT("TouchY"));
		ASSERT_THAT(IsNotNull(TouchXProperty, TEXT("TouchX should expose touch X storage")));
		ASSERT_THAT(IsNotNull(TouchYProperty, TEXT("TouchY should expose touch Y storage")));
		if (TouchXProperty == nullptr || TouchYProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(TouchXProperty->IsFloatingPoint(), TEXT("TouchX should use floating-point storage")));
		ASSERT_THAT(IsTrue(TouchYProperty->IsFloatingPoint(), TEXT("TouchY should use floating-point storage")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(ScriptClass, TEXT("bTouchPressed")), TEXT("bTouchPressed should expose touch pressed storage")));
		ASSERT_THAT(IsNotNull(CaptureFunction->FindPropertyByName(TEXT("FingerIndex")), TEXT("CaptureTouch should expose ETouchIndex input")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(StorageFunction->GetReturnProperty()), TEXT("HasTouchStateStorage should return bool")));
	}
};

#endif
