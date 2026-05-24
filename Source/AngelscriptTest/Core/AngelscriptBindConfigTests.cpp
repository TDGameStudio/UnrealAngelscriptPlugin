#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptUhtCoverageTestTypes.h"
#include "Binds/Helper_FunctionSignature.h"
#include "Testing/AngelscriptUhtOverloadCoverageTypes.h"
#include "ClassGenerator/ASClass.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "CQTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

class asIScriptGeneric;

struct FAngelscriptBindConfigTestAccess
{
	static void CallBinds(const TSet<FName>& DisabledBindNames)
	{
		FAngelscriptBinds::CallBinds(DisabledBindNames);
	}

	static void BindScriptTypes(FAngelscriptEngine& Engine)
	{
		Engine.BindScriptTypes();
	}

	static void SetRuntimeConfig(FAngelscriptEngine& Engine, const FAngelscriptEngineConfig& Config)
	{
		Engine.RuntimeConfig = Config;
	}

	static void DestroyGlobalEngine()
	{
		FAngelscriptEngine::DestroyGlobal();
	}

	static TSet<FName> CollectDisabledBindNames(const FAngelscriptEngine& Engine)
	{
		return Engine.CollectDisabledBindNames();
	}
};

namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private
{
	struct FBindExecutionRecorder
	{
		static TMap<FName, int32>& GetCounts()
		{
			static TMap<FName, int32> Counts;
			return Counts;
		}

		static void Reset(const FName CounterKey)
		{
			GetCounts().FindOrAdd(CounterKey) = 0;
		}

		static void Increment(const FName CounterKey)
		{
			++GetCounts().FindOrAdd(CounterKey);
		}

		static int32 Get(const FName CounterKey)
		{
			return GetCounts().FindRef(CounterKey);
		}
	};

	FName MakeUniqueBindTestName(const TCHAR* Prefix)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	TArray<FName> FindNewBindNames(const TArray<FName>& BeforeNames, const TArray<FName>& AfterNames)
	{
		TSet<FName> ExistingNames;
		for (const FName& BeforeName : BeforeNames)
		{
			ExistingNames.Add(BeforeName);
		}

		TArray<FName> NewNames;
		for (const FName& AfterName : AfterNames)
		{
			if (!ExistingNames.Contains(AfterName))
			{
				NewNames.Add(AfterName);
			}
		}

		return NewNames;
	}

	TSet<FName> BuildDisabledSetExcluding(const TArray<FName>& AllBindNames, const TSet<FName>& AllowedNames)
	{
		TSet<FName> DisabledBindNames;
		for (const FName& BindName : AllBindNames)
		{
			if (!AllowedNames.Contains(BindName))
			{
				DisabledBindNames.Add(BindName);
			}
		}

		return DisabledBindNames;
	}

	void ExecuteIsolatedBinds(const TSet<FName>& DisabledBindNames)
	{
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		FAngelscriptBindConfigTestAccess::CallBinds(DisabledBindNames);
		UE_SET_LOG_VERBOSITY(Angelscript, Log);
	}

	FAngelscriptBindExecutionSnapshot ObserveStartupBindPass(const FAngelscriptEngineConfig& Config)
	{
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptBindConfigTestAccess::DestroyGlobalEngine();
		}

		FAngelscriptBindExecutionObservation::Reset();
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		check(Engine.IsValid());
		FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
		Engine.Reset();
		DestroySharedTestEngine();

		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptBindConfigTestAccess::DestroyGlobalEngine();
		}

		return Snapshot;
	}

	int32 FindBindIndexByName(const TArray<FAngelscriptBinds::FBindInfo>& BindInfos, const FName BindName)
	{
		for (int32 BindIndex = 0; BindIndex < BindInfos.Num(); ++BindIndex)
		{
			if (BindInfos[BindIndex].BindName == BindName)
			{
				return BindIndex;
			}
		}

		return INDEX_NONE;
	}

	const FAngelscriptBinds::FBindInfo* FindBindInfoByName(const TArray<FAngelscriptBinds::FBindInfo>& BindInfos, const FName BindName)
	{
		for (const FAngelscriptBinds::FBindInfo& BindInfo : BindInfos)
		{
			if (BindInfo.BindName == BindName)
			{
				return &BindInfo;
			}
		}

		return nullptr;
	}

	bool IsFunctionEntryBound(const FFuncEntry& Entry)
	{
		FGenericFuncPtr FuncPtr = Entry.FuncPtr;
		return FuncPtr.IsBound() && Entry.Caller.IsBound();
	}

	bool AreFunctionEntriesEqual(const FFuncEntry& Left, const FFuncEntry& Right)
	{
		return FMemory::Memcmp(&Left.FuncPtr, &Right.FuncPtr, sizeof(FGenericFuncPtr)) == 0 &&
			FMemory::Memcmp(&Left.Caller, &Right.Caller, sizeof(ASAutoCaller::FunctionCaller)) == 0;
	}

	void CDECL NoOpGeneric(asIScriptGeneric* Generic)
	{
		(void)Generic;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBindConfigTests,
	"Angelscript.TestModule.Engine.BindConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GlobalDisabledBindNames)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("BindConfig.GlobalDisabledBindNames should access mutable settings"), Settings))
		{
			return;
		}

		const TArray<FName> PreviousDisabledBindNames = Settings->DisabledBindNames;
		ON_SCOPE_EXIT
		{
			Settings->DisabledBindNames = PreviousDisabledBindNames;
		};

		const FName NamedBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Global"));
		const FName CounterKey = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Global.Counter"));
		FBindExecutionRecorder::Reset(CounterKey);

		FAngelscriptBinds::FBind NamedBind(NamedBindName, [CounterKey]()
		{
			FBindExecutionRecorder::Increment(CounterKey);
		});

		const TArray<FName> AllBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
		TestRunner->TestTrue(TEXT("BindConfig.GlobalDisabledBindNames should expose newly registered named binds"), AllBindNames.Contains(NamedBindName));

		TSet<FName> AllowedBindNames;
		AllowedBindNames.Add(NamedBindName);

		ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));
		TestRunner->TestEqual(TEXT("BindConfig.GlobalDisabledBindNames should execute the named bind when it is enabled"), FBindExecutionRecorder::Get(CounterKey), 1);

		FBindExecutionRecorder::Reset(CounterKey);
		Settings->DisabledBindNames = { NamedBindName };

		FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		FAngelscriptEngine Engine(Config, Dependencies);
		FAngelscriptEngineScope EngineScope(Engine);
		const TSet<FName> MergedDisabledBindNames = FAngelscriptBindConfigTestAccess::CollectDisabledBindNames(Engine);
		TestRunner->TestTrue(TEXT("BindConfig.GlobalDisabledBindNames should merge the settings-level disabled bind name"), MergedDisabledBindNames.Contains(NamedBindName));

		TSet<FName> DisabledBindNames = BuildDisabledSetExcluding(AllBindNames, AllowedBindNames);
		DisabledBindNames.Append(MergedDisabledBindNames);
		ExecuteIsolatedBinds(DisabledBindNames);

		TestRunner->TestEqual(TEXT("BindConfig.GlobalDisabledBindNames should skip execution when disabled in settings"), FBindExecutionRecorder::Get(CounterKey), 0);

		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList(MergedDisabledBindNames);
		const FAngelscriptBinds::FBindInfo* NamedBindInfo = FindBindInfoByName(BindInfos, NamedBindName);
		if (!TestRunner->TestNotNull(TEXT("BindConfig.GlobalDisabledBindNames should expose bind info for the named bind"), NamedBindInfo))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("BindConfig.GlobalDisabledBindNames should report the disabled named bind as disabled"), NamedBindInfo->bEnabled);
	}

	TEST_METHOD(EngineDisabledBindNames)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("BindConfig.EngineDisabledBindNames should access mutable settings"), Settings))
		{
			return;
		}

		const TArray<FName> PreviousDisabledBindNames = Settings->DisabledBindNames;
		ON_SCOPE_EXIT
		{
			Settings->DisabledBindNames = PreviousDisabledBindNames;
		};
		Settings->DisabledBindNames.Reset();

		const FName NamedBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Engine"));
		const FName CounterKey = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Engine.Counter"));
		FBindExecutionRecorder::Reset(CounterKey);

		FAngelscriptBinds::FBind NamedBind(NamedBindName, [CounterKey]()
		{
			FBindExecutionRecorder::Increment(CounterKey);
		});

		const TArray<FName> AllBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
		TestRunner->TestTrue(TEXT("BindConfig.EngineDisabledBindNames should expose the named bind through the query API"), AllBindNames.Contains(NamedBindName));

		TSet<FName> AllowedBindNames;
		AllowedBindNames.Add(NamedBindName);

		ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));
		TestRunner->TestEqual(TEXT("BindConfig.EngineDisabledBindNames should execute the named bind before engine-level filtering is applied"), FBindExecutionRecorder::Get(CounterKey), 1);

		FBindExecutionRecorder::Reset(CounterKey);

		FAngelscriptEngineConfig Config;
		Config.DisabledBindNames.Add(NamedBindName);
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		FAngelscriptEngine Engine(Config, Dependencies);
		FAngelscriptEngineScope EngineScope(Engine);
		const TSet<FName> MergedDisabledBindNames = FAngelscriptBindConfigTestAccess::CollectDisabledBindNames(Engine);
		TestRunner->TestTrue(TEXT("BindConfig.EngineDisabledBindNames should include the engine-level disabled bind name"), MergedDisabledBindNames.Contains(NamedBindName));

		TSet<FName> DisabledBindNames = BuildDisabledSetExcluding(AllBindNames, AllowedBindNames);
		DisabledBindNames.Append(MergedDisabledBindNames);
		ExecuteIsolatedBinds(DisabledBindNames);

		TestRunner->TestEqual(TEXT("BindConfig.EngineDisabledBindNames should skip execution when disabled in the engine config"), FBindExecutionRecorder::Get(CounterKey), 0);
	}

	TEST_METHOD(UnnamedBindBackwardCompatibility)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		const TArray<FName> BaselineBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
		const FName CounterKey = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Unnamed.Counter"));
		FBindExecutionRecorder::Reset(CounterKey);

		FAngelscriptBinds::FBind UnnamedBind([CounterKey]()
		{
			FBindExecutionRecorder::Increment(CounterKey);
		}, nullptr);

		const TArray<FName> AllBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
		const TArray<FName> NewBindNames = FindNewBindNames(BaselineBindNames, AllBindNames);

		FName GeneratedUnnamedBindName = NAME_None;
		for (const FName& NewBindName : NewBindNames)
		{
			if (NewBindName.ToString().StartsWith(TEXT("UnnamedBind_")))
			{
				GeneratedUnnamedBindName = NewBindName;
				break;
			}
		}

		if (!TestRunner->TestFalse(TEXT("BindConfig.UnnamedBindBackwardCompatibility should register at least one new bind name"), NewBindNames.IsEmpty()))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("BindConfig.UnnamedBindBackwardCompatibility should auto-generate an unnamed bind name"), GeneratedUnnamedBindName != NAME_None))
		{
			return;
		}

		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList();
		const FAngelscriptBinds::FBindInfo* UnnamedBindInfo = FindBindInfoByName(BindInfos, GeneratedUnnamedBindName);
		if (!TestRunner->TestNotNull(TEXT("BindConfig.UnnamedBindBackwardCompatibility should expose bind info for the unnamed bind"), UnnamedBindInfo))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("BindConfig.UnnamedBindBackwardCompatibility should default unnamed bind order to zero"), UnnamedBindInfo->BindOrder, 0);
		TestRunner->TestTrue(TEXT("BindConfig.UnnamedBindBackwardCompatibility should report unnamed binds as enabled by default"), UnnamedBindInfo->bEnabled);

		TSet<FName> AllowedBindNames;
		AllowedBindNames.Add(GeneratedUnnamedBindName);
		ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));

		TestRunner->TestEqual(TEXT("BindConfig.UnnamedBindBackwardCompatibility should continue executing unnamed binds"), FBindExecutionRecorder::Get(CounterKey), 1);
	}

	TEST_METHOD(StartupBindInfoPreservesOrder)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		const FName EarlyBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.StartupOrder.Early"));
		const FName LateBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.StartupOrder.Late"));
		FAngelscriptBinds::FBind EarlyBind(EarlyBindName, -100, []() {});
		FAngelscriptBinds::FBind LateBind(LateBindName, 100, []() {});

		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList();
		const int32 EarlyInfoIndex = FindBindIndexByName(BindInfos, EarlyBindName);
		const int32 LateInfoIndex = FindBindIndexByName(BindInfos, LateBindName);
		if (!TestRunner->TestTrue(TEXT("BindConfig.StartupBindInfoPreservesOrder should expose the early named bind in bind info"), EarlyInfoIndex != INDEX_NONE)
			|| !TestRunner->TestTrue(TEXT("BindConfig.StartupBindInfoPreservesOrder should expose the late named bind in bind info"), LateInfoIndex != INDEX_NONE))
		{
			return;
		}

		const FAngelscriptBindExecutionSnapshot Snapshot = ObserveStartupBindPass(FAngelscriptEngineConfig());
		if (!TestRunner->TestEqual(TEXT("BindConfig.StartupBindInfoPreservesOrder should observe a single startup bind pass"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 1))
		{
			return;
		}

		const int32 EarlyExecutionIndex = Snapshot.ExecutedBindNames.IndexOfByKey(EarlyBindName);
		const int32 LateExecutionIndex = Snapshot.ExecutedBindNames.IndexOfByKey(LateBindName);
		if (!TestRunner->TestTrue(TEXT("BindConfig.StartupBindInfoPreservesOrder should execute the early named bind during startup"), EarlyExecutionIndex != INDEX_NONE)
			|| !TestRunner->TestTrue(TEXT("BindConfig.StartupBindInfoPreservesOrder should execute the late named bind during startup"), LateExecutionIndex != INDEX_NONE))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("BindConfig.StartupBindInfoPreservesOrder should sort bind info by bind order"), EarlyInfoIndex < LateInfoIndex);
		TestRunner->TestTrue(TEXT("BindConfig.StartupBindInfoPreservesOrder should preserve the same order in the startup bind pass"), EarlyExecutionIndex < LateExecutionIndex);
	}

	TEST_METHOD(StartupPathMergesDisabledBindNames)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("BindConfig.StartupPathMergesDisabledBindNames should access mutable settings"), Settings))
		{
			return;
		}

		const TArray<FName> PreviousDisabledBindNames = Settings->DisabledBindNames;
		ON_SCOPE_EXIT
		{
			Settings->DisabledBindNames = PreviousDisabledBindNames;
		};
		Settings->DisabledBindNames.Reset();

		const FName SettingsDisabledBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Startup.SettingsDisabled"));
		const FName EngineDisabledBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Startup.EngineDisabled"));
		const FName EnabledBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.Startup.Enabled"));
		FAngelscriptBinds::FBind SettingsDisabledBind(SettingsDisabledBindName, []() {});
		FAngelscriptBinds::FBind EngineDisabledBind(EngineDisabledBindName, []() {});
		FAngelscriptBinds::FBind EnabledBind(EnabledBindName, []() {});

		Settings->DisabledBindNames = { SettingsDisabledBindName };
		FAngelscriptEngineConfig Config;
		Config.DisabledBindNames.Add(EngineDisabledBindName);

		const FAngelscriptBindExecutionSnapshot Snapshot = ObserveStartupBindPass(Config);
		if (!TestRunner->TestEqual(TEXT("BindConfig.StartupPathMergesDisabledBindNames should observe one startup bind pass"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 1))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("BindConfig.StartupPathMergesDisabledBindNames should surface the settings-level disabled bind in the observed startup pass"), Snapshot.DisabledBindNames.Contains(SettingsDisabledBindName));
		TestRunner->TestTrue(TEXT("BindConfig.StartupPathMergesDisabledBindNames should surface the engine-level disabled bind in the observed startup pass"), Snapshot.DisabledBindNames.Contains(EngineDisabledBindName));
		TestRunner->TestFalse(TEXT("BindConfig.StartupPathMergesDisabledBindNames should skip the settings-disabled bind during startup"), Snapshot.ExecutedBindNames.Contains(SettingsDisabledBindName));
		TestRunner->TestFalse(TEXT("BindConfig.StartupPathMergesDisabledBindNames should skip the engine-disabled bind during startup"), Snapshot.ExecutedBindNames.Contains(EngineDisabledBindName));
		TestRunner->TestTrue(TEXT("BindConfig.StartupPathMergesDisabledBindNames should keep enabled binds visible in the startup execution list"), Snapshot.ExecutedBindNames.Contains(EnabledBindName));
	}

	TEST_METHOD(GeneratedBlueprintCallableEntriesPopulateClassMaps)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		UFunction* DestroyActorFunction = AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor"));
		UFunction* GetPlayerControllerFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController"));
		UFunction* IsDeveloperOnlyFunction = UASClass::StaticClass()->FindFunctionByName(TEXT("IsDeveloperOnly"));
		if (!TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find AActor::K2_DestroyActor"), DestroyActorFunction)
			|| !TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find UGameplayStatics::GetPlayerController"), GetPlayerControllerFunction)
			|| !TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find UASClass::IsDeveloperOnly"), IsDeveloperOnlyFunction))
		{ return; }

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		auto& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		const TMap<FString, FFuncEntry>* ActorEntries = ClassFuncMaps.Find(AActor::StaticClass());
		const TMap<FString, FFuncEntry>* GameplayStaticsEntries = ClassFuncMaps.Find(UGameplayStatics::StaticClass());
		const TMap<FString, FFuncEntry>* ScriptClassEntries = ClassFuncMaps.Find(UASClass::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for AActor"), ActorEntries)
			|| !TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for UGameplayStatics"), GameplayStaticsEntries)
			|| !TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for UASClass"), ScriptClassEntries))
		{ return; }

		const FFuncEntry* DestroyActorEntry = ActorEntries->Find(DestroyActorFunction->GetName());
		const FFuncEntry* GetPlayerControllerEntry = GameplayStaticsEntries->Find(GetPlayerControllerFunction->GetName());
		const FFuncEntry* IsDeveloperOnlyEntry = ScriptClassEntries->Find(IsDeveloperOnlyFunction->GetName());
		if (!TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register AActor::K2_DestroyActor"), DestroyActorEntry)
			|| !TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register UGameplayStatics::GetPlayerController"), GetPlayerControllerEntry)
			|| !TestRunner->TestNotNull(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register UASClass::IsDeveloperOnly"), IsDeveloperOnlyEntry))
		{ return; }

		TestRunner->TestTrue(TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should bind UASClass::IsDeveloperOnly to a direct native function entry"), IsFunctionEntryBound(*IsDeveloperOnlyEntry));
	}

	TEST_METHOD(AddFunctionEntryPreservesFirstRegistration)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); };

		const FString FunctionName = TEXT("K2_DestroyActor");
		const FFuncEntry FirstEntry = { ERASE_METHOD_PTR(AActor, K2_DestroyActor, (), ERASE_ARGUMENT_PACK(void)) };
		const FFuncEntry SecondEntry = { ERASE_NO_FUNCTION() };
		FAngelscriptBinds::AddFunctionEntry(AActor::StaticClass(), FunctionName, FirstEntry);
		FAngelscriptBinds::AddFunctionEntry(AActor::StaticClass(), FunctionName, SecondEntry);

		const TMap<FString, FFuncEntry>* ActorEntries = FAngelscriptBinds::GetClassFuncMaps().Find(AActor::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("AddFunctionEntryPreservesFirstRegistration should create a function entry map for AActor"), ActorEntries)) { return; }
		const FFuncEntry* StoredEntry = ActorEntries->Find(FunctionName);
		if (!TestRunner->TestNotNull(TEXT("AddFunctionEntryPreservesFirstRegistration should keep the first function entry"), StoredEntry)) { return; }

		TestRunner->TestTrue(TEXT("AddFunctionEntryPreservesFirstRegistration should keep the first registration bound"), IsFunctionEntryBound(*StoredEntry));
		TestRunner->TestTrue(TEXT("AddFunctionEntryPreservesFirstRegistration should preserve the first stored function pointer and caller"), AreFunctionEntriesEqual(*StoredEntry, FirstEntry));
		TestRunner->TestFalse(TEXT("AddFunctionEntryPreservesFirstRegistration should ignore the later duplicate registration"), AreFunctionEntriesEqual(*StoredEntry, SecondEntry));
	}

	TEST_METHOD(BlueprintInternalUseOnlyCanBeOverriddenForAngelscript)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		UFunction* WithOverride = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("InternalCallableWithOverride"));
		UFunction* WithoutOverride = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("InternalCallableWithoutOverride"));
		if (!TestRunner->TestNotNull(TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should find the override test function"), WithOverride)
			|| !TestRunner->TestNotNull(TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should find the control test function"), WithoutOverride))
		{ return; }

		TestRunner->TestTrue(TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should keep the control function marked as BlueprintInternalUseOnly"), WithoutOverride->HasMetaData(TEXT("BlueprintInternalUseOnly")));
		TestRunner->TestTrue(TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should mark the override function as UsableInAngelscript"), WithOverride->HasMetaData(TEXT("UsableInAngelscript")));
		TestRunner->TestFalse(TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should not skip override-marked functions"), FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(WithOverride));
		TestRunner->TestTrue(TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should still skip BlueprintInternalUseOnly functions without an override"), FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(WithoutOverride));
	}

	TEST_METHOD(FunctionLevelScriptMethodUsesFirstParameterAsMixin)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* ScriptMethodFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("GetCoverageValue"));
		if (!TestRunner->TestTrue(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should resolve a host type for signature construction"), HostType.IsValid())
			|| !TestRunner->TestNotNull(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should find the ScriptMethod test function"), ScriptMethodFunction))
		{ return; }

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), ScriptMethodFunction);
		TestRunner->TestTrue(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should keep the Unreal function static"), Signature.bStaticInUnreal);
		TestRunner->TestFalse(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should bind ScriptMethod functions as script members"), Signature.bStaticInScript);
		TestRunner->TestEqual(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should remove the first parameter from the exposed signature"), Signature.ArgumentTypes.Num(), 0);
		TestRunner->TestTrue(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should expose a const member declaration when the first parameter is const"), Signature.Declaration.Contains(TEXT("const")));
		TestRunner->TestTrue(TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should keep the generated script name"), Signature.Declaration.Contains(TEXT("GetCoverageValue")));
	}

	TEST_METHOD(CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* RequiredWorldContextFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		UFunction* OptionalWorldContextFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("CallableWithoutWorldContext"));
		if (!TestRunner->TestTrue(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should resolve a host type for signature construction"), HostType.IsValid())
			|| !TestRunner->TestNotNull(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should find the required world-context function"), RequiredWorldContextFunction)
			|| !TestRunner->TestNotNull(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should find the optional world-context function"), OptionalWorldContextFunction))
		{ return; }

		FAngelscriptFunctionSignature RequiredSignature(HostType.ToSharedRef(), RequiredWorldContextFunction);
		FAngelscriptFunctionSignature OptionalSignature(HostType.ToSharedRef(), OptionalWorldContextFunction);
		int RequiredFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(RequiredSignature.Declaration, &NoOpGeneric);
		int OptionalFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(OptionalSignature.Declaration, &NoOpGeneric);
		RequiredSignature.ModifyScriptFunction(RequiredFunctionId);
		OptionalSignature.ModifyScriptFunction(OptionalFunctionId);

		auto* RequiredScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(RequiredFunctionId));
		auto* OptionalScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(OptionalFunctionId));
		if (!TestRunner->TestNotNull(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a script function for the required world-context case"), RequiredScriptFunction)
			|| !TestRunner->TestNotNull(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a script function for the optional world-context case"), OptionalScriptFunction))
		{ return; }

		TestRunner->TestEqual(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should hide the world-context argument for required functions"), RequiredScriptFunction->hiddenArgumentIndex, 0);
		TestRunner->TestEqual(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should hide the world-context argument for callable-without-world-context functions"), OptionalScriptFunction->hiddenArgumentIndex, 0);
		TestRunner->TestTrue(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should mark required world-context functions with the world-context trait"), RequiredScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT));
		TestRunner->TestFalse(TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should not mark callable-without-world-context functions with the world-context trait"), OptionalScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT));
	}

	TEST_METHOD(ScriptAllowTemporaryThisAppendsAcceptTemporaryThis)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* TemporaryThisFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("GetTemporaryThisValue"));
		if (!TestRunner->TestTrue(TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should resolve the host type"), HostType.IsValid())
			|| !TestRunner->TestNotNull(TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should find the test function"), TemporaryThisFunction))
		{ return; }

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), TemporaryThisFunction);
		TestRunner->TestTrue(TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should bind ScriptMethod functions as members"), !Signature.bStaticInScript);
		TestRunner->TestTrue(TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should append accept_temporary_this to the declaration"), Signature.Declaration.Contains(TEXT(" accept_temporary_this")));
	}

	TEST_METHOD(UnsafeDuringActorConstructionSetsUnsafeTrait)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* UnsafeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("UnsafeDuringConstruction"));
		UFunction* SafeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("SafeDuringConstruction"));
		if (!TestRunner->TestTrue(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should resolve the host type"), HostType.IsValid())
			|| !TestRunner->TestNotNull(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should find the unsafe test function"), UnsafeFunction)
			|| !TestRunner->TestNotNull(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should find the safe test function"), SafeFunction))
		{ return; }

		FAngelscriptFunctionSignature UnsafeSignature(HostType.ToSharedRef(), UnsafeFunction);
		FAngelscriptFunctionSignature SafeSignature(HostType.ToSharedRef(), SafeFunction);
		const int UnsafeFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(UnsafeSignature.Declaration, &NoOpGeneric);
		const int SafeFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(SafeSignature.Declaration, &NoOpGeneric);
		UnsafeSignature.ModifyScriptFunction(UnsafeFunctionId);
		SafeSignature.ModifyScriptFunction(SafeFunctionId);

		auto* UnsafeScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(UnsafeFunctionId));
		auto* SafeScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(SafeFunctionId));
		if (!TestRunner->TestNotNull(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create the unsafe script function"), UnsafeScriptFunction)
			|| !TestRunner->TestNotNull(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create the safe script function"), SafeScriptFunction))
		{ return; }

		TestRunner->TestTrue(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should mark meta-present functions as unsafe during construction"), UnsafeScriptFunction->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION));
		TestRunner->TestFalse(TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should not mark explicit false meta functions as unsafe during construction"), SafeScriptFunction->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION));
	}

	TEST_METHOD(OverloadedExportedFunctionsCanRecoverDirectBind)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		UFunction* OverloadFunction = UAngelscriptUhtOverloadCoverageLibrary::StaticClass()->FindFunctionByName(TEXT("ResolveCoverageOverload"));
		if (!TestRunner->TestNotNull(TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should find the reflected overload function"), OverloadFunction)) { return; }

		const TMap<FString, FFuncEntry>* OverloadEntries = FAngelscriptBinds::GetClassFuncMaps().Find(UAngelscriptUhtOverloadCoverageLibrary::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should populate entries for the overload test library"), OverloadEntries)) { return; }

		const FFuncEntry* OverloadEntry = OverloadEntries->Find(OverloadFunction->GetName());
		if (!TestRunner->TestNotNull(TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should register the reflected overload function"), OverloadEntry)) { return; }

		TestRunner->TestTrue(TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"), IsFunctionEntryBound(*OverloadEntry));
	}

	TEST_METHOD(InlineDefinitionFunctionsCanRecoverDirectBind)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		UFunction* InlineFunction = URuntimeFloatCurveMixinLibrary::StaticClass()->FindFunctionByName(TEXT("GetNumKeys"));
		if (!TestRunner->TestNotNull(TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should find the reflected inline function"), InlineFunction)) { return; }
		const TMap<FString, FFuncEntry>* InlineEntries = FAngelscriptBinds::GetClassFuncMaps().Find(URuntimeFloatCurveMixinLibrary::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should populate entries for the inline function library"), InlineEntries)) { return; }
		const FFuncEntry* InlineEntry = InlineEntries->Find(InlineFunction->GetName());
		if (!TestRunner->TestNotNull(TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should register the reflected inline function"), InlineEntry)) { return; }

		TestRunner->TestTrue(TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"), IsFunctionEntryBound(*InlineEntry));
	}

	TEST_METHOD(InlineOutRefFunctionsCanRecoverDirectBind)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindConfigTests_Private;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("InlineOutRefFunctionsCanRecoverDirectBind should create a testing engine"), Engine.IsValid())) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		UFunction* InlineFunction = URuntimeFloatCurveMixinLibrary::StaticClass()->FindFunctionByName(TEXT("GetTimeRange"));
		if (!TestRunner->TestNotNull(TEXT("InlineOutRefFunctionsCanRecoverDirectBind should find the reflected out-ref function"), InlineFunction)) { return; }
		const TMap<FString, FFuncEntry>* InlineEntries = FAngelscriptBinds::GetClassFuncMaps().Find(URuntimeFloatCurveMixinLibrary::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("InlineOutRefFunctionsCanRecoverDirectBind should populate entries for the inline function library"), InlineEntries)) { return; }
		const FFuncEntry* InlineEntry = InlineEntries->Find(InlineFunction->GetName());
		if (!TestRunner->TestNotNull(TEXT("InlineOutRefFunctionsCanRecoverDirectBind should register the reflected out-ref function"), InlineEntry)) { return; }

		TestRunner->TestTrue(TEXT("InlineOutRefFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"), IsFunctionEntryBound(*InlineEntry));
	}
};

#endif
