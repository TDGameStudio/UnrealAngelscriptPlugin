#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "UObject/UObjectGlobals.h"

#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheProductionClassCompilerReuseTests,
	"Angelscript.TestModule.Cache.ProductionClassCompilerReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheProductionClassCompilerReuse"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionClassCompilerReuse/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionClassCompilerReuse/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(
			const FString& RelativePath,
			const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source,
				*(ScriptRoot / RelativePath),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	static const FAngelscriptCacheDecisionEvent* FindFunctionEvent(
		const FAngelscriptCacheDecisionTraceSnapshot& Trace,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const EAngelscriptCacheDecisionOutcome Outcome)
	{
		return Trace.Events.FindByPredicate(
			[&FunctionKey, Outcome](
				const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Stage
						== EAngelscriptCacheDecisionStage::FunctionLookup
					&& Event.Outcome == Outcome
					&& Event.FunctionKey.IsSet()
					&& Event.FunctionKey->Hash == FunctionKey.Hash;
			});
	}

public:
	TEST_METHOD(BodyOnlyEditRestoresUnchangedClassMethodAndCompilesChangedMethod)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString ColdTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheProductionClass%s : UObject
			{
				UFUNCTION()
				int ReadUnchanged()
				{
					return 1101;
				}

				UFUNCTION()
				int ReadChanged()
				{
					return 1102;
				}
			}
			)AS");
		const FString WarmTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheProductionClass%s : UObject
			{
				UFUNCTION()
				int ReadUnchanged()
				{
					return 1101;
				}

				UFUNCTION()
				int ReadChanged()
				{
					return 2102;
				}
			}
			)AS");
		const FString ColdSource = ColdTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		const FString WarmSource = WarmTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ClassBodyEdit.as"), ColdSource)));

		FAngelscriptHash256 ColdGenerationId;
		FAngelscriptHash256 ColdSourceSnapshot;
		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();

			const FAngelscriptCacheLifecyclePublications ColdPublications =
				Cold->GetCacheService()->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(ColdPublications.Current.IsValid()));
			ASSERT_THAT(AreEqual(1, ColdPublications.Current->Modules.Num(),
				TEXT("cold class capture must publish the admitted module")));
			ColdSourceSnapshot = ColdPublications.Current->SourceSnapshot;
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ClassBodyEdit.as"), WarmSource)));
		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 256);
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Warm->GetModuleByModuleName(TEXT("ClassBodyEdit"));
		ASSERT_THAT(IsTrue(Module.IsValid()));
		ASSERT_THAT(IsNotNull(Module->ScriptModule));
		ASSERT_THAT(AreEqual(1, Module->Classes.Num()));
		const TSharedRef<FAngelscriptClassDesc>& Class = Module->Classes[0];
		ASSERT_THAT(IsNotNull(Class->ScriptType));
		ASSERT_THAT(IsNotNull(Class->Class));
		asIScriptFunction* UnchangedFunction =
			Class->ScriptType->GetMethodByDecl("int ReadUnchanged()");
		asIScriptFunction* ChangedFunction =
			Class->ScriptType->GetMethodByDecl("int ReadChanged()");
		ASSERT_THAT(IsNotNull(UnchangedFunction));
		ASSERT_THAT(IsNotNull(ChangedFunction));

		FAngelscriptStableModuleKey ModuleKey;
		FString KeyFailure;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
				*Module, ModuleKey, &KeyFailure),
			*KeyFailure));
		FAngelscriptStableFunctionKey UnchangedKey;
		FAngelscriptStableFunctionKey ChangedKey;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey,
				*static_cast<asCScriptFunction*>(UnchangedFunction),
				UnchangedKey,
				&KeyFailure),
			*KeyFailure));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey,
				*static_cast<asCScriptFunction*>(ChangedFunction),
				ChangedKey,
				&KeyFailure),
			*KeyFailure));

		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), Class->Class);
		ASSERT_THAT(IsNotNull(Instance));
		FFunctionInvoker UnchangedInvoker(
			*TestRunner, Instance, FName(TEXT("ReadUnchanged")));
		FFunctionInvoker ChangedInvoker(
			*TestRunner, Instance, FName(TEXT("ReadChanged")));
		ASSERT_THAT(IsTrue(UnchangedInvoker.IsValid()));
		ASSERT_THAT(IsTrue(ChangedInvoker.IsValid()));
		const int32 UnchangedValue =
			UnchangedInvoker.CallAndReturn<int32>(INDEX_NONE);
		const int32 ChangedValue =
			ChangedInvoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(1101, UnchangedValue));
		ASSERT_THAT(AreEqual(2102, ChangedValue));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* DirectInputMiss =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Miss
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheExactStartupReason::
								DirectInputMismatch);
				});
		const FAngelscriptCacheDecisionEvent* UnchangedHit =
			FindFunctionEvent(
				Trace, UnchangedKey,
				EAngelscriptCacheDecisionOutcome::Restored);
		const FAngelscriptCacheDecisionEvent* ChangedCompile =
			FindFunctionEvent(
				Trace, ChangedKey,
				EAngelscriptCacheDecisionOutcome::Compiled);
		for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
		{
			if (Event.Stage != EAngelscriptCacheDecisionStage::FunctionLookup)
			{
				continue;
			}
			TestRunner->AddInfo(FString::Printf(
				TEXT("Class reuse trace: Outcome=%u Domain=%u Reason=%u Module=%s Function=%s Candidate=%s Detail=%s"),
				static_cast<uint32>(Event.Outcome),
				static_cast<uint32>(Event.ReasonDomain),
				Event.ReasonCode,
				Event.ModuleKeys.IsEmpty()
					? TEXT("none")
					: *Event.ModuleKeys[0].Hash.ToHexString(),
				Event.FunctionKey.IsSet()
					? *Event.FunctionKey->Hash.ToHexString()
					: TEXT("none"),
				Event.ExpectedCoordinate.IsSet()
					? *Event.ExpectedCoordinate->ToHexString()
					: TEXT("none"),
				*Event.Detail));
		}
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production class body-edit observation: Candidate=%s ColdSource=%s CurrentSource=%s Module=%s Unchanged=%s Changed=%s Values=%d/%d TraceEvents=%d Restored=%d Compiled=%d"),
			*ColdGenerationId.ToHexString(),
			*ColdSourceSnapshot.ToHexString(),
			*Service->GetLifecyclePublications().Current->SourceSnapshot.
				ToHexString(),
			*ModuleKey.Hash.ToHexString(),
			*UnchangedKey.Hash.ToHexString(),
			*ChangedKey.Hash.ToHexString(),
			UnchangedValue,
			ChangedValue,
			Trace.Events.Num(),
			UnchangedHit != nullptr ? 1 : 0,
			ChangedCompile != nullptr ? 1 : 0));
		ASSERT_THAT(IsNotNull(DirectInputMiss));
		ASSERT_THAT(IsNotNull(UnchangedHit,
			TEXT("body-only class edit must restore the unchanged method before its compiler closure")));
		ASSERT_THAT(IsNotNull(ChangedCompile,
			TEXT("body-only class edit must compile the changed method")));
		ASSERT_THAT(IsTrue(UnchangedHit->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			UnchangedHit->ExpectedCoordinate.GetValue() == ColdGenerationId));
		ASSERT_THAT(IsTrue(ChangedCompile->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			ChangedCompile->ExpectedCoordinate.GetValue() == ColdGenerationId));
		ASSERT_THAT(IsNull(FindFunctionEvent(
			Trace, ChangedKey, EAngelscriptCacheDecisionOutcome::Restored)));
		ASSERT_THAT(IsNull(FindFunctionEvent(
			Trace, UnchangedKey, EAngelscriptCacheDecisionOutcome::Compiled)));

		const FAngelscriptCacheLifecyclePublications WarmPublications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(WarmPublications.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, WarmPublications.Current->Modules.Num()));
		ASSERT_THAT(IsTrue(
			WarmPublications.Current->SourceSnapshot != ColdSourceSnapshot));
		ASSERT_THAT(IsFalse(WarmPublications.Current->bRestoredFromStore));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
