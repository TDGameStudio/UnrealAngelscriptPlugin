#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (UAnimNotify script subclassing)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptAnimNotifyScriptTests,
	"Angelscript.TestModule.Functional.Animation.AnimNotifyScript",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SubclassRegistersUPropertyAndDerivesFromUAnimNotify)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalAnimNotify"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* NotifyClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalAnimNotify.as"),
			TEXT(R"AS(
UCLASS()
class UFunctionalAnimNotify_ScriptEffect : UAnimNotify
{
	UPROPERTY(EditAnywhere)
	FName EffectTag = n"Default";

	UPROPERTY(EditAnywhere)
	float EffectStrength = 1.0;
}
)AS"),
			TEXT("UFunctionalAnimNotify_ScriptEffect"));
		if (NotifyClass == nullptr) { return; }

		TestRunner->TestTrue(
			TEXT("UFunctionalAnimNotify_ScriptEffect should derive from UAnimNotify"),
			NotifyClass->IsChildOf(UAnimNotify::StaticClass()));

		FNameProperty* EffectTagProp = FindFProperty<FNameProperty>(NotifyClass, TEXT("EffectTag"));
		if (TestRunner->TestNotNull(TEXT("EffectTag FNameProperty should be registered"), EffectTagProp))
		{
			UObject* CDO = NotifyClass->GetDefaultObject();
			if (TestRunner->TestNotNull(TEXT("UFunctionalAnimNotify_ScriptEffect should have a valid CDO"), CDO))
			{
				TestRunner->TestEqual(
					TEXT("EffectTag CDO default should be the n\"Default\" literal"),
					EffectTagProp->GetPropertyValue_InContainer(CDO),
					FName(TEXT("Default")));
			}
		}

		// AngelscriptSettings::bScriptFloatIsFloat64 defaults to true, so AS 'float' lowers to FDoubleProperty.
		FDoubleProperty* EffectStrengthProp = FindFProperty<FDoubleProperty>(NotifyClass, TEXT("EffectStrength"));
		TestRunner->TestNotNull(TEXT("EffectStrength FDoubleProperty should be registered"), EffectStrengthProp);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
