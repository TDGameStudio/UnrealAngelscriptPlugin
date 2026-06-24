#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadLifecycleTests,
	"Angelscript.TestModule.HotReload.SoftReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool DoesNotReplayBeginPlayOnLiveActor(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*LifecycleModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadLifecycleTarget : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int PersistentCounter = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}

	UFUNCTION()
	int GetValue()
	{
		return PersistentCounter;
	}
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadLifecycleTarget : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int PersistentCounter = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}

	UFUNCTION()
	int GetValue()
	{
		return PersistentCounter + 1;
	}
}
)AS");

	UClass* InitialClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
		Test,
		Engine,
		LifecycleModuleName,
		LifecycleFilename,
		ScriptV1,
		LifecycleClassName);
	if (InitialClass == nullptr)
	{
		return false;
	}

	FIntProperty* BeginPlayCountProperty = FindFProperty<FIntProperty>(InitialClass, TEXT("BeginPlayCount"));
	FIntProperty* PersistentCounterProperty = FindFProperty<FIntProperty>(InitialClass, TEXT("PersistentCounter"));
	UFunction* GetValueBeforeReload = FindGeneratedFunction(InitialClass, TEXT("GetValue"));
	if (!Test.TestNotNull(TEXT("Soft reload lifecycle test case should expose BeginPlayCount before reload"), BeginPlayCountProperty)
		|| !Test.TestNotNull(TEXT("Soft reload lifecycle test case should expose PersistentCounter before reload"), PersistentCounterProperty)
		|| !Test.TestNotNull(TEXT("Soft reload lifecycle test case should expose GetValue before reload"), GetValueBeforeReload))
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeLifecycleSpawner(Spawner);

	AActor* ExistingActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(Test, Spawner, InitialClass);
	if (ExistingActor == nullptr)
	{
		return false;
	}

	AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *ExistingActor);

	int32 BeginPlayCountBeforeReload = 0;
	if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, ExistingActor, TEXT("BeginPlayCount"), BeginPlayCountBeforeReload))
	{
		return false;
	}

	Test.TestEqual(
		TEXT("Soft reload lifecycle test case should invoke BeginPlay exactly once before reload"),
		BeginPlayCountBeforeReload,
		1);

	PersistentCounterProperty->SetPropertyValue_InContainer(ExistingActor, 41);

	int32 ValueBeforeReload = 0;
	if (!Test.TestTrue(
		TEXT("Soft reload lifecycle test case should execute GetValue before reload"),
		ExecuteGeneratedIntEventOnGameThread(ExistingActor, GetValueBeforeReload, ValueBeforeReload)))
	{
		return false;
	}

	Test.TestEqual(
		TEXT("Soft reload lifecycle test case should observe the modified runtime state before reload"),
		ValueBeforeReload,
		41);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!Test.TestTrue(
		TEXT("Soft reload lifecycle test case should compile the body-only update on the soft reload path"),
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, LifecycleModuleName, LifecycleFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!Test.TestTrue(
		TEXT("Soft reload lifecycle test case should remain on a handled soft reload path"),
		IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	UClass* ReloadedClass = FindGeneratedClass(&Engine, LifecycleClassName);
	if (!Test.TestNotNull(TEXT("Soft reload lifecycle test case should still expose the generated class after reload"), ReloadedClass))
	{
		return false;
	}

	Test.TestEqual(TEXT("Soft reload lifecycle test case should preserve the live UClass object"), ReloadedClass, InitialClass);
	Test.TestEqual(TEXT("Soft reload lifecycle test case should keep the live actor on the preserved class"), ExistingActor->GetClass(), ReloadedClass);
	Test.TestTrue(TEXT("Soft reload lifecycle test case should keep the actor in begun-play state"), ExistingActor->HasActorBegunPlay());

	FIntProperty* ReloadedBeginPlayCountProperty = FindFProperty<FIntProperty>(ReloadedClass, TEXT("BeginPlayCount"));
	FIntProperty* ReloadedPersistentCounterProperty = FindFProperty<FIntProperty>(ReloadedClass, TEXT("PersistentCounter"));
	UFunction* GetValueAfterReload = FindGeneratedFunction(ReloadedClass, TEXT("GetValue"));
	if (!Test.TestNotNull(TEXT("Soft reload lifecycle test case should still expose BeginPlayCount after reload"), ReloadedBeginPlayCountProperty)
		|| !Test.TestNotNull(TEXT("Soft reload lifecycle test case should still expose PersistentCounter after reload"), ReloadedPersistentCounterProperty)
		|| !Test.TestNotNull(TEXT("Soft reload lifecycle test case should still expose GetValue after reload"), GetValueAfterReload))
	{
		return false;
	}

	int32 BeginPlayCountAfterReload = 0;
	if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, ExistingActor, TEXT("BeginPlayCount"), BeginPlayCountAfterReload))
	{
		return false;
	}

	Test.TestEqual(
		TEXT("Soft reload lifecycle test case should not replay BeginPlay on the live actor after reload"),
		BeginPlayCountAfterReload,
		1);

	int32 PersistentCounterAfterReload = 0;
	if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, ExistingActor, TEXT("PersistentCounter"), PersistentCounterAfterReload))
	{
		return false;
	}

	Test.TestEqual(
		TEXT("Soft reload lifecycle test case should preserve the live actor runtime state after reload"),
		PersistentCounterAfterReload,
		41);

	int32 ValueAfterReload = 0;
	if (!Test.TestTrue(
		TEXT("Soft reload lifecycle test case should execute GetValue on the live actor after reload"),
		ExecuteGeneratedIntEventOnGameThread(ExistingActor, GetValueAfterReload, ValueAfterReload)))
	{
		return false;
	}

	Test.TestEqual(
		TEXT("Soft reload lifecycle test case should apply the updated function body without resetting state"),
		ValueAfterReload,
		42);
	}

	return true;
}

inline static const FName LifecycleModuleName = FName(TEXT("HotReloadLifecycle"));
inline static const FString LifecycleFilename = FString(TEXT("HotReloadLifecycle.as"));
inline static const FName LifecycleClassName = FName(TEXT("AHotReloadLifecycleTarget"));

static void InitializeLifecycleSpawner(FActorTestSpawner& Spawner)
{
	Spawner.InitializeGameSubsystems();
}

static bool IsHandledReloadResult(const ECompileResult ReloadResult)
{
	return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
}

public:
	TEST_METHOD(DoesNotReplayBeginPlayOnLiveActor)
	{
		ASSERT_THAT(IsTrue(DoesNotReplayBeginPlayOnLiveActor(*TestRunner)));
	}
};

#endif
