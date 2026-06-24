#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassTickSettingsTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName ASClassTickSettingsModuleName = FName(TEXT("ASClassTickSettings"));
inline static const FString ASClassTickSettingsFilename = FString(TEXT("ASClassTickSettings.as"));
inline static const FName ASClassTickParentName = FName(TEXT("AScriptTickParent"));
inline static const FName ASClassTickChildName = FName(TEXT("AScriptTickChild"));

public:
	TEST_METHOD(TickSettingsEnableChildTickWhenReceiveTickIsImplemented)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassTickSettingsModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		// UE 5.7: AActor::ReceiveTick is no longer a BlueprintImplementableEvent.
		// Use Tick (the AngelScript-idiomatic name) instead.
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class AScriptTickParent : AActor
{
}

UCLASS()
class AScriptTickChild : AScriptTickParent
{
	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
	}
}
)AS");

		UClass* ChildClass = CompileScriptModule(
			*TestRunner, Engine, ASClassTickSettingsModuleName, ASClassTickSettingsFilename, ScriptSource, ASClassTickChildName);
		if (ChildClass == nullptr) { return; }

		UASClass* ParentASClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassTickParentName));
		UASClass* ChildASClass = Cast<UASClass>(ChildClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("ASClass tick-settings test should resolve the generated parent UASClass")));
		ASSERT_THAT(IsNotNull(ChildASClass, TEXT("ASClass tick-settings test should compile the child actor as a UASClass")));

		ASSERT_THAT(IsFalse(ParentASClass->bCanEverTick, TEXT("ASClass tick-settings test should keep the parent class out of tick when it declares no tick overrides")));
		ASSERT_THAT(IsTrue(ChildASClass->bCanEverTick, TEXT("ASClass tick-settings test should enable tick on the child class when Tick is implemented")));
		ASSERT_THAT(IsTrue(ChildASClass->bStartWithTickEnabled, TEXT("ASClass tick-settings test should start the child class with tick enabled when Tick is implemented")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SpawnedChild = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(SpawnedChild, TEXT("ASClass tick-settings test should spawn a child actor instance")));

		ASSERT_THAT(IsTrue(SpawnedChild->PrimaryActorTick.bCanEverTick, TEXT("ASClass tick-settings test should propagate bCanEverTick onto a spawned child actor")));
		ASSERT_THAT(IsTrue(SpawnedChild->PrimaryActorTick.bStartWithTickEnabled, TEXT("ASClass tick-settings test should propagate bStartWithTickEnabled onto a spawned child actor")));
		}
	}
};

#endif
