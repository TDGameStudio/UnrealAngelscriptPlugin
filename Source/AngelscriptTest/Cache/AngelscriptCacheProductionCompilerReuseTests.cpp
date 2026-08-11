#include "Cache/AngelscriptCacheDecisionTrace.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "Shared/AngelscriptTestEngineHelper.h"

#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheProductionCompilerReuseTests,
	"Angelscript.TestModule.Cache.ProductionCompilerReuse",
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
				TEXT("Automation/AngelscriptCacheProductionCompilerReuse"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionCompilerReuse/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionCompilerReuse/")))
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
	TEST_METHOD(PartialPersistedGenerationRestoresEligibleFunctionDuringInitialCompile)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString CacheableTemplate = ASTEST_AS(R"AS(
			enum ECacheProductionReuse%sState
			{
				Ready = 1,
			}

			int ReadCacheProductionReuse%sValue()
			{
				return 901;
			}
			)AS");
		const FString CacheableSource = CacheableTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		const FString UnsupportedTemplate = ASTEST_AS(R"AS(
			enum ECacheProductionReuseUnsupported%sFirst
			{
				Ready = 1,
			}

			enum ECacheProductionReuseUnsupported%sSecond
			{
				Ready = 2,
			}

			int ReadCacheProductionReuseUnsupported%sValue()
			{
				return 902;
			}
			)AS");
		const FString UnsupportedSource = UnsupportedTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Cacheable.as"), CacheableSource)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Unsupported.as"), UnsupportedSource)));

		FAngelscriptHash256 ColdGenerationId;
		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();

			const FAngelscriptCacheLifecyclePublications ColdPublications =
				Cold->GetCacheService()->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(ColdPublications.Current.IsValid()));
			ASSERT_THAT(AreEqual(1, ColdPublications.Current->Modules.Num(),
				TEXT("cold capture must persist only the admitted module")));
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}

		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 128);
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> CacheableModule =
			Warm->GetModuleByModuleName(TEXT("Cacheable"));
		const TSharedPtr<FAngelscriptModuleDesc> UnsupportedModule =
			Warm->GetModuleByModuleName(TEXT("Unsupported"));
		ASSERT_THAT(IsTrue(CacheableModule.IsValid()));
		ASSERT_THAT(IsTrue(UnsupportedModule.IsValid()));

		int32 CacheableValue = 0;
		int32 UnsupportedValue = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(),
			FName(TEXT("Cacheable")),
			TEXT("int ReadCacheProductionReuse") + Unique + TEXT("Value()"),
			CacheableValue)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(),
			FName(TEXT("Unsupported")),
			TEXT("int ReadCacheProductionReuseUnsupported") + Unique
				+ TEXT("Value()"),
			UnsupportedValue)));
		ASSERT_THAT(AreEqual(901, CacheableValue));
		ASSERT_THAT(AreEqual(902, UnsupportedValue));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* ExactMiss =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Miss
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheExactStartupReason::ModuleSetMismatch);
				});
		const FAngelscriptCacheDecisionEvent* FunctionHit =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::FunctionLookup
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Restored
						&& Event.ReasonDomain
							== EAngelscriptCacheDecisionReasonDomain::FunctionLookup;
				});
		const FAngelscriptCacheDecisionEvent* CaptureSkip =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::SuccessfulPublication
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::NotCacheable
						&& Event.ReasonDomain
							== EAngelscriptCacheDecisionReasonDomain::CleanCapture;
				});
		ASSERT_THAT(IsNotNull(ExactMiss));
		ASSERT_THAT(IsNotNull(FunctionHit,
			TEXT("safe exact-start miss must retain the selected Generation and restore the admitted function before its compiler closure")));
		ASSERT_THAT(IsNotNull(CaptureSkip));
		ASSERT_THAT(IsFalse(CaptureSkip->Detail.IsEmpty(),
			TEXT("a production Clean Capture skip must retain its bounded typed failure detail")));
		ASSERT_THAT(IsTrue(FunctionHit->FunctionKey.IsSet()));
		ASSERT_THAT(AreEqual(1, FunctionHit->ModuleKeys.Num()));
		ASSERT_THAT(IsTrue(FunctionHit->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			FunctionHit->ExpectedCoordinate.GetValue() == ColdGenerationId));
		const FAngelscriptCacheDiagnosticSnapshot Diagnostic =
			Service->CaptureDiagnosticSnapshot();
		ASSERT_THAT(IsTrue(Diagnostic.FunctionReuse.bPresent));
		ASSERT_THAT(IsTrue(
			Diagnostic.FunctionReuse.CandidateGenerationId
				== ColdGenerationId));
		ASSERT_THAT(AreEqual(uint32(1),
			Diagnostic.FunctionReuse.CandidateModuleCount));
		ASSERT_THAT(IsTrue(
			Diagnostic.FunctionReuse.RestoredFunctionCount >= 1));

		const FAngelscriptCacheLifecyclePublications WarmPublications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(WarmPublications.Current.IsValid()));
		ASSERT_THAT(IsFalse(WarmPublications.Current->bRestoredFromStore,
			TEXT("hybrid compiler reuse is not an exact whole-Generation restore")));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production hybrid reuse: Generation=%s TraceEvents=%d Function=%s Values=%d/%d PersistedModules=1 CurrentModules=2"),
			*ColdGenerationId.ToHexString(),
			Trace.Events.Num(),
			FunctionHit->FunctionKey.IsSet()
				? *FunctionHit->FunctionKey->Hash.ToHexString() : TEXT("missing"),
			CacheableValue,
			UnsupportedValue));
	}

	TEST_METHOD(BodyOnlyEditRestoresUnchangedFunctionAndCompilesChangedFunctionDuringInitialCompile)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString ColdTemplate = ASTEST_AS(R"AS(
			enum ECacheProductionBodyEdit%sState
			{
				Ready = 1,
			}

			int ReadCacheProductionBodyUnchanged%s()
			{
				return 1001;
			}

			int ReadCacheProductionBodyChanged%s()
			{
				return 1002;
			}
			)AS");
		const FString WarmTemplate = ASTEST_AS(R"AS(
			enum ECacheProductionBodyEdit%sState
			{
				Ready = 1,
			}

			int ReadCacheProductionBodyUnchanged%s()
			{
				return 1001;
			}

			int ReadCacheProductionBodyChanged%s()
			{
				return 2002;
			}
			)AS");
		const FString ColdSource = ColdTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		const FString WarmSource = WarmTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("BodyEdit.as"), ColdSource)));

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
			ASSERT_THAT(AreEqual(1, ColdPublications.Current->Modules.Num()));
			ColdSourceSnapshot = ColdPublications.Current->SourceSnapshot;
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("BodyEdit.as"), WarmSource)));
		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 128);
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Warm->GetModuleByModuleName(TEXT("BodyEdit"));
		ASSERT_THAT(IsTrue(Module.IsValid()));
		ASSERT_THAT(IsNotNull(Module->ScriptModule));
		const FString UnchangedDeclaration =
			TEXT("int ReadCacheProductionBodyUnchanged") + Unique + TEXT("()");
		const FString ChangedDeclaration =
			TEXT("int ReadCacheProductionBodyChanged") + Unique + TEXT("()");
		asCScriptFunction* UnchangedFunction = static_cast<asCScriptFunction*>(
			Module->ScriptModule->GetFunctionByDecl(
				TCHAR_TO_UTF8(*UnchangedDeclaration)));
		asCScriptFunction* ChangedFunction = static_cast<asCScriptFunction*>(
			Module->ScriptModule->GetFunctionByDecl(
				TCHAR_TO_UTF8(*ChangedDeclaration)));
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
				ModuleKey, *UnchangedFunction, UnchangedKey, &KeyFailure),
			*KeyFailure));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *ChangedFunction, ChangedKey, &KeyFailure),
			*KeyFailure));

		int32 UnchangedValue = 0;
		int32 ChangedValue = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(), FName(TEXT("BodyEdit")), UnchangedDeclaration,
			UnchangedValue)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(), FName(TEXT("BodyEdit")), ChangedDeclaration,
			ChangedValue)));
		ASSERT_THAT(AreEqual(1001, UnchangedValue));
		ASSERT_THAT(AreEqual(2002, ChangedValue));

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
		ASSERT_THAT(IsNotNull(DirectInputMiss));
		ASSERT_THAT(IsNotNull(UnchangedHit,
			TEXT("body-only edit must restore the unchanged function before its compiler closure")));
		ASSERT_THAT(IsNotNull(ChangedCompile,
			TEXT("body-only edit must invoke the normal compiler for the changed function")));
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
		const FAngelscriptCacheDiagnosticSnapshot Diagnostic =
			Service->CaptureDiagnosticSnapshot();
		ASSERT_THAT(IsTrue(Diagnostic.FunctionReuse.bPresent));
		ASSERT_THAT(IsTrue(
			Diagnostic.FunctionReuse.CandidateGenerationId
				== ColdGenerationId));
		ASSERT_THAT(AreEqual(uint32(1),
			Diagnostic.FunctionReuse.CandidateModuleCount));
		ASSERT_THAT(IsTrue(
			Diagnostic.FunctionReuse.RestoredFunctionCount >= 1));
		ASSERT_THAT(IsTrue(
			Diagnostic.FunctionReuse.CompiledMissCount >= 1));

		const FAngelscriptCacheLifecyclePublications WarmPublications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(WarmPublications.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, WarmPublications.Current->Modules.Num()));
		ASSERT_THAT(IsTrue(
			WarmPublications.Current->SourceSnapshot != ColdSourceSnapshot));
		ASSERT_THAT(IsFalse(WarmPublications.Current->bRestoredFromStore));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production body-edit hybrid: Candidate=%s ColdSource=%s WarmSource=%s Unchanged=%s Changed=%s Values=%d/%d TraceEvents=%d"),
			*ColdGenerationId.ToHexString(),
			*ColdSourceSnapshot.ToHexString(),
			*WarmPublications.Current->SourceSnapshot.ToHexString(),
			*UnchangedKey.Hash.ToHexString(),
			*ChangedKey.Hash.ToHexString(),
			UnchangedValue,
			ChangedValue,
			Trace.Events.Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
