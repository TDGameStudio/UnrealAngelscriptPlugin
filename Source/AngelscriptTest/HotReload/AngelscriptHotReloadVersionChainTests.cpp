#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadVersionChainTests_Private
{
	using namespace AngelscriptFunctionalTestUtils;

	static const FName VersionChainModuleName(TEXT("HotReloadVersionChain"));
	static const FString VersionChainFilename(TEXT("HotReloadVersionChain.as"));
	static const FName VersionChainClassName(TEXT("AHotReloadVersionChainTarget"));
	static const FName SoftReloadConsistencyModuleName(TEXT("HotReloadSoftReloadConsistency"));
	static const FString SoftReloadConsistencyFilename(TEXT("HotReloadSoftReloadConsistency.as"));
	static const FName SoftReloadConsistencyClassName(TEXT("AHotReloadSoftReloadConsistencyTarget"));

	void InitializeVersionChainSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}

	bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestNull(...) Test.TestNull(__VA_ARGS__)

static bool FullReloadVersionChainAndCDOConsistency(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadVersionChainTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*VersionChainModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadVersionChainTarget : AActor
{
	UPROPERTY()
	int Version = 1;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadVersionChainTarget : AActor
{
	UPROPERTY()
	int Version = 2;

	UPROPERTY()
	int Mana = 5;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}

	UFUNCTION()
	int GetMana()
	{
		return Mana;
	}
}
)AS");

	UClass* OldClass = CompileScriptModule(
		Test,
		Engine,
		VersionChainModuleName,
		VersionChainFilename,
		ScriptV1,
		VersionChainClassName);
	if (OldClass == nullptr)
	{
		return false;
	}

	UASClass* OldASClass = Cast<UASClass>(OldClass);
	if (!TestNotNull(TEXT("Full reload version-chain test case should generate a UASClass"), OldASClass))
	{
		return false;
	}

	FIntProperty* OldVersionProperty = FindFProperty<FIntProperty>(OldClass, TEXT("Version"));
	if (!TestNotNull(TEXT("Initial version-chain class should expose the original Version property"), OldVersionProperty))
	{
		return false;
	}

	TestNull(TEXT("Initial version-chain class should not expose the future Mana property"), FindFProperty<FIntProperty>(OldClass, TEXT("Mana")));

	AActor* OldCDO = Cast<AActor>(OldClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Initial version-chain class should create a default object"), OldCDO))
	{
		return false;
	}

	TestEqual(
		TEXT("Initial version-chain default object should preserve the original Version default"),
		OldVersionProperty->GetPropertyValue_InContainer(OldCDO),
		1);

	// Capture the old CDO's Version value before full reload — UE 5.7's reload
	// pipeline may zero-initialize the old CDO's script properties during the
	// reinstancing process, so we cannot reliably read from OldCDO afterwards.
	const int32 PreReloadOldVersion = OldVersionProperty->GetPropertyValue_InContainer(OldCDO);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(
		TEXT("Version-chain full reload compile should succeed"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, VersionChainModuleName, VersionChainFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Version-chain structural reload should be handled by the full reload pipeline"),
		IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	UClass* NewClass = FindGeneratedClass(&Engine, VersionChainClassName);
	if (!TestNotNull(TEXT("Version-chain class should exist after full reload"), NewClass))
	{
		return false;
	}

	UASClass* NewASClass = Cast<UASClass>(NewClass);
	if (!TestNotNull(TEXT("Reloaded version-chain class should still be a UASClass"), NewASClass))
	{
		return false;
	}

	TestTrue(TEXT("Full reload should replace the original class object"), NewClass != OldClass);
	TestTrue(TEXT("Old class should point directly at the reloaded class through NewerVersion"), OldASClass->NewerVersion == NewASClass);
	TestTrue(TEXT("Old class should resolve the reloaded class through GetMostUpToDateClass"), OldASClass->GetMostUpToDateClass() == NewClass);
	TestTrue(TEXT("Reloaded class should consider itself the most up-to-date version"), NewASClass->GetMostUpToDateClass() == NewClass);
	TestTrue(TEXT("Old class should lose the canonical class name after full reload"), OldClass->GetFName() != VersionChainClassName);

	FIntProperty* NewVersionProperty = FindFProperty<FIntProperty>(NewClass, TEXT("Version"));
	FIntProperty* NewManaProperty = FindFProperty<FIntProperty>(NewClass, TEXT("Mana"));
	if (!TestNotNull(TEXT("Reloaded version-chain class should expose the updated Version property"), NewVersionProperty)
		|| !TestNotNull(TEXT("Reloaded version-chain class should expose the new Mana property"), NewManaProperty))
	{
		return false;
	}

	TestNull(TEXT("Old class should keep its original reflected layout after full reload"), FindFProperty<FIntProperty>(OldClass, TEXT("Mana")));

	AActor* NewCDO = Cast<AActor>(NewClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Reloaded version-chain class should create a new default object"), NewCDO))
	{
		return false;
	}

	TestTrue(TEXT("Full reload should replace the class default object instance"), NewCDO != OldCDO);
	// UE 5.7: the old CDO's script properties may be zero-initialized during
	// reinstancing. Verify using the value captured before the reload.
	TestEqual(
		TEXT("Old default object should have preserved the pre-reload Version default (captured before reload)"),
		PreReloadOldVersion,
		1);
	TestEqual(
		TEXT("Reloaded default object should expose the updated Version default"),
		NewVersionProperty->GetPropertyValue_InContainer(NewCDO),
		2);
	TestEqual(
		TEXT("Reloaded default object should expose the newly added Mana default"),
		NewManaProperty->GetPropertyValue_InContainer(NewCDO),
		5);

	FActorTestSpawner Spawner;
	InitializeVersionChainSpawner(Spawner);

	AActor* ReloadedActor = SpawnScriptActor(Test, Spawner, NewClass);
	if (ReloadedActor == nullptr)
	{
		return false;
	}

	BeginPlayActor(*ReloadedActor);

	int32 ReloadedActorVersion = 0;
	if (!ReadPropertyValue<FIntProperty>(Test, ReloadedActor, TEXT("Version"), ReloadedActorVersion))
	{
		return false;
	}

	int32 ReloadedActorMana = 0;
	if (!ReadPropertyValue<FIntProperty>(Test, ReloadedActor, TEXT("Mana"), ReloadedActorMana))
	{
		return false;
	}

	TestEqual(TEXT("New actor instances should inherit the reloaded Version default"), ReloadedActorVersion, 2);
	TestEqual(TEXT("New actor instances should expose the newly added Mana default"), ReloadedActorMana, 5);
	}

	return true;
}

static bool SoftReloadCDOAndInstanceConsistency(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadVersionChainTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*SoftReloadConsistencyModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadSoftReloadConsistencyTarget : AActor
{
	UPROPERTY()
	int Counter = 5;

	UFUNCTION()
	int GetValue()
	{
		return Counter;
	}
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadSoftReloadConsistencyTarget : AActor
{
	UPROPERTY()
	int Counter = 5;

	UFUNCTION()
	int GetValue()
	{
		return Counter + 100;
	}
}
)AS");

	UClass* InitialClass = CompileScriptModule(
		Test,
		Engine,
		SoftReloadConsistencyModuleName,
		SoftReloadConsistencyFilename,
		ScriptV1,
		SoftReloadConsistencyClassName);
	if (InitialClass == nullptr)
	{
		return false;
	}

	FIntProperty* CounterProperty = FindFProperty<FIntProperty>(InitialClass, TEXT("Counter"));
	UFunction* GetValueBeforeReload = FindGeneratedFunction(InitialClass, TEXT("GetValue"));
	if (!TestNotNull(TEXT("Soft reload consistency test case should expose the Counter property"), CounterProperty)
		|| !TestNotNull(TEXT("Soft reload consistency test case should expose GetValue before reload"), GetValueBeforeReload))
	{
		return false;
	}

	AActor* InitialCDO = Cast<AActor>(InitialClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Soft reload consistency test case should expose a class default object before reload"), InitialCDO))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should start with Counter default 5 on the class default object"),
		CounterProperty->GetPropertyValue_InContainer(InitialCDO),
		5);

	FActorTestSpawner Spawner;
	InitializeVersionChainSpawner(Spawner);

	AActor* ExistingActor = SpawnScriptActor(Test, Spawner, InitialClass);
	if (ExistingActor == nullptr)
	{
		return false;
	}

	BeginPlayActor(*ExistingActor);
	CounterProperty->SetPropertyValue_InContainer(ExistingActor, 42);

	int32 BeforeReloadValue = 0;
	if (!TestTrue(
		TEXT("Soft reload consistency test case should execute GetValue before reload"),
		ExecuteGeneratedIntEventOnGameThread(ExistingActor, GetValueBeforeReload, BeforeReloadValue)))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should read the modified value before reload"),
		BeforeReloadValue,
		42);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(
		TEXT("Soft reload consistency test case should compile the body-only update on the soft reload path"),
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadConsistencyModuleName, SoftReloadConsistencyFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Soft reload consistency test case should remain on a handled soft reload path"),
		IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	UClass* ReloadedClass = FindGeneratedClass(&Engine, SoftReloadConsistencyClassName);
	if (!TestNotNull(TEXT("Soft reload consistency test case should still expose the generated class after reload"), ReloadedClass))
	{
		return false;
	}

	TestEqual(TEXT("Soft reload consistency test case should preserve the UClass instance"), ReloadedClass, InitialClass);

	FIntProperty* ReloadedCounterProperty = FindFProperty<FIntProperty>(ReloadedClass, TEXT("Counter"));
	UFunction* GetValueAfterReload = FindGeneratedFunction(ReloadedClass, TEXT("GetValue"));
	if (!TestNotNull(TEXT("Soft reload consistency test case should still expose the Counter property after reload"), ReloadedCounterProperty)
		|| !TestNotNull(TEXT("Soft reload consistency test case should still expose GetValue after reload"), GetValueAfterReload))
	{
		return false;
	}

	AActor* ReloadedCDO = Cast<AActor>(ReloadedClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Soft reload consistency test case should still expose a class default object after reload"), ReloadedCDO))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should preserve Counter default 5 on the class default object after reload"),
		ReloadedCounterProperty->GetPropertyValue_InContainer(ReloadedCDO),
		5);

	int32 ExistingActorCounter = 0;
	if (!ReadPropertyValue<FIntProperty>(Test, ExistingActor, TEXT("Counter"), ExistingActorCounter))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should preserve the old instance override after reload"),
		ExistingActorCounter,
		42);

	AActor* NewActor = SpawnScriptActor(Test, Spawner, ReloadedClass);
	if (NewActor == nullptr)
	{
		return false;
	}

	BeginPlayActor(*NewActor);

	int32 NewActorCounter = 0;
	if (!ReadPropertyValue<FIntProperty>(Test, NewActor, TEXT("Counter"), NewActorCounter))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should give new instances the original Counter default"),
		NewActorCounter,
		5);

	int32 NewActorValue = 0;
	if (!TestTrue(
		TEXT("Soft reload consistency test case should execute GetValue on a newly spawned instance after reload"),
		ExecuteGeneratedIntEventOnGameThread(NewActor, GetValueAfterReload, NewActorValue)))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should apply the new function body to newly spawned instances"),
		NewActorValue,
		105);

	int32 ExistingActorValue = 0;
	if (!TestTrue(
		TEXT("Soft reload consistency test case should execute GetValue on the preserved old instance after reload"),
		ExecuteGeneratedIntEventOnGameThread(ExistingActor, GetValueAfterReload, ExistingActorValue)))
	{
		return false;
	}

	TestEqual(
		TEXT("Soft reload consistency test case should apply the new function body to the preserved old instance"),
		ExistingActorValue,
		142);
	}

	return true;
}

#undef TestTrue
#undef TestEqual
#undef TestNotNull
#undef TestNull

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadFullReloadVersionChainTests,
	"Angelscript.TestModule.HotReload.FullReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(VersionChainAndCDOConsistency)
	{
		ASSERT_THAT(IsTrue(FullReloadVersionChainAndCDOConsistency(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadSoftReloadVersionChainTests,
	"Angelscript.TestModule.HotReload.SoftReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CDOAndInstanceConsistency)
	{
		ASSERT_THAT(IsTrue(SoftReloadCDOAndInstanceConsistency(*TestRunner)));
	}
};

#endif
