#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadTests,
	"Angelscript.TestModule.HotReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool HotReloadPropertyPreserved(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TestHotReloadPropertyPreserved"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class ATestHotReloadPropertyPreserved : AActor
{
	UPROPERTY()
	int Counter = 0;

	UFUNCTION()
	int GetValue()
	{
		return Counter;
	}
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class ATestHotReloadPropertyPreserved : AActor
{
	UPROPERTY()
	int Counter = 0;

	UFUNCTION()
	int GetValue()
	{
		return Counter + 100;
	}
}
)AS");

	UClass* ClassV1 = AngelscriptFunctionalTestUtils::CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TestHotReloadPropertyPreserved.as"),
		ScriptV1,
		TEXT("ATestHotReloadPropertyPreserved"));
	if (ClassV1 == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeHotReloadTestCaseSpawner(Spawner);
	AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(Test, Spawner, ClassV1);
	if (Actor == nullptr)
	{
		return false;
	}
	AngelscriptFunctionalTestUtils::BeginPlayActor(*Actor);

	FIntProperty* CounterProperty = FindFProperty<FIntProperty>(ClassV1, TEXT("Counter"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload property should exist before reload"), CounterProperty))
	{
		return false;
	}
	CounterProperty->SetPropertyValue_InContainer(Actor, 42);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!Test.TestTrue(TEXT("TestCase hot-reload property-preserved compile should succeed on the soft reload path"),
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("TestHotReloadPropertyPreserved.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!Test.TestTrue(TEXT("TestCase hot-reload property-preserved should stay on the soft reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("ATestHotReloadPropertyPreserved"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload property-preserved class should exist after reload"), ClassAfterReload))
	{
		return false;
	}
	Test.TestEqual(TEXT("TestCase hot-reload property-preserved should keep the generated actor class instance"), ClassAfterReload, ClassV1);

	int32 CounterValue = 0;
	if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Actor, TEXT("Counter"), CounterValue))
	{
		return false;
	}
	Test.TestEqual(TEXT("TestCase hot-reload property-preserved should keep the actor property value after soft reload"), CounterValue, 42);

	UFunction* GetValueFunction = FindGeneratedFunction(ClassAfterReload, TEXT("GetValue"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload property-preserved function should still exist after reload"), GetValueFunction))
	{
		return false;
	}

	int32 Result = 0;
	if (!Test.TestTrue(TEXT("TestCase hot-reload property-preserved function should execute after reload"), ExecuteGeneratedIntEventOnGameThread(Actor, GetValueFunction, Result)))
	{
		return false;
	}
	Test.TestEqual(TEXT("TestCase hot-reload property-preserved function should observe the preserved property value after reload"), Result, 142);
	}

	return true;
}

static bool HotReloadAddProperty(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TestHotReloadAddProperty"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class ATestHotReloadAddProperty : AActor
{
	UPROPERTY()
	int ExistingValue = 1;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class ATestHotReloadAddProperty : AActor
{
	UPROPERTY()
	int ExistingValue = 1;

	UPROPERTY()
	int NewValue = 99;
}
)AS");

	UClass* ClassV1 = AngelscriptFunctionalTestUtils::CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TestHotReloadAddProperty.as"),
		ScriptV1,
		TEXT("ATestHotReloadAddProperty"));
	if (ClassV1 == nullptr)
	{
		return false;
	}

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!Test.TestTrue(TEXT("TestCase hot-reload add-property compile should succeed on the full reload path"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, TEXT("TestHotReloadAddProperty.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!Test.TestTrue(TEXT("TestCase hot-reload add-property should be handled by a full reload"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassV2 = FindGeneratedClass(&Engine, TEXT("ATestHotReloadAddProperty"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload add-property class should exist after reload"), ClassV2))
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeHotReloadTestCaseSpawner(Spawner);
	AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(Test, Spawner, ClassV2);
	if (Actor == nullptr)
	{
		return false;
	}
	AngelscriptFunctionalTestUtils::BeginPlayActor(*Actor);

	int32 ExistingValue = 0;
	if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Actor, TEXT("ExistingValue"), ExistingValue))
	{
		return false;
	}
	int32 NewValue = 0;
	if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Actor, TEXT("NewValue"), NewValue))
	{
		return false;
	}

	Test.TestEqual(TEXT("TestCase hot-reload add-property should preserve the original property default"), ExistingValue, 1);
	Test.TestEqual(TEXT("TestCase hot-reload add-property should expose the newly added property with its default value"), NewValue, 99);
	}

	return true;
}

static bool HotReloadFunctionChange(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TestHotReloadFunctionChange"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class ATestHotReloadFunctionChange : AActor
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class ATestHotReloadFunctionChange : AActor
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS");

	UClass* ClassV1 = AngelscriptFunctionalTestUtils::CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TestHotReloadFunctionChange.as"),
		ScriptV1,
		TEXT("ATestHotReloadFunctionChange"));
	if (ClassV1 == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeHotReloadTestCaseSpawner(Spawner);
	AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(Test, Spawner, ClassV1);
	if (Actor == nullptr)
	{
		return false;
	}
	AngelscriptFunctionalTestUtils::BeginPlayActor(*Actor);

	UFunction* GetValueBeforeReload = FindGeneratedFunction(ClassV1, TEXT("GetValue"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload function-change function should exist before reload"), GetValueBeforeReload))
	{
		return false;
	}

	int32 BeforeReloadResult = 0;
	if (!Test.TestTrue(TEXT("TestCase hot-reload function-change function should execute before reload"), ExecuteGeneratedIntEventOnGameThread(Actor, GetValueBeforeReload, BeforeReloadResult)))
	{
		return false;
	}
	Test.TestEqual(TEXT("TestCase hot-reload function-change should return the original value before reload"), BeforeReloadResult, 1);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!Test.TestTrue(TEXT("TestCase hot-reload function-change compile should succeed on the soft reload path"),
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("TestHotReloadFunctionChange.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!Test.TestTrue(TEXT("TestCase hot-reload function-change should stay on the soft reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("ATestHotReloadFunctionChange"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload function-change class should exist after reload"), ClassAfterReload))
	{
		return false;
	}

	UFunction* GetValueAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("GetValue"));
	if (!Test.TestNotNull(TEXT("TestCase hot-reload function-change function should exist after reload"), GetValueAfterReload))
	{
		return false;
	}

	int32 AfterReloadResult = 0;
	if (!Test.TestTrue(TEXT("TestCase hot-reload function-change function should execute after reload"), ExecuteGeneratedIntEventOnGameThread(Actor, GetValueAfterReload, AfterReloadResult)))
	{
		return false;
	}
	Test.TestEqual(TEXT("TestCase hot-reload function-change should expose the updated function body on the same actor instance"), AfterReloadResult, 2);
	}

	return true;
}

static bool HotReloadPIEStructuralChangeNeedsFullReload(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TestHotReloadPIEStructuralChange"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class ATestHotReloadPIEStructuralChange : AActor
{
	UPROPERTY()
	int Value = 1;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class ATestHotReloadPIEStructuralChange : AActor
{
	UPROPERTY()
	int Value = 1;

	UPROPERTY()
	int AddedValue = 2;
}
)AS");

	UClass* BaselineClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TestHotReloadPIEStructuralChange.as"),
		ScriptV1,
		TEXT("ATestHotReloadPIEStructuralChange"));
	if (BaselineClass == nullptr)
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(
		&Engine,
		ModuleName,
		TEXT("TestHotReloadPIEStructuralChange.as"),
		ScriptV2,
		ReloadRequirement,
		bWantsFullReload,
		bNeedsFullReload);
	if (!Test.TestTrue(TEXT("PIE structural hot-reload analysis should complete"), bAnalyzed))
	{
		return false;
	}

	Test.TestTrue(TEXT("Structural actor change should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	return Test.TestTrue(
		TEXT("Structural actor change should not stay on the soft reload path"),
		ReloadRequirement == FAngelscriptClassGenerator::FullReloadRequired
		|| ReloadRequirement == FAngelscriptClassGenerator::FullReloadSuggested);

	}
}

static void InitializeHotReloadTestCaseSpawner(FActorTestSpawner& Spawner)
{
	Spawner.InitializeGameSubsystems();
}

public:
	TEST_METHOD(PropertyPreserved)
	{
		ASSERT_THAT(IsTrue(HotReloadPropertyPreserved(*TestRunner)));
	}

	TEST_METHOD(AddProperty)
	{
		ASSERT_THAT(IsTrue(HotReloadAddProperty(*TestRunner)));
	}

	TEST_METHOD(FunctionChange)
	{
		ASSERT_THAT(IsTrue(HotReloadFunctionChange(*TestRunner)));
	}

	TEST_METHOD(PIEStructuralChangeNeedsFullReload)
	{
		ASSERT_THAT(IsTrue(HotReloadPIEStructuralChangeNeedsFullReload(*TestRunner)));
	}
};

#endif
