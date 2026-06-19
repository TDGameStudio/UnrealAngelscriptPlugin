#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptSourceProvider.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private
{
}


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

	static void CheckForFileChanges(FAngelscriptEngine& Engine)
	{
		Engine.bUseHotReloadCheckerThread = true;
		Engine.CheckForFileChanges();
	}

};

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define AddExpectedError(...) Test.AddExpectedError(__VA_ARGS__)

static bool ModuleRecordTracking(FAutomationTestBase& Test)
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

static bool DiscardModule(FAutomationTestBase& Test)
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

static bool DiscardAndRecompile(FAutomationTestBase& Test)
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

static bool SourceProviderSuppressesTimestampOnlyChange(FAutomationTestBase& Test)
{
	struct FMutableStateSourceProvider final : IAngelscriptSourceProvider
	{
		FAngelscriptSource Source = FAngelscriptSource::FromGameFile(
			TEXT("HotReload/ProviderState.as"),
			TEXT("J:/ProviderProject/Script/HotReload/ProviderState.as"));
		FDateTime Timestamp = FDateTime(2026, 6, 16, 10, 0, 0);
		uint64 ContentHash = 12345;

		virtual void FindSources(
			const TArray<FAngelscriptSourceRoot>& ScriptRoots,
			bool bSkipDevelopmentScripts,
			bool bSkipEditorScripts,
			TArray<FAngelscriptSource>& OutSources) override
		{
			OutSources.Add(Source);
		}

		virtual bool LoadSourceText(const FAngelscriptSource& InSource, FString& OutSourceText) override
		{
			OutSourceText = TEXT("int Entry() { return 41; }");
			return true;
		}

		virtual bool QuerySourceState(const FAngelscriptSource& InSource, FAngelscriptSourceState& OutState) override
		{
			OutState.Timestamp = Timestamp;
			OutState.ContentHash = ContentHash;
			OutState.bHasContentHash = true;
			return true;
		}
	};

	TSharedRef<FMutableStateSourceProvider> Provider = MakeShared<FMutableStateSourceProvider>();

	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	Dependencies.SourceProvider = Provider;

	FAngelscriptEngine Engine(FAngelscriptEngineConfig(), Dependencies);
	Engine.AllScriptRoots.Add(FAngelscriptSourceRoot::FromGameRoot(TEXT("J:/ProviderProject/Script")));

	FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);
	TestEqual(TEXT("First source-state scan should queue the initially unknown source"), FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine), 1);

	Provider->Timestamp = FDateTime(2026, 6, 16, 10, 1, 0);
	FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);
	TestEqual(TEXT("Timestamp-only source-state churn should not queue a reload when content hash is unchanged"), FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine), 0);

	Provider->Timestamp = FDateTime(2026, 6, 16, 10, 2, 0);
	Provider->ContentHash = 67890;
	FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);
	return TestEqual(TEXT("Content hash changes should queue a reload"), FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine), 1);
}

static bool SourceProviderSeparatesVirtualPathState(FAutomationTestBase& Test)
{
	struct FCollisionSourceProvider final : IAngelscriptSourceProvider
	{
		TArray<FAngelscriptSource> Sources = {
			FAngelscriptSource::FromGameFile(
				TEXT("Shared/State.as"),
				TEXT("J:/ProviderProject/Script/Shared/State.as")),
			FAngelscriptSource::FromPluginFile(
				TEXT("Inventory"),
				TEXT("Shared/State.as"),
				TEXT("J:/ProviderProject/Plugins/Inventory/Script/Shared/State.as")),
		};
		TMap<FString, uint64> ContentHashByVirtualPath;
		FDateTime Timestamp = FDateTime(2026, 6, 16, 10, 0, 0);

		FCollisionSourceProvider()
		{
			ContentHashByVirtualPath.Add(TEXT("/Angelscript/Game/Shared/State.as"), 11);
			ContentHashByVirtualPath.Add(TEXT("/Angelscript/Plugin/Inventory/Shared/State.as"), 22);
		}

		virtual void FindSources(
			const TArray<FAngelscriptSourceRoot>& ScriptRoots,
			bool bSkipDevelopmentScripts,
			bool bSkipEditorScripts,
			TArray<FAngelscriptSource>& OutSources) override
		{
			OutSources.Append(Sources);
		}

		virtual bool LoadSourceText(const FAngelscriptSource& InSource, FString& OutSourceText) override
		{
			OutSourceText = TEXT("int Entry() { return 1; }");
			return true;
		}

		virtual bool QuerySourceState(const FAngelscriptSource& InSource, FAngelscriptSourceState& OutState) override
		{
			const uint64* ContentHash = ContentHashByVirtualPath.Find(InSource.VirtualPath.ToString());
			if (ContentHash == nullptr)
			{
				return false;
			}

			OutState.Timestamp = Timestamp;
			OutState.ContentHash = *ContentHash;
			OutState.bHasContentHash = true;
			return true;
		}
	};

	TSharedRef<FCollisionSourceProvider> Provider = MakeShared<FCollisionSourceProvider>();

	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	Dependencies.SourceProvider = Provider;

	FAngelscriptEngine Engine(FAngelscriptEngineConfig(), Dependencies);
	Engine.AllScriptRoots.Add(FAngelscriptSourceRoot::FromGameRoot(TEXT("J:/ProviderProject/Script")));
	Engine.AllScriptRoots.Add(FAngelscriptSourceRoot::FromPluginRoot(TEXT("Inventory"), TEXT("J:/ProviderProject/Plugins/Inventory/Script")));

	FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);
	if (!TestEqual(TEXT("First scan should queue both colliding relative paths"), FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine), 2))
	{
		return false;
	}

	Provider->Timestamp = FDateTime(2026, 6, 16, 10, 1, 0);
	Provider->ContentHashByVirtualPath[TEXT("/Angelscript/Plugin/Inventory/Shared/State.as")] = 33;
	FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);

	if (!TestEqual(TEXT("Only the source with a changed canonical virtual path state should reload"), FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine), 1))
	{
		return false;
	}

	return TestEqual(
		TEXT("Reload should preserve the plugin virtual path instead of merging by relative filename"),
		Engine.FileChangesDetectedForReload[0].VirtualPath,
		FString(TEXT("/Angelscript/Plugin/Inventory/Shared/State.as")));
}

static bool DiscardModuleRemovesGlobalFunctionAvailability(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	const FName FirstModuleName(TEXT("DiscardGlobalFunctionA"));
	const FName SecondModuleName(TEXT("DiscardGlobalFunctionB"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*FirstModuleName.ToString());
		Engine.DiscardModule(*SecondModuleName.ToString());
	};

	if (!TestTrue(
		TEXT("Compile first discard-global-function module should succeed"),
		CompileModuleFromMemory(&Engine, FirstModuleName, TEXT("DiscardGlobalFunctionA.as"), TEXT("int Entry() { return 1; }"))))
	{
		return false;
	}

	int32 FirstResult = 0;
	if (!TestTrue(
		TEXT("First discard-global-function module should execute before discard"),
		ExecuteIntFunction(&Engine, FirstModuleName, TEXT("int Entry()"), FirstResult)))
	{
		return false;
	}
	TestEqual(TEXT("First discard-global-function module should return 1"), FirstResult, 1);

	if (!TestTrue(TEXT("DiscardModule should succeed for first global-function module"), Engine.DiscardModule(*FirstModuleName.ToString())))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Compile second module with same global function signature should succeed after discard"),
		CompileModuleFromMemory(&Engine, SecondModuleName, TEXT("DiscardGlobalFunctionB.as"), TEXT("int Entry() { return 2; }"))))
	{
		return false;
	}

	int32 SecondResult = 0;
	if (!TestTrue(
		TEXT("Second discard-global-function module should execute after first discard"),
		ExecuteIntFunction(&Engine, SecondModuleName, TEXT("int Entry()"), SecondResult)))
	{
		return false;
	}
	TestEqual(TEXT("Second discard-global-function module should return 2"), SecondResult, 2);

	TestFalse(TEXT("DiscardModule should fail when discarding an already discarded module"), Engine.DiscardModule(*FirstModuleName.ToString()));

	SecondResult = 0;
	if (!TestTrue(
		TEXT("Failed discard of old module should not remove second global function availability"),
		ExecuteIntFunction(&Engine, SecondModuleName, TEXT("int Entry()"), SecondResult)))
	{
		return false;
	}
	TestEqual(TEXT("Second discard-global-function module should remain callable after failed old discard"), SecondResult, 2);

	}

	return true;
}

static bool ModuleWatcherQueuesFileChanges(FAutomationTestBase& Test)
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

static bool HotReloadModifyLookupFlow(FAutomationTestBase& Test)
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

static bool HotReloadFailureKeepsOldCode(FAutomationTestBase& Test)
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

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull
#undef AddExpectedError

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadFunctionTests,
	"Angelscript.TestModule.HotReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ModuleRecordTracking)
	{
		ASSERT_THAT(IsTrue(::ModuleRecordTracking(*TestRunner)));
	}

	TEST_METHOD(DiscardModule)
	{
		ASSERT_THAT(IsTrue(::DiscardModule(*TestRunner)));
	}

	TEST_METHOD(DiscardAndRecompile)
	{
		ASSERT_THAT(IsTrue(::DiscardAndRecompile(*TestRunner)));
	}

	TEST_METHOD(DiscardModuleRemovesGlobalFunctionAvailability)
	{
		ASSERT_THAT(IsTrue(::DiscardModuleRemovesGlobalFunctionAvailability(*TestRunner)));
	}

	TEST_METHOD(ModuleWatcherQueuesFileChanges)
	{
		ASSERT_THAT(IsTrue(::ModuleWatcherQueuesFileChanges(*TestRunner)));
	}

	TEST_METHOD(AddModifyLookupFlow)
	{
		ASSERT_THAT(IsTrue(HotReloadModifyLookupFlow(*TestRunner)));
	}

	TEST_METHOD(FailureKeepsOldCodeAndDiagnostics)
	{
		ASSERT_THAT(IsTrue(HotReloadFailureKeepsOldCode(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadSourceProviderTests,
	"Angelscript.TestModule.HotReload.SourceProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SuppressTimestampOnlyChange)
	{
		ASSERT_THAT(IsTrue(::SourceProviderSuppressesTimestampOnlyChange(*TestRunner)));
	}

	TEST_METHOD(SeparatesVirtualPathState)
	{
		ASSERT_THAT(IsTrue(::SourceProviderSeparatesVirtualPathState(*TestRunner)));
	}
};

#endif
