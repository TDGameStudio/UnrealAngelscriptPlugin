#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (UMG BindWidget metadata)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBindWidgetTests,
	"Angelscript.TestModule.Functional.Widget.BindWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MetadataAndPropertyTypes)
	{
		using namespace AngelscriptFunctionalTestUtils;
		// Widget test stays on Full engine: compiling a UserWidget subclass that
		// overrides Construct/Tick triggers a hot-reload-style validation that
		// behaves differently against a shared engine with residual module
		// state (see Angelscript.TestModule.Functional.Widget.BindWidget
		// regression observed when this was briefly converted to
		// ASTEST_CREATE_ENGINE).
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalBindWidget"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* WidgetClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalBindWidget.as"),
			TEXT(R"AS(
UCLASS()
class UFunctionalScoreWidget : UUserWidget
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
			TEXT("UFunctionalScoreWidget"));
		if (WidgetClass == nullptr) { return; }

		TestRunner->TestTrue(
			TEXT("UFunctionalScoreWidget should derive from UUserWidget"),
			WidgetClass->IsChildOf(UUserWidget::StaticClass()));

		FObjectProperty* ScoreTextProp = FindFProperty<FObjectProperty>(WidgetClass, TEXT("ScoreText"));
		if (TestRunner->TestNotNull(TEXT("ScoreText FObjectProperty should be registered"), ScoreTextProp))
		{
			TestRunner->TestTrue(
				TEXT("ScoreText property class should be UTextBlock"),
				ScoreTextProp->PropertyClass != nullptr
				&& ScoreTextProp->PropertyClass->IsChildOf(UTextBlock::StaticClass()));
			TestRunner->TestTrue(
				TEXT("ScoreText should carry BindWidget metadata"),
				ScoreTextProp->HasMetaData(TEXT("BindWidget")));
		}

		FObjectProperty* HealthBarProp = FindFProperty<FObjectProperty>(WidgetClass, TEXT("HealthBar"));
		if (TestRunner->TestNotNull(TEXT("HealthBar FObjectProperty should be registered"), HealthBarProp))
		{
			TestRunner->TestTrue(
				TEXT("HealthBar property class should be UProgressBar"),
				HealthBarProp->PropertyClass != nullptr
				&& HealthBarProp->PropertyClass->IsChildOf(UProgressBar::StaticClass()));
			TestRunner->TestTrue(
				TEXT("HealthBar should carry BindWidget metadata"),
				HealthBarProp->HasMetaData(TEXT("BindWidget")));
		}

		FObjectProperty* RestartButtonProp = FindFProperty<FObjectProperty>(WidgetClass, TEXT("RestartButton"));
		if (TestRunner->TestNotNull(TEXT("RestartButton FObjectProperty should be registered"), RestartButtonProp))
		{
			TestRunner->TestTrue(
				TEXT("RestartButton property class should be UButton"),
				RestartButtonProp->PropertyClass != nullptr
				&& RestartButtonProp->PropertyClass->IsChildOf(UButton::StaticClass()));
			TestRunner->TestTrue(
				TEXT("RestartButton should carry BindWidget metadata"),
				RestartButtonProp->HasMetaData(TEXT("BindWidget")));
		}

		UFunction* ConstructFunction = WidgetClass->FindFunctionByName(TEXT("Construct"));
		UFunction* TickFunction = WidgetClass->FindFunctionByName(TEXT("Tick"));
		TestRunner->TestNotNull(TEXT("UserWidget Construct override should generate a UFunction"), ConstructFunction);
		if (TestRunner->TestNotNull(TEXT("UserWidget Tick override should generate a UFunction"), TickFunction))
		{
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
			TestRunner->TestTrue(TEXT("Tick should expose FGeometry parameter"), bHasGeometryParam);
			TestRunner->TestTrue(TEXT("Tick should expose float/double DeltaTime parameter"), bHasDeltaParam);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
