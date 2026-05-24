#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private
{
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptModuleRecordTrackingTest,
	"Angelscript.TestModule.HotReload.ModuleRecordTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDiscardModuleTest,
	"Angelscript.TestModule.HotReload.DiscardModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDiscardAndRecompileTest,
	"Angelscript.TestModule.HotReload.DiscardAndRecompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptModuleWatcherQueuesFileChangesTest,
	"Angelscript.TestModule.HotReload.ModuleWatcherQueuesFileChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHotReloadModifyLookupFlowTest,
	"Angelscript.TestModule.HotReload.AddModifyLookupFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHotReloadFailureKeepsOldCodeTest,
	"Angelscript.TestModule.HotReload.FailureKeepsOldCodeAndDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

struct FAngelscriptHotReloadTestAccess
{
	static void QueueFileChange(FAngelscriptEngine& Engine, const FAngelscriptEngine::FFilenamePair& Filename)
	{
		Engine.FileChangesDetectedForReload.AddUnique(Filename);
	}

	static int32 GetQueuedFileChangeCount(const FAngelscriptEngine& Engine)
	{
		return Engine.FileChangesDetectedForReload.Num();
	}

	static int32 GetQueuedFullReloadCount(const FAngelscriptEngine& Engine)
	{
		return Engine.QueuedFullReloadFiles.Num();
	}

	static void CheckForHotReload(FAngelscriptEngine& Engine, ECompileType CompileType)
	{
		Engine.CheckForHotReload(CompileType);
	}

	static int32 GetDiagnosticsCount(const FAngelscriptEngine& Engine, const FString& AbsoluteFilename)
	{
		if (const FAngelscriptEngine::FDiagnostics* FileDiagnostics = Engine.Diagnostics.Find(AbsoluteFilename))
		{
			return FileDiagnostics->Diagnostics.Num();
		}
		return 0;
	}

};

bool FAngelscriptModuleRecordTrackingTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptA = TEXT(R"AS(
UCLASS()
class UTrackedObjectA : UObject
{
	UPROPERTY()
	int ValueA;

	default ValueA = 10;

	UFUNCTION()
	int GetValueA()
	{
		return ValueA;
	}
}
)AS");
	const FString ScriptB = TEXT(R"AS(
UCLASS()
class UTrackedObjectB : UObject
{
	UPROPERTY()
	int ValueB;

	default ValueB = 20;

	UFUNCTION()
	int GetValueB()
	{
		return ValueB;
	}
}
)AS");
	if (!TestTrue(TEXT("Compile module A should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ModuleA"), TEXT("ModuleA.as"), ScriptA)) ||

		!TestTrue(TEXT("Compile module B should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ModuleB"), TEXT("ModuleB.as"), ScriptB)))
	{
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	TestTrue(TEXT("At least two modules should be tracked after compiling module A and B"), ActiveModules.Num() >= 2);

	TSharedPtr<FAngelscriptModuleDesc> RecordA = Engine.GetModuleByModuleName(TEXT("ModuleA"));
	TSharedPtr<FAngelscriptModuleDesc> RecordB = Engine.GetModuleByModuleName(TEXT("ModuleB"));
	TestTrue(TEXT("Module A record should exist"), RecordA.IsValid());
	TestTrue(TEXT("Module B record should exist"), RecordB.IsValid());

	if (RecordA.IsValid())
	{
		TestEqual(TEXT("Module A should track one generated class"), RecordA->Classes.Num(), 1);
		TestTrue(TEXT("Module A should track UTrackedObjectA"), RecordA->GetClass(TEXT("UTrackedObjectA")).IsValid());
	}

	if (RecordB.IsValid())
	{
		TestEqual(TEXT("Module B should track one generated class"), RecordB->Classes.Num(), 1);
		TestTrue(TEXT("Module B should track UTrackedObjectB"), RecordB->GetClass(TEXT("UTrackedObjectB")).IsValid());
	}

	TestNotNull(TEXT("UTrackedObjectA class should exist"), FindGeneratedClass(&Engine, TEXT("UTrackedObjectA")));
	TestNotNull(TEXT("UTrackedObjectB class should exist"), FindGeneratedClass(&Engine, TEXT("UTrackedObjectB")));
	TestTrue(TEXT("Unknown module record should not exist"), !Engine.GetModuleByModuleName(TEXT("NonExistent")).IsValid());
	}

	return true;
}

bool FAngelscriptDiscardModuleTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptA = TEXT(R"AS(
UCLASS()
class UDiscardableObject : UObject
{
	UPROPERTY()
	int Score;

	default Score = 42;

	UFUNCTION()
	int GetScore()
	{
		return Score;
	}
}
)AS");
	const FString ScriptB = TEXT(R"AS(
int SurvivorEntry()
{
	return 99;
}
)AS");
	if (!TestTrue(TEXT("Compile discardable module should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("DiscardA"), TEXT("DiscardA.as"), ScriptA)) ||

		!TestTrue(TEXT("Compile survivor module should succeed"), CompileModuleFromMemory(&Engine, TEXT("SurvivorB"), TEXT("SurvivorB.as"), ScriptB)))
	{
		return false;
	}

	TestNotNull(TEXT("Discardable class should exist before discard"), FindGeneratedClass(&Engine, TEXT("UDiscardableObject")));
	TestTrue(TEXT("Discardable module record should exist before discard"), Engine.GetModuleByModuleName(TEXT("DiscardA")).IsValid());

	int32 SurvivorResult = 0;
	if (!TestTrue(TEXT("Survivor module should execute before discard"), ExecuteIntFunction(&Engine, TEXT("SurvivorB"), TEXT("int SurvivorEntry()"), SurvivorResult)))
	{
		return false;
	}
	TestEqual(TEXT("Survivor module should return 99 before discard"), SurvivorResult, 99);

	if (!TestTrue(TEXT("DiscardModule should succeed for tracked module"), Engine.DiscardModule(TEXT("DiscardA"))))
	{
		return false;
	}

	TestTrue(TEXT("Discardable module record should be gone after discard"), !Engine.GetModuleByModuleName(TEXT("DiscardA")).IsValid());

	SurvivorResult = 0;
	if (!TestTrue(TEXT("Survivor module should still execute after discard"), ExecuteIntFunction(&Engine, TEXT("SurvivorB"), TEXT("int SurvivorEntry()"), SurvivorResult)))
	{
		return false;
	}
	TestEqual(TEXT("Survivor module should still return 99 after discard"), SurvivorResult, 99);

	TestFalse(TEXT("Discarding the same module twice should fail"), Engine.DiscardModule(TEXT("DiscardA")));
	}

	return true;
}

bool FAngelscriptDiscardAndRecompileTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UDiscardRecompileTarget : UObject
{
	UPROPERTY()
	int Version;

	default Version = 1;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UDiscardRecompileTargetV2 : UObject
{
	UPROPERTY()
	int Version;

	default Version = 2;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}
}
)AS");

	if (!TestTrue(TEXT("Compile reload target v1 should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("DiscardRecompileMod"), TEXT("DiscardRecompileMod.as"), ScriptV1)))
	{
		return false;
	}

	UClass* ClassV1 = FindGeneratedClass(&Engine, TEXT("UDiscardRecompileTarget"));
	if (!TestNotNull(TEXT("Reload target class v1 should exist"), ClassV1))
	{
		return false;
	}

	if (!TestTrue(TEXT("DiscardModule should succeed for reload target"), Engine.DiscardModule(TEXT("DiscardRecompileMod"))))
	{
		return false;
	}

	TestTrue(TEXT("Module record should be gone after discard"), !Engine.GetModuleByModuleName(TEXT("DiscardRecompileMod")).IsValid());

	if (!TestTrue(TEXT("Compile new class in same module should succeed after discard"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("DiscardRecompileMod"), TEXT("DiscardRecompileMod.as"), ScriptV2)))
	{
		return false;
	}

	UClass* ClassV2 = FindGeneratedClass(&Engine, TEXT("UDiscardRecompileTargetV2"));
	if (!TestNotNull(TEXT("New class v2 should exist after recompile"), ClassV2))
	{
		return false;
	}

	FIntProperty* VersionProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Version"));
	if (!TestNotNull(TEXT("Version property should exist after recompile"), VersionProperty))
	{
		return false;
	}

	UObject* ObjV2 = NewObject<UObject>(GetTransientPackage(), ClassV2);
	if (!TestNotNull(TEXT("Reload target object v2 should instantiate"), ObjV2))
	{
		return false;
	}

	TestEqual(TEXT("Version default should be 2 after discard and recompile"), VersionProperty->GetPropertyValue_InContainer(ObjV2), 2);
	TestTrue(TEXT("Reload module record should exist after recompile"), Engine.GetModuleByModuleName(TEXT("DiscardRecompileMod")).IsValid());
	}

	return true;
}

bool FAngelscriptModuleWatcherQueuesFileChangesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	const FAngelscriptEngine::FFilenamePair FilenamePair{
		TEXT("J:/UnrealEngine/Temp/UE-Angelscript/Saved/Automation/WatcherTest.as"),
		TEXT("Automation/WatcherTest.as")
	};

	TestEqual(
		TEXT("Hot reload watcher queue should start empty for this test"),
		FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
		0);

	FAngelscriptHotReloadTestAccess::QueueFileChange(Engine, FilenamePair);
	TestEqual(
		TEXT("QueueFileChange should add the changed file once"),
		FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
		1);

	FAngelscriptHotReloadTestAccess::QueueFileChange(Engine, FilenamePair);
	return TestEqual(
		TEXT("QueueFileChange should keep the queue de-duplicated"),
		FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
		1);

	}
}

bool FAngelscriptHotReloadModifyLookupFlowTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("HotReloadModifyLookupFlow"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadModifyLookupFlow : UObject
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
class UHotReloadModifyLookupFlow : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS");

	if (!TestTrue(TEXT("Modify/lookup flow should compile the initial module"), CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("HotReloadModifyLookupFlow.as"), ScriptV1)))
	{
		return false;
	}

	TestTrue(TEXT("Modify/lookup flow should register the module after initial compile"), Engine.GetModuleByModuleName(ModuleName.ToString()).IsValid());
	UClass* ClassBeforeReload = FindGeneratedClass(&Engine, TEXT("UHotReloadModifyLookupFlow"));
	if (!TestNotNull(TEXT("Modify/lookup flow should expose the generated class before reload"), ClassBeforeReload))
	{
		return false;
	}

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("Modify/lookup flow should compile the body-only update on the soft reload path"), CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("HotReloadModifyLookupFlow.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}

	TestTrue(TEXT("Modify/lookup flow should stay on a handled reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled);
	UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("UHotReloadModifyLookupFlow"));
	if (!TestNotNull(TEXT("Modify/lookup flow should keep the generated class visible after reload"), ClassAfterReload))
	{
		return false;
	}

	UFunction* GetValueFunction = FindGeneratedFunction(ClassAfterReload, TEXT("GetValue"));
	if (!TestNotNull(TEXT("Modify/lookup flow should keep the generated function visible after reload"), GetValueFunction))
	{
		return false;
	}

	UObject* TestObject = NewObject<UObject>(GetTransientPackage(), ClassAfterReload);
	if (!TestNotNull(TEXT("Modify/lookup flow should instantiate the reloaded generated class"), TestObject))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Modify/lookup flow should execute the reloaded generated function"), ExecuteGeneratedIntEventOnGameThread(&Engine, TestObject, GetValueFunction, Result)))
	{
		return false;
	}

	TestEqual(TEXT("Modify/lookup flow should surface the modified function body result after reload"), Result, 2);
	Engine.DiscardModule(*ModuleName.ToString());
	bPassed = TestTrue(TEXT("Modify/lookup flow should clear the module lookup after discard"), !Engine.GetModuleByModuleName(ModuleName.ToString()).IsValid());
	}

	return bPassed;
}

bool FAngelscriptHotReloadFailureKeepsOldCodeTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	AddExpectedError(TEXT("HotReloadFailureKeepsOldCode.as:"), EAutomationExpectedErrorFlags::Contains, 2);
	AddExpectedError(TEXT("Identifier 'MissingType' is not a data type in global namespace"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Identifier 'MissingType' is not a data type"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 1);

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("HotReloadFailureKeepsOldCode"));
	const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("HotReloadFailureKeepsOldCode.as"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ResetSharedCloneEngine(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadFailureKeepsOldCode : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 5;
	}
}
)AS");
	const FString BrokenScript = TEXT(R"AS(
UCLASS()
class UHotReloadFailureKeepsOldCode : UObject
{
	UFUNCTION()
	MissingType GetValue()
	{
		MissingType Value;
		return Value;
	}
}
)AS");

	if (!TestTrue(TEXT("Failure fallback test should compile the initial module"), CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("HotReloadFailureKeepsOldCode.as"), ScriptV1)))
	{
		return false;
	}

	UClass* ClassBeforeFailure = FindGeneratedClass(&Engine, TEXT("UHotReloadFailureKeepsOldCode"));
	if (!TestNotNull(TEXT("Failure fallback test should expose the generated class before reload failure"), ClassBeforeFailure))
	{
		return false;
	}

	UFunction* GetValueBeforeFailure = FindGeneratedFunction(ClassBeforeFailure, TEXT("GetValue"));
	if (!TestNotNull(TEXT("Failure fallback test should expose the generated function before reload failure"), GetValueBeforeFailure))
	{
		return false;
	}

	UObject* TestObject = NewObject<UObject>(GetTransientPackage(), ClassBeforeFailure);
	if (!TestNotNull(TEXT("Failure fallback test should instantiate the pre-failure generated class"), TestObject))
	{
		return false;
	}

	int32 ResultBeforeFailure = 0;
	if (!TestTrue(TEXT("Failure fallback test should execute the initial generated function"), ExecuteGeneratedIntEventOnGameThread(&Engine, TestObject, GetValueBeforeFailure, ResultBeforeFailure)))
	{
		return false;
	}
	TestEqual(TEXT("Failure fallback test should observe the old code result before reload failure"), ResultBeforeFailure, 5);

	ECompileResult ReloadResult = ECompileResult::FullyHandled;
	const bool bCompiled = CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, TEXT("HotReloadFailureKeepsOldCode.as"), BrokenScript, ReloadResult);
	TestFalse(TEXT("Failure fallback test should fail the broken hot reload compile"), bCompiled);
	TestTrue(TEXT("Failure fallback test should report an error reload state"), ReloadResult == ECompileResult::Error || ReloadResult == ECompileResult::ErrorNeedFullReload);
	TestTrue(TEXT("Failure fallback test should collect diagnostics for the broken file"), FAngelscriptHotReloadTestAccess::GetDiagnosticsCount(Engine, AbsoluteFilename) > 0);

	int32 ResultAfterFailure = 0;
	if (!TestTrue(TEXT("Failure fallback test should still execute the old generated function after reload failure"), ExecuteGeneratedIntEventOnGameThread(&Engine, TestObject, GetValueBeforeFailure, ResultAfterFailure)))
	{
		return false;
	}

	TestEqual(TEXT("Failure fallback test should keep the old code active after the broken reload"), ResultAfterFailure, 5);
	}

	return true;
}

#endif
