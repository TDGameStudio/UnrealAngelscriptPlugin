#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/ScopeExit.h"
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
		UClass* WidgetClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			Source,
			ClassName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should compile"), ClassName), WidgetClass))
		{
			return nullptr;
		}
		Test.TestTrue(*FString::Printf(TEXT("%s should derive from UUserWidget"), ClassName),
			WidgetClass->IsChildOf(UUserWidget::StaticClass()));
		return WidgetClass;
	}

	UUserWidget* CreateRuntimeWidget(FAutomationTestBase& Test, UClass* WidgetClass)
	{
		UUserWidget* Widget = NewObject<UUserWidget>(
			GetTransientPackage(),
			WidgetClass,
			RuntimeWidgetName,
			RF_Transient);
		if (!Test.TestNotNull(TEXT("runtime UUserWidget fixture should be created"), Widget))
		{
			return nullptr;
		}

		UWidgetTree* WidgetTree = NewObject<UWidgetTree>(Widget, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transient);
		if (!Test.TestNotNull(TEXT("runtime UUserWidget fixture should create WidgetTree"), WidgetTree))
		{
			return nullptr;
		}
		Widget->WidgetTree = WidgetTree;
		return Widget;
	}

	FString BuildWidgetTreeScript(const UUserWidget& Widget)
	{
		FString Script = TEXT(R"AS(
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageWidgetTest,
	"Angelscript.TestModule.Coverage.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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
			TEXT(R"AS(
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
		ASSERT_THAT(IsTrue(ScoreTextProperty->PropertyClass != nullptr
			&& ScoreTextProperty->PropertyClass->IsChildOf(UTextBlock::StaticClass()),
			TEXT("ScoreText should be typed as UTextBlock")));
		ASSERT_THAT(IsTrue(ScoreTextProperty->HasMetaData(TEXT("BindWidget")),
			TEXT("ScoreText should carry BindWidget metadata")));

		FObjectProperty* HealthBarProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("HealthBar"));
		ASSERT_THAT(IsNotNull(HealthBarProperty, TEXT("HealthBar should be reflected")));
		ASSERT_THAT(IsTrue(HealthBarProperty->PropertyClass != nullptr
			&& HealthBarProperty->PropertyClass->IsChildOf(UProgressBar::StaticClass()),
			TEXT("HealthBar should be typed as UProgressBar")));
		ASSERT_THAT(IsTrue(HealthBarProperty->HasMetaData(TEXT("BindWidget")),
			TEXT("HealthBar should carry BindWidget metadata")));

		FObjectProperty* RestartButtonProperty = FindFProperty<FObjectProperty>(WidgetClass, TEXT("RestartButton"));
		ASSERT_THAT(IsNotNull(RestartButtonProperty, TEXT("RestartButton should be reflected")));
		ASSERT_THAT(IsTrue(RestartButtonProperty->PropertyClass != nullptr
			&& RestartButtonProperty->PropertyClass->IsChildOf(UButton::StaticClass()),
			TEXT("RestartButton should be typed as UButton")));
		ASSERT_THAT(IsTrue(RestartButtonProperty->HasMetaData(TEXT("BindWidget")),
			TEXT("RestartButton should carry BindWidget metadata")));

		UFunction* ConstructFunction = WidgetClass->FindFunctionByName(TEXT("Construct"));
		UFunction* TickFunction = WidgetClass->FindFunctionByName(TEXT("Tick"));
		ASSERT_THAT(IsNotNull(ConstructFunction, TEXT("Construct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(TickFunction, TEXT("Tick override should generate a UFunction")));

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
			TEXT(R"AS(
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
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FixtureResolves()"), TEXT("FindObject should resolve transient widget fixture"), 1 },
			{ TEXT("int InitialRootIsNull()"), TEXT("runtime widget should start without root"), 1 },
			{ TEXT("int ConstructTextRoot()"), TEXT("ConstructWidget should create a UTextBlock root candidate"), 1 },
			{ TEXT("int RootRoundTripAndEnumeration()"), TEXT("SetRootWidget/GetRootWidget/GetAllWidgets should round-trip root"), 1 },
			{ TEXT("int RemoveClearsRootAndTree()"), TEXT("RemoveWidget should clear root and tree enumeration"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, Module, Cases);

		TArray<UWidget*> Widgets;
		Widget->WidgetTree->GetAllWidgets(Widgets);
		ASSERT_THAT(IsNull(Widget->GetRootWidget(), TEXT("native postcondition should have no root widget")));
		ASSERT_THAT(AreEqual(0, Widgets.Num(), TEXT("native postcondition should have empty WidgetTree")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
