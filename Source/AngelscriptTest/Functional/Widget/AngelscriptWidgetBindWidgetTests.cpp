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

		ASSERT_THAT(IsTrue(
			WidgetClass->IsChildOf(UUserWidget::StaticClass()),
			TEXT("UFunctionalScoreWidget should derive from UUserWidget")));

		FObjectProperty* ScoreTextProp = FindFProperty<FObjectProperty>(WidgetClass, TEXT("ScoreText"));
		ASSERT_THAT(IsNotNull(ScoreTextProp, TEXT("ScoreText FObjectProperty should be registered")));
		ASSERT_THAT(IsTrue(
			ScoreTextProp->PropertyClass != nullptr
			&& ScoreTextProp->PropertyClass->IsChildOf(UTextBlock::StaticClass()),
			TEXT("ScoreText property class should be UTextBlock")));
		ASSERT_THAT(IsTrue(
			ScoreTextProp->HasMetaData(TEXT("BindWidget")),
			TEXT("ScoreText should carry BindWidget metadata")));

		FObjectProperty* HealthBarProp = FindFProperty<FObjectProperty>(WidgetClass, TEXT("HealthBar"));
		ASSERT_THAT(IsNotNull(HealthBarProp, TEXT("HealthBar FObjectProperty should be registered")));
		ASSERT_THAT(IsTrue(
			HealthBarProp->PropertyClass != nullptr
			&& HealthBarProp->PropertyClass->IsChildOf(UProgressBar::StaticClass()),
			TEXT("HealthBar property class should be UProgressBar")));
		ASSERT_THAT(IsTrue(
			HealthBarProp->HasMetaData(TEXT("BindWidget")),
			TEXT("HealthBar should carry BindWidget metadata")));

		FObjectProperty* RestartButtonProp = FindFProperty<FObjectProperty>(WidgetClass, TEXT("RestartButton"));
		ASSERT_THAT(IsNotNull(RestartButtonProp, TEXT("RestartButton FObjectProperty should be registered")));
		ASSERT_THAT(IsTrue(
			RestartButtonProp->PropertyClass != nullptr
			&& RestartButtonProp->PropertyClass->IsChildOf(UButton::StaticClass()),
			TEXT("RestartButton property class should be UButton")));
		ASSERT_THAT(IsTrue(
			RestartButtonProp->HasMetaData(TEXT("BindWidget")),
			TEXT("RestartButton should carry BindWidget metadata")));

		UFunction* ConstructFunction = WidgetClass->FindFunctionByName(TEXT("Construct"));
		UFunction* TickFunction = WidgetClass->FindFunctionByName(TEXT("Tick"));
		ASSERT_THAT(IsNotNull(ConstructFunction, TEXT("UserWidget Construct override should generate a UFunction")));
		ASSERT_THAT(IsNotNull(TickFunction, TEXT("UserWidget Tick override should generate a UFunction")));
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
		ASSERT_THAT(IsTrue(bHasGeometryParam, TEXT("Tick should expose FGeometry parameter")));
		ASSERT_THAT(IsTrue(bHasDeltaParam, TEXT("Tick should expose float/double DeltaTime parameter")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
