#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadTestCaseTests_Private
{
	using namespace AngelscriptFunctionalTestUtils;
void InitializeHotReloadTestCaseSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestHotReloadPropertyPreservedTest,
	"Angelscript.TestModule.HotReload.PropertyPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestHotReloadAddPropertyTest,
	"Angelscript.TestModule.HotReload.AddProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestHotReloadFunctionChangeTest,
	"Angelscript.TestModule.HotReload.FunctionChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTestHotReloadPIEStructuralChangeNeedsFullReloadTest,
	"Angelscript.TestModule.HotReload.PIEStructuralChangeNeedsFullReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTestHotReloadPropertyPreservedTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadTestCaseTests_Private;
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

	UClass* ClassV1 = CompileScriptModule(
		*this,
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
	AActor* Actor = SpawnScriptActor(*this, Spawner, ClassV1);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(*Actor);

	FIntProperty* CounterProperty = FindFProperty<FIntProperty>(ClassV1, TEXT("Counter"));
	if (!TestNotNull(TEXT("TestCase hot-reload property should exist before reload"), CounterProperty))
	{
		return false;
	}
	CounterProperty->SetPropertyValue_InContainer(Actor, 42);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("TestCase hot-reload property-preserved compile should succeed on the soft reload path"),
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("TestHotReloadPropertyPreserved.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!TestTrue(TEXT("TestCase hot-reload property-preserved should stay on the soft reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("ATestHotReloadPropertyPreserved"));
	if (!TestNotNull(TEXT("TestCase hot-reload property-preserved class should exist after reload"), ClassAfterReload))
	{
		return false;
	}
	TestEqual(TEXT("TestCase hot-reload property-preserved should keep the generated actor class instance"), ClassAfterReload, ClassV1);

	int32 CounterValue = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("Counter"), CounterValue))
	{
		return false;
	}
	TestEqual(TEXT("TestCase hot-reload property-preserved should keep the actor property value after soft reload"), CounterValue, 42);

	UFunction* GetValueFunction = FindGeneratedFunction(ClassAfterReload, TEXT("GetValue"));
	if (!TestNotNull(TEXT("TestCase hot-reload property-preserved function should still exist after reload"), GetValueFunction))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("TestCase hot-reload property-preserved function should execute after reload"), ExecuteGeneratedIntEventOnGameThread(Actor, GetValueFunction, Result)))
	{
		return false;
	}
	TestEqual(TEXT("TestCase hot-reload property-preserved function should observe the preserved property value after reload"), Result, 142);
	}

	return true;
}

bool FAngelscriptTestHotReloadAddPropertyTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadTestCaseTests_Private;
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

	UClass* ClassV1 = CompileScriptModule(
		*this,
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
	if (!TestTrue(TEXT("TestCase hot-reload add-property compile should succeed on the full reload path"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, TEXT("TestHotReloadAddProperty.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!TestTrue(TEXT("TestCase hot-reload add-property should be handled by a full reload"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassV2 = FindGeneratedClass(&Engine, TEXT("ATestHotReloadAddProperty"));
	if (!TestNotNull(TEXT("TestCase hot-reload add-property class should exist after reload"), ClassV2))
	{
		return false;
	}

	FActorTestSpawner Spawner;
	InitializeHotReloadTestCaseSpawner(Spawner);
	AActor* Actor = SpawnScriptActor(*this, Spawner, ClassV2);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(*Actor);

	int32 ExistingValue = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ExistingValue"), ExistingValue))
	{
		return false;
	}
	int32 NewValue = 0;
	if (!ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("NewValue"), NewValue))
	{
		return false;
	}

	TestEqual(TEXT("TestCase hot-reload add-property should preserve the original property default"), ExistingValue, 1);
	TestEqual(TEXT("TestCase hot-reload add-property should expose the newly added property with its default value"), NewValue, 99);
	}

	return true;
}

bool FAngelscriptTestHotReloadFunctionChangeTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadTestCaseTests_Private;
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

	UClass* ClassV1 = CompileScriptModule(
		*this,
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
	AActor* Actor = SpawnScriptActor(*this, Spawner, ClassV1);
	if (Actor == nullptr)
	{
		return false;
	}
	BeginPlayActor(*Actor);

	UFunction* GetValueBeforeReload = FindGeneratedFunction(ClassV1, TEXT("GetValue"));
	if (!TestNotNull(TEXT("TestCase hot-reload function-change function should exist before reload"), GetValueBeforeReload))
	{
		return false;
	}

	int32 BeforeReloadResult = 0;
	if (!TestTrue(TEXT("TestCase hot-reload function-change function should execute before reload"), ExecuteGeneratedIntEventOnGameThread(Actor, GetValueBeforeReload, BeforeReloadResult)))
	{
		return false;
	}
	TestEqual(TEXT("TestCase hot-reload function-change should return the original value before reload"), BeforeReloadResult, 1);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("TestCase hot-reload function-change compile should succeed on the soft reload path"),
		CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("TestHotReloadFunctionChange.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!TestTrue(TEXT("TestCase hot-reload function-change should stay on the soft reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("ATestHotReloadFunctionChange"));
	if (!TestNotNull(TEXT("TestCase hot-reload function-change class should exist after reload"), ClassAfterReload))
	{
		return false;
	}

	UFunction* GetValueAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("GetValue"));
	if (!TestNotNull(TEXT("TestCase hot-reload function-change function should exist after reload"), GetValueAfterReload))
	{
		return false;
	}

	int32 AfterReloadResult = 0;
	if (!TestTrue(TEXT("TestCase hot-reload function-change function should execute after reload"), ExecuteGeneratedIntEventOnGameThread(Actor, GetValueAfterReload, AfterReloadResult)))
	{
		return false;
	}
	TestEqual(TEXT("TestCase hot-reload function-change should expose the updated function body on the same actor instance"), AfterReloadResult, 2);
	}

	return true;
}

bool FAngelscriptTestHotReloadPIEStructuralChangeNeedsFullReloadTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadTestCaseTests_Private;
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

	UClass* BaselineClass = CompileScriptModule(
		*this,
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
	if (!TestTrue(TEXT("PIE structural hot-reload analysis should complete"), bAnalyzed))
	{
		return false;
	}

	TestTrue(TEXT("Structural actor change should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	return TestTrue(
		TEXT("Structural actor change should not stay on the soft reload path"),
		ReloadRequirement == FAngelscriptClassGenerator::FullReloadRequired
		|| ReloadRequirement == FAngelscriptClassGenerator::FullReloadSuggested);

	}
}

#endif
