#include "Cache/AngelscriptCacheDecisionTrace.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheSettings.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/StringOutputDevice.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDecisionTraceTests,
	"Angelscript.TestModule.Cache.DecisionTrace",
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
				TEXT("Automation/AngelscriptCacheDecisionTrace"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheDecisionTrace/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheDecisionTrace/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("Trace.as")));
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

	static FAngelscriptCacheFreezePublicationResult FreezePendingCopy(
		FAngelscriptCacheService& Service,
		const FAngelscriptCacheSuccessfulPublicationDto& Source)
	{
		FAngelscriptCacheMutationGuard Guard = Service.EnterMutation(
			EAngelscriptCacheMutationKind::RuntimeReload);
		if (!Guard.IsEntered())
		{
			return {};
		}
		FAngelscriptCacheSuccessfulPublicationInput Input;
		Input.Kind = EAngelscriptCacheSuccessfulCompileKind::SoftReload;
		Input.Disposition =
			EAngelscriptCachePublicationDisposition::PendingColdStart;
		Input.Compatibility = Source.Compatibility;
		Input.Context = Source.Context;
		Input.Profile = Source.Profile;
		Input.Modules = Source.Modules;
		return Service.FreezeSuccessfulCompileArtifacts(
			Guard.GetToken(), MoveTemp(Input));
	}

public:
	TEST_METHOD(BoundedTraceEvictsOldestPublicationAndStatusJsonUsesSameDto)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheDecisionTrace%sState
{
	Ready = 1,
}

int ReadCacheDecisionTrace%sValue()
{
	return 501;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 2);

		Engine->InitialCompile();
		const FAngelscriptCacheLifecyclePublications Initial =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Initial.Current.IsValid()));
		const FAngelscriptCacheDecisionTraceSnapshot BeforeCopies =
			Service->CaptureDecisionTrace();
		ASSERT_THAT(IsTrue(FreezePendingCopy(
			*Service, *Initial.Current).IsSuccess()));
		ASSERT_THAT(IsTrue(FreezePendingCopy(
			*Service, *Initial.Current).IsSuccess()));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 decision trace: Enabled=%d Capacity=%u Events=%d Evicted=%llu FirstEvent=%llu LastEvent=%llu"),
			Trace.bEnabled ? 1 : 0,
			Trace.Capacity,
			Trace.Events.Num(),
			Trace.EvictedEventCount,
			Trace.Events.IsEmpty() ? 0 : Trace.Events[0].EventOrdinal,
			Trace.Events.IsEmpty() ? 0 : Trace.Events.Last().EventOrdinal));
		ASSERT_THAT(IsTrue(Trace.bEnabled));
		ASSERT_THAT(AreEqual(static_cast<uint32>(2), Trace.Capacity));
		ASSERT_THAT(AreEqual(2, Trace.Events.Num()));
		// InitialCompile now emits the real SuccessfulPublication and
		// StableRoute decisions. Appending exactly one journal-capacity of
		// synthetic publications must evict every event present beforehand,
		// independent of how many production stages preceded this assertion.
		ASSERT_THAT(AreEqual(
			BeforeCopies.EvictedEventCount
				+ static_cast<uint64>(BeforeCopies.Events.Num()),
			Trace.EvictedEventCount));
		ASSERT_THAT(AreEqual(BeforeCopies.NextEventOrdinal + 1,
			Trace.Events[0].EventOrdinal));
		ASSERT_THAT(AreEqual(BeforeCopies.NextEventOrdinal + 2,
			Trace.Events[1].EventOrdinal));
		ASSERT_THAT(AreEqual(static_cast<uint64>(2),
			Trace.Events[0].TransactionOrdinal));
		ASSERT_THAT(AreEqual(static_cast<uint64>(3),
			Trace.Events[1].TransactionOrdinal));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionStage::SuccessfulPublication,
			Trace.Events[0].Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionOutcome::Published,
			Trace.Events[0].Outcome));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionReasonDomain::FreezePublication,
			Trace.Events[0].ReasonDomain));
		ASSERT_THAT(AreEqual(1, Trace.Events[0].ModuleKeys.Num()));
		ASSERT_THAT(IsTrue(
			Trace.Events[0].ModuleKeys[0]
				== Initial.Current->Modules[0].ModuleKey));

		const FAngelscriptCacheDiagnosticJsonResult Status =
			CaptureAngelscriptCacheDiagnosticJson(Engine.Get());
		ASSERT_THAT(IsTrue(Status.IsSuccess()));
		ASSERT_THAT(IsTrue(Status.Json.Contains(
			TEXT("\"decisionTrace\""))));
		ASSERT_THAT(IsTrue(Status.Json.Contains(FString::Printf(
			TEXT("\"evictedEventCount\":\"%llu\""),
			Trace.EvictedEventCount))));
		ASSERT_THAT(IsFalse(Status.Json.Contains(TEXT("functionId"))));
		ASSERT_THAT(IsFalse(Status.Json.Contains(TEXT("serviceIdentity"))));
	}

	TEST_METHOD(TraceConsoleControlsAreRegisteredAndClearTheCurrentEngineJournal)
	{
		const UAngelscriptCacheSettings* Settings =
			GetDefault<UAngelscriptCacheSettings>();
		ASSERT_THAT(IsNotNull(Settings));
		ASSERT_THAT(IsFalse(Settings->bEnableDecisionTrace));
		ASSERT_THAT(AreEqual(static_cast<uint32>(1024),
			Settings->DecisionTraceCapacity));

		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptEngineScope Scope(Fixture.GetEngine());
		FAngelscriptCacheService* Service =
			Fixture.GetEngine().GetCacheService();
		ASSERT_THAT(IsNotNull(Service));

		IConsoleObject* TraceCommand =
			IConsoleManager::Get().FindConsoleObject(TEXT("as.Cache.Trace"));
		ASSERT_THAT(IsNotNull(TraceCommand));
		ASSERT_THAT(IsNotNull(TraceCommand->AsCommand()));

		FStringOutputDevice Output;
		ASSERT_THAT(IsTrue(IConsoleManager::Get().ProcessUserConsoleInput(
			TEXT("as.Cache.Trace Enable Capacity=3"), Output, nullptr)));
		ASSERT_THAT(IsTrue(Service->CaptureDecisionTrace().bEnabled));
		ASSERT_THAT(AreEqual(static_cast<uint32>(3),
			Service->CaptureDecisionTrace().Capacity));
		ASSERT_THAT(IsTrue(IConsoleManager::Get().ProcessUserConsoleInput(
			TEXT("as.Cache.Trace Clear"), Output, nullptr)));
		ASSERT_THAT(IsTrue(
			Service->CaptureDecisionTrace().Events.IsEmpty()));
		ASSERT_THAT(IsTrue(IConsoleManager::Get().ProcessUserConsoleInput(
			TEXT("as.Cache.Trace Disable"), Output, nullptr)));
		ASSERT_THAT(IsFalse(Service->CaptureDecisionTrace().bEnabled));
	}

	TEST_METHOD(ExplicitFlushRecordsOneTypedLifecycleDecision)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheDecisionFlush%sState
{
	Ready = 1,
}

int ReadCacheDecisionFlush%sValue()
{
	return 601;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 8);
		Engine->InitialCompile();
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		Service->ClearDecisionTrace();

		const FAngelscriptCacheFlushApiResult Flush =
			FlushAngelscriptCacheToStore(Engine.Get(), 5.0);
		ASSERT_THAT(IsTrue(Flush.IsSuccess()));
		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		ASSERT_THAT(AreEqual(1, Trace.Events.Num()));
		const FAngelscriptCacheDecisionEvent& Event = Trace.Events[0];
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 lifecycle trace: Event=%llu Tx=%llu Outcome=%u Reason=%u CurrentGeneration=%s ElapsedUs=%llu"),
			Event.EventOrdinal,
			Event.TransactionOrdinal,
			static_cast<uint32>(Event.Outcome),
			Event.ReasonCode,
			Event.CurrentCoordinate.IsSet()
				? *Event.CurrentCoordinate->ToHexString() : TEXT("none"),
			Event.ElapsedMicroseconds));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionStage::LifecycleFlush,
			Event.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionOutcome::Completed,
			Event.Outcome));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionReasonDomain::LifecycleFlush,
			Event.ReasonDomain));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(EAngelscriptCacheLifecycleFlushError::None),
			Event.ReasonCode));
		ASSERT_THAT(AreEqual(
			Publications.Current->TransactionOrdinal,
			Event.TransactionOrdinal));
		ASSERT_THAT(AreEqual(1, Event.ModuleKeys.Num()));
		ASSERT_THAT(IsTrue(
			Event.ModuleKeys[0] == Publications.Current->Modules[0].ModuleKey));
		ASSERT_THAT(IsTrue(Event.CurrentCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			Event.CurrentCoordinate.GetValue()
				== Flush.Flush.Current.GenerationId));
		ASSERT_THAT(AreEqual(static_cast<uint32>(1), Event.PrimaryCount));
		ASSERT_THAT(AreEqual(static_cast<uint32>(1), Event.SecondaryCount));
	}
};

#endif
