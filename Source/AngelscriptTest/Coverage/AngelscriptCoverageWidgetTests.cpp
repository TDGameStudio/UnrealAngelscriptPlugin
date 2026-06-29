#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/ListViewBase.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/TreeView.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Components/SlateWrapperTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopeExit.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "UObject/NoExportTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageWidgetTests
// -----------------------------------------------------------------------------
// Coverage landing file for headless-friendly UUserWidget and UWidgetTree usage.
// It intentionally avoids real viewport rendering and animation playback; those
// require a fuller UMG runtime/PIE surface.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptCoverageWidgetTest
{
	static constexpr TCHAR WidgetClassModuleName[] = TEXT("ASCoverageWidget_Class");
	static constexpr TCHAR WidgetTreeModuleName[] = TEXT("ASCoverageWidget_TreeRuntime");
	static constexpr TCHAR WidgetClassName[] = TEXT("UCoverageScoreWidget");
	static constexpr TCHAR TreeWidgetClassName[] = TEXT("UCoverageRuntimeWidget");
	static constexpr TCHAR RuntimeWidgetName[] = TEXT("CoverageRuntimeWidget");
	static constexpr TCHAR RuntimeRootName[] = TEXT("CoverageRuntimeRoot");

	struct FScopedRootedObject
	{
		explicit FScopedRootedObject(UObject* InObject)
			: Object(InObject)
		{
			if (Object != nullptr)
			{
				Object->AddToRoot();
			}
		}

		~FScopedRootedObject()
		{
			if (Object != nullptr)
			{
				if (Object->IsRooted())
				{
					Object->RemoveFromRoot();
				}
				Object->MarkAsGarbage();
			}
		}

		FScopedRootedObject(const FScopedRootedObject&) = delete;
		FScopedRootedObject& operator=(const FScopedRootedObject&) = delete;

	private:
		UObject* Object = nullptr;
	};

	void ReplaceToken(FString& Source, const TCHAR* Token, const FString& Replacement)
	{
		Source.ReplaceInline(Token, *Replacement, ESearchCase::CaseSensitive);
	}

	FString EscapeScriptString(const FString& Value)
	{
		return Value.ReplaceCharWithEscapedChar();
	}

	UClass* CompileWidgetClass(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName& ModuleName,
		const TCHAR* ScriptFilename,
		const FString& Source,
		const TCHAR* ClassName)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UClass* WidgetClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			Source,
			ClassName);
		if (!LocalAssert.IsNotNull(WidgetClass, *FString::Printf(TEXT("%s should compile"), ClassName)))
		{
			return nullptr;
		}
		if (!LocalAssert.IsTrue(
			WidgetClass->IsChildOf(UUserWidget::StaticClass()),
			*FString::Printf(TEXT("%s should derive from UUserWidget"), ClassName)))
		{
			return nullptr;
		}
		return WidgetClass;
	}

	UUserWidget* CreateRuntimeWidget(FAutomationTestBase& Test, UClass* WidgetClass)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UUserWidget* Widget = NewObject<UUserWidget>(
			GetTransientPackage(),
			WidgetClass,
			RuntimeWidgetName,
			RF_Transient);
		if (!LocalAssert.IsNotNull(Widget, TEXT("runtime UUserWidget fixture should be created")))
		{
			return nullptr;
		}

		UWidgetTree* WidgetTree = NewObject<UWidgetTree>(Widget, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transient);
		if (!LocalAssert.IsNotNull(WidgetTree, TEXT("runtime UUserWidget fixture should create WidgetTree")))
		{
			return nullptr;
		}
		Widget->WidgetTree = WidgetTree;
		return Widget;
	}

	FString BuildWidgetTreeScript(const UUserWidget& Widget)
	{
		FString Script = ASTEST_AS(R"AS(
			UUserWidget GetFixture()
			{
				return Cast<UUserWidget>(FindObject("__WIDGET_PATH__"));
			}

			UWidget MakeTextRoot(UUserWidget Widget)
			{
				return Widget.ConstructWidget(UTextBlock::StaticClass(), n"__ROOT_NAME__");
			}

			int FixtureResolves()
			{
				return GetFixture() != null ? 1 : 0;
			}

			int InitialRootIsNull()
			{
				UUserWidget Widget = GetFixture();
				return Widget != null && Widget.GetRootWidget() == null ? 1 : 0;
			}

			int ConstructTextRoot()
			{
				UUserWidget Widget = GetFixture();
				UWidget Root = MakeTextRoot(Widget);
				if (Root == null)
				{
					return 0;
				}
				bool bIsText = Cast<UTextBlock>(Root) != null;
				Widget.SetRootWidget(Root);
				Widget.RemoveWidget(Root);
				return bIsText ? 1 : 0;
			}

			int RootRoundTripAndEnumeration()
			{
				UUserWidget Widget = GetFixture();
				UWidget Root = MakeTextRoot(Widget);
				if (Root == null)
				{
					return 0;
				}

				Widget.SetRootWidget(Root);
				TArray<UWidget> Widgets;
				Widget.GetAllWidgets(Widgets);
				bool bOk = Widget.GetRootWidget() == Root && Widgets.Num() == 1 && Widgets[0] == Root;
				Widget.RemoveWidget(Root);
				return bOk ? 1 : 0;
			}

			int RemoveClearsRootAndTree()
			{
				UUserWidget Widget = GetFixture();
				UWidget Root = MakeTextRoot(Widget);
				if (Root == null)
				{
					return 0;
				}

				Widget.SetRootWidget(Root);
				bool bRemoved = Widget.RemoveWidget(Root);
				TArray<UWidget> Widgets;
				Widget.GetAllWidgets(Widgets);
				return bRemoved && Widget.GetRootWidget() == null && Widgets.Num() == 0 ? 1 : 0;
			}
			)AS");
		ReplaceToken(Script, TEXT("__WIDGET_PATH__"), EscapeScriptString(Widget.GetPathName()));
		ReplaceToken(Script, TEXT("__ROOT_NAME__"), FString(RuntimeRootName));
		return Script;
	}

	FString BuildRuntimeApiScript(const UUserWidget& Widget, const FString& Body)
	{
		FString Script = ASTEST_AS(R"AS(
			UUserWidget GetFixture()
			{
				return Cast<UUserWidget>(FindObject("__WIDGET_PATH__"));
			}

			UWidget MakeWidget(UClass WidgetClass, FName WidgetName)
			{
				UUserWidget Widget = GetFixture();
				if (Widget == null)
				{
					return null;
				}

				return Widget.ConstructWidget(WidgetClass, WidgetName);
			}

			__BODY__
			)AS");
		ReplaceToken(Script, TEXT("__WIDGET_PATH__"), EscapeScriptString(Widget.GetPathName()));
		ReplaceToken(Script, TEXT("__BODY__"), Body);
		return Script;
	}

	UUserWidget* CreateNativeRuntimeWidget(FAutomationTestBase& Test, const TCHAR* WidgetNameBase)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FName UniqueWidgetName = MakeUniqueObjectName(
			GetTransientPackage(),
			UUserWidget::StaticClass(),
			FName(WidgetNameBase));

		UUserWidget* Widget = NewObject<UUserWidget>(
			GetTransientPackage(),
			UUserWidget::StaticClass(),
			UniqueWidgetName,
			RF_Transient);
		if (!LocalAssert.IsNotNull(Widget, TEXT("native runtime UUserWidget fixture should be created")))
		{
			return nullptr;
		}

		UWidgetTree* WidgetTree = NewObject<UWidgetTree>(Widget, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transient);
		if (!LocalAssert.IsNotNull(WidgetTree, TEXT("native runtime UUserWidget fixture should create WidgetTree")))
		{
			return nullptr;
		}

		Widget->WidgetTree = WidgetTree;
		return Widget;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageWidgetTest,
	"Angelscript.TestModule.Coverage.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// UUserWidget subclass compilation stays on a fresh full engine. The
	// matching functional BindWidget test documents residual shared-engine
	// module state affecting Construct/Tick override validation for this shape.
	TEST_METHOD(WidgetClassAndBindWidgetReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FName ModuleName(AngelscriptCoverageWidgetTest::WidgetClassModuleName);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* WidgetClass = AngelscriptCoverageWidgetTest::CompileWidgetClass(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWidgetClass.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageScoreWidget : UUserWidget
			{
				UPROPERTY(BindWidget)
				UTextBlock ScoreText;

				UPROPERTY(BindWidget)
				UProgressBar HealthBar;

				UPROPERTY(BindWidget)
				UButton RestartButton;

				UPROPERTY()
				bool bConstructCalled = false;

				UPROPERTY()
				float LastTickDelta = 0.0f;

				UFUNCTION(BlueprintOverride)
				void Construct()
				{
					bConstructCalled = true;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(FGeometry MyGeometry, float DeltaTime)
				{
					LastTickDelta = DeltaTime;
				}
			}
			)AS"),
			AngelscriptCoverageWidgetTest::WidgetClassName);
		if (WidgetClass == nullptr)
		{
			return;
		}

		FObjectProperty* ScoreTextProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("ScoreText"));
		ASSERT_THAT(IsNotNull(ScoreTextProperty, TEXT("ScoreText should be reflected")));
		if (ScoreTextProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScoreTextProperty->PropertyClass != nullptr
			&& ScoreTextProperty->PropertyClass->IsChildOf(UTextBlock::StaticClass()),
			TEXT("ScoreText should be typed as UTextBlock")));
		ASSERT_THAT(IsTrue(ScoreTextProperty->HasMetaData(TEXT("BindWidget")),
			TEXT("ScoreText should carry BindWidget metadata")));

		FObjectProperty* HealthBarProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("HealthBar"));
		ASSERT_THAT(IsNotNull(HealthBarProperty, TEXT("HealthBar should be reflected")));
		if (HealthBarProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(HealthBarProperty->PropertyClass != nullptr
			&& HealthBarProperty->PropertyClass->IsChildOf(UProgressBar::StaticClass()),
			TEXT("HealthBar should be typed as UProgressBar")));
		ASSERT_THAT(IsTrue(HealthBarProperty->HasMetaData(TEXT("BindWidget")),
			TEXT("HealthBar should carry BindWidget metadata")));

		FObjectProperty* RestartButtonProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("RestartButton"));
		ASSERT_THAT(IsNotNull(RestartButtonProperty, TEXT("RestartButton should be reflected")));
		if (RestartButtonProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RestartButtonProperty->PropertyClass != nullptr
			&& RestartButtonProperty->PropertyClass->IsChildOf(UButton::StaticClass()),
			TEXT("RestartButton should be typed as UButton")));
		ASSERT_THAT(IsTrue(RestartButtonProperty->HasMetaData(TEXT("BindWidget")),
			TEXT("RestartButton should carry BindWidget metadata")));

		UFunction* ConstructFunction = WidgetClass->FindFunctionByName(TEXT("Construct"));
		UFunction* TickFunction = WidgetClass->FindFunctionByName(TEXT("Tick"));
		ASSERT_THAT(IsNotNull(ConstructFunction, TEXT("Construct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(TickFunction, TEXT("Tick override should generate a UFunction")));
		if (TickFunction == nullptr)
		{
			return;
		}

		bool bHasGeometryParam = false;
		bool bHasDeltaParam = false;
		for (TFieldIterator<FProperty> ParamIt(TickFunction); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (FStructProperty* StructParam = CastField<FStructProperty>(Param))
			{
				bHasGeometryParam |= StructParam->Struct != nullptr
					&& StructParam->Struct->GetStructCPPName() == TEXT("FGeometry");
			}
			bHasDeltaParam |= Param->IsA<FFloatProperty>() || Param->IsA<FDoubleProperty>();
		}
		ASSERT_THAT(IsTrue(bHasGeometryParam, TEXT("Tick should expose FGeometry")));
		ASSERT_THAT(IsTrue(bHasDeltaParam, TEXT("Tick should expose DeltaTime as float/double")));
	}

	TEST_METHOD(WidgetLifecycleAndAnimationOverrideReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FName ModuleName(TEXT("ASCoverageWidget_LifecycleAnimationOverrides"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* WidgetClass = AngelscriptCoverageWidgetTest::CompileWidgetClass(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWidgetLifecycleAnimationOverrides.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageLifecycleAnimationWidget : UUserWidget
			{
				UPROPERTY()
				bool bInitialized = false;

				UPROPERTY()
				bool bConstructed = false;

				UPROPERTY()
				bool bDestructed = false;

				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				UWidgetAnimation LastStartedAnimation;

				UPROPERTY()
				UWidgetAnimation LastFinishedAnimation;

				UFUNCTION(BlueprintOverride)
				void OnInitialized()
				{
					bInitialized = true;
				}

				UFUNCTION(BlueprintOverride)
				void Construct()
				{
					bConstructed = true;
				}

				UFUNCTION(BlueprintOverride)
				void Destruct()
				{
					bDestructed = true;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(FGeometry MyGeometry, float DeltaTime)
				{
					TickCount++;
				}

				UFUNCTION(BlueprintOverride)
				void OnAnimationStarted(const UWidgetAnimation Animation)
				{
					LastStartedAnimation = null;
				}

				UFUNCTION(BlueprintOverride)
				void OnAnimationFinished(const UWidgetAnimation Animation)
				{
					LastFinishedAnimation = null;
				}
			}
			)AS"),
			TEXT("UCoverageLifecycleAnimationWidget"));
		if (WidgetClass == nullptr)
		{
			return;
		}

		const FBoolProperty* InitializedProperty = FindFProperty<FBoolProperty>(WidgetClass, TEXT("bInitialized"));
		const FBoolProperty* ConstructedProperty = FindFProperty<FBoolProperty>(WidgetClass, TEXT("bConstructed"));
		const FBoolProperty* DestructedProperty = FindFProperty<FBoolProperty>(WidgetClass, TEXT("bDestructed"));
		const FIntProperty* TickCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("TickCount"));
		const FObjectProperty* StartedAnimationProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("LastStartedAnimation"));
		const FObjectProperty* FinishedAnimationProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("LastFinishedAnimation"));
		ASSERT_THAT(IsNotNull(InitializedProperty, TEXT("OnInitialized sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(ConstructedProperty, TEXT("Construct sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(DestructedProperty, TEXT("Destruct sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(TickCountProperty, TEXT("Tick sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(StartedAnimationProperty, TEXT("OnAnimationStarted sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(FinishedAnimationProperty, TEXT("OnAnimationFinished sentinel property should be reflected")));
		if (StartedAnimationProperty == nullptr || FinishedAnimationProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StartedAnimationProperty->PropertyClass != nullptr
			&& StartedAnimationProperty->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()),
			TEXT("LastStartedAnimation should be typed as UWidgetAnimation")));
		ASSERT_THAT(IsTrue(FinishedAnimationProperty->PropertyClass != nullptr
			&& FinishedAnimationProperty->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()),
			TEXT("LastFinishedAnimation should be typed as UWidgetAnimation")));

		UFunction* InitializedFunction = WidgetClass->FindFunctionByName(TEXT("OnInitialized"));
		UFunction* ConstructFunction = WidgetClass->FindFunctionByName(TEXT("Construct"));
		UFunction* DestructFunction = WidgetClass->FindFunctionByName(TEXT("Destruct"));
		UFunction* TickFunction = WidgetClass->FindFunctionByName(TEXT("Tick"));
		UFunction* AnimationStartedFunction = WidgetClass->FindFunctionByName(TEXT("OnAnimationStarted"));
		UFunction* AnimationFinishedFunction = WidgetClass->FindFunctionByName(TEXT("OnAnimationFinished"));
		ASSERT_THAT(IsNotNull(InitializedFunction, TEXT("OnInitialized override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(ConstructFunction, TEXT("Construct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(DestructFunction, TEXT("Destruct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(TickFunction, TEXT("Tick override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(AnimationStartedFunction, TEXT("OnAnimationStarted override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(AnimationFinishedFunction, TEXT("OnAnimationFinished override should generate a UFunction")));
		if (InitializedFunction == nullptr
			|| ConstructFunction == nullptr
			|| DestructFunction == nullptr
			|| TickFunction == nullptr
			|| AnimationStartedFunction == nullptr
			|| AnimationFinishedFunction == nullptr)
		{
			return;
		}

		auto CountParameters = [](const UFunction* Function) -> int32
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					++Count;
				}
			}
			return Count;
		};
		auto HasWidgetAnimationParameter = [](const UFunction* Function) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				const FObjectProperty* ObjectParam = CastField<FObjectProperty>(*ParamIt);
				if (ObjectParam != nullptr
					&& ObjectParam->PropertyClass != nullptr
					&& ObjectParam->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()))
				{
					return true;
				}
			}
			return false;
		};

		ASSERT_THAT(AreEqual(0, CountParameters(InitializedFunction), TEXT("OnInitialized should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountParameters(ConstructFunction), TEXT("Construct should expose no parameters")));
		ASSERT_THAT(AreEqual(0, CountParameters(DestructFunction), TEXT("Destruct should expose no parameters")));
		ASSERT_THAT(AreEqual(2, CountParameters(TickFunction), TEXT("Tick should expose geometry and delta parameters")));
		ASSERT_THAT(AreEqual(1, CountParameters(AnimationStartedFunction), TEXT("OnAnimationStarted should expose one animation parameter")));
		ASSERT_THAT(AreEqual(1, CountParameters(AnimationFinishedFunction), TEXT("OnAnimationFinished should expose one animation parameter")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(AnimationStartedFunction),
			TEXT("OnAnimationStarted should expose UWidgetAnimation to script")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(AnimationFinishedFunction),
			TEXT("OnAnimationFinished should expose UWidgetAnimation to script")));
	}

	TEST_METHOD(WidgetInputEventOverrideReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FName ModuleName(TEXT("ASCoverageWidget_InputEventOverrides"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* WidgetClass = AngelscriptCoverageWidgetTest::CompileWidgetClass(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWidgetInputEventOverrides.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageInputEventWidget : UUserWidget
			{
				UPROPERTY()
				int MouseDownCount = 0;

				UPROPERTY()
				int MouseUpCount = 0;

				UPROPERTY()
				int MouseMoveCount = 0;

				UPROPERTY()
				int MouseEnterCount = 0;

				UPROPERTY()
				int MouseLeaveCount = 0;

				UPROPERTY()
				int KeyDownCount = 0;

				UPROPERTY()
				int KeyUpCount = 0;

				UFUNCTION(BlueprintOverride)
				FEventReply OnMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
				{
					MouseDownCount++;
					return FEventReply::Handled();
				}

				UFUNCTION(BlueprintOverride)
				FEventReply OnMouseButtonUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
				{
					MouseUpCount++;
					return FEventReply::Handled();
				}

				UFUNCTION(BlueprintOverride)
				FEventReply OnMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
				{
					MouseMoveCount++;
					return FEventReply::Unhandled();
				}

				UFUNCTION(BlueprintOverride)
				void OnMouseEnter(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
				{
					MouseEnterCount++;
				}

				UFUNCTION(BlueprintOverride)
				void OnMouseLeave(const FPointerEvent& MouseEvent)
				{
					MouseLeaveCount++;
				}

				UFUNCTION(BlueprintOverride)
				FEventReply OnKeyDown(FGeometry MyGeometry, FKeyEvent InKeyEvent)
				{
					KeyDownCount++;
					return FEventReply::Handled();
				}

				UFUNCTION(BlueprintOverride)
				FEventReply OnKeyUp(FGeometry MyGeometry, FKeyEvent InKeyEvent)
				{
					KeyUpCount++;
					return FEventReply::Unhandled();
				}
			}
			)AS"),
			TEXT("UCoverageInputEventWidget"));
		if (WidgetClass == nullptr)
		{
			return;
		}

		const FIntProperty* MouseDownCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("MouseDownCount"));
		const FIntProperty* MouseUpCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("MouseUpCount"));
		const FIntProperty* MouseMoveCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("MouseMoveCount"));
		const FIntProperty* MouseEnterCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("MouseEnterCount"));
		const FIntProperty* MouseLeaveCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("MouseLeaveCount"));
		const FIntProperty* KeyDownCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("KeyDownCount"));
		const FIntProperty* KeyUpCountProperty = FindFProperty<FIntProperty>(WidgetClass, TEXT("KeyUpCount"));
		ASSERT_THAT(IsNotNull(MouseDownCountProperty, TEXT("OnMouseButtonDown sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(MouseUpCountProperty, TEXT("OnMouseButtonUp sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(MouseMoveCountProperty, TEXT("OnMouseMove sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(MouseEnterCountProperty, TEXT("OnMouseEnter sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(MouseLeaveCountProperty, TEXT("OnMouseLeave sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(KeyDownCountProperty, TEXT("OnKeyDown sentinel property should be reflected")));
		ASSERT_THAT(IsNotNull(KeyUpCountProperty, TEXT("OnKeyUp sentinel property should be reflected")));

		UFunction* MouseDownFunction = WidgetClass->FindFunctionByName(TEXT("OnMouseButtonDown"));
		UFunction* MouseUpFunction = WidgetClass->FindFunctionByName(TEXT("OnMouseButtonUp"));
		UFunction* MouseMoveFunction = WidgetClass->FindFunctionByName(TEXT("OnMouseMove"));
		UFunction* MouseEnterFunction = WidgetClass->FindFunctionByName(TEXT("OnMouseEnter"));
		UFunction* MouseLeaveFunction = WidgetClass->FindFunctionByName(TEXT("OnMouseLeave"));
		UFunction* KeyDownFunction = WidgetClass->FindFunctionByName(TEXT("OnKeyDown"));
		UFunction* KeyUpFunction = WidgetClass->FindFunctionByName(TEXT("OnKeyUp"));
		ASSERT_THAT(IsNotNull(MouseDownFunction, TEXT("OnMouseButtonDown override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(MouseUpFunction, TEXT("OnMouseButtonUp override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(MouseMoveFunction, TEXT("OnMouseMove override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(MouseEnterFunction, TEXT("OnMouseEnter override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(MouseLeaveFunction, TEXT("OnMouseLeave override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(KeyDownFunction, TEXT("OnKeyDown override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(KeyUpFunction, TEXT("OnKeyUp override should generate a UFunction")));
		if (MouseDownFunction == nullptr
			|| MouseUpFunction == nullptr
			|| MouseMoveFunction == nullptr
			|| MouseEnterFunction == nullptr
			|| MouseLeaveFunction == nullptr
			|| KeyDownFunction == nullptr
			|| KeyUpFunction == nullptr)
		{
			return;
		}

		auto CountParameters = [](const UFunction* Function) -> int32
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					++Count;
				}
			}
			return Count;
		};
		auto HasStructParameter = [](const UFunction* Function, const UScriptStruct* ExpectedStruct) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				const FStructProperty* StructParam = CastField<FStructProperty>(*ParamIt);
				if (StructParam != nullptr && StructParam->Struct == ExpectedStruct)
				{
					return true;
				}
			}
			return false;
		};
		auto HasEventReplyReturn = [](const UFunction* Function) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				const FStructProperty* StructParam = CastField<FStructProperty>(*ParamIt);
				if (StructParam != nullptr
					&& StructParam->HasAnyPropertyFlags(CPF_ReturnParm)
					&& StructParam->Struct == FEventReply::StaticStruct())
				{
					return true;
				}
			}
			return false;
		};

		ASSERT_THAT(AreEqual(2, CountParameters(MouseDownFunction), TEXT("OnMouseButtonDown should expose geometry and pointer parameters")));
		ASSERT_THAT(AreEqual(2, CountParameters(MouseUpFunction), TEXT("OnMouseButtonUp should expose geometry and pointer parameters")));
		ASSERT_THAT(AreEqual(2, CountParameters(MouseMoveFunction), TEXT("OnMouseMove should expose geometry and pointer parameters")));
		ASSERT_THAT(AreEqual(2, CountParameters(MouseEnterFunction), TEXT("OnMouseEnter should expose geometry and pointer parameters")));
		ASSERT_THAT(AreEqual(1, CountParameters(MouseLeaveFunction), TEXT("OnMouseLeave should expose one pointer parameter")));
		ASSERT_THAT(AreEqual(2, CountParameters(KeyDownFunction), TEXT("OnKeyDown should expose geometry and key parameters")));
		ASSERT_THAT(AreEqual(2, CountParameters(KeyUpFunction), TEXT("OnKeyUp should expose geometry and key parameters")));

		ASSERT_THAT(IsTrue(HasStructParameter(MouseDownFunction, FGeometry::StaticStruct()), TEXT("OnMouseButtonDown should expose FGeometry")));
		ASSERT_THAT(IsTrue(HasStructParameter(MouseDownFunction, FPointerEvent::StaticStruct()), TEXT("OnMouseButtonDown should expose FPointerEvent")));
		ASSERT_THAT(IsTrue(HasStructParameter(MouseUpFunction, FPointerEvent::StaticStruct()), TEXT("OnMouseButtonUp should expose FPointerEvent")));
		ASSERT_THAT(IsTrue(HasStructParameter(MouseMoveFunction, FPointerEvent::StaticStruct()), TEXT("OnMouseMove should expose FPointerEvent")));
		ASSERT_THAT(IsTrue(HasStructParameter(MouseEnterFunction, FPointerEvent::StaticStruct()), TEXT("OnMouseEnter should expose FPointerEvent")));
		ASSERT_THAT(IsTrue(HasStructParameter(MouseLeaveFunction, FPointerEvent::StaticStruct()), TEXT("OnMouseLeave should expose FPointerEvent")));
		ASSERT_THAT(IsTrue(HasStructParameter(KeyDownFunction, FKeyEvent::StaticStruct()), TEXT("OnKeyDown should expose FKeyEvent")));
		ASSERT_THAT(IsTrue(HasStructParameter(KeyUpFunction, FKeyEvent::StaticStruct()), TEXT("OnKeyUp should expose FKeyEvent")));

		ASSERT_THAT(IsTrue(HasEventReplyReturn(MouseDownFunction), TEXT("OnMouseButtonDown should return FEventReply")));
		ASSERT_THAT(IsTrue(HasEventReplyReturn(MouseUpFunction), TEXT("OnMouseButtonUp should return FEventReply")));
		ASSERT_THAT(IsTrue(HasEventReplyReturn(MouseMoveFunction), TEXT("OnMouseMove should return FEventReply")));
		ASSERT_THAT(IsTrue(HasEventReplyReturn(KeyDownFunction), TEXT("OnKeyDown should return FEventReply")));
		ASSERT_THAT(IsTrue(HasEventReplyReturn(KeyUpFunction), TEXT("OnKeyUp should return FEventReply")));
	}

	TEST_METHOD(CreateWidgetAndViewportSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FName WidgetClassModuleName(TEXT("ASCoverageWidget_CreateWidgetClass"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*WidgetClassModuleName.ToString());
		};

		UClass* WidgetClass = AngelscriptCoverageWidgetTest::CompileWidgetClass(
			*TestRunner,
			Engine,
			WidgetClassModuleName,
			TEXT("ASCoverageWidgetCreateWidgetClass.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageCreateWidgetTarget : UUserWidget
			{
			}
			)AS"),
			TEXT("UCoverageCreateWidgetTarget"));
		if (WidgetClass == nullptr)
		{
			return;
		}

		static const FName ModuleName(TEXT("ASCoverageWidget_CreateViewportSurface"));
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			*ModuleName.ToString(),
			ASTEST_AS(R"AS(
			UUserWidget CreateViaBlueprintNamespace(TSubclassOf<UUserWidget> WidgetClass, APlayerController OwningPlayer)
			{
				return WidgetBlueprint::CreateWidget(WidgetClass, OwningPlayer);
			}

			void AddAndRemoveViaViewportMethods(UUserWidget Widget)
			{
				Widget.AddToViewport(7);
				Widget.RemoveFromViewport();
			}
			)AS"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("CreateWidget/AddToViewport/RemoveFromViewport surface script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		asIScriptFunction* CreateFunction = Module.GetFunctionByDecl("UUserWidget CreateViaBlueprintNamespace(TSubclassOf<UUserWidget>, APlayerController)");
		asIScriptFunction* ViewportFunction = Module.GetFunctionByDecl("void AddAndRemoveViaViewportMethods(UUserWidget)");
		ASSERT_THAT(IsNotNull(CreateFunction, TEXT("WidgetBlueprint::CreateWidget should compile into a callable AS helper")));
		ASSERT_THAT(IsNotNull(ViewportFunction, TEXT("AddToViewport/RemoveFromViewport should compile into a callable AS helper")));

		ASSERT_THAT(IsTrue(UUserWidget::StaticClass()->FindFunctionByName(TEXT("AddToViewport")) != nullptr,
			TEXT("UUserWidget.AddToViewport should remain reflected for script-facing viewport calls")));
		ASSERT_THAT(IsTrue(UUserWidget::StaticClass()->FindFunctionByName(TEXT("RemoveFromViewport")) != nullptr,
			TEXT("UUserWidget.RemoveFromViewport should remain reflected for script-facing viewport removal")));
	}

	TEST_METHOD(GetWidgetFromNameUnsupportedBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		const TArray<FString> ExpectedDiagnostics = { TEXT("GetWidgetFromName") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_GetWidgetFromNameUnsupported"),
			ASTEST_AS(R"AS(
			UWidget LookupNamedWidget(UUserWidget Widget)
			{
				return Widget.GetWidgetFromName(n"CoverageLookupText");
			}
			)AS"),
			TEXT("UUserWidget.GetWidgetFromName should remain an explicit AS binding boundary"),
			ExpectedDiagnostics),
			TEXT("unbound UUserWidget.GetWidgetFromName should fail to compile as a documented boundary")));
	}

	TEST_METHOD(WidgetTreeRuntimeOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FName ModuleName(TEXT("ASCoverageWidget_TreeClass"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* WidgetClass = AngelscriptCoverageWidgetTest::CompileWidgetClass(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWidgetTreeClass.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageRuntimeWidget : UUserWidget
			{
			}
			)AS"),
			AngelscriptCoverageWidgetTest::TreeWidgetClassName);
		if (WidgetClass == nullptr)
		{
			return;
		}

		UUserWidget* Widget = AngelscriptCoverageWidgetTest::CreateRuntimeWidget(*TestRunner, WidgetClass);
		if (Widget == nullptr || Widget->WidgetTree == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			AngelscriptCoverageWidgetTest::WidgetTreeModuleName,
			AngelscriptCoverageWidgetTest::BuildWidgetTreeScript(*Widget));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Widget tree runtime operation script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const FExpectedInt Cases[] = {
			{ TEXT("int FixtureResolves()"), TEXT("FindObject should resolve transient widget fixture"), 1 },
			{ TEXT("int InitialRootIsNull()"), TEXT("runtime widget should start without root"), 1 },
			{ TEXT("int ConstructTextRoot()"), TEXT("ConstructWidget should create a UTextBlock root candidate"), 1 },
			{ TEXT("int RootRoundTripAndEnumeration()"), TEXT("SetRootWidget/GetRootWidget/GetAllWidgets should round-trip root"), 1 },
			{ TEXT("int RemoveClearsRootAndTree()"), TEXT("RemoveWidget should clear root and tree enumeration"), 1 },
		};
		const bool bExecuted = ExecuteBatchAndExpectInt(*TestRunner, Engine, Module, Cases);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Widget tree runtime operation cases should execute")));
		if (!bExecuted)
		{
			return;
		}

		TArray<UWidget*> Widgets;
		Widget->WidgetTree->GetAllWidgets(Widgets);
		ASSERT_THAT(IsNull(Widget->GetRootWidget(), TEXT("native postcondition should have no root widget")));
		ASSERT_THAT(AreEqual(0, Widgets.Num(), TEXT("native postcondition should have empty WidgetTree")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageWidgetRuntimeApiTest,
	"Angelscript.TestModule.Coverage.Widget.RuntimeApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UUserWidget* CreateWidgetFixture(FAutomationTestBase& Test, const TCHAR* WidgetNameBase)
	{
		return AngelscriptCoverageWidgetTest::CreateNativeRuntimeWidget(Test, WidgetNameBase);
	}

	template <typename WidgetType>
	static WidgetType* FindWidgetByName(UUserWidget& Widget, const TCHAR* WidgetName)
	{
		if (Widget.WidgetTree == nullptr)
		{
			return nullptr;
		}

		if (WidgetType* TreeWidget = Cast<WidgetType>(Widget.WidgetTree->FindWidget(FName(WidgetName))))
		{
			return TreeWidget;
		}

		return FindObjectFast<WidgetType>(Widget.WidgetTree, FName(WidgetName));
	}

	static FString BuildScript(const UUserWidget& Widget, const FString& Body)
	{
		return AngelscriptCoverageWidgetTest::BuildRuntimeApiScript(Widget, Body);
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

	TEST_METHOD(WidgetVisibilityEnabledAndFocusQueries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetVisibilityFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget visibility fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			bool SetAndReadVisibility(UWidget Widget, ESlateVisibility Visibility)
			{
				if (Widget == null)
				{
					return false;
				}

				Widget.SetVisibility(Visibility);
				return Widget.GetVisibility() == Visibility;
			}

			int VisibilityEnumRoundTrip()
			{
				UWidget Widget = MakeWidget(UButton::StaticClass(), n"VisibilityProbe");
				if (Widget == null)
				{
					return 0;
				}

				if (!SetAndReadVisibility(Widget, ESlateVisibility::Visible))
				{
					return 10;
				}
				if (!SetAndReadVisibility(Widget, ESlateVisibility::Collapsed))
				{
					return 20;
				}
				if (!SetAndReadVisibility(Widget, ESlateVisibility::Hidden))
				{
					return 30;
				}
				if (!SetAndReadVisibility(Widget, ESlateVisibility::HitTestInvisible))
				{
					return 40;
				}
				if (!SetAndReadVisibility(Widget, ESlateVisibility::SelfHitTestInvisible))
				{
					return 50;
				}

				return 1;
			}

			int EnabledRoundTrip()
			{
				UWidget Widget = MakeWidget(UButton::StaticClass(), n"EnabledProbe");
				if (Widget == null)
				{
					return 0;
				}

				Widget.SetIsEnabled(false);
				if (Widget.GetIsEnabled())
				{
					return 10;
				}

				Widget.SetIsEnabled(true);
				return Widget.GetIsEnabled() ? 1 : 20;
			}

			int KeyboardFocusQueryIsCallable()
			{
				UWidget Widget = MakeWidget(UButton::StaticClass(), n"FocusProbe");
				if (Widget == null)
				{
					return 0;
				}

				return Widget.HasKeyboardFocus() ? 2 : 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_VisibilityEnabledFocus"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Widget visibility/enabled/focus script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		const FExpectedInt Cases[] = {
			{ TEXT("int VisibilityEnumRoundTrip()"), TEXT("SetVisibility/GetVisibility should round-trip all ESlateVisibility values"), 1 },
			{ TEXT("int EnabledRoundTrip()"), TEXT("SetIsEnabled/GetIsEnabled should round-trip enabled state"), 1 },
			{ TEXT("int KeyboardFocusQueryIsCallable()"), TEXT("HasKeyboardFocus should be callable in a headless widget fixture"), 1 },
		};
		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteBatchAndExpectInt(*TestRunner, Engine, Module, Cases);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Widget visibility/enabled/focus cases should execute")));
		if (!bExecuted)
		{
			return;
		}

		UButton* VisibilityProbe = FindWidgetByName<UButton>(*Widget, TEXT("VisibilityProbe"));
		UButton* EnabledProbe = FindWidgetByName<UButton>(*Widget, TEXT("EnabledProbe"));
		ASSERT_THAT(IsNotNull(VisibilityProbe, TEXT("VisibilityProbe should remain in the widget tree")));
		if (VisibilityProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ESlateVisibility::SelfHitTestInvisible),
			static_cast<int32>(VisibilityProbe->GetVisibility()),
			TEXT("VisibilityProbe should keep the last script-assigned visibility")));
		ASSERT_THAT(IsNotNull(EnabledProbe, TEXT("EnabledProbe should remain in the widget tree")));
		if (EnabledProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EnabledProbe->GetIsEnabled(), TEXT("EnabledProbe should keep the final enabled state")));
	}

	TEST_METHOD(TextBlockTextAndColorRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetTextBlockTextFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget text block fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int TextBlockTextAndColor()
			{
				UTextBlock Text = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"FocusedTextProbe"));
				if (Text == null)
				{
					return 0;
				}

				Text.SetText(FText::FromString("Coverage Text"));
				if (Text.GetText().ToString() != "Coverage Text")
				{
					return 10;
				}

				Text.SetText(FText::FromString("Updated Coverage Text"));
				if (Text.GetText().ToString() != "Updated Coverage Text")
				{
					return 20;
				}

				Text.SetColorAndOpacity(FSlateColor(FLinearColor(0.125f, 0.375f, 0.625f, 1.0f)));
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_TextBlockTextColor"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("TextBlock text/color script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteAndExpectInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int TextBlockTextAndColor()"),
			TEXT("UTextBlock SetText/GetText/SetColorAndOpacity should execute from script"),
			1);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("TextBlock text/color case should execute")));
		if (!bExecuted)
		{
			return;
		}

		UTextBlock* TextProbe = FindWidgetByName<UTextBlock>(*Widget, TEXT("FocusedTextProbe"));
		ASSERT_THAT(IsNotNull(TextProbe, TEXT("FocusedTextProbe should remain in the widget tree")));
		if (TextProbe == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(TEXT("Updated Coverage Text")),
			TextProbe->GetText().ToString(),
			TEXT("FocusedTextProbe should keep script-assigned text")));
		ASSERT_THAT(IsTrue(
			TextProbe->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor(0.125f, 0.375f, 0.625f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("FocusedTextProbe should keep script-assigned color and opacity")));
	}

	TEST_METHOD(CommonControlPropertyMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetControlFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget control fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int TextBlockControl()
			{
				UTextBlock Text = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"TextProbe"));
				if (Text == null)
				{
					return 0;
				}

				Text.SetText(FText::FromString("Score 42"));
				FText CurrentText = Text.GetText();
				Text.SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f)));
				FSlateFontInfo Font;
				Text.SetFont(Font);
				Text.SetFontSize(24);
				Text.SetMinDesiredWidth(128.0f);
				Text.SetJustification(ETextJustify::Center);
				return 1;
			}

			int EditableTextControl()
			{
				UEditableText Editable = Cast<UEditableText>(MakeWidget(UEditableText::StaticClass(), n"EditableProbe"));
				if (Editable == null)
				{
					return 0;
				}

				Editable.SetText(FText::FromString("Player"));
				Editable.SetHintText(FText::FromString("Name"));
				Editable.SetIsReadOnly(true);
				Editable.SetJustification(ETextJustify::Right);
				FText CurrentText = Editable.GetText();
				return 1;
			}

			int ProgressBarControl()
			{
				UProgressBar Bar = Cast<UProgressBar>(MakeWidget(UProgressBar::StaticClass(), n"ProgressProbe"));
				if (Bar == null)
				{
					return 0;
				}

				Bar.SetPercent(0.75f);
				Bar.SetFillColorAndOpacity(FLinearColor(0.1f, 0.2f, 0.3f, 1.0f));
				return 1;
			}

			int SliderControl()
			{
				USlider Slider = Cast<USlider>(MakeWidget(USlider::StaticClass(), n"SliderProbe"));
				if (Slider == null)
				{
					return 0;
				}

				Slider.SetMinValue(-2.0f);
				Slider.SetMaxValue(2.0f);
				Slider.SetStepSize(0.25f);
				Slider.SetValue(1.5f);
				return Slider.GetValue() == 1.5f ? 1 : 10;
			}

			int CheckBoxControl()
			{
				UCheckBox CheckBox = Cast<UCheckBox>(MakeWidget(UCheckBox::StaticClass(), n"CheckBoxProbe"));
				if (CheckBox == null)
				{
					return 0;
				}

				CheckBox.SetIsChecked(true);
				if (!CheckBox.IsChecked())
				{
					return 10;
				}

				CheckBox.SetCheckedState(ECheckBoxState::Unchecked);
				if (CheckBox.IsChecked())
				{
					return 20;
				}

				CheckBox.SetIsChecked(true);
				return CheckBox.IsChecked() ? 1 : 30;
			}

			int ImageControl()
			{
				UImage Image = Cast<UImage>(MakeWidget(UImage::StaticClass(), n"ImageProbe"));
				if (Image == null)
				{
					return 0;
				}

				Image.SetBrushFromTexture(null, false);
				Image.SetBrushFromMaterial(null);
				Image.SetColorAndOpacity(FLinearColor(0.2f, 0.3f, 0.4f, 1.0f));
				Image.SetBrushTintColor(FSlateColor(FLinearColor(0.4f, 0.6f, 0.8f, 1.0f)));
				Image.SetDesiredSizeOverride(FVector2D(64.0f, 32.0f));
				return 1;
			}

			int ButtonControl()
			{
				UButton Button = Cast<UButton>(MakeWidget(UButton::StaticClass(), n"ButtonProbe"));
				if (Button == null)
				{
					return 0;
				}

				Button.SetIsEnabled(false);
				Button.SetColorAndOpacity(FLinearColor(0.3f, 0.2f, 0.1f, 1.0f));
				Button.SetBackgroundColor(FLinearColor(0.8f, 0.7f, 0.6f, 1.0f));
				return !Button.GetIsEnabled() ? 1 : 10;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_CommonControls"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Common widget control script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		const FExpectedInt Cases[] = {
			{ TEXT("int TextBlockControl()"), TEXT("UTextBlock setters should be callable from script"), 1 },
			{ TEXT("int EditableTextControl()"), TEXT("UEditableText setters/getters should be callable from script"), 1 },
			{ TEXT("int ProgressBarControl()"), TEXT("UProgressBar percent and fill color should be callable from script"), 1 },
			{ TEXT("int SliderControl()"), TEXT("USlider value/range/step methods should be callable from script"), 1 },
			{ TEXT("int CheckBoxControl()"), TEXT("UCheckBox checked state methods should be callable from script"), 1 },
			{ TEXT("int ImageControl()"), TEXT("UImage brush tint/texture/size methods should be callable from script"), 1 },
			{ TEXT("int ButtonControl()"), TEXT("UButton inherited enabled state and color methods should be callable from script"), 1 },
		};
		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteBatchAndExpectInt(*TestRunner, Engine, Module, Cases);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Common widget control cases should execute")));
		if (!bExecuted)
		{
			return;
		}

		UTextBlock* TextProbe = FindWidgetByName<UTextBlock>(*Widget, TEXT("TextProbe"));
		UEditableText* EditableProbe = FindWidgetByName<UEditableText>(*Widget, TEXT("EditableProbe"));
		UProgressBar* ProgressProbe = FindWidgetByName<UProgressBar>(*Widget, TEXT("ProgressProbe"));
		USlider* SliderProbe = FindWidgetByName<USlider>(*Widget, TEXT("SliderProbe"));
		UCheckBox* CheckBoxProbe = FindWidgetByName<UCheckBox>(*Widget, TEXT("CheckBoxProbe"));
		UImage* ImageProbe = FindWidgetByName<UImage>(*Widget, TEXT("ImageProbe"));
		UButton* ButtonProbe = FindWidgetByName<UButton>(*Widget, TEXT("ButtonProbe"));

		ASSERT_THAT(IsNotNull(TextProbe, TEXT("TextProbe should remain in the widget tree")));
		if (TextProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Score 42")), TextProbe->GetText().ToString(), TEXT("TextProbe should keep script-assigned text")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(24.0f, TextProbe->GetFontSize()), TEXT("TextProbe should keep script-assigned font size")));
		ASSERT_THAT(IsNotNull(EditableProbe, TEXT("EditableProbe should remain in the widget tree")));
		if (EditableProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Player")), EditableProbe->GetText().ToString(), TEXT("EditableProbe should keep script-assigned text")));
		ASSERT_THAT(AreEqual(FString(TEXT("Name")), EditableProbe->GetHintText().ToString(), TEXT("EditableProbe should keep script-assigned hint text")));
		ASSERT_THAT(IsTrue(EditableProbe->GetIsReadOnly(), TEXT("EditableProbe should keep script-assigned read-only state")));
		ASSERT_THAT(IsNotNull(ProgressProbe, TEXT("ProgressProbe should remain in the widget tree")));
		if (ProgressProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(0.75f, ProgressProbe->GetPercent()), TEXT("ProgressProbe should keep script-assigned percent")));
		ASSERT_THAT(IsNotNull(SliderProbe, TEXT("SliderProbe should remain in the widget tree")));
		if (SliderProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(1.5f, SliderProbe->GetValue()), TEXT("SliderProbe should keep script-assigned value")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(-2.0f, SliderProbe->GetMinValue()), TEXT("SliderProbe should keep script-assigned min value")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(2.0f, SliderProbe->GetMaxValue()), TEXT("SliderProbe should keep script-assigned max value")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(0.25f, SliderProbe->GetStepSize()), TEXT("SliderProbe should keep script-assigned step size")));
		ASSERT_THAT(IsNotNull(CheckBoxProbe, TEXT("CheckBoxProbe should remain in the widget tree")));
		if (CheckBoxProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(CheckBoxProbe->IsChecked(), TEXT("CheckBoxProbe should keep final script-assigned checked state")));
		ASSERT_THAT(IsNotNull(ImageProbe, TEXT("ImageProbe should remain in the widget tree")));
		if (ImageProbe == nullptr)
		{
			return;
		}
		const FVector2f ImageSize = ImageProbe->GetBrush().ImageSize;
		ASSERT_THAT(IsTrue(
			FMath::IsNearlyEqual(ImageSize.X, 64.0f) && FMath::IsNearlyEqual(ImageSize.Y, 32.0f),
			TEXT("ImageProbe should keep script-assigned desired brush size")));
		ASSERT_THAT(IsNotNull(ButtonProbe, TEXT("ButtonProbe should remain in the widget tree")));
		if (ButtonProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ButtonProbe->GetIsEnabled(), TEXT("ButtonProbe should keep final script-assigned disabled state")));
	}

	TEST_METHOD(ImageResourceBrushRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetImageResourceFixture"));
		const FName TextureName = MakeUniqueObjectName(
			GetTransientPackage(),
			UTexture2D::StaticClass(),
			TEXT("CoverageWidgetImageResourceTexture"));
		UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage(), TextureName, RF_Transient);
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget image resource fixture should be created")));
		ASSERT_THAT(IsNotNull(Texture, TEXT("Texture resource fixture should be created")));
		if (Widget == nullptr || Texture == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);
		AngelscriptCoverageWidgetTest::FScopedRootedObject TextureRoot(Texture);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int ImageResourceBrushRoundTrip(UTexture2D Texture)
			{
				UImage Image = Cast<UImage>(MakeWidget(UImage::StaticClass(), n"ImageResourceProbe"));
				if (Image == null || Texture == null)
				{
					return 0;
				}

				Image.SetBrushFromTexture(Texture, true);
				FSlateBrush TextureBrush(Texture, FVector2D(32.0f, 16.0f), FLinearColor(0.2f, 0.4f, 0.6f, 1.0f));
				Image.SetBrush(TextureBrush);
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_ImageResourceBrush"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("UImage texture brush resource script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		FASGlobalFunctionInvoker Executor(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int ImageResourceBrushRoundTrip(UTexture2D)"));
		ASSERT_THAT(IsTrue(Executor.IsValid(), TEXT("Image resource brush round-trip function should resolve")));
		if (!Executor.IsValid())
		{
			return;
		}
		const int32 Result = Executor.AddArgObject(Texture).CallAndReturn<int32>(0);
		ASSERT_THAT(AreEqual(1, Result, TEXT("UImage texture brush resource case should execute")));
		if (Result != 1)
		{
			return;
		}

		UImage* ImageProbe = FindWidgetByName<UImage>(*Widget, TEXT("ImageResourceProbe"));
		ASSERT_THAT(IsNotNull(ImageProbe, TEXT("ImageResourceProbe should remain in the widget tree")));
		if (ImageProbe == nullptr)
		{
			return;
		}

		const FSlateBrush& Brush = ImageProbe->GetBrush();
		ASSERT_THAT(IsTrue(Brush.GetResourceObject() == Texture, TEXT("ImageResourceProbe should keep the script-assigned texture resource")));
		ASSERT_THAT(IsTrue(
			FMath::IsNearlyEqual(32.0f, Brush.ImageSize.X) && FMath::IsNearlyEqual(16.0f, Brush.ImageSize.Y),
			TEXT("ImageResourceProbe should keep the script-assigned texture brush size")));
		ASSERT_THAT(IsTrue(
			Brush.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.2f, 0.4f, 0.6f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("ImageResourceProbe should keep the script-assigned texture brush tint")));
	}

	TEST_METHOD(ButtonStyleRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetButtonStyleFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget button style fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int ButtonStyleRoundTrip()
			{
				UButton Button = Cast<UButton>(MakeWidget(UButton::StaticClass(), n"ButtonStyleProbe"));
				if (Button == null)
				{
					return 0;
				}

				FButtonStyle Style = Button.WidgetStyle;
				Style.Normal = FSlateBrush(FLinearColor(0.10f, 0.20f, 0.30f, 1.0f));
				Style.Hovered = FSlateBrush(FLinearColor(0.20f, 0.30f, 0.40f, 1.0f));
				Style.Pressed = FSlateBrush(FLinearColor(0.30f, 0.40f, 0.50f, 1.0f));
				Style.Disabled = FSlateBrush(FLinearColor(0.40f, 0.50f, 0.60f, 1.0f));
				Style.NormalPadding = FMargin(1.0f, 2.0f, 3.0f, 4.0f);
				Style.PressedPadding = FMargin(5.0f, 6.0f, 7.0f, 8.0f);
				Button.WidgetStyle = Style;

				FButtonStyle CopiedStyle = Button.WidgetStyle;
				CopiedStyle.Normal = FSlateBrush(FLinearColor(0.55f, 0.65f, 0.75f, 1.0f));
				CopiedStyle.PressedPadding = FMargin(9.0f, 10.0f, 11.0f, 12.0f);
				Button.SetStyle(CopiedStyle);
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_ButtonStyleRoundTrip"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("UButton FButtonStyle round-trip script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteAndExpectInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int ButtonStyleRoundTrip()"),
			TEXT("UButton WidgetStyle and SetStyle should accept script-populated FButtonStyle"),
			1);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("UButton FButtonStyle round-trip case should execute")));
		if (!bExecuted)
		{
			return;
		}

		UButton* ButtonProbe = FindWidgetByName<UButton>(*Widget, TEXT("ButtonStyleProbe"));
		ASSERT_THAT(IsNotNull(ButtonProbe, TEXT("ButtonStyleProbe should remain in the widget tree")));
		if (ButtonProbe == nullptr)
		{
			return;
		}

		const FButtonStyle& AppliedStyle = ButtonProbe->GetStyle();
		ASSERT_THAT(IsTrue(
			AppliedStyle.Normal.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.55f, 0.65f, 0.75f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("ButtonStyleProbe should keep the SetStyle-applied normal brush")));
		ASSERT_THAT(IsTrue(
			AppliedStyle.Hovered.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.20f, 0.30f, 0.40f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("ButtonStyleProbe should preserve WidgetStyle-assigned hovered brush through copied SetStyle")));
		ASSERT_THAT(IsTrue(
			AppliedStyle.Pressed.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.30f, 0.40f, 0.50f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("ButtonStyleProbe should preserve WidgetStyle-assigned pressed brush through copied SetStyle")));
		ASSERT_THAT(IsTrue(
			AppliedStyle.Disabled.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.40f, 0.50f, 0.60f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("ButtonStyleProbe should preserve WidgetStyle-assigned disabled brush through copied SetStyle")));
		ASSERT_THAT(IsTrue(
			AppliedStyle.NormalPadding == FMargin(1.0f, 2.0f, 3.0f, 4.0f),
			TEXT("ButtonStyleProbe should preserve WidgetStyle-assigned normal padding through copied SetStyle")));
		ASSERT_THAT(IsTrue(
			AppliedStyle.PressedPadding == FMargin(9.0f, 10.0f, 11.0f, 12.0f),
			TEXT("ButtonStyleProbe should keep the SetStyle-applied pressed padding")));
	}

	TEST_METHOD(TextBlockSetFontAppliesSlateFontInfoFields)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetFontFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget font fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}

		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		UScriptStruct* SlateFontInfoStruct = FSlateFontInfo::StaticStruct();
		ASSERT_THAT(IsNotNull(SlateFontInfoStruct, TEXT("FSlateFontInfo should have reflected struct metadata")));
		if (SlateFontInfoStruct == nullptr)
		{
			return;
		}

		FProperty* SizeProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("Size"));
		FProperty* TypefaceProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("TypefaceFontName"));
		FProperty* LetterSpacingProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("LetterSpacing"));
		FProperty* SkewAmountProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("SkewAmount"));
		FProperty* ForceMonospacedProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("bForceMonospaced"));
		FProperty* MonospacedWidthProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("MonospacedWidth"));
		FProperty* MaterialIsStencilProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("bMaterialIsStencil"));
		FProperty* CompositeFontProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("CompositeFont"));
		FProperty* FontFallbackProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("FontFallback"));
		FProperty* FontObjectProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("FontObject"));
		FProperty* FontMaterialProperty = SlateFontInfoStruct->FindPropertyByName(TEXT("FontMaterial"));
		FStructProperty* OutlineSettingsProperty = FindFProperty<FStructProperty>(SlateFontInfoStruct, TEXT("OutlineSettings"));
		ASSERT_THAT(IsNotNull(SizeProperty, TEXT("FSlateFontInfo.Size should be reflected")));
		ASSERT_THAT(IsNotNull(TypefaceProperty, TEXT("FSlateFontInfo.TypefaceFontName should be reflected")));
		ASSERT_THAT(IsNotNull(FontObjectProperty, TEXT("FSlateFontInfo.FontObject should be reflected")));
		ASSERT_THAT(IsNotNull(FontMaterialProperty, TEXT("FSlateFontInfo.FontMaterial should be reflected")));
		ASSERT_THAT(IsNotNull(OutlineSettingsProperty, TEXT("FSlateFontInfo.OutlineSettings should be reflected")));
		if (SizeProperty == nullptr
			|| TypefaceProperty == nullptr
			|| FontObjectProperty == nullptr
			|| FontMaterialProperty == nullptr
			|| OutlineSettingsProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(OutlineSettingsProperty != nullptr && OutlineSettingsProperty->Struct == FFontOutlineSettings::StaticStruct(), TEXT("FSlateFontInfo.OutlineSettings should be typed as FFontOutlineSettings")));
		ASSERT_THAT(IsTrue(SizeProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.Size should be script-readable and writable")));
		ASSERT_THAT(IsTrue(TypefaceProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.TypefaceFontName should be script-readable and writable")));
		ASSERT_THAT(IsTrue(FontObjectProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.FontObject should be script-readable and writable")));
		ASSERT_THAT(IsTrue(FontMaterialProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.FontMaterial should be script-readable and writable")));
		ASSERT_THAT(IsTrue(OutlineSettingsProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.OutlineSettings should be script-readable and writable")));
		ASSERT_THAT(IsTrue(LetterSpacingProperty == nullptr || LetterSpacingProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.LetterSpacing should be script-readable when reflected")));
		ASSERT_THAT(IsTrue(SkewAmountProperty == nullptr || SkewAmountProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.SkewAmount should be script-readable when reflected")));
		ASSERT_THAT(IsTrue(ForceMonospacedProperty == nullptr || ForceMonospacedProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.bForceMonospaced should be script-readable when reflected")));
		ASSERT_THAT(IsTrue(MonospacedWidthProperty == nullptr || MonospacedWidthProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.MonospacedWidth should be script-readable when reflected")));
		ASSERT_THAT(IsTrue(MaterialIsStencilProperty == nullptr || MaterialIsStencilProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FSlateFontInfo.bMaterialIsStencil should be script-readable when reflected")));
		ASSERT_THAT(IsNull(CompositeFontProperty, TEXT("FSlateFontInfo.CompositeFont is not a reflected script field")));
		ASSERT_THAT(IsNull(FontFallbackProperty, TEXT("FSlateFontInfo.FontFallback is not a reflected script field")));

		UScriptStruct* FontOutlineSettingsStruct = FFontOutlineSettings::StaticStruct();
		ASSERT_THAT(IsNotNull(FontOutlineSettingsStruct, TEXT("FFontOutlineSettings should have reflected struct metadata")));
		if (FontOutlineSettingsStruct == nullptr)
		{
			return;
		}
		FProperty* OutlineSizeProperty = FontOutlineSettingsStruct->FindPropertyByName(TEXT("OutlineSize"));
		FProperty* MiteredCornersProperty = FontOutlineSettingsStruct->FindPropertyByName(TEXT("bMiteredCorners"));
		FProperty* SeparateFillAlphaProperty = FontOutlineSettingsStruct->FindPropertyByName(TEXT("bSeparateFillAlpha"));
		FProperty* ApplyOutlineToDropShadowsProperty = FontOutlineSettingsStruct->FindPropertyByName(TEXT("bApplyOutlineToDropShadows"));
		FProperty* OutlineMaterialProperty = FontOutlineSettingsStruct->FindPropertyByName(TEXT("OutlineMaterial"));
		FStructProperty* OutlineColorProperty = FindFProperty<FStructProperty>(FontOutlineSettingsStruct, TEXT("OutlineColor"));
		ASSERT_THAT(IsNotNull(OutlineSizeProperty, TEXT("FFontOutlineSettings.OutlineSize should be reflected")));
		ASSERT_THAT(IsNotNull(OutlineMaterialProperty, TEXT("FFontOutlineSettings.OutlineMaterial should be reflected")));
		ASSERT_THAT(IsNotNull(OutlineColorProperty, TEXT("FFontOutlineSettings.OutlineColor should be reflected")));
		if (OutlineSizeProperty == nullptr
			|| OutlineMaterialProperty == nullptr
			|| OutlineColorProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(OutlineColorProperty != nullptr && OutlineColorProperty->Struct == TBaseStructure<FLinearColor>::Get(), TEXT("FFontOutlineSettings.OutlineColor should be typed as FLinearColor")));
		ASSERT_THAT(IsTrue(MiteredCornersProperty == nullptr || MiteredCornersProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FFontOutlineSettings.bMiteredCorners should be script-readable when reflected")));
		ASSERT_THAT(IsTrue(SeparateFillAlphaProperty == nullptr || SeparateFillAlphaProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FFontOutlineSettings.bSeparateFillAlpha should be script-readable when reflected")));
		ASSERT_THAT(IsTrue(ApplyOutlineToDropShadowsProperty == nullptr || ApplyOutlineToDropShadowsProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("FFontOutlineSettings.bApplyOutlineToDropShadows should be script-readable when reflected")));

		FString ScriptBody = ASTEST_AS(R"AS(
			int SetFontFromStruct()
			{
				UTextBlock Text = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"FontInfoProbe"));
				if (Text == null)
				{
					return 0;
				}

				FSlateFontInfo Font;
				Font.FontMaterial = null;
				Font.Size = 31.0f;
				Font.TypefaceFontName = n"Bold";
				__SET_OPTIONAL_FONT_FIELDS__
				Font.OutlineSettings.OutlineSize = 2;
				Font.OutlineSettings.OutlineMaterial = null;
				Font.OutlineSettings.OutlineColor = FLinearColor(0.2f, 0.4f, 0.6f, 0.8f);
				__SET_OPTIONAL_OUTLINE_FIELDS__

				if (Font.FontObject != null || Font.FontMaterial != null)
				{
					return 5;
				}
				if (Font.Size != 31.0f)
				{
					return 10;
				}
				if (Font.TypefaceFontName != n"Bold")
				{
					return 20;
				}
				__CHECK_OPTIONAL_FONT_FIELDS__
				if (Font.OutlineSettings.OutlineSize != 2
					|| Font.OutlineSettings.OutlineMaterial != null)
				{
					return 45;
				}
				__CHECK_OPTIONAL_OUTLINE_FIELDS__
				if (Font.OutlineSettings.OutlineColor.R != 0.2f
					|| Font.OutlineSettings.OutlineColor.G != 0.4f
					|| Font.OutlineSettings.OutlineColor.B != 0.6f
					|| Font.OutlineSettings.OutlineColor.A != 0.8f)
				{
					return 46;
				}

				Font.Size += 1.0f;
				__MUTATE_OPTIONAL_FONT_FIELDS__
				Font.OutlineSettings.OutlineSize += 1;
				FSlateFontInfo CopiedFont = Font;
				Text.SetFont(CopiedFont);
				return 1;
			}
			)AS");
		AngelscriptCoverageWidgetTest::ReplaceToken(
			ScriptBody,
			TEXT("__SET_OPTIONAL_FONT_FIELDS__"),
			FString()
			+ (LetterSpacingProperty != nullptr ? TEXT("\n\t\t\t\tFont.LetterSpacing = 4;") : TEXT(""))
			+ (SkewAmountProperty != nullptr ? TEXT("\n\t\t\t\tFont.SkewAmount = 0.125f;") : TEXT(""))
			+ (ForceMonospacedProperty != nullptr ? TEXT("\n\t\t\t\tFont.bForceMonospaced = true;") : TEXT(""))
			+ (MaterialIsStencilProperty != nullptr ? TEXT("\n\t\t\t\tFont.bMaterialIsStencil = true;") : TEXT(""))
			+ (MonospacedWidthProperty != nullptr ? TEXT("\n\t\t\t\tFont.MonospacedWidth = 1.25f;") : TEXT("")));
		AngelscriptCoverageWidgetTest::ReplaceToken(
			ScriptBody,
			TEXT("__SET_OPTIONAL_OUTLINE_FIELDS__"),
			FString()
			+ (MiteredCornersProperty != nullptr ? TEXT("\n\t\t\t\tFont.OutlineSettings.bMiteredCorners = true;") : TEXT(""))
			+ (SeparateFillAlphaProperty != nullptr ? TEXT("\n\t\t\t\tFont.OutlineSettings.bSeparateFillAlpha = true;") : TEXT(""))
			+ (ApplyOutlineToDropShadowsProperty != nullptr ? TEXT("\n\t\t\t\tFont.OutlineSettings.bApplyOutlineToDropShadows = true;") : TEXT("")));
		AngelscriptCoverageWidgetTest::ReplaceToken(
			ScriptBody,
			TEXT("__CHECK_OPTIONAL_FONT_FIELDS__"),
			FString()
			+ (LetterSpacingProperty != nullptr ? TEXT("\n\t\t\t\tif (Font.LetterSpacing != 4)\n\t\t\t\t{\n\t\t\t\t\treturn 30;\n\t\t\t\t}") : TEXT(""))
			+ (SkewAmountProperty != nullptr ? TEXT("\n\t\t\t\tif (Font.SkewAmount != 0.125f)\n\t\t\t\t{\n\t\t\t\t\treturn 31;\n\t\t\t\t}") : TEXT(""))
			+ (ForceMonospacedProperty != nullptr ? TEXT("\n\t\t\t\tif (!Font.bForceMonospaced)\n\t\t\t\t{\n\t\t\t\t\treturn 35;\n\t\t\t\t}") : TEXT(""))
			+ (MaterialIsStencilProperty != nullptr ? TEXT("\n\t\t\t\tif (!Font.bMaterialIsStencil)\n\t\t\t\t{\n\t\t\t\t\treturn 36;\n\t\t\t\t}") : TEXT(""))
			+ (MonospacedWidthProperty != nullptr ? TEXT("\n\t\t\t\tif (Font.MonospacedWidth != 1.25f)\n\t\t\t\t{\n\t\t\t\t\treturn 37;\n\t\t\t\t}") : TEXT("")));
		AngelscriptCoverageWidgetTest::ReplaceToken(
			ScriptBody,
			TEXT("__CHECK_OPTIONAL_OUTLINE_FIELDS__"),
			FString()
			+ (MiteredCornersProperty != nullptr ? TEXT("\n\t\t\t\tif (!Font.OutlineSettings.bMiteredCorners)\n\t\t\t\t{\n\t\t\t\t\treturn 47;\n\t\t\t\t}") : TEXT(""))
			+ (SeparateFillAlphaProperty != nullptr ? TEXT("\n\t\t\t\tif (!Font.OutlineSettings.bSeparateFillAlpha)\n\t\t\t\t{\n\t\t\t\t\treturn 48;\n\t\t\t\t}") : TEXT(""))
			+ (ApplyOutlineToDropShadowsProperty != nullptr ? TEXT("\n\t\t\t\tif (!Font.OutlineSettings.bApplyOutlineToDropShadows)\n\t\t\t\t{\n\t\t\t\t\treturn 49;\n\t\t\t\t}") : TEXT("")));
		AngelscriptCoverageWidgetTest::ReplaceToken(
			ScriptBody,
			TEXT("__MUTATE_OPTIONAL_FONT_FIELDS__"),
			FString()
			+ (LetterSpacingProperty != nullptr ? TEXT("\n\t\t\t\tFont.LetterSpacing += 2;") : TEXT(""))
			+ (SkewAmountProperty != nullptr ? TEXT("\n\t\t\t\tFont.SkewAmount += 0.125f;") : TEXT(""))
			+ (MonospacedWidthProperty != nullptr ? TEXT("\n\t\t\t\tFont.MonospacedWidth += 0.25f;") : TEXT("")));
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_SlateFontInfoSetFont"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("FSlateFontInfo SetFont script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteAndExpectInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int SetFontFromStruct()"),
			TEXT("UTextBlock.SetFont should accept script-populated FSlateFontInfo"),
			1);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("FSlateFontInfo SetFont case should execute")));
		if (!bExecuted)
		{
			return;
		}

		UTextBlock* TextProbe = FindWidgetByName<UTextBlock>(*Widget, TEXT("FontInfoProbe"));
		ASSERT_THAT(IsNotNull(TextProbe, TEXT("FontInfoProbe should remain in the widget tree")));
		if (TextProbe == nullptr)
		{
			return;
		}

		const FSlateFontInfo& AppliedFont = TextProbe->GetFont();
		auto ReadIntProperty = [](const FProperty* Property, const void* Container, int64& OutValue) -> bool
		{
			const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
			if (NumericProperty == nullptr || !NumericProperty->IsInteger())
			{
				return false;
			}

			const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
			OutValue = NumericProperty->GetSignedIntPropertyValue(ValuePtr);
			return true;
		};
		auto ReadFloatProperty = [](const FProperty* Property, const void* Container, double& OutValue) -> bool
		{
			const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
			if (NumericProperty == nullptr || !NumericProperty->IsFloatingPoint())
			{
				return false;
			}

			const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
			OutValue = NumericProperty->GetFloatingPointPropertyValue(ValuePtr);
			return true;
		};
		auto ReadBoolProperty = [](const FProperty* Property, const void* Container, bool& bOutValue) -> bool
		{
			const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
			if (BoolProperty == nullptr)
			{
				return false;
			}

			bOutValue = BoolProperty->GetPropertyValue_InContainer(Container);
			return true;
		};

		ASSERT_THAT(IsNull(AppliedFont.FontObject.Get(), TEXT("SetFont should apply FSlateFontInfo.FontObject null state")));
		ASSERT_THAT(IsNull(AppliedFont.FontMaterial.Get(), TEXT("SetFont should apply FSlateFontInfo.FontMaterial null state")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AppliedFont.Size, 32.0f), TEXT("SetFont should apply mutated FSlateFontInfo.Size to native font state")));
		ASSERT_THAT(AreEqual(FString(TEXT("Bold")), AppliedFont.TypefaceFontName.ToString(), TEXT("SetFont should apply FSlateFontInfo.TypefaceFontName to native font state")));
		if (LetterSpacingProperty != nullptr)
		{
			int64 LetterSpacing = 0;
			ASSERT_THAT(IsTrue(ReadIntProperty(LetterSpacingProperty, &AppliedFont, LetterSpacing), TEXT("FSlateFontInfo.LetterSpacing should read as an integer property")));
			ASSERT_THAT(AreEqual(6, static_cast<int32>(LetterSpacing), TEXT("SetFont should apply mutated FSlateFontInfo.LetterSpacing to native font state")));
		}
		if (SkewAmountProperty != nullptr)
		{
			double SkewAmount = 0.0;
			ASSERT_THAT(IsTrue(ReadFloatProperty(SkewAmountProperty, &AppliedFont, SkewAmount), TEXT("FSlateFontInfo.SkewAmount should read as a floating-point property")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SkewAmount, 0.25), TEXT("SetFont should apply mutated FSlateFontInfo.SkewAmount to native font state")));
		}
		if (ForceMonospacedProperty != nullptr)
		{
			bool bForceMonospaced = false;
			ASSERT_THAT(IsTrue(ReadBoolProperty(ForceMonospacedProperty, &AppliedFont, bForceMonospaced), TEXT("FSlateFontInfo.bForceMonospaced should read as a bool property")));
			ASSERT_THAT(IsTrue(bForceMonospaced, TEXT("SetFont should apply FSlateFontInfo.bForceMonospaced to native font state")));
		}
		if (MaterialIsStencilProperty != nullptr)
		{
			bool bMaterialIsStencil = false;
			ASSERT_THAT(IsTrue(ReadBoolProperty(MaterialIsStencilProperty, &AppliedFont, bMaterialIsStencil), TEXT("FSlateFontInfo.bMaterialIsStencil should read as a bool property")));
			ASSERT_THAT(IsTrue(bMaterialIsStencil, TEXT("SetFont should apply FSlateFontInfo.bMaterialIsStencil to native font state")));
		}
		if (MonospacedWidthProperty != nullptr)
		{
			double MonospacedWidth = 0.0;
			ASSERT_THAT(IsTrue(ReadFloatProperty(MonospacedWidthProperty, &AppliedFont, MonospacedWidth), TEXT("FSlateFontInfo.MonospacedWidth should read as a floating-point property")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MonospacedWidth, 1.5), TEXT("SetFont should apply mutated FSlateFontInfo.MonospacedWidth to native font state")));
		}
		ASSERT_THAT(AreEqual(3, AppliedFont.OutlineSettings.OutlineSize, TEXT("SetFont should apply mutated FSlateFontInfo.OutlineSettings.OutlineSize to native font state")));
		if (MiteredCornersProperty != nullptr)
		{
			bool bMiteredCorners = false;
			ASSERT_THAT(IsTrue(ReadBoolProperty(MiteredCornersProperty, &AppliedFont.OutlineSettings, bMiteredCorners), TEXT("FFontOutlineSettings.bMiteredCorners should read as a bool property")));
			ASSERT_THAT(IsTrue(bMiteredCorners, TEXT("SetFont should apply FFontOutlineSettings.bMiteredCorners to native font state")));
		}
		if (SeparateFillAlphaProperty != nullptr)
		{
			bool bSeparateFillAlpha = false;
			ASSERT_THAT(IsTrue(ReadBoolProperty(SeparateFillAlphaProperty, &AppliedFont.OutlineSettings, bSeparateFillAlpha), TEXT("FFontOutlineSettings.bSeparateFillAlpha should read as a bool property")));
			ASSERT_THAT(IsTrue(bSeparateFillAlpha, TEXT("SetFont should apply FFontOutlineSettings.bSeparateFillAlpha to native font state")));
		}
		if (ApplyOutlineToDropShadowsProperty != nullptr)
		{
			bool bApplyOutlineToDropShadows = false;
			ASSERT_THAT(IsTrue(ReadBoolProperty(ApplyOutlineToDropShadowsProperty, &AppliedFont.OutlineSettings, bApplyOutlineToDropShadows), TEXT("FFontOutlineSettings.bApplyOutlineToDropShadows should read as a bool property")));
			ASSERT_THAT(IsTrue(bApplyOutlineToDropShadows, TEXT("SetFont should apply FFontOutlineSettings.bApplyOutlineToDropShadows to native font state")));
		}
		ASSERT_THAT(IsNull(AppliedFont.OutlineSettings.OutlineMaterial.Get(), TEXT("SetFont should apply FSlateFontInfo.OutlineSettings.OutlineMaterial null state")));
		ASSERT_THAT(IsTrue(AppliedFont.OutlineSettings.OutlineColor.Equals(FLinearColor(0.2f, 0.4f, 0.6f, 0.8f), KINDA_SMALL_NUMBER), TEXT("SetFont should apply FSlateFontInfo.OutlineSettings.OutlineColor to native font state")));
	}

	TEST_METHOD(ContainerLayoutOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetContainerFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget container fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int CanvasSlotOperations()
			{
				UCanvasPanel Canvas = Cast<UCanvasPanel>(MakeWidget(UCanvasPanel::StaticClass(), n"CanvasProbe"));
				UTextBlock Child = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"CanvasChild"));
				if (Canvas == null || Child == null)
				{
					return 0;
				}

				UCanvasPanelSlot Slot = Canvas.AddChildToCanvas(Child);
				if (Slot == null || Child.GetParent() != Canvas)
				{
					return 10;
				}

				Slot.SetPosition(FVector2D(12.0f, 34.0f));
				Slot.SetSize(FVector2D(56.0f, 78.0f));
				Slot.SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				return 1;
			}

			int BoxChildOperations()
			{
				UVerticalBox Vertical = Cast<UVerticalBox>(MakeWidget(UVerticalBox::StaticClass(), n"VerticalProbe"));
				UTextBlock First = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"VerticalFirst"));
				UTextBlock Second = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"VerticalSecond"));
				if (Vertical == null || First == null || Second == null)
				{
					return 0;
				}

				if (Vertical.AddChildToVerticalBox(First) == null || Vertical.AddChildToVerticalBox(Second) == null)
				{
					return 10;
				}
				if (First.GetParent() != Vertical || Second.GetParent() != Vertical)
				{
					return 20;
				}
				if (Vertical.GetChildAt(0) != First || Vertical.GetChildAt(1) != Second)
				{
					return 30;
				}
				if (!Vertical.RemoveChildAt(0))
				{
					return 40;
				}
				if (First.GetParent() != null || Vertical.GetChildAt(0) != Second)
				{
					return 50;
				}

				Vertical.ClearChildren();
				return Second.GetParent() == null && !Vertical.HasAnyChildren() ? 1 : 60;
			}

			int HorizontalOverlayAndScrollOperations()
			{
				UHorizontalBox Horizontal = Cast<UHorizontalBox>(MakeWidget(UHorizontalBox::StaticClass(), n"HorizontalProbe"));
				UOverlay Overlay = Cast<UOverlay>(MakeWidget(UOverlay::StaticClass(), n"OverlayProbe"));
				UScrollBox Scroll = Cast<UScrollBox>(MakeWidget(UScrollBox::StaticClass(), n"ScrollProbe"));
				USizeBox SizeBox = Cast<USizeBox>(MakeWidget(USizeBox::StaticClass(), n"SizeBoxProbe"));
				UTextBlock HorizontalChild = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"HorizontalChild"));
				UTextBlock OverlayChild = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"OverlayChild"));
				if (Horizontal == null || Overlay == null || Scroll == null || SizeBox == null || HorizontalChild == null || OverlayChild == null)
				{
					return 0;
				}

				if (Horizontal.AddChildToHorizontalBox(HorizontalChild) == null || HorizontalChild.GetParent() != Horizontal)
				{
					return 10;
				}
				if (Overlay.AddChildToOverlay(OverlayChild) == null || OverlayChild.GetParent() != Overlay)
				{
					return 20;
				}
				if (Horizontal.GetChildAt(0) != HorizontalChild || Overlay.GetChildAt(0) != OverlayChild)
				{
					return 30;
				}

				Scroll.SetScrollOffset(37.0f);
				Scroll.ScrollToStart();
				Scroll.ScrollToEnd();
				SizeBox.SetWidthOverride(200.0f);
				SizeBox.SetHeightOverride(50.0f);
				SizeBox.SetMinDesiredWidth(100.0f);
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_Containers"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Widget container layout script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		const FExpectedInt Cases[] = {
			{ TEXT("int CanvasSlotOperations()"), TEXT("UCanvasPanel AddChildToCanvas and slot setters should execute"), 1 },
			{ TEXT("int BoxChildOperations()"), TEXT("UVerticalBox child add/remove/clear operations should execute"), 1 },
			{ TEXT("int HorizontalOverlayAndScrollOperations()"), TEXT("HorizontalBox, Overlay, ScrollBox and SizeBox methods should execute"), 1 },
		};
		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteBatchAndExpectInt(*TestRunner, Engine, Module, Cases);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Widget container layout cases should execute")));
		if (!bExecuted)
		{
			return;
		}

		UTextBlock* CanvasChild = FindWidgetByName<UTextBlock>(*Widget, TEXT("CanvasChild"));
		UCanvasPanel* CanvasProbe = FindWidgetByName<UCanvasPanel>(*Widget, TEXT("CanvasProbe"));
		UVerticalBox* VerticalProbe = FindWidgetByName<UVerticalBox>(*Widget, TEXT("VerticalProbe"));
		UTextBlock* HorizontalChild = FindWidgetByName<UTextBlock>(*Widget, TEXT("HorizontalChild"));
		UHorizontalBox* HorizontalProbe = FindWidgetByName<UHorizontalBox>(*Widget, TEXT("HorizontalProbe"));
		UTextBlock* OverlayChild = FindWidgetByName<UTextBlock>(*Widget, TEXT("OverlayChild"));
		UOverlay* OverlayProbe = FindWidgetByName<UOverlay>(*Widget, TEXT("OverlayProbe"));

		ASSERT_THAT(IsNotNull(CanvasProbe, TEXT("CanvasProbe should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(CanvasChild, TEXT("CanvasChild should remain in the widget tree")));
		if (CanvasProbe == nullptr || CanvasChild == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(CanvasChild->GetParent() == CanvasProbe, TEXT("CanvasChild should keep script-assigned parent")));
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasChild->Slot);
		ASSERT_THAT(IsNotNull(CanvasSlot, TEXT("CanvasChild should have a UCanvasPanelSlot")));
		if (CanvasSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(CanvasSlot->GetPosition().Equals(FVector2D(12.0, 34.0)), TEXT("CanvasChild slot should keep script-assigned position")));
		ASSERT_THAT(IsTrue(CanvasSlot->GetSize().Equals(FVector2D(56.0, 78.0)), TEXT("CanvasChild slot should keep script-assigned size")));
		ASSERT_THAT(IsNotNull(VerticalProbe, TEXT("VerticalProbe should remain in the widget tree")));
		if (VerticalProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(VerticalProbe->HasAnyChildren(), TEXT("VerticalProbe should be empty after script ClearChildren")));
		ASSERT_THAT(IsNotNull(HorizontalProbe, TEXT("HorizontalProbe should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(HorizontalChild, TEXT("HorizontalChild should remain in the widget tree")));
		if (HorizontalProbe == nullptr || HorizontalChild == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(HorizontalChild->GetParent() == HorizontalProbe, TEXT("HorizontalChild should keep script-assigned parent")));
		ASSERT_THAT(IsNotNull(OverlayProbe, TEXT("OverlayProbe should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(OverlayChild, TEXT("OverlayChild should remain in the widget tree")));
		if (OverlayProbe == nullptr || OverlayChild == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OverlayChild->GetParent() == OverlayProbe, TEXT("OverlayChild should keep script-assigned parent")));
	}

	TEST_METHOD(OverlayLayerOrderOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetOverlayLayerFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget overlay layer fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int OverlayLayerOrderOperations()
			{
				UOverlay Overlay = Cast<UOverlay>(MakeWidget(UOverlay::StaticClass(), n"LayeredOverlayProbe"));
				UTextBlock Back = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"OverlayBackLayer"));
				UTextBlock Middle = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"OverlayMiddleLayer"));
				UTextBlock Front = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"OverlayFrontLayer"));
				if (Overlay == null || Back == null || Middle == null || Front == null)
				{
					return 0;
				}

				UOverlaySlot BackSlot = Overlay.AddChildToOverlay(Back);
				UOverlaySlot MiddleSlot = Overlay.AddChildToOverlay(Middle);
				UOverlaySlot FrontSlot = Overlay.AddChildToOverlay(Front);
				if (BackSlot == null || MiddleSlot == null || FrontSlot == null)
				{
					return 10;
				}
				if (Overlay.GetChildrenCount() != 3)
				{
					return 20;
				}
				if (Overlay.GetChildAt(0) != Back || Overlay.GetChildAt(1) != Middle || Overlay.GetChildAt(2) != Front)
				{
					return 30;
				}

				BackSlot.SetPadding(FMargin(1.0f, 2.0f, 3.0f, 4.0f));
				MiddleSlot.SetHorizontalAlignment(HAlign_Center);
				FrontSlot.SetVerticalAlignment(VAlign_Bottom);
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_OverlayLayerOrder"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("UOverlay layer order script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteAndExpectInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int OverlayLayerOrderOperations()"),
			TEXT("UOverlay should preserve multi-child order and slot settings from script"),
			1);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("UOverlay layer order case should execute")));
		if (!bExecuted)
		{
			return;
		}

		UOverlay* OverlayProbe = FindWidgetByName<UOverlay>(*Widget, TEXT("LayeredOverlayProbe"));
		UTextBlock* BackLayer = FindWidgetByName<UTextBlock>(*Widget, TEXT("OverlayBackLayer"));
		UTextBlock* MiddleLayer = FindWidgetByName<UTextBlock>(*Widget, TEXT("OverlayMiddleLayer"));
		UTextBlock* FrontLayer = FindWidgetByName<UTextBlock>(*Widget, TEXT("OverlayFrontLayer"));
		ASSERT_THAT(IsNotNull(OverlayProbe, TEXT("LayeredOverlayProbe should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(BackLayer, TEXT("OverlayBackLayer should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(MiddleLayer, TEXT("OverlayMiddleLayer should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(FrontLayer, TEXT("OverlayFrontLayer should remain in the widget tree")));
		if (OverlayProbe == nullptr || BackLayer == nullptr || MiddleLayer == nullptr || FrontLayer == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(3, OverlayProbe->GetChildrenCount(), TEXT("LayeredOverlayProbe should keep three script-added children")));
		ASSERT_THAT(IsTrue(OverlayProbe->GetChildAt(0) == BackLayer, TEXT("LayeredOverlayProbe should keep the first layer at index 0")));
		ASSERT_THAT(IsTrue(OverlayProbe->GetChildAt(1) == MiddleLayer, TEXT("LayeredOverlayProbe should keep the middle layer at index 1")));
		ASSERT_THAT(IsTrue(OverlayProbe->GetChildAt(2) == FrontLayer, TEXT("LayeredOverlayProbe should keep the front layer at index 2")));

		UOverlaySlot* BackSlot = Cast<UOverlaySlot>(BackLayer->Slot);
		UOverlaySlot* MiddleSlot = Cast<UOverlaySlot>(MiddleLayer->Slot);
		UOverlaySlot* FrontSlot = Cast<UOverlaySlot>(FrontLayer->Slot);
		ASSERT_THAT(IsNotNull(BackSlot, TEXT("OverlayBackLayer should have a UOverlaySlot")));
		ASSERT_THAT(IsNotNull(MiddleSlot, TEXT("OverlayMiddleLayer should have a UOverlaySlot")));
		ASSERT_THAT(IsNotNull(FrontSlot, TEXT("OverlayFrontLayer should have a UOverlaySlot")));
		if (BackSlot == nullptr || MiddleSlot == nullptr || FrontSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BackSlot->Padding == FMargin(1.0f, 2.0f, 3.0f, 4.0f), TEXT("OverlayBackLayer should keep script-assigned padding")));
		ASSERT_THAT(AreEqual(static_cast<int32>(HAlign_Center), static_cast<int32>(MiddleSlot->HorizontalAlignment.GetValue()), TEXT("OverlayMiddleLayer should keep script-assigned horizontal alignment")));
		ASSERT_THAT(AreEqual(static_cast<int32>(VAlign_Bottom), static_cast<int32>(FrontSlot->VerticalAlignment.GetValue()), TEXT("OverlayFrontLayer should keep script-assigned vertical alignment")));
	}

	TEST_METHOD(SlateStyleValueTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetSlateStyleFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget slate style fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int SlateColorAndBrushConstructors()
			{
				FSlateColor LinearColor = FSlateColor(FLinearColor(0.2f, 0.4f, 0.6f, 1.0f));
				FSlateColor TableColor = FSlateColor(EStyleColor::Foreground);
				FSlateBrush ColorBrush = FSlateBrush(FLinearColor(0.9f, 0.1f, 0.2f, 1.0f));
				FSlateBrush StyleBrush = FSlateBrush(n"WhiteBrush");

				UImage Image = Cast<UImage>(MakeWidget(UImage::StaticClass(), n"SlateStyleImageProbe"));
				UTextBlock Text = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"SlateStyleTextProbe"));
				if (Image == null || Text == null)
				{
					return 0;
				}

				Image.SetBrush(ColorBrush);
				Image.SetBrushTintColor(LinearColor);
				Text.SetColorAndOpacity(TableColor);
				Text.SetFontSize(18);
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_SlateStyleValues"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Slate style value script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteAndExpectInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int SlateColorAndBrushConstructors()"),
			TEXT("FSlateColor/FSlateBrush constructors should be callable from script and applicable to widgets"),
			1);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Slate style value case should execute")));
		if (!bExecuted)
		{
			return;
		}

		UImage* ImageProbe = FindWidgetByName<UImage>(*Widget, TEXT("SlateStyleImageProbe"));
		UTextBlock* TextProbe = FindWidgetByName<UTextBlock>(*Widget, TEXT("SlateStyleTextProbe"));
		ASSERT_THAT(IsNotNull(ImageProbe, TEXT("SlateStyleImageProbe should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(TextProbe, TEXT("SlateStyleTextProbe should remain in the widget tree")));
		if (TextProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(18.0f, TextProbe->GetFontSize()), TEXT("SlateStyleTextProbe should keep script-assigned font size")));
	}

	TEST_METHOD(ComboPanelSizeBoxAndBrushStateRoundTrips)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetComboPanelSizeFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget combo/panel/size fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int ComboBoxStringOptions()
			{
				UComboBoxString Combo = Cast<UComboBoxString>(MakeWidget(UComboBoxString::StaticClass(), n"ComboProbe"));
				if (Combo == null)
				{
					return 0;
				}

				Combo.AddOption("Low");
				Combo.AddOption("Medium");
				Combo.AddOption("High");
				if (Combo.GetOptionCount() != 3 || Combo.FindOptionIndex("Medium") != 1)
				{
					return 10;
				}

				Combo.SetSelectedOption("High");
				if (Combo.GetSelectedOption() != "High")
				{
					return 20;
				}

				if (!Combo.RemoveOption("Medium"))
				{
					return 30;
				}
				if (Combo.GetOptionCount() != 2 || Combo.FindOptionIndex("Medium") != -1)
				{
					return 40;
				}

				return 1;
			}

			int PanelWidgetBaseOperations()
			{
				UPanelWidget Panel = Cast<UPanelWidget>(MakeWidget(UVerticalBox::StaticClass(), n"PanelBaseProbe"));
				UTextBlock First = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"PanelBaseFirst"));
				UTextBlock Second = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"PanelBaseSecond"));
				if (Panel == null || First == null || Second == null)
				{
					return 0;
				}

				if (Panel.AddChild(First) == null || Panel.AddChild(Second) == null)
				{
					return 10;
				}
				if (Panel.GetChildrenCount() != 2 || Panel.GetChildAt(1) != Second)
				{
					return 20;
				}
				if (!Panel.RemoveChild(First) || First.GetParent() != null)
				{
					return 30;
				}
				if (Panel.GetChildrenCount() != 1 || Panel.GetChildAt(0) != Second)
				{
					return 40;
				}

				Panel.ClearChildren();
				return !Panel.HasAnyChildren() && Second.GetParent() == null ? 1 : 50;
			}

			int SizeBoxOverrideAccessors()
			{
				USizeBox SizeBox = Cast<USizeBox>(MakeWidget(USizeBox::StaticClass(), n"SizeBoxRoundTripProbe"));
				if (SizeBox == null)
				{
					return 0;
				}

				SizeBox.SetWidthOverride(321.0f);
				SizeBox.SetHeightOverride(123.0f);
				SizeBox.SetMinDesiredWidth(222.0f);
				return 1;
			}

			int SlateBrushFieldRoundTrip()
			{
				UImage Image = Cast<UImage>(MakeWidget(UImage::StaticClass(), n"BrushFieldProbe"));
				if (Image == null)
				{
					return 0;
				}

				FSlateBrush Brush = FSlateBrush(FLinearColor(0.15f, 0.25f, 0.35f, 1.0f));
				Brush.ImageSize = FVector2f(48.0f, 24.0f);
				Brush.DrawAs = ESlateBrushDrawType::Box;
				Brush.TintColor = FSlateColor(FLinearColor(0.45f, 0.55f, 0.65f, 1.0f));
				Image.SetBrush(Brush);
				return 1;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_ComboPanelSizeBrush"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("ComboBoxString/panel/SizeBox/brush state script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		const FExpectedInt Cases[] = {
			{ TEXT("int ComboBoxStringOptions()"), TEXT("UComboBoxString option and selected option methods should execute"), 1 },
			{ TEXT("int PanelWidgetBaseOperations()"), TEXT("UPanelWidget AddChild/RemoveChild/ClearChildren should execute through the base type"), 1 },
			{ TEXT("int SizeBoxOverrideAccessors()"), TEXT("USizeBox override setters/getters should round-trip"), 1 },
			{ TEXT("int SlateBrushFieldRoundTrip()"), TEXT("FSlateBrush ImageSize/DrawAs/TintColor fields should be script-writable"), 1 },
		};
		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteBatchAndExpectInt(*TestRunner, Engine, Module, Cases);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("ComboBoxString/panel/SizeBox/brush state cases should execute")));
		if (!bExecuted)
		{
			return;
		}

		UComboBoxString* ComboProbe = FindWidgetByName<UComboBoxString>(*Widget, TEXT("ComboProbe"));
		UVerticalBox* PanelBaseProbe = FindWidgetByName<UVerticalBox>(*Widget, TEXT("PanelBaseProbe"));
		USizeBox* SizeBoxProbe = FindWidgetByName<USizeBox>(*Widget, TEXT("SizeBoxRoundTripProbe"));
		UImage* BrushFieldProbe = FindWidgetByName<UImage>(*Widget, TEXT("BrushFieldProbe"));
		ASSERT_THAT(IsNotNull(ComboProbe, TEXT("ComboProbe should remain in the widget tree")));
		if (ComboProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(2, ComboProbe->GetOptionCount(), TEXT("ComboProbe should keep script-mutated option count")));
		ASSERT_THAT(AreEqual(FString(TEXT("High")), ComboProbe->GetSelectedOption(), TEXT("ComboProbe should keep script-selected option")));
		ASSERT_THAT(IsNotNull(PanelBaseProbe, TEXT("PanelBaseProbe should remain in the widget tree")));
		if (PanelBaseProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(PanelBaseProbe->HasAnyChildren(), TEXT("PanelBaseProbe should be empty after base ClearChildren")));
		ASSERT_THAT(IsNotNull(SizeBoxProbe, TEXT("SizeBoxRoundTripProbe should remain in the widget tree")));
		if (SizeBoxProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SizeBoxProbe->IsWidthOverride(), TEXT("SizeBoxRoundTripProbe should keep width override flag")));
		ASSERT_THAT(IsTrue(SizeBoxProbe->IsHeightOverride(), TEXT("SizeBoxRoundTripProbe should keep height override flag")));
		ASSERT_THAT(IsTrue(SizeBoxProbe->IsMinDesiredWidthOverride(), TEXT("SizeBoxRoundTripProbe should keep min desired width override flag")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(321.0f, SizeBoxProbe->GetWidthOverride()), TEXT("SizeBoxRoundTripProbe should keep width override")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(123.0f, SizeBoxProbe->GetHeightOverride()), TEXT("SizeBoxRoundTripProbe should keep height override")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(222.0f, SizeBoxProbe->GetMinDesiredWidth()), TEXT("SizeBoxRoundTripProbe should keep min desired width")));
		ASSERT_THAT(IsNotNull(BrushFieldProbe, TEXT("BrushFieldProbe should remain in the widget tree")));
		if (BrushFieldProbe == nullptr)
		{
			return;
		}
		const FSlateBrush& Brush = BrushFieldProbe->GetBrush();
		ASSERT_THAT(IsTrue(
			FMath::IsNearlyEqual(48.0f, Brush.ImageSize.X) && FMath::IsNearlyEqual(24.0f, Brush.ImageSize.Y),
			TEXT("BrushFieldProbe should keep script-assigned brush image size")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ESlateBrushDrawType::Box),
			static_cast<int32>(Brush.DrawAs.GetValue()),
			TEXT("BrushFieldProbe should keep script-assigned brush draw type")));
		ASSERT_THAT(IsTrue(
			Brush.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.45f, 0.55f, 0.65f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("BrushFieldProbe should keep script-assigned brush tint color")));
	}

	TEST_METHOD(AdvancedListViewSurfaces)
	{
		UListView* ListView = NewObject<UListView>(GetTransientPackage());
		UTileView* TileView = NewObject<UTileView>(GetTransientPackage());
		UTreeView* TreeView = NewObject<UTreeView>(GetTransientPackage());
		UObject* FirstItem = NewObject<UObject>(GetTransientPackage());
		UObject* SecondItem = NewObject<UObject>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(ListView, TEXT("UListView fixture should be created")));
		ASSERT_THAT(IsNotNull(TileView, TEXT("UTileView fixture should be created")));
		ASSERT_THAT(IsNotNull(TreeView, TEXT("UTreeView fixture should be created")));
		ASSERT_THAT(IsNotNull(FirstItem, TEXT("First list item fixture should be created")));
		ASSERT_THAT(IsNotNull(SecondItem, TEXT("Second list item fixture should be created")));
		if (ListView == nullptr
			|| TileView == nullptr
			|| TreeView == nullptr
			|| FirstItem == nullptr
			|| SecondItem == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject ListRoot(ListView);
		AngelscriptCoverageWidgetTest::FScopedRootedObject TileRoot(TileView);
		AngelscriptCoverageWidgetTest::FScopedRootedObject TreeRoot(TreeView);
		AngelscriptCoverageWidgetTest::FScopedRootedObject FirstItemRoot(FirstItem);
		AngelscriptCoverageWidgetTest::FScopedRootedObject SecondItemRoot(SecondItem);

		TArray<UObject*> Items;
		Items.Add(FirstItem);
		Items.Add(SecondItem);
		ListView->SetListItems(Items);
		TileView->SetListItems(Items);
		TreeView->SetListItems(Items);
		TileView->SetEntryWidth(144.0f);
		TileView->SetEntryHeight(96.0f);

		ASSERT_THAT(AreEqual(2, ListView->GetNumItems(), TEXT("UListView.SetListItems should populate the item source")));
		ASSERT_THAT(AreEqual(2, TileView->GetNumItems(), TEXT("UTileView.SetListItems should populate the inherited item source")));
		ASSERT_THAT(AreEqual(2, TreeView->GetNumItems(), TEXT("UTreeView.SetListItems should populate root items")));
		ASSERT_THAT(IsTrue(ListView->GetItemAt(0) == FirstItem, TEXT("UListView should keep first item order")));
		ASSERT_THAT(IsTrue(TileView->GetItemAt(1) == SecondItem, TEXT("UTileView should keep second item order")));
		ASSERT_THAT(IsTrue(TreeView->GetItemAt(0) == FirstItem, TEXT("UTreeView should keep root item order")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(144.0f, TileView->GetEntryWidth()), TEXT("UTileView should keep script-relevant entry width state")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(96.0f, TileView->GetEntryHeight()), TEXT("UTileView should keep script-relevant entry height state")));

		UFunction* SetListItemsFunction = UListView::StaticClass()->FindFunctionByName(TEXT("BP_SetListItems"));
		UFunction* RegenerateAllEntriesFunction = UListViewBase::StaticClass()->FindFunctionByName(TEXT("RegenerateAllEntries"));
		UFunction* TileSetEntryWidthFunction = UTileView::StaticClass()->FindFunctionByName(TEXT("SetEntryWidth"));
		UFunction* TileSetEntryHeightFunction = UTileView::StaticClass()->FindFunctionByName(TEXT("SetEntryHeight"));
		UFunction* TreeSetItemExpansionFunction = UTreeView::StaticClass()->FindFunctionByName(TEXT("SetItemExpansion"));
		FMulticastInlineDelegateProperty* ItemClickedProperty = FindFProperty<FMulticastInlineDelegateProperty>(UListView::StaticClass(), TEXT("BP_OnItemClicked"));
		FDelegateProperty* OnGetChildrenProperty = FindFProperty<FDelegateProperty>(UTreeView::StaticClass(), TEXT("BP_OnGetItemChildren"));
		FClassProperty* EntryWidgetClassProperty = FindFProperty<FClassProperty>(UListViewBase::StaticClass(), TEXT("EntryWidgetClass"));
		ASSERT_THAT(IsNotNull(SetListItemsFunction, TEXT("UListView SetListItems Blueprint surface should remain reflected")));
		ASSERT_THAT(IsNotNull(RegenerateAllEntriesFunction, TEXT("UListViewBase.RegenerateAllEntries should remain reflected")));
		ASSERT_THAT(IsNotNull(TileSetEntryWidthFunction, TEXT("UTileView.SetEntryWidth should remain reflected")));
		ASSERT_THAT(IsNotNull(TileSetEntryHeightFunction, TEXT("UTileView.SetEntryHeight should remain reflected")));
		ASSERT_THAT(IsNotNull(TreeSetItemExpansionFunction, TEXT("UTreeView.SetItemExpansion should remain reflected")));
		ASSERT_THAT(IsNotNull(ItemClickedProperty, TEXT("UListView OnItemClicked event property should remain reflected")));
		ASSERT_THAT(IsNotNull(OnGetChildrenProperty, TEXT("UTreeView OnGetChildren event property should remain reflected")));
		ASSERT_THAT(IsNotNull(EntryWidgetClassProperty, TEXT("UListViewBase.EntryWidgetClass should remain reflected")));
		if (SetListItemsFunction == nullptr
			|| RegenerateAllEntriesFunction == nullptr
			|| TileSetEntryWidthFunction == nullptr
			|| TileSetEntryHeightFunction == nullptr
			|| TreeSetItemExpansionFunction == nullptr
			|| ItemClickedProperty == nullptr
			|| OnGetChildrenProperty == nullptr
			|| EntryWidgetClassProperty == nullptr)
		{
			return;
		}

		EntryWidgetClassProperty->SetObjectPropertyValue_InContainer(ListView, UUserWidget::StaticClass());
		EntryWidgetClassProperty->SetObjectPropertyValue_InContainer(TileView, UUserWidget::StaticClass());
		EntryWidgetClassProperty->SetObjectPropertyValue_InContainer(TreeView, UUserWidget::StaticClass());
		ASSERT_THAT(IsTrue(ListView->GetEntryWidgetClass() == UUserWidget::StaticClass(), TEXT("UListView EntryWidgetClass should accept a UUserWidget subclass")));
		ASSERT_THAT(IsTrue(TileView->GetEntryWidgetClass() == UUserWidget::StaticClass(), TEXT("UTileView EntryWidgetClass should accept a UUserWidget subclass")));
		ASSERT_THAT(IsTrue(TreeView->GetEntryWidgetClass() == UUserWidget::StaticClass(), TEXT("UTreeView EntryWidgetClass should accept a UUserWidget subclass")));
		ASSERT_THAT(IsTrue(EntryWidgetClassProperty->MetaClass != nullptr
			&& EntryWidgetClassProperty->MetaClass->IsChildOf(UUserWidget::StaticClass()),
			TEXT("EntryWidgetClass should be constrained to UUserWidget subclasses")));
	}

	TEST_METHOD(WidgetDynamicEventsInvokeScriptHandlers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWidget_DynamicEvents"));
		UClass* HarnessClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWidgetDynamicEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageWidgetEventHarness : UObject
			{
				UPROPERTY()
				int ClickCount = 0;

				UPROPERTY()
				int PressCount = 0;

				UPROPERTY()
				int ReleaseCount = 0;

				UPROPERTY()
				int HoverCount = 0;

				UPROPERTY()
				int UnhoverCount = 0;

				UPROPERTY()
				int CheckChangedCount = 0;

				UPROPERTY()
				int TextChangedCount = 0;

				UPROPERTY()
				bool bLastChecked = false;

				UPROPERTY()
				FString LastText;

				UFUNCTION()
				void Bind(UButton Button, UCheckBox CheckBox, UEditableText Editable)
				{
					Button.OnClicked.AddUFunction(this, n"HandleClicked");
					Button.OnPressed.AddUFunction(this, n"HandlePressed");
					Button.OnReleased.AddUFunction(this, n"HandleReleased");
					Button.OnHovered.AddUFunction(this, n"HandleHovered");
					Button.OnUnhovered.AddUFunction(this, n"HandleUnhovered");
					CheckBox.OnCheckStateChanged.AddUFunction(this, n"HandleCheckStateChanged");
					Editable.OnTextChanged.AddUFunction(this, n"HandleTextChanged");
				}

				UFUNCTION()
				void HandleClicked()
				{
					ClickCount += 1;
				}

				UFUNCTION()
				void HandlePressed()
				{
					PressCount += 1;
				}

				UFUNCTION()
				void HandleReleased()
				{
					ReleaseCount += 1;
				}

				UFUNCTION()
				void HandleHovered()
				{
					HoverCount += 1;
				}

				UFUNCTION()
				void HandleUnhovered()
				{
					UnhoverCount += 1;
				}

				UFUNCTION()
				void HandleCheckStateChanged(bool bChecked)
				{
					CheckChangedCount += 1;
					bLastChecked = bChecked;
				}

				UFUNCTION()
				void HandleTextChanged(FText Text)
				{
					TextChangedCount += 1;
					LastText = Text.ToString();
				}

			}
			)AS"),
			TEXT("UCoverageWidgetEventHarness"));
		ASSERT_THAT(IsNotNull(HarnessClass, TEXT("Widget dynamic event harness class should compile")));
		if (HarnessClass == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), HarnessClass);
		UButton* Button = NewObject<UButton>(GetTransientPackage());
		UCheckBox* CheckBox = NewObject<UCheckBox>(GetTransientPackage());
		UEditableText* Editable = NewObject<UEditableText>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(Harness, TEXT("Widget dynamic event harness should be created")));
		ASSERT_THAT(IsNotNull(Button, TEXT("Button event fixture should be created")));
		ASSERT_THAT(IsNotNull(CheckBox, TEXT("CheckBox event fixture should be created")));
		ASSERT_THAT(IsNotNull(Editable, TEXT("EditableText event fixture should be created")));
		if (Harness == nullptr || Button == nullptr || CheckBox == nullptr || Editable == nullptr)
		{
			return;
		}

		FFunctionInvoker BindInvoker(*TestRunner, Harness, TEXT("Bind"));
		ASSERT_THAT(IsTrue(BindInvoker.IsValid(), TEXT("Widget event bind function should resolve")));
		if (!BindInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			BindInvoker.AddParam<UButton*>(Button).AddParam<UCheckBox*>(CheckBox).AddParam<UEditableText*>(Editable).Call(),
			TEXT("Widget event bind function should execute")));

		Button->OnClicked.Broadcast();
		Button->OnPressed.Broadcast();
		Button->OnReleased.Broadcast();
		Button->OnHovered.Broadcast();
		Button->OnUnhovered.Broadcast();
		CheckBox->OnCheckStateChanged.Broadcast(true);
		Editable->OnTextChanged.Broadcast(FText::FromString(TEXT("Changed")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("ClickCount"), 1, TEXT("UButton OnClicked should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("PressCount"), 1, TEXT("UButton OnPressed should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("ReleaseCount"), 1, TEXT("UButton OnReleased should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("HoverCount"), 1, TEXT("UButton OnHovered should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("UnhoverCount"), 1, TEXT("UButton OnUnhovered should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("CheckChangedCount"), 1, TEXT("UCheckBox OnCheckStateChanged should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Harness, TEXT("bLastChecked"), true, TEXT("UCheckBox OnCheckStateChanged should pass the checked state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("TextChangedCount"), 1, TEXT("UEditableText OnTextChanged should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Harness, TEXT("LastText"), FString(TEXT("Changed")), TEXT("UEditableText OnTextChanged should pass changed text"))));
	}

	TEST_METHOD(AdditionalWidgetDynamicEventsInvokeScriptHandlers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageWidget_AdditionalDynamicEvents"));
		UClass* HarnessClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageWidgetAdditionalDynamicEvents.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageWidgetAdditionalEventHarness : UObject
			{
				UPROPERTY()
				int TextCommittedCount = 0;

				UPROPERTY()
				int SliderChangedCount = 0;

				UPROPERTY()
				int ComboSelectionChangedCount = 0;

				UPROPERTY()
				FString LastCommittedText;

				UPROPERTY()
				int LastCommitMethod = -1;

				UPROPERTY()
				float LastSliderValue = 0.0f;

				UPROPERTY()
				FString LastSelectedItem;

				UPROPERTY()
				int LastSelectionType = -1;

				UFUNCTION()
				void Bind(UEditableText Editable, USlider Slider, UComboBoxString Combo)
				{
					Editable.OnTextCommitted.AddUFunction(this, n"HandleTextCommitted");
					Slider.OnValueChanged.AddUFunction(this, n"HandleSliderValueChanged");
					Combo.OnSelectionChanged.AddUFunction(this, n"HandleSelectionChanged");
				}

				UFUNCTION()
				void HandleTextCommitted(FText Text, ETextCommit CommitMethod)
				{
					TextCommittedCount += 1;
					LastCommittedText = Text.ToString();
					LastCommitMethod = int(CommitMethod);
				}

				UFUNCTION()
				void HandleSliderValueChanged(float Value)
				{
					SliderChangedCount += 1;
					LastSliderValue = Value;
				}

				UFUNCTION()
				void HandleSelectionChanged(FString SelectedItem, ESelectInfo SelectionType)
				{
					ComboSelectionChangedCount += 1;
					LastSelectedItem = SelectedItem;
					LastSelectionType = int(SelectionType);
				}
			}
			)AS"),
			TEXT("UCoverageWidgetAdditionalEventHarness"));
		ASSERT_THAT(IsNotNull(HarnessClass, TEXT("Additional widget dynamic event harness class should compile")));
		if (HarnessClass == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UObject* Harness = NewObject<UObject>(GetTransientPackage(), HarnessClass);
		UEditableText* Editable = NewObject<UEditableText>(GetTransientPackage());
		USlider* Slider = NewObject<USlider>(GetTransientPackage());
		UComboBoxString* Combo = NewObject<UComboBoxString>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(Harness, TEXT("Additional widget dynamic event harness should be created")));
		ASSERT_THAT(IsNotNull(Editable, TEXT("EditableText committed event fixture should be created")));
		ASSERT_THAT(IsNotNull(Slider, TEXT("Slider value event fixture should be created")));
		ASSERT_THAT(IsNotNull(Combo, TEXT("ComboBoxString selection event fixture should be created")));
		if (Harness == nullptr || Editable == nullptr || Slider == nullptr || Combo == nullptr)
		{
			return;
		}

		FFunctionInvoker BindInvoker(*TestRunner, Harness, TEXT("Bind"));
		ASSERT_THAT(IsTrue(BindInvoker.IsValid(), TEXT("Additional widget event bind function should resolve")));
		if (!BindInvoker.IsValid())
		{
			return;
		}
		const bool bBoundHandlers = BindInvoker
			.AddParam<UEditableText*>(Editable)
			.AddParam<USlider*>(Slider)
			.AddParam<UComboBoxString*>(Combo)
			.Call();
		ASSERT_THAT(IsTrue(bBoundHandlers, TEXT("Additional widget event bind function should execute")));
		if (!bBoundHandlers)
		{
			return;
		}

		Editable->OnTextCommitted.Broadcast(FText::FromString(TEXT("Committed")), ETextCommit::OnEnter);
		Slider->OnValueChanged.Broadcast(0.625f);
		Combo->OnSelectionChanged.Broadcast(FString(TEXT("High")), ESelectInfo::OnMouseClick);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("TextCommittedCount"), 1, TEXT("UEditableText OnTextCommitted should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Harness, TEXT("LastCommittedText"), FString(TEXT("Committed")), TEXT("UEditableText OnTextCommitted should pass committed text"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("LastCommitMethod"), static_cast<int32>(ETextCommit::OnEnter), TEXT("UEditableText OnTextCommitted should pass commit method"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("SliderChangedCount"), 1, TEXT("USlider OnValueChanged should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Harness, TEXT("LastSliderValue"), 0.625, TEXT("USlider OnValueChanged should pass the changed value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("ComboSelectionChangedCount"), 1, TEXT("UComboBoxString OnSelectionChanged should invoke the AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Harness, TEXT("LastSelectedItem"), FString(TEXT("High")), TEXT("UComboBoxString OnSelectionChanged should pass the selected item"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Harness, TEXT("LastSelectionType"), static_cast<int32>(ESelectInfo::OnMouseClick), TEXT("UComboBoxString OnSelectionChanged should pass the selection type"))));
	}

	TEST_METHOD(ButtonEventDelegateReflectionSurfaces)
	{
		auto FindButtonDelegateProperty = [](const TCHAR* PropertyName) -> FMulticastDelegateProperty*
		{
			return FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), PropertyName);
		};
		auto CountSignatureParameters = [](const UFunction* SignatureFunction) -> int32
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> ParamIt(SignatureFunction); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					++Count;
				}
			}
			return Count;
		};

		FMulticastDelegateProperty* ClickedProperty = FindButtonDelegateProperty(TEXT("OnClicked"));
		FMulticastDelegateProperty* PressedProperty = FindButtonDelegateProperty(TEXT("OnPressed"));
		FMulticastDelegateProperty* ReleasedProperty = FindButtonDelegateProperty(TEXT("OnReleased"));
		FMulticastDelegateProperty* HoveredProperty = FindButtonDelegateProperty(TEXT("OnHovered"));
		FMulticastDelegateProperty* UnhoveredProperty = FindButtonDelegateProperty(TEXT("OnUnhovered"));
		ASSERT_THAT(IsNotNull(ClickedProperty, TEXT("UButton.OnClicked should remain reflected")));
		ASSERT_THAT(IsNotNull(PressedProperty, TEXT("UButton.OnPressed should remain reflected")));
		ASSERT_THAT(IsNotNull(ReleasedProperty, TEXT("UButton.OnReleased should remain reflected")));
		ASSERT_THAT(IsNotNull(HoveredProperty, TEXT("UButton.OnHovered should remain reflected")));
		ASSERT_THAT(IsNotNull(UnhoveredProperty, TEXT("UButton.OnUnhovered should remain reflected")));
		if (ClickedProperty == nullptr
			|| PressedProperty == nullptr
			|| ReleasedProperty == nullptr
			|| HoveredProperty == nullptr
			|| UnhoveredProperty == nullptr)
		{
			return;
		}

		UFunction* ClickedSignature = ClickedProperty->SignatureFunction;
		UFunction* PressedSignature = PressedProperty->SignatureFunction;
		UFunction* ReleasedSignature = ReleasedProperty->SignatureFunction;
		UFunction* HoveredSignature = HoveredProperty->SignatureFunction;
		UFunction* UnhoveredSignature = UnhoveredProperty->SignatureFunction;
		ASSERT_THAT(IsNotNull(ClickedSignature, TEXT("UButton.OnClicked should expose a delegate signature")));
		ASSERT_THAT(IsNotNull(PressedSignature, TEXT("UButton.OnPressed should expose a delegate signature")));
		ASSERT_THAT(IsNotNull(ReleasedSignature, TEXT("UButton.OnReleased should expose a delegate signature")));
		ASSERT_THAT(IsNotNull(HoveredSignature, TEXT("UButton.OnHovered should expose a delegate signature")));
		ASSERT_THAT(IsNotNull(UnhoveredSignature, TEXT("UButton.OnUnhovered should expose a delegate signature")));
		if (ClickedSignature == nullptr
			|| PressedSignature == nullptr
			|| ReleasedSignature == nullptr
			|| HoveredSignature == nullptr
			|| UnhoveredSignature == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, CountSignatureParameters(ClickedSignature), TEXT("UButton.OnClicked should expose no payload parameters")));
		ASSERT_THAT(AreEqual(0, CountSignatureParameters(PressedSignature), TEXT("UButton.OnPressed should expose no payload parameters")));
		ASSERT_THAT(AreEqual(0, CountSignatureParameters(ReleasedSignature), TEXT("UButton.OnReleased should expose no payload parameters")));
		ASSERT_THAT(AreEqual(0, CountSignatureParameters(HoveredSignature), TEXT("UButton.OnHovered should expose no payload parameters")));
		ASSERT_THAT(AreEqual(0, CountSignatureParameters(UnhoveredSignature), TEXT("UButton.OnUnhovered should expose no payload parameters")));
	}

	TEST_METHOD(WidgetAdditionalEventDelegateReflectionSurfaces)
	{
		auto FindMulticastDelegateProperty = [](UClass* Class, const TCHAR* PropertyName) -> FMulticastDelegateProperty*
		{
			return FindFProperty<FMulticastDelegateProperty>(Class, PropertyName);
		};
		auto CountSignatureParameters = [](const UFunction* SignatureFunction) -> int32
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> ParamIt(SignatureFunction); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					++Count;
				}
			}
			return Count;
		};
		auto SignatureHasPropertyType = [](const UFunction* SignatureFunction, const FFieldClass* PropertyClass) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(SignatureFunction); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm) && ParamIt->IsA(PropertyClass))
				{
					return true;
				}
			}
			return false;
		};
		auto SignatureHasEnumParameter = [](const UFunction* SignatureFunction, const TCHAR* EnumName) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(SignatureFunction); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				const FByteProperty* ByteParam = CastField<FByteProperty>(*ParamIt);
				if (ByteParam != nullptr && ByteParam->Enum != nullptr && ByteParam->Enum->GetName() == EnumName)
				{
					return true;
				}

				const FEnumProperty* EnumParam = CastField<FEnumProperty>(*ParamIt);
				if (EnumParam != nullptr && EnumParam->GetEnum() != nullptr && EnumParam->GetEnum()->GetName() == EnumName)
				{
					return true;
				}
			}
			return false;
		};

		FMulticastDelegateProperty* EditableTextCommittedProperty = FindMulticastDelegateProperty(UEditableText::StaticClass(), TEXT("OnTextCommitted"));
		FMulticastDelegateProperty* SliderValueChangedProperty = FindMulticastDelegateProperty(USlider::StaticClass(), TEXT("OnValueChanged"));
		FMulticastDelegateProperty* ComboSelectionChangedProperty = FindMulticastDelegateProperty(UComboBoxString::StaticClass(), TEXT("OnSelectionChanged"));
		ASSERT_THAT(IsNotNull(EditableTextCommittedProperty, TEXT("UEditableText.OnTextCommitted should remain reflected")));
		ASSERT_THAT(IsNotNull(SliderValueChangedProperty, TEXT("USlider.OnValueChanged should remain reflected")));
		ASSERT_THAT(IsNotNull(ComboSelectionChangedProperty, TEXT("UComboBoxString.OnSelectionChanged should remain reflected")));
		if (EditableTextCommittedProperty == nullptr
			|| SliderValueChangedProperty == nullptr
			|| ComboSelectionChangedProperty == nullptr)
		{
			return;
		}

		UFunction* EditableTextCommittedSignature = EditableTextCommittedProperty->SignatureFunction;
		UFunction* SliderValueChangedSignature = SliderValueChangedProperty->SignatureFunction;
		UFunction* ComboSelectionChangedSignature = ComboSelectionChangedProperty->SignatureFunction;
		ASSERT_THAT(IsNotNull(EditableTextCommittedSignature, TEXT("UEditableText.OnTextCommitted should expose a delegate signature")));
		ASSERT_THAT(IsNotNull(SliderValueChangedSignature, TEXT("USlider.OnValueChanged should expose a delegate signature")));
		ASSERT_THAT(IsNotNull(ComboSelectionChangedSignature, TEXT("UComboBoxString.OnSelectionChanged should expose a delegate signature")));
		if (EditableTextCommittedSignature == nullptr
			|| SliderValueChangedSignature == nullptr
			|| ComboSelectionChangedSignature == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, CountSignatureParameters(EditableTextCommittedSignature), TEXT("UEditableText.OnTextCommitted should expose text and commit method parameters")));
		ASSERT_THAT(IsTrue(SignatureHasPropertyType(EditableTextCommittedSignature, FTextProperty::StaticClass()), TEXT("UEditableText.OnTextCommitted should expose an FText parameter")));
		ASSERT_THAT(IsTrue(SignatureHasEnumParameter(EditableTextCommittedSignature, TEXT("ETextCommit")), TEXT("UEditableText.OnTextCommitted should expose ETextCommit")));

		ASSERT_THAT(AreEqual(1, CountSignatureParameters(SliderValueChangedSignature), TEXT("USlider.OnValueChanged should expose one value parameter")));
		ASSERT_THAT(IsTrue(
			SignatureHasPropertyType(SliderValueChangedSignature, FFloatProperty::StaticClass())
				|| SignatureHasPropertyType(SliderValueChangedSignature, FDoubleProperty::StaticClass()),
			TEXT("USlider.OnValueChanged should expose a float/double value parameter")));

		ASSERT_THAT(AreEqual(2, CountSignatureParameters(ComboSelectionChangedSignature), TEXT("UComboBoxString.OnSelectionChanged should expose item and selection type parameters")));
		ASSERT_THAT(IsTrue(SignatureHasPropertyType(ComboSelectionChangedSignature, FStrProperty::StaticClass()), TEXT("UComboBoxString.OnSelectionChanged should expose a string item parameter")));
		ASSERT_THAT(IsTrue(SignatureHasEnumParameter(ComboSelectionChangedSignature, TEXT("ESelectInfo")), TEXT("UComboBoxString.OnSelectionChanged should expose ESelectInfo")));
	}

	TEST_METHOD(WidgetAnimationPlaybackReflectionSurfaces)
	{
		auto CountParameters = [](const UFunction* Function) -> int32
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					++Count;
				}
			}
			return Count;
		};
		auto HasWidgetAnimationParameter = [](const UFunction* Function) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				const FObjectProperty* ObjectParam = CastField<FObjectProperty>(*ParamIt);
				if (ObjectParam != nullptr
					&& ObjectParam->PropertyClass != nullptr
					&& ObjectParam->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()))
				{
					return true;
				}
			}
			return false;
		};
		auto HasBoolReturn = [](const UFunction* Function) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (ParamIt->HasAnyPropertyFlags(CPF_ReturnParm) && ParamIt->IsA<FBoolProperty>())
				{
					return true;
				}
			}
			return false;
		};

		UClass* UserWidgetClass = UUserWidget::StaticClass();
		UFunction* PlayAnimationFunction = UserWidgetClass->FindFunctionByName(TEXT("PlayAnimation"));
		UFunction* StopAnimationFunction = UserWidgetClass->FindFunctionByName(TEXT("StopAnimation"));
		UFunction* PauseAnimationFunction = UserWidgetClass->FindFunctionByName(TEXT("PauseAnimation"));
		UFunction* ResumeAnimationFunction = UserWidgetClass->FindFunctionByName(TEXT("ResumeAnimation"));
		UFunction* ReverseAnimationFunction = UserWidgetClass->FindFunctionByName(TEXT("ReverseAnimation"));
		UFunction* IsAnimationPlayingFunction = UserWidgetClass->FindFunctionByName(TEXT("IsAnimationPlaying"));
		UFunction* BindStartedFunction = UserWidgetClass->FindFunctionByName(TEXT("BindToAnimationStarted"));
		UFunction* BindFinishedFunction = UserWidgetClass->FindFunctionByName(TEXT("BindToAnimationFinished"));
		ASSERT_THAT(IsNotNull(PlayAnimationFunction, TEXT("UUserWidget.PlayAnimation should remain reflected")));
		ASSERT_THAT(IsNotNull(StopAnimationFunction, TEXT("UUserWidget.StopAnimation should remain reflected")));
		ASSERT_THAT(IsNotNull(PauseAnimationFunction, TEXT("UUserWidget.PauseAnimation should remain reflected")));
		ASSERT_THAT(IsNotNull(ResumeAnimationFunction, TEXT("UUserWidget.ResumeAnimation should remain reflected")));
		ASSERT_THAT(IsNotNull(ReverseAnimationFunction, TEXT("UUserWidget.ReverseAnimation should remain reflected")));
		ASSERT_THAT(IsNotNull(IsAnimationPlayingFunction, TEXT("UUserWidget.IsAnimationPlaying should remain reflected")));
		ASSERT_THAT(IsNotNull(BindStartedFunction, TEXT("UUserWidget.BindToAnimationStarted should remain reflected")));
		ASSERT_THAT(IsNotNull(BindFinishedFunction, TEXT("UUserWidget.BindToAnimationFinished should remain reflected")));
		if (PlayAnimationFunction == nullptr
			|| StopAnimationFunction == nullptr
			|| PauseAnimationFunction == nullptr
			|| ResumeAnimationFunction == nullptr
			|| ReverseAnimationFunction == nullptr
			|| IsAnimationPlayingFunction == nullptr
			|| BindStartedFunction == nullptr
			|| BindFinishedFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(PlayAnimationFunction), TEXT("PlayAnimation should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(StopAnimationFunction), TEXT("StopAnimation should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(PauseAnimationFunction), TEXT("PauseAnimation should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(ResumeAnimationFunction), TEXT("ResumeAnimation should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(ReverseAnimationFunction), TEXT("ReverseAnimation should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(IsAnimationPlayingFunction), TEXT("IsAnimationPlaying should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(BindStartedFunction), TEXT("BindToAnimationStarted should accept UWidgetAnimation")));
		ASSERT_THAT(IsTrue(HasWidgetAnimationParameter(BindFinishedFunction), TEXT("BindToAnimationFinished should accept UWidgetAnimation")));

		ASSERT_THAT(IsTrue(CountParameters(PlayAnimationFunction) >= 1, TEXT("PlayAnimation should expose animation plus optional playback parameters")));
		ASSERT_THAT(IsTrue(CountParameters(BindStartedFunction) >= 2, TEXT("BindToAnimationStarted should expose animation and delegate parameters")));
		ASSERT_THAT(IsTrue(CountParameters(BindFinishedFunction) >= 2, TEXT("BindToAnimationFinished should expose animation and delegate parameters")));
		ASSERT_THAT(IsTrue(HasBoolReturn(IsAnimationPlayingFunction), TEXT("IsAnimationPlaying should return a bool")));
	}

	TEST_METHOD(WidgetFocusAndInputModeReflectionSurfaces)
	{
		auto CountParameters = [](const UFunction* Function) -> int32
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (!ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					++Count;
				}
			}
			return Count;
		};
		auto HasBoolReturn = [](const UFunction* Function) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (ParamIt->HasAnyPropertyFlags(CPF_ReturnParm) && ParamIt->IsA<FBoolProperty>())
				{
					return true;
				}
			}
			return false;
		};
		auto HasObjectParameter = [](const UFunction* Function, UClass* ExpectedClass) -> bool
		{
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
			{
				if (ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				const FObjectPropertyBase* ObjectParam = CastField<FObjectPropertyBase>(*ParamIt);
				if (ObjectParam != nullptr
					&& ObjectParam->PropertyClass != nullptr
					&& ObjectParam->PropertyClass->IsChildOf(ExpectedClass))
				{
					return true;
				}
			}
			return false;
		};

		UClass* WidgetClass = UWidget::StaticClass();
		UFunction* SetKeyboardFocusFunction = WidgetClass->FindFunctionByName(TEXT("SetKeyboardFocus"));
		UFunction* HasKeyboardFocusFunction = WidgetClass->FindFunctionByName(TEXT("HasKeyboardFocus"));
		UFunction* SetUserFocusFunction = WidgetClass->FindFunctionByName(TEXT("SetUserFocus"));
		ASSERT_THAT(IsNotNull(SetKeyboardFocusFunction, TEXT("UWidget.SetKeyboardFocus should remain reflected")));
		ASSERT_THAT(IsNotNull(HasKeyboardFocusFunction, TEXT("UWidget.HasKeyboardFocus should remain reflected")));
		ASSERT_THAT(IsNotNull(SetUserFocusFunction, TEXT("UWidget.SetUserFocus should remain reflected")));
		if (SetKeyboardFocusFunction == nullptr
			|| HasKeyboardFocusFunction == nullptr
			|| SetUserFocusFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, CountParameters(SetKeyboardFocusFunction), TEXT("SetKeyboardFocus should not require payload parameters")));
		ASSERT_THAT(AreEqual(0, CountParameters(HasKeyboardFocusFunction), TEXT("HasKeyboardFocus should not require payload parameters")));
		ASSERT_THAT(IsTrue(HasBoolReturn(HasKeyboardFocusFunction), TEXT("HasKeyboardFocus should return a bool")));
		ASSERT_THAT(AreEqual(1, CountParameters(SetUserFocusFunction), TEXT("SetUserFocus should expose the player controller parameter")));
		UClass* PlayerControllerClass = APlayerController::StaticClass();
		ASSERT_THAT(IsNotNull(PlayerControllerClass, TEXT("PlayerController class should remain reflected")));
		if (PlayerControllerClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(HasObjectParameter(SetUserFocusFunction, PlayerControllerClass), TEXT("SetUserFocus should accept APlayerController")));

		UClass* WidgetBlueprintLibraryClass = UWidgetBlueprintLibrary::StaticClass();
		ASSERT_THAT(IsNotNull(WidgetBlueprintLibraryClass, TEXT("WidgetBlueprintLibrary class should remain reflected")));
		if (WidgetBlueprintLibraryClass == nullptr)
		{
			return;
		}

		UFunction* SetInputModeUIOnlyFunction = WidgetBlueprintLibraryClass->FindFunctionByName(TEXT("SetInputMode_UIOnlyEx"));
		UFunction* SetInputModeGameAndUIFunction = WidgetBlueprintLibraryClass->FindFunctionByName(TEXT("SetInputMode_GameAndUIEx"));
		UFunction* SetInputModeGameOnlyFunction = WidgetBlueprintLibraryClass->FindFunctionByName(TEXT("SetInputMode_GameOnly"));
		ASSERT_THAT(IsNotNull(SetInputModeUIOnlyFunction, TEXT("WidgetBlueprintLibrary.SetInputMode_UIOnlyEx should remain reflected")));
		ASSERT_THAT(IsNotNull(SetInputModeGameAndUIFunction, TEXT("WidgetBlueprintLibrary.SetInputMode_GameAndUIEx should remain reflected")));
		ASSERT_THAT(IsNotNull(SetInputModeGameOnlyFunction, TEXT("WidgetBlueprintLibrary.SetInputMode_GameOnly should remain reflected")));
		if (SetInputModeUIOnlyFunction == nullptr
			|| SetInputModeGameAndUIFunction == nullptr
			|| SetInputModeGameOnlyFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasObjectParameter(SetInputModeUIOnlyFunction, PlayerControllerClass), TEXT("SetInputMode_UIOnlyEx should accept APlayerController")));
		ASSERT_THAT(IsTrue(HasObjectParameter(SetInputModeUIOnlyFunction, UWidget::StaticClass()), TEXT("SetInputMode_UIOnlyEx should accept a widget focus target")));
		ASSERT_THAT(IsTrue(HasObjectParameter(SetInputModeGameAndUIFunction, PlayerControllerClass), TEXT("SetInputMode_GameAndUIEx should accept APlayerController")));
		ASSERT_THAT(IsTrue(HasObjectParameter(SetInputModeGameAndUIFunction, UWidget::StaticClass()), TEXT("SetInputMode_GameAndUIEx should accept a widget focus target")));
		ASSERT_THAT(IsTrue(HasObjectParameter(SetInputModeGameOnlyFunction, PlayerControllerClass), TEXT("SetInputMode_GameOnly should accept APlayerController")));
		ASSERT_THAT(IsTrue(CountParameters(SetInputModeUIOnlyFunction) >= 3, TEXT("SetInputMode_UIOnlyEx should expose controller, focus widget and mouse lock parameters")));
		ASSERT_THAT(IsTrue(CountParameters(SetInputModeGameAndUIFunction) >= 4, TEXT("SetInputMode_GameAndUIEx should expose controller, focus widget, mouse lock and cursor capture parameters")));
		ASSERT_THAT(IsTrue(CountParameters(SetInputModeGameOnlyFunction) >= 1, TEXT("SetInputMode_GameOnly should expose at least the controller parameter")));
	}

	TEST_METHOD(BorderAndMarginStateRoundTrips)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UUserWidget* Widget = CreateWidgetFixture(*TestRunner, TEXT("CoverageWidgetBorderMarginFixture"));
		ASSERT_THAT(IsNotNull(Widget, TEXT("Widget border/margin fixture should be created")));
		if (Widget == nullptr)
		{
			return;
		}
		AngelscriptCoverageWidgetTest::FScopedRootedObject WidgetRoot(Widget);

		const FString ScriptBody = ASTEST_AS(R"AS(
			int BorderAndMarginOperations()
			{
				UBorder Border = Cast<UBorder>(MakeWidget(UBorder::StaticClass(), n"BorderProbe"));
				UTextBlock Content = Cast<UTextBlock>(MakeWidget(UTextBlock::StaticClass(), n"BorderContentProbe"));
				if (Border == null || Content == null)
				{
					return 0;
				}

				FMargin Padding = FMargin(4.0f, 5.0f, 6.0f, 7.0f);
				if (Padding.GetTotalSpaceAlongHorizontal() != 10.0f
					|| Padding.GetTotalSpaceAlongVertical() != 12.0f)
				{
					return 10;
				}

				FSlateBrush Brush = FSlateBrush(FLinearColor(0.05f, 0.15f, 0.25f, 1.0f));
				Brush.DrawAs = ESlateBrushDrawType::Border;
				Brush.Margin = Padding;
				Brush.TintColor = FSlateColor(FLinearColor(0.35f, 0.45f, 0.55f, 1.0f));
				Border.SetBrush(Brush);
				Border.SetBrushColor(FLinearColor(0.65f, 0.55f, 0.45f, 1.0f));
				Border.SetContentColorAndOpacity(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f));
				Border.SetPadding(Padding);
				Border.SetHorizontalAlignment(HAlign_Center);
				Border.SetVerticalAlignment(VAlign_Bottom);
				return Border.SetContent(Content) != null && Content.GetParent() == Border ? 1 : 20;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASCoverageWidget_BorderMargin"),
			BuildScript(*Widget, ScriptBody));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Border/FMargin state script should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const bool bExecuted = ExecuteAndExpectInt(
			*TestRunner,
			Engine,
			Module,
			TEXT("int BorderAndMarginOperations()"),
			TEXT("UBorder setters and FMargin operations should execute from script"),
			1);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Border/FMargin state case should execute")));
		if (!bExecuted)
		{
			return;
		}

		UBorder* BorderProbe = FindWidgetByName<UBorder>(*Widget, TEXT("BorderProbe"));
		UTextBlock* ContentProbe = FindWidgetByName<UTextBlock>(*Widget, TEXT("BorderContentProbe"));
		ASSERT_THAT(IsNotNull(BorderProbe, TEXT("BorderProbe should remain in the widget tree")));
		ASSERT_THAT(IsNotNull(ContentProbe, TEXT("BorderContentProbe should remain in the widget tree")));
		if (BorderProbe == nullptr || ContentProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ContentProbe->GetParent() == BorderProbe, TEXT("BorderContentProbe should keep script-assigned parent")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ESlateBrushDrawType::Border),
			static_cast<int32>(BorderProbe->Background.DrawAs.GetValue()),
			TEXT("BorderProbe should keep script-assigned brush draw type")));
		ASSERT_THAT(IsTrue(
			BorderProbe->Background.Margin == FMargin(4.0f, 5.0f, 6.0f, 7.0f),
			TEXT("BorderProbe should keep script-assigned brush margin")));
		ASSERT_THAT(IsTrue(
			BorderProbe->Background.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.35f, 0.45f, 0.55f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("BorderProbe should keep script-assigned brush tint")));
		ASSERT_THAT(IsTrue(
			BorderProbe->GetBrushColor().Equals(FLinearColor(0.65f, 0.55f, 0.45f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("BorderProbe should keep script-assigned brush color")));
		ASSERT_THAT(IsTrue(
			BorderProbe->GetContentColorAndOpacity().Equals(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f), KINDA_SMALL_NUMBER),
			TEXT("BorderProbe should keep script-assigned content color")));
		ASSERT_THAT(IsTrue(BorderProbe->GetPadding() == FMargin(4.0f, 5.0f, 6.0f, 7.0f), TEXT("BorderProbe should keep script-assigned padding")));
		ASSERT_THAT(AreEqual(static_cast<int32>(HAlign_Center), static_cast<int32>(BorderProbe->GetHorizontalAlignment()), TEXT("BorderProbe should keep script-assigned horizontal alignment")));
		ASSERT_THAT(AreEqual(static_cast<int32>(VAlign_Bottom), static_cast<int32>(BorderProbe->GetVerticalAlignment()), TEXT("BorderProbe should keep script-assigned vertical alignment")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
