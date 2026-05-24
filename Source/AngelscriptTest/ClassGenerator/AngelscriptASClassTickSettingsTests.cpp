#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_ClassGenerator_AngelscriptASClassTickSettingsTests_Private
{
	static const FName ASClassTickSettingsModuleName(TEXT("ASClassTickSettings"));
	static const FString ASClassTickSettingsFilename(TEXT("ASClassTickSettings.as"));
	static const FName ASClassTickParentName(TEXT("AScriptTickParent"));
	static const FName ASClassTickChildName(TEXT("AScriptTickChild"));
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassTickSettingsTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(TickSettingsEnableChildTickWhenReceiveTickIsImplemented)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASClassTickSettingsTests_Private;
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
		if (!TestRunner->TestNotNull(TEXT("ASClass tick-settings test should resolve the generated parent UASClass"), ParentASClass)
			|| !TestRunner->TestNotNull(TEXT("ASClass tick-settings test should compile the child actor as a UASClass"), ChildASClass))
		{ return; }

		TestRunner->TestFalse(TEXT("ASClass tick-settings test should keep the parent class out of tick when it declares no tick overrides"), ParentASClass->bCanEverTick);
		TestRunner->TestTrue(TEXT("ASClass tick-settings test should enable tick on the child class when Tick is implemented"), ChildASClass->bCanEverTick);
		TestRunner->TestTrue(TEXT("ASClass tick-settings test should start the child class with tick enabled when Tick is implemented"), ChildASClass->bStartWithTickEnabled);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SpawnedChild = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		if (!TestRunner->TestNotNull(TEXT("ASClass tick-settings test should spawn a child actor instance"), SpawnedChild)) { return; }

		TestRunner->TestTrue(TEXT("ASClass tick-settings test should propagate bCanEverTick onto a spawned child actor"), SpawnedChild->PrimaryActorTick.bCanEverTick);
		TestRunner->TestTrue(TEXT("ASClass tick-settings test should propagate bStartWithTickEnabled onto a spawned child actor"), SpawnedChild->PrimaryActorTick.bStartWithTickEnabled);
		}
	}
};

#endif
