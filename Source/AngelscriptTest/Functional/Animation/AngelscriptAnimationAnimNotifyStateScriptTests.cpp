#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (UAnimNotifyState script subclassing)
#if WITH_ANGELSCRIPT_UNITTESTS


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

		ASSERT_THAT(IsTrue(
			StateClass->IsChildOf(UAnimNotifyState::StaticClass()),
			TEXT("UFunctionalAnimNotifyState_ScriptWindow should derive from UAnimNotifyState")));

		// AngelscriptSettings::bScriptFloatIsFloat64 defaults to true, so AS 'float' lowers to FDoubleProperty.
		FDoubleProperty* WindowStrengthProp = FindFProperty<FDoubleProperty>(StateClass, TEXT("WindowStrength"));
		ASSERT_THAT(IsNotNull(WindowStrengthProp, TEXT("WindowStrength FDoubleProperty should be registered")));

		FIntProperty* WindowPriorityProp = FindFProperty<FIntProperty>(StateClass, TEXT("WindowPriority"));
		ASSERT_THAT(IsNotNull(WindowPriorityProp, TEXT("WindowPriority FIntProperty should be registered")));

		FBoolProperty* FiresOnTickProp = FindFProperty<FBoolProperty>(StateClass, TEXT("bFiresOnTick"));
		if (this->Assert.IsNotNull(FiresOnTickProp, TEXT("bFiresOnTick FBoolProperty should be registered")))
		{
			UObject* CDO = StateClass->GetDefaultObject();
			if (this->Assert.IsNotNull(CDO, TEXT("UFunctionalAnimNotifyState_ScriptWindow should have a valid CDO")))
			{
				ASSERT_THAT(IsTrue(
					FiresOnTickProp->GetPropertyValue_InContainer(CDO),
					TEXT("bFiresOnTick CDO default should be true")));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
