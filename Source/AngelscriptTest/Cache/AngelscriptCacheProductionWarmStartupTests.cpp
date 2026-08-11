#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "Shared/AngelscriptTestEngineHelper.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheProductionWarmStartupTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheProductionWarmStartup"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionWarmStartup/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionWarmStartup/")))
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
		const FScopedProjectRoot& Project,
		IAngelscriptCacheRestoreFaultInjector* RestoreFaultInjector = nullptr)
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
		Dependencies.CacheRestoreFaultInjector = RestoreFaultInjector;
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	class FStopAtSecondPrepared final
		: public IAngelscriptCacheRestoreFaultInjector
	{
	public:
		virtual bool ShouldStopAt(
			const EAngelscriptCacheRestoreFaultPoint Point,
			const uint32 ModuleOrdinal,
			const FAngelscriptStableModuleKey&) override
		{
			SeenOrdinals.Add(ModuleOrdinal);
			if (!bStopped
				&& Point
					== EAngelscriptCacheRestoreFaultPoint::AfterModulePrepared
				&& ModuleOrdinal == 1)
			{
				bStopped = true;
				if (ObservedEngine != nullptr)
				{
					ActiveModuleCountAtStop =
						ObservedEngine->GetActiveModules().Num();
					const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
						ESPMode::ThreadSafe> Routes =
						ObservedEngine->GetFunctionRouteSnapshot();
					RouteCountAtStop = Routes.IsValid()
						? Routes->FunctionRoutes.Num() : 0;
				}
				return true;
			}
			return false;
		}

		FAngelscriptEngine* ObservedEngine = nullptr;
		TArray<uint32> SeenOrdinals;
		int32 ActiveModuleCountAtStop = -1;
		int32 RouteCountAtStop = -1;
		bool bStopped = false;
	};

	static int32 CountFrontendEvents(
		const TConstArrayView<FAngelscriptCompilationEvent> Events)
	{
		int32 Count = 0;
		for (const FAngelscriptCompilationEvent& Event : Events)
		{
			switch (Event.Type)
			{
			case EAngelscriptCompilationEventType::PreprocessProcessChunks:
			case EAngelscriptCompilationEventType::PreprocessPostProcessCode:
			case EAngelscriptCompilationEventType::CompileModuleParse:
			case EAngelscriptCompilationEventType::CompileModuleCompileCode:
				++Count;
				break;
			default:
				break;
			}
		}
		return Count;
	}

	static FAngelscriptHash256 SnapshotStore(
		const FString& CacheRoot,
		int32& OutFileCount)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(
			Files, *CacheRoot, TEXT("*"), true, false);
		Files.Sort();
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-production-warm-store-v1"));
		Writer.WriteUInt32(static_cast<uint32>(Files.Num()));
		for (const FString& File : Files)
		{
			TArray<uint8> Bytes;
			check(FFileHelper::LoadFileToArray(Bytes, *File));
			FString Relative = File;
			FPaths::MakePathRelativeTo(Relative, *CacheRoot);
			Writer.WriteString(Relative);
			Writer.WriteUInt64(static_cast<uint64>(Bytes.Num()));
			Writer.WriteHash(FAngelscriptHash256{FBlake3::HashBuffer(
				Bytes.GetData(), static_cast<uint64>(Bytes.Num()))});
		}
		OutFileCount = Files.Num();
		return Writer.FinalizeHash();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheProductionWarmStartupTests,
	"Angelscript.TestModule.Cache.ProductionWarmStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SecondEngineConsumesPersistedTwoModuleGenerationBeforeFrontend)
	{
		using namespace AngelscriptCacheProductionWarmStartupTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECacheProductionWarmFirst%sState
{
	Ready = 1,
}

int ReadCacheProductionWarmFirst%sValue()
{
	return 501;
}
)AS"), *Unique, *Unique);
		const FString Second = FString::Printf(TEXT(R"AS(
enum ECacheProductionWarmSecond%sState
{
	Ready = 1,
}

int ReadCacheProductionWarmSecond%sValue()
{
	return 502;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Second.as"), Second)));

		FAngelscriptHash256 ColdGenerationId;
		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Production warm cold flush: Error=%u Lifecycle=%u Attempted=%d Generation=%s Detail=%s"),
				static_cast<uint32>(Flush.Error),
				static_cast<uint32>(Flush.Flush.Error),
				Flush.Flush.Current.bAttempted ? 1 : 0,
				*Flush.Flush.Current.GenerationId.ToHexString(),
				*Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}

		int32 BeforeFileCount = 0;
		const FAngelscriptHash256 BeforeStore = SnapshotStore(
			Project.CacheRoot, BeforeFileCount);
		ASSERT_THAT(IsTrue(BeforeFileCount > 0));

		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> FirstModule =
			Warm->GetModuleByModuleName(TEXT("First"));
		const TSharedPtr<FAngelscriptModuleDesc> SecondModule =
			Warm->GetModuleByModuleName(TEXT("Second"));
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(SecondModule.IsValid()));
		ASSERT_THAT(IsTrue(FirstModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsTrue(SecondModule->bLoadedIncrementalCache));
		ASSERT_THAT(AreEqual(0, CountFrontendEvents(Events)));
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RouteSnapshot =
			Warm->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(RouteSnapshot.IsValid()));
		ASSERT_THAT(AreEqual(2, RouteSnapshot->FunctionRoutes.Num()));
		ASSERT_THAT(AreEqual(uint32(2), RouteSnapshot->VmRouteCount));
		int32 FirstValue = 0;
		int32 SecondValue = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(),
			FName(TEXT("First")),
			TEXT("int ReadCacheProductionWarmFirst") + Unique
				+ TEXT("Value()"),
			FirstValue)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(),
			FName(TEXT("Second")),
			TEXT("int ReadCacheProductionWarmSecond") + Unique
				+ TEXT("Value()"),
			SecondValue)));
		ASSERT_THAT(AreEqual(501, FirstValue));
		ASSERT_THAT(AreEqual(502, SecondValue));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* RestoreEvent =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Restored
						&& Event.ReasonDomain
							== EAngelscriptCacheDecisionReasonDomain::ExactStartup;
				});
		ASSERT_THAT(IsNotNull(RestoreEvent));
		ASSERT_THAT(AreEqual(uint32(2), RestoreEvent->PrimaryCount));
		ASSERT_THAT(IsTrue(RestoreEvent->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			RestoreEvent->ExpectedCoordinate.GetValue() == ColdGenerationId));
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(IsTrue(Publications.Current->bRestoredFromStore));
		ASSERT_THAT(IsTrue(
			Publications.Current->PersistedGenerationId == ColdGenerationId));
		ASSERT_THAT(AreEqual(2, Publications.Current->Modules.Num()));
		const FAngelscriptCacheDiagnosticJsonResult Diagnostic =
			CaptureAngelscriptCacheDiagnosticJson(Warm.Get());
		ASSERT_THAT(IsTrue(Diagnostic.IsSuccess(), *Diagnostic.Detail));
		ASSERT_THAT(IsTrue(Diagnostic.Json.Contains(
			TEXT("\"restoredFromStore\":true"))));
		ASSERT_THAT(IsTrue(Diagnostic.Json.Contains(
			*ColdGenerationId.ToHexString())));
		const FAngelscriptCacheFlushApiResult WarmFlush =
			FlushAngelscriptCacheToStore(Warm.Get(), 5.0);
		ASSERT_THAT(IsTrue(WarmFlush.IsSuccess(), *WarmFlush.Detail));
		ASSERT_THAT(IsFalse(WarmFlush.Flush.Current.bAttempted));

		int32 AfterFileCount = 0;
		const FAngelscriptHash256 AfterStore = SnapshotStore(
			Project.CacheRoot, AfterFileCount);
		ASSERT_THAT(AreEqual(BeforeFileCount, AfterFileCount));
		ASSERT_THAT(AreEqual(
			BeforeStore.ToHexString(), AfterStore.ToHexString()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production warm accepted: Generation=%s FrontendEvents=%d Modules=%u Routes=%d Values=%d/%d StoreFiles=%d StoreDigest=%s"),
			*ColdGenerationId.ToHexString(),
			CountFrontendEvents(Events),
			RestoreEvent->PrimaryCount,
			RouteSnapshot->FunctionRoutes.Num(),
			FirstValue,
			SecondValue,
			AfterFileCount,
			*AfterStore.ToHexString()));
	}

	TEST_METHOD(ChangedSourceMissesBeforeMutationThenCompilesAndPublishesNewCurrent)
	{
		using namespace AngelscriptCacheProductionWarmStartupTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		auto MakeFirst = [&Unique](const int32 Value)
		{
			return FString::Printf(TEXT(R"AS(
enum ECacheProductionChangedFirst%sState
{
	Ready = 1,
}

int ReadCacheProductionChangedFirst%sValue()
{
	return %d;
}
)AS"), *Unique, *Unique, Value);
		};
		const FString Second = FString::Printf(TEXT(R"AS(
enum ECacheProductionChangedSecond%sState
{
	Ready = 1,
}

int ReadCacheProductionChangedSecond%sValue()
{
	return 602;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("First.as"), MakeFirst(601))));
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Second.as"), Second)));

		FAngelscriptHash256 ColdGenerationId;
		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("First.as"), MakeFirst(611))));

		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> FirstModule =
			Warm->GetModuleByModuleName(TEXT("First"));
		const TSharedPtr<FAngelscriptModuleDesc> SecondModule =
			Warm->GetModuleByModuleName(TEXT("Second"));
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(SecondModule.IsValid()));
		ASSERT_THAT(IsFalse(FirstModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsFalse(SecondModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsTrue(CountFrontendEvents(Events) > 0));
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(IsFalse(Publications.Current->bRestoredFromStore));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* MissEvent =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Miss
						&& Event.ReasonDomain
							== EAngelscriptCacheDecisionReasonDomain::ExactStartup
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheExactStartupReason::DirectInputMismatch);
				});
		ASSERT_THAT(IsNotNull(MissEvent));
		ASSERT_THAT(AreEqual(uint32(0), MissEvent->PrimaryCount));

		int32 ChangedValue = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(),
			FName(TEXT("First")),
			TEXT("int ReadCacheProductionChangedFirst") + Unique
				+ TEXT("Value()"),
			ChangedValue)));
		ASSERT_THAT(AreEqual(611, ChangedValue));

		const FAngelscriptCacheFlushApiResult Flush =
			FlushAngelscriptCacheToStore(Warm.Get(), 5.0);
		ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
		ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
		ASSERT_THAT(IsFalse(
			Flush.Flush.Current.GenerationId == ColdGenerationId));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production changed-source fallback: OldGeneration=%s NewGeneration=%s FrontendEvents=%d Result=%d MissReason=%u"),
			*ColdGenerationId.ToHexString(),
			*Flush.Flush.Current.GenerationId.ToHexString(),
			CountFrontendEvents(Events),
			ChangedValue,
			MissEvent->ReasonCode));
	}

	TEST_METHOD(PartialPersistedGenerationFallsBackBeforeActivationAndKeepsAllModulesLive)
	{
		using namespace AngelscriptCacheProductionWarmStartupTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECacheProductionPartialFirst%sState
{
	Ready = 1,
}

int ReadCacheProductionPartialFirst%sValue()
{
	return 701;
}
)AS"), *Unique, *Unique);
		const FString Unsupported = FString::Printf(TEXT(R"AS(
enum ECacheProductionPartialUnsupported%sFirst
{
	Ready = 1,
}

enum ECacheProductionPartialUnsupported%sSecond
{
	Ready = 2,
}

int ReadCacheProductionPartialUnsupported%sValue()
{
	return 702;
}
)AS"), *Unique, *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Unsupported.as"), Unsupported)));

		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();
			const FAngelscriptCacheLifecyclePublications ColdPublications =
				Cold->GetCacheService()->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(ColdPublications.Current.IsValid()));
			ASSERT_THAT(AreEqual(1, ColdPublications.Current->Modules.Num()));
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
		}

		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> FirstModule =
			Warm->GetModuleByModuleName(TEXT("First"));
		const TSharedPtr<FAngelscriptModuleDesc> UnsupportedModule =
			Warm->GetModuleByModuleName(TEXT("Unsupported"));
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(UnsupportedModule.IsValid()));
		ASSERT_THAT(IsFalse(FirstModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsFalse(UnsupportedModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsTrue(CountFrontendEvents(Events) > 0));
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(IsFalse(Publications.Current->bRestoredFromStore));
		ASSERT_THAT(AreEqual(1, Publications.Current->Modules.Num()));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* MissEvent =
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
		ASSERT_THAT(IsNotNull(MissEvent));
		ASSERT_THAT(AreEqual(uint32(0), MissEvent->PrimaryCount));

		int32 UnsupportedValue = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(),
			FName(TEXT("Unsupported")),
			TEXT("int ReadCacheProductionPartialUnsupported") + Unique
				+ TEXT("Value()"),
			UnsupportedValue)));
		ASSERT_THAT(AreEqual(702, UnsupportedValue));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production partial-generation fallback: PersistedModules=1 CurrentSources=2 FrontendEvents=%d UnsupportedResult=%d MissReason=%u"),
			CountFrontendEvents(Events),
			UnsupportedValue,
			MissEvent->ReasonCode));
	}

	TEST_METHOD(SecondPreparedModuleFailureRollsBackBatchBeforeNormalCompileFallback)
	{
		using namespace AngelscriptCacheProductionWarmStartupTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECacheAtomicFirst%sState
{
	Ready = 1,
}

int ReadCacheAtomicFirst%sValue()
{
	return 801;
}
)AS"), *Unique, *Unique);
		const FString Second = FString::Printf(TEXT(R"AS(
enum ECacheAtomicSecond%sState
{
	Ready = 1,
}

int ReadCacheAtomicSecond%sValue()
{
	return 802;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Second.as"), Second)));

		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
		}

		FStopAtSecondPrepared Injector;
		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project, &Injector);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		Injector.ObservedEngine = Warm.Get();
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};
		Warm->InitialCompile();

		ASSERT_THAT(IsTrue(Injector.bStopped));
		ASSERT_THAT(AreEqual(2, Injector.SeenOrdinals.Num()));
		ASSERT_THAT(AreEqual(0, Injector.ActiveModuleCountAtStop,
			TEXT("prepared modules must remain outside ActiveModules until batch commit")));
		ASSERT_THAT(AreEqual(0, Injector.RouteCountAtStop,
			TEXT("prepared functions must not publish a route snapshot before batch commit")));

		const TSharedPtr<FAngelscriptModuleDesc> FirstModule =
			Warm->GetModuleByModuleName(TEXT("First"));
		const TSharedPtr<FAngelscriptModuleDesc> SecondModule =
			Warm->GetModuleByModuleName(TEXT("Second"));
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(SecondModule.IsValid()));
		ASSERT_THAT(IsFalse(FirstModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsFalse(SecondModule->bLoadedIncrementalCache));
		ASSERT_THAT(IsTrue(CountFrontendEvents(Events) > 0));

		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(IsFalse(Publications.Current->bRestoredFromStore));
		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* RestoreReject =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Rejected
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheExactStartupReason::RestoreRejected);
				});
		ASSERT_THAT(IsNotNull(RestoreReject));
		ASSERT_THAT(AreEqual(uint32(0), RestoreReject->PrimaryCount));

		int32 FirstValue = 0;
		int32 SecondValue = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(), FName(TEXT("First")),
			TEXT("int ReadCacheAtomicFirst") + Unique + TEXT("Value()"),
			FirstValue)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			Warm.Get(), FName(TEXT("Second")),
			TEXT("int ReadCacheAtomicSecond") + Unique + TEXT("Value()"),
			SecondValue)));
		ASSERT_THAT(AreEqual(801, FirstValue));
		ASSERT_THAT(AreEqual(802, SecondValue));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Atomic restore fault fallback: ActiveAtFault=%d RoutesAtFault=%d FrontendEvents=%d Values=%d/%d"),
			Injector.ActiveModuleCountAtStop,
			Injector.RouteCountAtStop,
			CountFrontendEvents(Events),
			FirstValue,
			SecondValue));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
