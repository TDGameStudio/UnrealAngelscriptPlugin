#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (Property meta specifier matrix)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptPropertyMetaMatrixTests,
	"Angelscript.TestModule.Functional.Property.MetaSpecifiersMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MetaSpecifiersAreReflectedOnFProperty)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalPropertyMetaMatrix"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalPropertyMetaMatrix.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalPropertyMetaMatrixActor : AActor
{
	UPROPERTY(Category = "Coverage|Property")
	float CategorizedFloat = 0.0;

	UPROPERTY(NotEditable)
	bool bHiddenToggle = false;

	UPROPERTY(EditConst)
	bool bLockedToggle = false;

	UPROPERTY(BlueprintReadOnly)
	bool bBlueprintReadable = false;

	UPROPERTY(EditDefaultsOnly)
	int DefaultsOnlyValue = 0;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	bool bEnableHealth = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEnableHealth", ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float Health = 50.0;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEnableHealth", EditConditionHides))
	int32 HealthRegenLevel = 1;

	UPROPERTY(EditAnywhere, meta = (MakeEditWidget))
	FVector EditableLocation = FVector::ZeroVector;
}
)AS"),
			TEXT("AFunctionalPropertyMetaMatrixActor"));
		if (ActorClass == nullptr) { return; }

		auto VerifyMeta = [&](const TCHAR* PropertyName, const TCHAR* MetaKey, const TCHAR* ExpectedValue)
		{
			FProperty* Prop = ActorClass->FindPropertyByName(PropertyName);
			ASSERT_THAT(IsNotNull(Prop, FString::Printf(TEXT("%s should be registered"), PropertyName)));

			ASSERT_THAT(IsTrue(
				Prop->HasMetaData(MetaKey),
				FString::Printf(TEXT("%s should carry meta '%s'"), PropertyName, MetaKey)));

			if (ExpectedValue != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(ExpectedValue),
					Prop->GetMetaData(MetaKey),
					FString::Printf(TEXT("%s meta '%s' should match expected literal"), PropertyName, MetaKey)));
			}
		};

		VerifyMeta(TEXT("bEnableHealth"), TEXT("InlineEditConditionToggle"), nullptr);

		VerifyMeta(TEXT("Health"), TEXT("EditCondition"), TEXT("bEnableHealth"));
		VerifyMeta(TEXT("Health"), TEXT("ClampMin"), TEXT("0.0"));
		VerifyMeta(TEXT("Health"), TEXT("ClampMax"), TEXT("100.0"));
		VerifyMeta(TEXT("Health"), TEXT("UIMin"), TEXT("0.0"));
		VerifyMeta(TEXT("Health"), TEXT("UIMax"), TEXT("100.0"));

		VerifyMeta(TEXT("HealthRegenLevel"), TEXT("EditCondition"), TEXT("bEnableHealth"));
		VerifyMeta(TEXT("HealthRegenLevel"), TEXT("EditConditionHides"), nullptr);

		VerifyMeta(TEXT("EditableLocation"), TEXT("MakeEditWidget"), nullptr);

		VerifyMeta(TEXT("CategorizedFloat"), TEXT("Category"), TEXT("Coverage|Property"));

		FProperty* HiddenToggle = ActorClass->FindPropertyByName(TEXT("bHiddenToggle"));
		FProperty* LockedToggle = ActorClass->FindPropertyByName(TEXT("bLockedToggle"));
		FProperty* BlueprintReadable = ActorClass->FindPropertyByName(TEXT("bBlueprintReadable"));
		FProperty* DefaultsOnlyValue = ActorClass->FindPropertyByName(TEXT("DefaultsOnlyValue"));
		ASSERT_THAT(IsNotNull(HiddenToggle, TEXT("NotEditable property should be registered")));
		ASSERT_THAT(IsFalse(HiddenToggle->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable property should not carry CPF_Edit")));
		ASSERT_THAT(IsNotNull(LockedToggle, TEXT("EditConst property should be registered")));
		ASSERT_THAT(IsTrue(LockedToggle->HasAnyPropertyFlags(CPF_EditConst), TEXT("EditConst property should carry CPF_EditConst")));
		ASSERT_THAT(IsNotNull(BlueprintReadable, TEXT("BlueprintReadOnly property should be registered")));
		ASSERT_THAT(IsTrue(BlueprintReadable->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly property should be Blueprint-visible")));
		ASSERT_THAT(IsTrue(BlueprintReadable->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly property should carry CPF_BlueprintReadOnly")));
		ASSERT_THAT(IsNotNull(DefaultsOnlyValue, TEXT("EditDefaultsOnly property should be registered")));
		ASSERT_THAT(IsTrue(DefaultsOnlyValue->HasAnyPropertyFlags(CPF_DisableEditOnInstance), TEXT("EditDefaultsOnly property should disable instance editing")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
