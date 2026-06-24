#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptUhtCoverageTestTypes.h"
#include "Binds/Helper_FunctionSignature.h"
#include "Testing/AngelscriptUhtOverloadCoverageTypes.h"
#include "ClassGenerator/ASClass.h"
#include "AngelscriptTestUtilities.h"
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


TEST_CLASS_WITH_FLAGS(FAngelscriptBindConfigTests,
	"Angelscript.TestModule.Engine.BindConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

static FName MakeUniqueBindTestName(const TCHAR* Prefix)
{
	return FName(*FString::Printf(TEXT("%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

static TArray<FName> FindNewBindNames(const TArray<FName>& BeforeNames, const TArray<FName>& AfterNames)
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

static TSet<FName> BuildDisabledSetExcluding(const TArray<FName>& AllBindNames, const TSet<FName>& AllowedNames)
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

static void ExecuteIsolatedBinds(const TSet<FName>& DisabledBindNames)
{
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	FAngelscriptBindConfigTestAccess::CallBinds(DisabledBindNames);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
}

static FAngelscriptBindExecutionSnapshot ObserveStartupBindPass(const FAngelscriptEngineConfig& Config)
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

static int32 FindBindIndexByName(const TArray<FAngelscriptBinds::FBindInfo>& BindInfos, const FName BindName)
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

static const FAngelscriptBinds::FBindInfo* FindBindInfoByName(const TArray<FAngelscriptBinds::FBindInfo>& BindInfos, const FName BindName)
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

static bool IsFunctionEntryBound(const FFuncEntry& Entry)
{
	FGenericFuncPtr FuncPtr = Entry.FuncPtr;
	return FuncPtr.IsBound() && Entry.Caller.IsBound();
}

static bool AreFunctionEntriesEqual(const FFuncEntry& Left, const FFuncEntry& Right)
{
	return FMemory::Memcmp(&Left.FuncPtr, &Right.FuncPtr, sizeof(FGenericFuncPtr)) == 0 &&
		FMemory::Memcmp(&Left.Caller, &Right.Caller, sizeof(ASAutoCaller::FunctionCaller)) == 0;
}

static void CDECL NoOpGeneric(asIScriptGeneric* Generic)
{
	(void)Generic;
}

public:
	TEST_METHOD(GlobalDisabledBindNames)
	{
UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!this->Assert.IsNotNull(Settings, TEXT("BindConfig.GlobalDisabledBindNames should access mutable settings")))
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
		bool bOk = this->Assert.IsTrue(AllBindNames.Contains(NamedBindName), TEXT("BindConfig.GlobalDisabledBindNames should expose newly registered named binds"));

		TSet<FName> AllowedBindNames;
		AllowedBindNames.Add(NamedBindName);

		ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));
		bOk &= this->Assert.AreEqual(1, FBindExecutionRecorder::Get(CounterKey), TEXT("BindConfig.GlobalDisabledBindNames should execute the named bind when it is enabled"));

		FBindExecutionRecorder::Reset(CounterKey);
		Settings->DisabledBindNames = { NamedBindName };

		FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		FAngelscriptEngine Engine(Config, Dependencies);
		FAngelscriptEngineScope EngineScope(Engine);
		const TSet<FName> MergedDisabledBindNames = FAngelscriptBindConfigTestAccess::CollectDisabledBindNames(Engine);
		bOk &= this->Assert.IsTrue(MergedDisabledBindNames.Contains(NamedBindName), TEXT("BindConfig.GlobalDisabledBindNames should merge the settings-level disabled bind name"));

		TSet<FName> DisabledBindNames = BuildDisabledSetExcluding(AllBindNames, AllowedBindNames);
		DisabledBindNames.Append(MergedDisabledBindNames);
		ExecuteIsolatedBinds(DisabledBindNames);

		bOk &= this->Assert.AreEqual(0, FBindExecutionRecorder::Get(CounterKey), TEXT("BindConfig.GlobalDisabledBindNames should skip execution when disabled in settings"));

		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList(MergedDisabledBindNames);
		const FAngelscriptBinds::FBindInfo* NamedBindInfo = FindBindInfoByName(BindInfos, NamedBindName);
		if (!this->Assert.IsNotNull(NamedBindInfo, TEXT("BindConfig.GlobalDisabledBindNames should expose bind info for the named bind")))
		{
			return;
		}

		bOk &= this->Assert.IsFalse(NamedBindInfo->bEnabled, TEXT("BindConfig.GlobalDisabledBindNames should report the disabled named bind as disabled"));
		(void)bOk;
	}

	TEST_METHOD(EngineDisabledBindNames)
	{
UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!this->Assert.IsNotNull(Settings, TEXT("BindConfig.EngineDisabledBindNames should access mutable settings")))
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
		bool bOk = this->Assert.IsTrue(AllBindNames.Contains(NamedBindName), TEXT("BindConfig.EngineDisabledBindNames should expose the named bind through the query API"));

		TSet<FName> AllowedBindNames;
		AllowedBindNames.Add(NamedBindName);

		ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));
		bOk &= this->Assert.AreEqual(1, FBindExecutionRecorder::Get(CounterKey), TEXT("BindConfig.EngineDisabledBindNames should execute the named bind before engine-level filtering is applied"));

		FBindExecutionRecorder::Reset(CounterKey);

		FAngelscriptEngineConfig Config;
		Config.DisabledBindNames.Add(NamedBindName);
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		FAngelscriptEngine Engine(Config, Dependencies);
		FAngelscriptEngineScope EngineScope(Engine);
		const TSet<FName> MergedDisabledBindNames = FAngelscriptBindConfigTestAccess::CollectDisabledBindNames(Engine);
		bOk &= this->Assert.IsTrue(MergedDisabledBindNames.Contains(NamedBindName), TEXT("BindConfig.EngineDisabledBindNames should include the engine-level disabled bind name"));

		TSet<FName> DisabledBindNames = BuildDisabledSetExcluding(AllBindNames, AllowedBindNames);
		DisabledBindNames.Append(MergedDisabledBindNames);
		ExecuteIsolatedBinds(DisabledBindNames);

		bOk &= this->Assert.AreEqual(0, FBindExecutionRecorder::Get(CounterKey), TEXT("BindConfig.EngineDisabledBindNames should skip execution when disabled in the engine config"));
		(void)bOk;
	}

	TEST_METHOD(UnnamedBindBackwardCompatibility)
	{
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

		if (!this->Assert.IsFalse(NewBindNames.IsEmpty(), TEXT("BindConfig.UnnamedBindBackwardCompatibility should register at least one new bind name")))
		{
			return;
		}

		if (!this->Assert.IsTrue(GeneratedUnnamedBindName != NAME_None, TEXT("BindConfig.UnnamedBindBackwardCompatibility should auto-generate an unnamed bind name")))
		{
			return;
		}

		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList();
		const FAngelscriptBinds::FBindInfo* UnnamedBindInfo = FindBindInfoByName(BindInfos, GeneratedUnnamedBindName);
		if (!this->Assert.IsNotNull(UnnamedBindInfo, TEXT("BindConfig.UnnamedBindBackwardCompatibility should expose bind info for the unnamed bind")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.AreEqual(0, UnnamedBindInfo->BindOrder, TEXT("BindConfig.UnnamedBindBackwardCompatibility should default unnamed bind order to zero"));
		bOk &= this->Assert.IsTrue(UnnamedBindInfo->bEnabled, TEXT("BindConfig.UnnamedBindBackwardCompatibility should report unnamed binds as enabled by default"));

		TSet<FName> AllowedBindNames;
		AllowedBindNames.Add(GeneratedUnnamedBindName);
		ExecuteIsolatedBinds(BuildDisabledSetExcluding(AllBindNames, AllowedBindNames));

		bOk &= this->Assert.AreEqual(1, FBindExecutionRecorder::Get(CounterKey), TEXT("BindConfig.UnnamedBindBackwardCompatibility should continue executing unnamed binds"));
		(void)bOk;
	}

	TEST_METHOD(StartupBindInfoPreservesOrder)
	{
const FName EarlyBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.StartupOrder.Early"));
		const FName LateBindName = MakeUniqueBindTestName(TEXT("Automation.BindConfig.StartupOrder.Late"));
		FAngelscriptBinds::FBind EarlyBind(EarlyBindName, -100, []() {});
		FAngelscriptBinds::FBind LateBind(LateBindName, 100, []() {});

		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList();
		const int32 EarlyInfoIndex = FindBindIndexByName(BindInfos, EarlyBindName);
		const int32 LateInfoIndex = FindBindIndexByName(BindInfos, LateBindName);
		if (!this->Assert.IsTrue(EarlyInfoIndex != INDEX_NONE, TEXT("BindConfig.StartupBindInfoPreservesOrder should expose the early named bind in bind info"))
			|| !this->Assert.IsTrue(LateInfoIndex != INDEX_NONE, TEXT("BindConfig.StartupBindInfoPreservesOrder should expose the late named bind in bind info")))
		{
			return;
		}

		const FAngelscriptBindExecutionSnapshot Snapshot = ObserveStartupBindPass(FAngelscriptEngineConfig());
		if (!this->Assert.AreEqual(1, FAngelscriptBindExecutionObservation::GetInvocationCount(), TEXT("BindConfig.StartupBindInfoPreservesOrder should observe a single startup bind pass")))
		{
			return;
		}

		const int32 EarlyExecutionIndex = Snapshot.ExecutedBindNames.IndexOfByKey(EarlyBindName);
		const int32 LateExecutionIndex = Snapshot.ExecutedBindNames.IndexOfByKey(LateBindName);
		if (!this->Assert.IsTrue(EarlyExecutionIndex != INDEX_NONE, TEXT("BindConfig.StartupBindInfoPreservesOrder should execute the early named bind during startup"))
			|| !this->Assert.IsTrue(LateExecutionIndex != INDEX_NONE, TEXT("BindConfig.StartupBindInfoPreservesOrder should execute the late named bind during startup")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.IsTrue(EarlyInfoIndex < LateInfoIndex, TEXT("BindConfig.StartupBindInfoPreservesOrder should sort bind info by bind order"));
		bOk &= this->Assert.IsTrue(EarlyExecutionIndex < LateExecutionIndex, TEXT("BindConfig.StartupBindInfoPreservesOrder should preserve the same order in the startup bind pass"));
		(void)bOk;
	}

	TEST_METHOD(StartupPathMergesDisabledBindNames)
	{
UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!this->Assert.IsNotNull(Settings, TEXT("BindConfig.StartupPathMergesDisabledBindNames should access mutable settings")))
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
		if (!this->Assert.AreEqual(1, FAngelscriptBindExecutionObservation::GetInvocationCount(), TEXT("BindConfig.StartupPathMergesDisabledBindNames should observe one startup bind pass")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.IsTrue(Snapshot.DisabledBindNames.Contains(SettingsDisabledBindName), TEXT("BindConfig.StartupPathMergesDisabledBindNames should surface the settings-level disabled bind in the observed startup pass"));
		bOk &= this->Assert.IsTrue(Snapshot.DisabledBindNames.Contains(EngineDisabledBindName), TEXT("BindConfig.StartupPathMergesDisabledBindNames should surface the engine-level disabled bind in the observed startup pass"));
		bOk &= this->Assert.IsFalse(Snapshot.ExecutedBindNames.Contains(SettingsDisabledBindName), TEXT("BindConfig.StartupPathMergesDisabledBindNames should skip the settings-disabled bind during startup"));
		bOk &= this->Assert.IsFalse(Snapshot.ExecutedBindNames.Contains(EngineDisabledBindName), TEXT("BindConfig.StartupPathMergesDisabledBindNames should skip the engine-disabled bind during startup"));
		bOk &= this->Assert.IsTrue(Snapshot.ExecutedBindNames.Contains(EnabledBindName), TEXT("BindConfig.StartupPathMergesDisabledBindNames should keep enabled binds visible in the startup execution list"));
		(void)bOk;
	}

	TEST_METHOD(GeneratedBlueprintCallableEntriesPopulateClassMaps)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		UFunction* DestroyActorFunction = AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor"));
		UFunction* GetPlayerControllerFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController"));
		UFunction* IsDeveloperOnlyFunction = UASClass::StaticClass()->FindFunctionByName(TEXT("IsDeveloperOnly"));
		if (!this->Assert.IsNotNull(DestroyActorFunction, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find AActor::K2_DestroyActor"))
			|| !this->Assert.IsNotNull(GetPlayerControllerFunction, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find UGameplayStatics::GetPlayerController"))
			|| !this->Assert.IsNotNull(IsDeveloperOnlyFunction, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find UASClass::IsDeveloperOnly")))
		{ return; }

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		auto& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		const TMap<FString, FFuncEntry>* ActorEntries = ClassFuncMaps.Find(AActor::StaticClass());
		const TMap<FString, FFuncEntry>* GameplayStaticsEntries = ClassFuncMaps.Find(UGameplayStatics::StaticClass());
		const TMap<FString, FFuncEntry>* ScriptClassEntries = ClassFuncMaps.Find(UASClass::StaticClass());
		if (!this->Assert.IsNotNull(ActorEntries, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for AActor"))
			|| !this->Assert.IsNotNull(GameplayStaticsEntries, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for UGameplayStatics"))
			|| !this->Assert.IsNotNull(ScriptClassEntries, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for UASClass")))
		{ return; }

		const FFuncEntry* DestroyActorEntry = ActorEntries->Find(DestroyActorFunction->GetName());
		const FFuncEntry* GetPlayerControllerEntry = GameplayStaticsEntries->Find(GetPlayerControllerFunction->GetName());
		const FFuncEntry* IsDeveloperOnlyEntry = ScriptClassEntries->Find(IsDeveloperOnlyFunction->GetName());
		if (!this->Assert.IsNotNull(DestroyActorEntry, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register AActor::K2_DestroyActor"))
			|| !this->Assert.IsNotNull(GetPlayerControllerEntry, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register UGameplayStatics::GetPlayerController"))
			|| !this->Assert.IsNotNull(IsDeveloperOnlyEntry, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register UASClass::IsDeveloperOnly")))
		{ return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*IsDeveloperOnlyEntry), TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should bind UASClass::IsDeveloperOnly to a direct native function entry"));
	}

	TEST_METHOD(AddFunctionEntryPreservesFirstRegistration)
	{
FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); };

		const FString FunctionName = TEXT("K2_DestroyActor");
		const FFuncEntry FirstEntry = { ERASE_METHOD_PTR(AActor, K2_DestroyActor, (), ERASE_ARGUMENT_PACK(void)) };
		const FFuncEntry SecondEntry = { ERASE_NO_FUNCTION() };
		FAngelscriptBinds::AddFunctionEntry(AActor::StaticClass(), FunctionName, FirstEntry);
		FAngelscriptBinds::AddFunctionEntry(AActor::StaticClass(), FunctionName, SecondEntry);

		const TMap<FString, FFuncEntry>* ActorEntries = FAngelscriptBinds::GetClassFuncMaps().Find(AActor::StaticClass());
		if (!this->Assert.IsNotNull(ActorEntries, TEXT("AddFunctionEntryPreservesFirstRegistration should create a function entry map for AActor"))) { return; }
		const FFuncEntry* StoredEntry = ActorEntries->Find(FunctionName);
		if (!this->Assert.IsNotNull(StoredEntry, TEXT("AddFunctionEntryPreservesFirstRegistration should keep the first function entry"))) { return; }

		bool bOk = true;
		bOk &= this->Assert.IsTrue(IsFunctionEntryBound(*StoredEntry), TEXT("AddFunctionEntryPreservesFirstRegistration should keep the first registration bound"));
		bOk &= this->Assert.IsTrue(AreFunctionEntriesEqual(*StoredEntry, FirstEntry), TEXT("AddFunctionEntryPreservesFirstRegistration should preserve the first stored function pointer and caller"));
		bOk &= this->Assert.IsFalse(AreFunctionEntriesEqual(*StoredEntry, SecondEntry), TEXT("AddFunctionEntryPreservesFirstRegistration should ignore the later duplicate registration"));
		(void)bOk;
	}

	TEST_METHOD(BlueprintInternalUseOnlyCanBeOverriddenForAngelscript)
	{
UFunction* WithOverride = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("InternalCallableWithOverride"));
		UFunction* WithoutOverride = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("InternalCallableWithoutOverride"));
		if (!this->Assert.IsNotNull(WithOverride, TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should find the override test function"))
			|| !this->Assert.IsNotNull(WithoutOverride, TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should find the control test function")))
		{ return; }

		bool bOk = true;
		bOk &= this->Assert.IsTrue(WithoutOverride->HasMetaData(TEXT("BlueprintInternalUseOnly")), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should keep the control function marked as BlueprintInternalUseOnly"));
		bOk &= this->Assert.IsTrue(WithOverride->HasMetaData(TEXT("UsableInAngelscript")), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should mark the override function as UsableInAngelscript"));
		bOk &= this->Assert.IsFalse(FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(WithOverride), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should not skip override-marked functions"));
		bOk &= this->Assert.IsTrue(FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(WithoutOverride), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should still skip BlueprintInternalUseOnly functions without an override"));
		(void)bOk;
	}

	TEST_METHOD(FunctionLevelScriptMethodUsesFirstParameterAsMixin)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* ScriptMethodFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("GetCoverageValue"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should resolve a host type for signature construction"))
			|| !this->Assert.IsNotNull(ScriptMethodFunction, TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should find the ScriptMethod test function")))
		{ return; }

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), ScriptMethodFunction);
		bool bOk = true;
		bOk &= this->Assert.IsTrue(Signature.bStaticInUnreal, TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should keep the Unreal function static"));
		bOk &= this->Assert.IsFalse(Signature.bStaticInScript, TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should bind ScriptMethod functions as script members"));
		bOk &= this->Assert.AreEqual(0, Signature.ArgumentTypes.Num(), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should remove the first parameter from the exposed signature"));
		bOk &= this->Assert.IsTrue(Signature.Declaration.Contains(TEXT("const")), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should expose a const member declaration when the first parameter is const"));
		bOk &= this->Assert.IsTrue(Signature.Declaration.Contains(TEXT("GetCoverageValue")), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should keep the generated script name"));
		(void)bOk;
	}

	TEST_METHOD(CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* RequiredWorldContextFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		UFunction* OptionalWorldContextFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("CallableWithoutWorldContext"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should resolve a host type for signature construction"))
			|| !this->Assert.IsNotNull(RequiredWorldContextFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should find the required world-context function"))
			|| !this->Assert.IsNotNull(OptionalWorldContextFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should find the optional world-context function")))
		{ return; }

		FAngelscriptFunctionSignature RequiredSignature(HostType.ToSharedRef(), RequiredWorldContextFunction);
		FAngelscriptFunctionSignature OptionalSignature(HostType.ToSharedRef(), OptionalWorldContextFunction);
		int RequiredFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(RequiredSignature.Declaration, &NoOpGeneric);
		int OptionalFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(OptionalSignature.Declaration, &NoOpGeneric);
		RequiredSignature.ModifyScriptFunction(RequiredFunctionId);
		OptionalSignature.ModifyScriptFunction(OptionalFunctionId);

		auto* RequiredScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(RequiredFunctionId));
		auto* OptionalScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(OptionalFunctionId));
		if (!this->Assert.IsNotNull(RequiredScriptFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a script function for the required world-context case"))
			|| !this->Assert.IsNotNull(OptionalScriptFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a script function for the optional world-context case")))
		{ return; }

		bool bOk = true;
		bOk &= this->Assert.AreEqual(0, RequiredScriptFunction->hiddenArgumentIndex, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should hide the world-context argument for required functions"));
		bOk &= this->Assert.AreEqual(0, OptionalScriptFunction->hiddenArgumentIndex, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should hide the world-context argument for callable-without-world-context functions"));
		bOk &= this->Assert.IsTrue(RequiredScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should mark required world-context functions with the world-context trait"));
		bOk &= this->Assert.IsFalse(OptionalScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should not mark callable-without-world-context functions with the world-context trait"));
		(void)bOk;
	}

	TEST_METHOD(ScriptAllowTemporaryThisAppendsAcceptTemporaryThis)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* TemporaryThisFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("GetTemporaryThisValue"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should resolve the host type"))
			|| !this->Assert.IsNotNull(TemporaryThisFunction, TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should find the test function")))
		{ return; }

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), TemporaryThisFunction);
		bool bOk = true;
		bOk &= this->Assert.IsTrue(!Signature.bStaticInScript, TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should bind ScriptMethod functions as members"));
		bOk &= this->Assert.IsTrue(Signature.Declaration.Contains(TEXT(" accept_temporary_this")), TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should append accept_temporary_this to the declaration"));
		(void)bOk;
	}

	TEST_METHOD(UnsafeDuringActorConstructionSetsUnsafeTrait)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UObject::StaticClass());
		UFunction* UnsafeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("UnsafeDuringConstruction"));
		UFunction* SafeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("SafeDuringConstruction"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should resolve the host type"))
			|| !this->Assert.IsNotNull(UnsafeFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should find the unsafe test function"))
			|| !this->Assert.IsNotNull(SafeFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should find the safe test function")))
		{ return; }

		FAngelscriptFunctionSignature UnsafeSignature(HostType.ToSharedRef(), UnsafeFunction);
		FAngelscriptFunctionSignature SafeSignature(HostType.ToSharedRef(), SafeFunction);
		const int UnsafeFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(UnsafeSignature.Declaration, &NoOpGeneric);
		const int SafeFunctionId = FAngelscriptBinds::BindGlobalGenericFunction(SafeSignature.Declaration, &NoOpGeneric);
		UnsafeSignature.ModifyScriptFunction(UnsafeFunctionId);
		SafeSignature.ModifyScriptFunction(SafeFunctionId);

		auto* UnsafeScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(UnsafeFunctionId));
		auto* SafeScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(SafeFunctionId));
		if (!this->Assert.IsNotNull(UnsafeScriptFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create the unsafe script function"))
			|| !this->Assert.IsNotNull(SafeScriptFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create the safe script function")))
		{ return; }

		bool bOk = true;
		bOk &= this->Assert.IsTrue(UnsafeScriptFunction->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should mark meta-present functions as unsafe during construction"));
		bOk &= this->Assert.IsFalse(SafeScriptFunction->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should not mark explicit false meta functions as unsafe during construction"));
		(void)bOk;
	}

	TEST_METHOD(OverloadedExportedFunctionsCanRecoverDirectBind)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		UFunction* OverloadFunction = UAngelscriptUhtOverloadCoverageLibrary::StaticClass()->FindFunctionByName(TEXT("ResolveCoverageOverload"));
		if (!this->Assert.IsNotNull(OverloadFunction, TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should find the reflected overload function"))) { return; }

		const TMap<FString, FFuncEntry>* OverloadEntries = FAngelscriptBinds::GetClassFuncMaps().Find(UAngelscriptUhtOverloadCoverageLibrary::StaticClass());
		if (!this->Assert.IsNotNull(OverloadEntries, TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should populate entries for the overload test library"))) { return; }

		const FFuncEntry* OverloadEntry = OverloadEntries->Find(OverloadFunction->GetName());
		if (!this->Assert.IsNotNull(OverloadEntry, TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should register the reflected overload function"))) { return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*OverloadEntry), TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"));
	}

	TEST_METHOD(InlineDefinitionFunctionsCanRecoverDirectBind)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		UFunction* InlineFunction = URuntimeFloatCurveMixinLibrary::StaticClass()->FindFunctionByName(TEXT("GetNumKeys"));
		if (!this->Assert.IsNotNull(InlineFunction, TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should find the reflected inline function"))) { return; }
		const TMap<FString, FFuncEntry>* InlineEntries = FAngelscriptBinds::GetClassFuncMaps().Find(URuntimeFloatCurveMixinLibrary::StaticClass());
		if (!this->Assert.IsNotNull(InlineEntries, TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should populate entries for the inline function library"))) { return; }
		const FFuncEntry* InlineEntry = InlineEntries->Find(InlineFunction->GetName());
		if (!this->Assert.IsNotNull(InlineEntry, TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should register the reflected inline function"))) { return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*InlineEntry), TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"));
	}

	TEST_METHOD(InlineOutRefFunctionsCanRecoverDirectBind)
	{
DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); }
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT { FAngelscriptBinds::ResetBindState(); DestroySharedTestEngine(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptBindConfigTestAccess::DestroyGlobalEngine(); } };

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("InlineOutRefFunctionsCanRecoverDirectBind should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);

		UFunction* InlineFunction = URuntimeFloatCurveMixinLibrary::StaticClass()->FindFunctionByName(TEXT("GetTimeRange"));
		if (!this->Assert.IsNotNull(InlineFunction, TEXT("InlineOutRefFunctionsCanRecoverDirectBind should find the reflected out-ref function"))) { return; }
		const TMap<FString, FFuncEntry>* InlineEntries = FAngelscriptBinds::GetClassFuncMaps().Find(URuntimeFloatCurveMixinLibrary::StaticClass());
		if (!this->Assert.IsNotNull(InlineEntries, TEXT("InlineOutRefFunctionsCanRecoverDirectBind should populate entries for the inline function library"))) { return; }
		const FFuncEntry* InlineEntry = InlineEntries->Find(InlineFunction->GetName());
		if (!this->Assert.IsNotNull(InlineEntry, TEXT("InlineOutRefFunctionsCanRecoverDirectBind should register the reflected out-ref function"))) { return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*InlineEntry), TEXT("InlineOutRefFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"));
	}
};

#endif
