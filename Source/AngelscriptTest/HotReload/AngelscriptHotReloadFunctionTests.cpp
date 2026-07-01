#include "CQTest.h"
#include "AngelscriptSourceProvider.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

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

	static void ClearQueuedFileChanges(FAngelscriptEngine& Engine)
	{
		Engine.FileChangesDetectedForReload.Reset();
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

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadFunctionTests,
	"Angelscript.TestModule.HotReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleAName = FName(TEXT("ModuleA"));
	inline static const FName ModuleBName = FName(TEXT("ModuleB"));
	inline static const FName DiscardModuleName = FName(TEXT("DiscardA"));
	inline static const FName SurvivorModuleName = FName(TEXT("SurvivorB"));
	inline static const FName DiscardRecompileModuleName = FName(TEXT("DiscardRecompileMod"));
	inline static const FName FirstGlobalFunctionModuleName = FName(TEXT("DiscardGlobalFunctionA"));
	inline static const FName SecondGlobalFunctionModuleName = FName(TEXT("DiscardGlobalFunctionB"));
	inline static const FName ModifyLookupModuleName = FName(TEXT("HotReloadModifyLookupFlow"));
	inline static const FName FailureFallbackModuleName = FName(TEXT("HotReloadFailureKeepsOldCode"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ModuleRecordTracking)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleAName.ToString());
			Engine.DiscardModule(*ModuleBName.ToString());
		};

		const FString ModuleASource = ASTEST_AS(R"AS(
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

		const FString ModuleBSource = ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleAName, TEXT("ModuleA.as"), ModuleASource),
			TEXT("Compile module A should succeed")));
		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleBName, TEXT("ModuleB.as"), ModuleBSource),
			TEXT("Compile module B should succeed")));

		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		ASSERT_THAT(IsTrue(ActiveModules.Num() >= 2, TEXT("At least two modules should be tracked after compiling module A and B")));

		const TSharedPtr<FAngelscriptModuleDesc> RecordA = Engine.GetModuleByModuleName(ModuleAName.ToString());
		const TSharedPtr<FAngelscriptModuleDesc> RecordB = Engine.GetModuleByModuleName(ModuleBName.ToString());
		ASSERT_THAT(IsTrue(RecordA.IsValid(), TEXT("Module A record should exist")));
		ASSERT_THAT(IsTrue(RecordB.IsValid(), TEXT("Module B record should exist")));

		ASSERT_THAT(AreEqual(1, RecordA->Classes.Num(), TEXT("Module A should track one generated class")));
		ASSERT_THAT(IsTrue(RecordA->GetClass(TEXT("UTrackedObjectA")).IsValid(), TEXT("Module A should track UTrackedObjectA")));
		ASSERT_THAT(AreEqual(1, RecordB->Classes.Num(), TEXT("Module B should track one generated class")));
		ASSERT_THAT(IsTrue(RecordB->GetClass(TEXT("UTrackedObjectB")).IsValid(), TEXT("Module B should track UTrackedObjectB")));

		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, TEXT("UTrackedObjectA")), TEXT("UTrackedObjectA class should exist")));
		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, TEXT("UTrackedObjectB")), TEXT("UTrackedObjectB class should exist")));
		ASSERT_THAT(IsFalse(Engine.GetModuleByModuleName(TEXT("NonExistent")).IsValid(), TEXT("Unknown module record should not exist")));
	}

	TEST_METHOD(DiscardModule)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DiscardModuleName.ToString());
			Engine.DiscardModule(*SurvivorModuleName.ToString());
		};

		const FString DiscardableSource = ASTEST_AS(R"AS(
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

		const FString SurvivorSource = ASTEST_AS(R"AS(
			int SurvivorEntry()
			{
				return 99;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, DiscardModuleName, TEXT("DiscardA.as"), DiscardableSource),
			TEXT("Compile discardable module should succeed")));
		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(&Engine, SurvivorModuleName, TEXT("SurvivorB.as"), SurvivorSource),
			TEXT("Compile survivor module should succeed")));

		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, TEXT("UDiscardableObject")), TEXT("Discardable class should exist before discard")));
		ASSERT_THAT(IsTrue(Engine.GetModuleByModuleName(DiscardModuleName.ToString()).IsValid(), TEXT("Discardable module record should exist before discard")));

		int32 SurvivorResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, SurvivorModuleName, TEXT("int SurvivorEntry()"), SurvivorResult),
			TEXT("Survivor module should execute before discard")));
		ASSERT_THAT(AreEqual(99, SurvivorResult, TEXT("Survivor module should return 99 before discard")));

		ASSERT_THAT(IsTrue(Engine.DiscardModule(*DiscardModuleName.ToString()), TEXT("DiscardModule should succeed for tracked module")));

		ASSERT_THAT(IsFalse(Engine.GetModuleByModuleName(DiscardModuleName.ToString()).IsValid(), TEXT("Discardable module record should be gone after discard")));

		SurvivorResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, SurvivorModuleName, TEXT("int SurvivorEntry()"), SurvivorResult),
			TEXT("Survivor module should still execute after discard")));
		ASSERT_THAT(AreEqual(99, SurvivorResult, TEXT("Survivor module should still return 99 after discard")));

		ASSERT_THAT(IsFalse(Engine.DiscardModule(*DiscardModuleName.ToString()), TEXT("Discarding the same module twice should fail")));
	}

	TEST_METHOD(DiscardAndRecompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DiscardRecompileModuleName.ToString());
		};

		const FString DiscardRecompileV1Source = ASTEST_AS(R"AS(
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

		const FString DiscardRecompileV2Source = ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, DiscardRecompileModuleName, TEXT("DiscardRecompileMod.as"), DiscardRecompileV1Source),
			TEXT("Compile reload target v1 should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, TEXT("UDiscardRecompileTarget"));
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Reload target class v1 should exist")));

		ASSERT_THAT(IsTrue(Engine.DiscardModule(*DiscardRecompileModuleName.ToString()), TEXT("DiscardModule should succeed for reload target")));
		ASSERT_THAT(IsFalse(Engine.GetModuleByModuleName(DiscardRecompileModuleName.ToString()).IsValid(), TEXT("Module record should be gone after discard")));

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, DiscardRecompileModuleName, TEXT("DiscardRecompileMod.as"), DiscardRecompileV2Source),
			TEXT("Compile new class in same module should succeed after discard")));

		UClass* ClassV2 = FindGeneratedClass(&Engine, TEXT("UDiscardRecompileTargetV2"));
		ASSERT_THAT(IsNotNull(ClassV2, TEXT("New class v2 should exist after recompile")));

		FIntProperty* VersionProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Version"));
		ASSERT_THAT(IsNotNull(VersionProperty, TEXT("Version property should exist after recompile")));

		UObject* ObjV2 = NewObject<UObject>(GetTransientPackage(), ClassV2);
		ASSERT_THAT(IsNotNull(ObjV2, TEXT("Reload target object v2 should instantiate")));

		ASSERT_THAT(AreEqual(2, VersionProperty->GetPropertyValue_InContainer(ObjV2), TEXT("Version default should be 2 after discard and recompile")));
		ASSERT_THAT(IsTrue(Engine.GetModuleByModuleName(DiscardRecompileModuleName.ToString()).IsValid(), TEXT("Reload module record should exist after recompile")));
	}

	TEST_METHOD(DiscardModuleRemovesGlobalFunctionAvailability)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FirstGlobalFunctionModuleName.ToString());
			Engine.DiscardModule(*SecondGlobalFunctionModuleName.ToString());
		};

		const FString FirstGlobalFunctionSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");

		const FString SecondGlobalFunctionSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 2;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(&Engine, FirstGlobalFunctionModuleName, TEXT("DiscardGlobalFunctionA.as"), FirstGlobalFunctionSource),
			TEXT("Compile first discard-global-function module should succeed")));

		int32 FirstResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, FirstGlobalFunctionModuleName, TEXT("int Entry()"), FirstResult),
			TEXT("First discard-global-function module should execute before discard")));
		ASSERT_THAT(AreEqual(1, FirstResult, TEXT("First discard-global-function module should return 1")));

		ASSERT_THAT(IsTrue(Engine.DiscardModule(*FirstGlobalFunctionModuleName.ToString()), TEXT("DiscardModule should succeed for first global-function module")));

		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(&Engine, SecondGlobalFunctionModuleName, TEXT("DiscardGlobalFunctionB.as"), SecondGlobalFunctionSource),
			TEXT("Compile second module with same global function signature should succeed after discard")));

		int32 SecondResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, SecondGlobalFunctionModuleName, TEXT("int Entry()"), SecondResult),
			TEXT("Second discard-global-function module should execute after first discard")));
		ASSERT_THAT(AreEqual(2, SecondResult, TEXT("Second discard-global-function module should return 2")));

		ASSERT_THAT(IsFalse(Engine.DiscardModule(*FirstGlobalFunctionModuleName.ToString()), TEXT("DiscardModule should fail when discarding an already discarded module")));

		SecondResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, SecondGlobalFunctionModuleName, TEXT("int Entry()"), SecondResult),
			TEXT("Failed discard of old module should not remove second global function availability")));
		ASSERT_THAT(AreEqual(2, SecondResult, TEXT("Second discard-global-function module should remain callable after failed old discard")));
	}

	TEST_METHOD(ModuleWatcherQueuesFileChanges)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			FAngelscriptHotReloadTestAccess::ClearQueuedFileChanges(Engine);
		};

		FAngelscriptHotReloadTestAccess::ClearQueuedFileChanges(Engine);

		const FAngelscriptEngine::FFilenamePair FilenamePair{
			TEXT("J:/UnrealEngine/Temp/UE-Angelscript/Saved/Automation/WatcherTest.as"),
			TEXT("Automation/WatcherTest.as")
		};

		ASSERT_THAT(AreEqual(
			0,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("Hot reload watcher queue should start empty for this test")));

		FAngelscriptHotReloadTestAccess::QueueFileChange(Engine, FilenamePair);
		ASSERT_THAT(AreEqual(
			1,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("QueueFileChange should add the changed file once")));

		FAngelscriptHotReloadTestAccess::QueueFileChange(Engine, FilenamePair);
		ASSERT_THAT(AreEqual(
			1,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("QueueFileChange should keep the queue de-duplicated")));
	}

	TEST_METHOD(AddModifyLookupFlow)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModifyLookupModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		const FString ModifyLookupV1Source = ASTEST_AS(R"AS(
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

		const FString ModifyLookupV2Source = ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModifyLookupModuleName, TEXT("HotReloadModifyLookupFlow.as"), ModifyLookupV1Source),
			TEXT("Modify/lookup flow should compile the initial module")));

		ASSERT_THAT(IsTrue(Engine.GetModuleByModuleName(ModifyLookupModuleName.ToString()).IsValid(), TEXT("Modify/lookup flow should register the module after initial compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, TEXT("UHotReloadModifyLookupFlow"));
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("Modify/lookup flow should expose the generated class before reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModifyLookupModuleName, TEXT("HotReloadModifyLookupFlow.as"), ModifyLookupV2Source, ReloadResult),
			TEXT("Modify/lookup flow should compile the body-only update on the soft reload path")));

		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Modify/lookup flow should stay on a handled reload path")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("UHotReloadModifyLookupFlow"));
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Modify/lookup flow should keep the generated class visible after reload")));

		UFunction* GetValueFunction = FindGeneratedFunction(ClassAfterReload, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(GetValueFunction, TEXT("Modify/lookup flow should keep the generated function visible after reload")));

		UObject* TestObject = NewObject<UObject>(GetTransientPackage(), ClassAfterReload);
		ASSERT_THAT(IsNotNull(TestObject, TEXT("Modify/lookup flow should instantiate the reloaded generated class")));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, TestObject, GetValueFunction, Result),
			TEXT("Modify/lookup flow should execute the reloaded generated function")));

		ASSERT_THAT(AreEqual(2, Result, TEXT("Modify/lookup flow should surface the modified function body result after reload")));
		ASSERT_THAT(IsTrue(Engine.DiscardModule(*ModifyLookupModuleName.ToString()), TEXT("Modify/lookup flow should discard the module after execution")));
		ASSERT_THAT(IsFalse(Engine.GetModuleByModuleName(ModifyLookupModuleName.ToString()).IsValid(), TEXT("Modify/lookup flow should clear the module lookup after discard")));
	}

	TEST_METHOD(FailureKeepsOldCodeAndDiagnostics)
	{
		TestRunner->AddExpectedError(TEXT("HotReloadFailureKeepsOldCode.as:"), EAutomationExpectedErrorFlags::Contains, 2);
		TestRunner->AddExpectedError(TEXT("Identifier 'MissingType' is not a data type in global namespace"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Identifier 'MissingType' is not a data type"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("HotReloadFailureKeepsOldCode.as"));

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FailureFallbackModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		const FString FailureFallbackV1Source = ASTEST_AS(R"AS(
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

		const FString FailureFallbackBrokenSource = ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, FailureFallbackModuleName, TEXT("HotReloadFailureKeepsOldCode.as"), FailureFallbackV1Source),
			TEXT("Failure fallback test should compile the initial module")));

		UClass* ClassBeforeFailure = FindGeneratedClass(&Engine, TEXT("UHotReloadFailureKeepsOldCode"));
		ASSERT_THAT(IsNotNull(ClassBeforeFailure, TEXT("Failure fallback test should expose the generated class before reload failure")));

		UFunction* GetValueBeforeFailure = FindGeneratedFunction(ClassBeforeFailure, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(GetValueBeforeFailure, TEXT("Failure fallback test should expose the generated function before reload failure")));

		UObject* TestObject = NewObject<UObject>(GetTransientPackage(), ClassBeforeFailure);
		ASSERT_THAT(IsNotNull(TestObject, TEXT("Failure fallback test should instantiate the pre-failure generated class")));

		int32 ResultBeforeFailure = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, TestObject, GetValueBeforeFailure, ResultBeforeFailure),
			TEXT("Failure fallback test should execute the initial generated function")));
		ASSERT_THAT(AreEqual(5, ResultBeforeFailure, TEXT("Failure fallback test should observe the old code result before reload failure")));

		ECompileResult ReloadResult = ECompileResult::FullyHandled;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			FailureFallbackModuleName,
			TEXT("HotReloadFailureKeepsOldCode.as"),
			FailureFallbackBrokenSource,
			ReloadResult);
		ASSERT_THAT(IsFalse(bCompiled, TEXT("Failure fallback test should fail the broken hot reload compile")));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::Error || ReloadResult == ECompileResult::ErrorNeedFullReload,
			TEXT("Failure fallback test should report an error reload state")));
		ASSERT_THAT(IsTrue(
			FAngelscriptHotReloadTestAccess::GetDiagnosticsCount(Engine, AbsoluteFilename) > 0,
			TEXT("Failure fallback test should collect diagnostics for the broken file")));

		int32 ResultAfterFailure = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, TestObject, GetValueBeforeFailure, ResultAfterFailure),
			TEXT("Failure fallback test should still execute the old generated function after reload failure")));

		ASSERT_THAT(AreEqual(5, ResultAfterFailure, TEXT("Failure fallback test should keep the old code active after the broken reload")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadSourceProviderTests,
	"Angelscript.TestModule.HotReload.SourceProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SuppressTimestampOnlyChange)
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
				OutSourceText = ASTEST_AS(R"AS(
					int Entry()
					{
						return 41;
					}
					)AS");
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
		ASSERT_THAT(AreEqual(
			1,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("First source-state scan should queue the initially unknown source")));

		Provider->Timestamp = FDateTime(2026, 6, 16, 10, 1, 0);
		FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);
		ASSERT_THAT(AreEqual(
			0,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("Timestamp-only source-state churn should not queue a reload when content hash is unchanged")));

		Provider->Timestamp = FDateTime(2026, 6, 16, 10, 2, 0);
		Provider->ContentHash = 67890;
		FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);
		ASSERT_THAT(AreEqual(
			1,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("Content hash changes should queue a reload")));
	}

	TEST_METHOD(SeparatesVirtualPathState)
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
				OutSourceText = ASTEST_AS(R"AS(
					int Entry()
					{
						return 1;
					}
					)AS");
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
		ASSERT_THAT(AreEqual(
			2,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("First scan should queue both colliding relative paths")));

		Provider->Timestamp = FDateTime(2026, 6, 16, 10, 1, 0);
		Provider->ContentHashByVirtualPath[TEXT("/Angelscript/Plugin/Inventory/Shared/State.as")] = 33;
		FAngelscriptHotReloadTestAccess::CheckForFileChanges(Engine);

		ASSERT_THAT(AreEqual(
			1,
			FAngelscriptHotReloadTestAccess::GetQueuedFileChangeCount(Engine),
			TEXT("Only the source with a changed canonical virtual path state should reload")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Plugin/Inventory/Shared/State.as")),
			Engine.FileChangesDetectedForReload[0].VirtualPath,
			TEXT("Reload should preserve the plugin virtual path instead of merging by relative filename")));
	}
};

#endif
