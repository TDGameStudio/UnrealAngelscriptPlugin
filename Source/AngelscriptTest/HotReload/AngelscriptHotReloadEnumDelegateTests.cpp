#include "CQTest.h"
#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadEnumDelegateTests,
	"Angelscript.TestModule.HotReload.ReloadDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName EnumCreatedWarmupModuleName = FName(TEXT("HotReloadEnumCreatedWarmupMod"));
	inline static const FString EnumCreatedWarmupFilename = FString(TEXT("HotReloadEnumCreatedWarmupMod.as"));
	inline static const FName EnumCreatedModuleName = FName(TEXT("HotReloadEnumCreatedMod"));
	inline static const FString EnumCreatedFilename = FString(TEXT("HotReloadEnumCreatedMod.as"));
	inline static const FString EnumCreatedName = FString(TEXT("EHotReloadCreatedState"));

	inline static const FName EnumChangedModuleName = FName(TEXT("HotReloadEnumChangedMod"));
	inline static const FString EnumChangedFilename = FString(TEXT("HotReloadEnumChangedMod.as"));
	inline static const FString EnumChangedName = FString(TEXT("EHotReloadChangedState"));
	inline static const FName EnumChangedCarrierClassName = FName(TEXT("UHotReloadEnumChangedCarrier"));

	struct FEnumCreatedObservation
	{
		int32 EnumCreatedCount = 0;
		int32 EnumChangedCount = 0;
		UEnum* EnumCreatedDuringCompile = nullptr;
		FString EnumCreatedNameDuringCompile;
	};

	struct FEnumChangedObservation
	{
		int32 EnumCreatedCount = 0;
		int32 EnumChangedCount = 0;
		int32 FullReloadCount = 0;
		int32 PostReloadCount = 0;
		bool bPostReloadSawFullReload = false;
		bool bEnumVisibleDuringPostReload = false;
		bool bCarrierVisibleDuringPostReload = false;
		UEnum* EnumSeenDuringReload = nullptr;
		TArray<TPair<FName, int64>> OldNamesSeenDuringReload;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool ContainsEnumEntryWithSuffix(const TArray<TPair<FName, int64>>& Entries, const TCHAR* Suffix, const int64 ExpectedValue)
	{
		const FString ExpectedSuffix(Suffix);
		for (const TPair<FName, int64>& Entry : Entries)
		{
			if (Entry.Value == ExpectedValue && Entry.Key.ToString().EndsWith(ExpectedSuffix))
			{
				return true;
			}
		}

		return false;
	}

	static bool TryFindEnumValueBySuffix(const UEnum& Enum, const TCHAR* Suffix, int64& OutValue)
	{
		const FString ExpectedSuffix(Suffix);
		const int32 EnumeratorsToCheck = Enum.NumEnums();
		for (int32 Index = 0; Index < EnumeratorsToCheck; ++Index)
		{
			const FString EnumEntryName = Enum.GetNameByIndex(Index).ToString();
			if (EnumEntryName.EndsWith(TEXT("_MAX")))
			{
				continue;
			}

			if (EnumEntryName.EndsWith(ExpectedSuffix))
			{
				OutValue = Enum.GetValueByIndex(Index);
				return true;
			}
		}

		return false;
	}

	static asIScriptModule* FindScriptModule(FAngelscriptEngine& Engine, FName ModuleName)
	{
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ModuleName.ToString());
		return ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	}

	static bool ExecuteIntGlobal(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const TCHAR* Declaration,
		const int32 ExpectedResult,
		const TCHAR* Context)
	{
		asIScriptModule* Module = FindScriptModule(Engine, ModuleName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Module, TEXT("Enum reload-delegate test should expose a script module")))
		{
			return false;
		}

		FAngelscriptTestExecutor Executor(Test, Engine, *Module, Declaration);
		if (!LocalAssert.IsTrue(Executor.IsValid(), TEXT("Enum reload-delegate test should resolve the global entry function")))
		{
			return false;
		}

		const int32 Actual = Executor.ExecuteAndGet<int32>(INDEX_NONE);
		return LocalAssert.AreEqual(ExpectedResult, Actual, Context);
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

	TEST_METHOD(BroadcastEnumCreatedOnFirstCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FEnumCreatedObservation Observation;
			FDelegateHandle EnumCreatedHandle;
			FDelegateHandle EnumChangedHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnEnumCreated().Remove(EnumCreatedHandle);
				Engine.GetOnEnumChanged().Remove(EnumChangedHandle);
				Engine.DiscardModule(*EnumCreatedModuleName.ToString());
				Engine.DiscardModule(*EnumCreatedWarmupModuleName.ToString());
			};

			const FString WarmupSource = ASTEST_AS(R"AS(
				UCLASS()
				class UEnumCreatedWarmupCarrier : UObject
				{
					UPROPERTY()
					int Revision = 1;
				}
				)AS");

			const FString EnumCreatedSource = ASTEST_AS(R"AS(
				UENUM(BlueprintType)
				enum class EHotReloadCreatedState : uint8
				{
					Alpha,
					Beta
				}

				int RunCreatedEnumProbe()
				{
					EHotReloadCreatedState State = EHotReloadCreatedState::Beta;
					int Result = State == EHotReloadCreatedState::Beta ? 2 : 0;
					Log(n"HotReloadEnumDelegateTests", "Created V1 RunCreatedEnumProbe State=Beta Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, EnumCreatedWarmupModuleName, EnumCreatedWarmupFilename, WarmupSource),
				TEXT("Enum-created delegate test warmup compile should succeed")));

			ASSERT_THAT(IsTrue(Engine.IsInitialCompileFinished(), TEXT("Warmup compile should mark the initial compile as finished")));

			EnumCreatedHandle = Engine.GetOnEnumCreated().AddLambda(
				[&Observation](UEnum* Enum)
				{
					++Observation.EnumCreatedCount;
					Observation.EnumCreatedDuringCompile = Enum;
					Observation.EnumCreatedNameDuringCompile = Enum != nullptr ? Enum->GetName() : FString();
				});

			EnumChangedHandle = Engine.GetOnEnumChanged().AddLambda(
				[&Observation](UEnum*, EnumNameList)
				{
					++Observation.EnumChangedCount;
				});

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, EnumCreatedModuleName, EnumCreatedFilename, EnumCreatedSource),
				TEXT("First enum-declaring module compile should succeed")));

			const TSharedPtr<FAngelscriptEnumDesc> CreatedEnumDesc = Engine.GetEnum(EnumCreatedName);
			ASSERT_THAT(IsTrue(CreatedEnumDesc.IsValid(), TEXT("Engine should register the created enum after the first compile")));

			UEnum* CreatedEnumObject = CreatedEnumDesc->Enum;
			ASSERT_THAT(IsNotNull(CreatedEnumObject, TEXT("Engine should expose a live UEnum for the created enum")));

			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				EnumCreatedModuleName,
				TEXT("int RunCreatedEnumProbe()"),
				2,
				TEXT("Created enum AS probe should execute and log the Beta path"))));

			ASSERT_THAT(AreEqual(1, Observation.EnumCreatedCount, TEXT("OnEnumCreated should broadcast once for the first created enum")));
			ASSERT_THAT(AreEqual(0, Observation.EnumChangedCount, TEXT("OnEnumChanged should not broadcast when the enum is first created")));
			ASSERT_THAT(AreEqual(EnumCreatedName, Observation.EnumCreatedNameDuringCompile, TEXT("OnEnumCreated should broadcast the created enum name")));
			ASSERT_THAT(AreEqual(CreatedEnumObject, Observation.EnumCreatedDuringCompile, TEXT("OnEnumCreated should expose the same enum object registered on the engine")));
		}
	}

	TEST_METHOD(BroadcastEnumChangedOnFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FEnumChangedObservation Observation;
			FDelegateHandle EnumCreatedHandle;
			FDelegateHandle EnumChangedHandle;
			FDelegateHandle FullReloadHandle;
			FDelegateHandle PostReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnEnumCreated().Remove(EnumCreatedHandle);
				Engine.GetOnEnumChanged().Remove(EnumChangedHandle);
				Engine.GetOnFullReload().Remove(FullReloadHandle);
				Engine.GetOnPostReload().Remove(PostReloadHandle);
				Engine.DiscardModule(*EnumChangedModuleName.ToString());
			};

			const FString EnumChangedV1Source = ASTEST_AS(R"AS(
				UENUM(BlueprintType)
				enum class EHotReloadChangedState : uint16
				{
					Alpha,
					Beta = 4
				}

				UCLASS()
				class UHotReloadEnumChangedCarrier : UObject
				{
					UPROPERTY()
					EHotReloadChangedState State;

					default State = EHotReloadChangedState::Alpha;
				}

				int RunChangedEnumProbe()
				{
					EHotReloadChangedState State = EHotReloadChangedState::Beta;
					int Result = State == EHotReloadChangedState::Beta ? 4 : 0;
					Log(n"HotReloadEnumDelegateTests", "Changed V1 RunChangedEnumProbe State=Beta Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, EnumChangedModuleName, EnumChangedFilename, EnumChangedV1Source),
				TEXT("Initial enum-changed module compile should succeed")));

			const TSharedPtr<FAngelscriptEnumDesc> EnumBeforeReload = Engine.GetEnum(EnumChangedName);
			ASSERT_THAT(IsTrue(EnumBeforeReload.IsValid(), TEXT("Enum metadata should exist before enum-changed reload test")));

			UEnum* EnumObjectBeforeReload = EnumBeforeReload->Enum;
			ASSERT_THAT(IsNotNull(EnumObjectBeforeReload, TEXT("Enum object should exist before enum-changed reload")));

			ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, EnumChangedCarrierClassName), TEXT("Enum carrier class should exist before enum-changed reload")));

			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				EnumChangedModuleName,
				TEXT("int RunChangedEnumProbe()"),
				4,
				TEXT("Changed enum AS V1 probe should execute and log the Beta path"))));

			EnumCreatedHandle = Engine.GetOnEnumCreated().AddLambda(
				[&Observation](UEnum*)
				{
					++Observation.EnumCreatedCount;
				});

			EnumChangedHandle = Engine.GetOnEnumChanged().AddLambda(
				[&Observation](UEnum* Enum, EnumNameList OldNames)
				{
					++Observation.EnumChangedCount;
					Observation.EnumSeenDuringReload = Enum;
					Observation.OldNamesSeenDuringReload = OldNames;
				});

			FullReloadHandle = Engine.GetOnFullReload().AddLambda(
				[&Observation]()
				{
					++Observation.FullReloadCount;
				});

			PostReloadHandle = Engine.GetOnPostReload().AddLambda(
				[&Engine, &Observation](const bool bWasFullReload)
				{
					++Observation.PostReloadCount;
					Observation.bPostReloadSawFullReload = bWasFullReload;
					Observation.bEnumVisibleDuringPostReload = Engine.GetEnum(EnumChangedName).IsValid();
					Observation.bCarrierVisibleDuringPostReload = FindGeneratedClass(&Engine, EnumChangedCarrierClassName) != nullptr;
				});

			const FString EnumChangedV2Source = ASTEST_AS(R"AS(
				UENUM(BlueprintType)
				enum class EHotReloadChangedState : uint16
				{
					Alpha,
					Beta = 4,
					Gamma = 9
				}

				UCLASS()
				class UHotReloadEnumChangedCarrier : UObject
				{
					UPROPERTY()
					EHotReloadChangedState State;

					default State = EHotReloadChangedState::Gamma;
				}

				int RunChangedEnumProbe()
				{
					EHotReloadChangedState State = EHotReloadChangedState::Gamma;
					int Result = State == EHotReloadChangedState::Gamma ? 9 : 0;
					Log(n"HotReloadEnumDelegateTests", "Changed V2 RunChangedEnumProbe State=Gamma Result=" + Result);
					return Result;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, EnumChangedModuleName, EnumChangedFilename, EnumChangedV2Source, ReloadResult),
				TEXT("Enum-changed full reload compile should succeed")));

			ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Enum-changed full reload should stay on a handled reload path")));

			const TSharedPtr<FAngelscriptEnumDesc> EnumAfterReload = Engine.GetEnum(EnumChangedName);
			ASSERT_THAT(IsTrue(EnumAfterReload.IsValid(), TEXT("Enum metadata should still exist after enum-changed full reload")));

			UEnum* EnumObjectAfterReload = EnumAfterReload->Enum;
			ASSERT_THAT(IsNotNull(EnumObjectAfterReload, TEXT("Reloaded enum object should still be queryable")));

			ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, EnumChangedCarrierClassName), TEXT("Enum carrier class should still be queryable after full reload")));

			ASSERT_THAT(AreEqual(0, Observation.EnumCreatedCount, TEXT("Enum full reload should not rebroadcast enum-created for an existing enum")));
			ASSERT_THAT(AreEqual(1, Observation.FullReloadCount, TEXT("Enum full reload should broadcast the full-reload delegate exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.EnumChangedCount, TEXT("Enum full reload should broadcast the enum-changed delegate exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.PostReloadCount, TEXT("Enum full reload should broadcast the post-reload delegate exactly once")));
			ASSERT_THAT(IsTrue(Observation.bPostReloadSawFullReload, TEXT("Post-reload delegate should report that the completed reload was a full reload")));
			ASSERT_THAT(IsTrue(Observation.bEnumVisibleDuringPostReload, TEXT("Enum should already be queryable when post-reload broadcasts")));
			ASSERT_THAT(IsTrue(Observation.bCarrierVisibleDuringPostReload, TEXT("Carrier class should already be queryable when post-reload broadcasts")));
			ASSERT_THAT(AreEqual(EnumObjectAfterReload, Observation.EnumSeenDuringReload, TEXT("Enum-changed broadcast should expose the live enum object")));

			ASSERT_THAT(AreEqual(2, Observation.OldNamesSeenDuringReload.Num(), TEXT("Enum-changed broadcast should preserve the old enum member count")));
			ASSERT_THAT(IsTrue(ContainsEnumEntryWithSuffix(Observation.OldNamesSeenDuringReload, TEXT("Alpha"), 0), TEXT("Old enum member list should keep Alpha before reload")));
			ASSERT_THAT(IsTrue(ContainsEnumEntryWithSuffix(Observation.OldNamesSeenDuringReload, TEXT("Beta"), 4), TEXT("Old enum member list should keep Beta before reload")));
			ASSERT_THAT(IsFalse(ContainsEnumEntryWithSuffix(Observation.OldNamesSeenDuringReload, TEXT("Gamma"), 9), TEXT("Old enum member list should not include the new Gamma value")));

			int64 GammaValue = INDEX_NONE;
			ASSERT_THAT(IsTrue(TryFindEnumValueBySuffix(*EnumObjectAfterReload, TEXT("Gamma"), GammaValue), TEXT("Reloaded enum should expose the new Gamma enumerator")));
			ASSERT_THAT(AreEqual(int64(9), GammaValue, TEXT("Reloaded enum should assign the expected value to Gamma")));

			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				EnumChangedModuleName,
				TEXT("int RunChangedEnumProbe()"),
				9,
				TEXT("Changed enum AS V2 probe should execute and log the Gamma path"))));
		}
	}
};
