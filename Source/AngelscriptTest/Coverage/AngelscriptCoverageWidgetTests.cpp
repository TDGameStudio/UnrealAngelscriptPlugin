#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
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
};

#endif // WITH_DEV_AUTOMATION_TESTS
