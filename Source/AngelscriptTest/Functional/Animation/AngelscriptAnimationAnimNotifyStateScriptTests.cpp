#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (UAnimNotifyState script subclassing)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptAnimNotifyStateScriptTests,
	"Angelscript.TestModule.Functional.Animation.AnimNotifyStateScript",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SubclassRegistersUPropertyAndDerivesFromUAnimNotifyState)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalAnimNotifyState"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* StateClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalAnimNotifyState.as"),
			TEXT(R"AS(
UCLASS()
class UFunctionalAnimNotifyState_ScriptWindow : UAnimNotifyState
{
	UPROPERTY(EditAnywhere)
	float WindowStrength = 0.5;

	UPROPERTY(EditAnywhere)
	int32 WindowPriority = 3;

	UPROPERTY(EditAnywhere)
	bool bFiresOnTick = true;
}
)AS"),
			TEXT("UFunctionalAnimNotifyState_ScriptWindow"));
		if (StateClass == nullptr) { return; }

		TestRunner->TestTrue(
			TEXT("UFunctionalAnimNotifyState_ScriptWindow should derive from UAnimNotifyState"),
			StateClass->IsChildOf(UAnimNotifyState::StaticClass()));

		// AngelscriptSettings::bScriptFloatIsFloat64 defaults to true, so AS 'float' lowers to FDoubleProperty.
		FDoubleProperty* WindowStrengthProp = FindFProperty<FDoubleProperty>(StateClass, TEXT("WindowStrength"));
		TestRunner->TestNotNull(TEXT("WindowStrength FDoubleProperty should be registered"), WindowStrengthProp);

		FIntProperty* WindowPriorityProp = FindFProperty<FIntProperty>(StateClass, TEXT("WindowPriority"));
		TestRunner->TestNotNull(TEXT("WindowPriority FIntProperty should be registered"), WindowPriorityProp);

		FBoolProperty* FiresOnTickProp = FindFProperty<FBoolProperty>(StateClass, TEXT("bFiresOnTick"));
		if (TestRunner->TestNotNull(TEXT("bFiresOnTick FBoolProperty should be registered"), FiresOnTickProp))
		{
			UObject* CDO = StateClass->GetDefaultObject();
			if (TestRunner->TestNotNull(TEXT("UFunctionalAnimNotifyState_ScriptWindow should have a valid CDO"), CDO))
			{
				TestRunner->TestTrue(
					TEXT("bFiresOnTick CDO default should be true"),
					FiresOnTickProp->GetPropertyValue_InContainer(CDO));
			}
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
