#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "Async/Async.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheServiceTests_Private
{
	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2ServiceTests"),
			TEXT("VmExecutionCodec=5"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static bool CaptureSimpleModule(
		FAutomationTestBase& Test,
		FAngelscriptTestFixture& Fixture,
		const char* ModuleName,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
	{
		static constexpr const char* Source = R"AS(
enum ECacheServiceMode
{
	Ready = 1,
}

int ReadCacheServiceAnswer()
{
	return 42;
}
)AS";
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			ModuleName, UTF8_TO_TCHAR(Source));
		if (ScriptModule == nullptr)
		{
			return false;
		}
		TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			return false;
		}
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("V6.1 service capture: Error=%u Records=%d Graph=%u Detail=%s"),
			static_cast<uint32>(Capture.Error), OutArtifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount, *Capture.Detail));
		return Capture.IsSuccess();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheServiceTests,
	"Angelscript.TestModule.Cache.Service",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EngineOwnsIsolatedServiceAndReentryRequiresCurrentToken)
	{
		using namespace AngelscriptCacheServiceTests_Private;
		uint64 FirstServiceIdentity = 0;
		{
			FAngelscriptTestFixture Fixture(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Fixture.IsValid()));
			FAngelscriptCacheService* Service =
				Fixture.GetEngine().GetCacheService();
			ASSERT_THAT(IsNotNull(Service));
			FirstServiceIdentity = Service->GetEphemeralServiceIdentity();
			ASSERT_THAT(IsTrue(FirstServiceIdentity != 0));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheMutationPhase::RuntimeGameThread,
				Service->GetMutationPhase()));

			FAngelscriptCacheMutationGuard Outer = Service->EnterMutation(
				EAngelscriptCacheMutationKind::InitialCompile);
			ASSERT_THAT(IsTrue(Outer.IsEntered()));

			FAngelscriptCacheMutationGuard ImplicitNested =
				Service->EnterMutation(
					EAngelscriptCacheMutationKind::ModuleSwap);
			ASSERT_THAT(IsFalse(ImplicitNested.IsEntered()));

			FAngelscriptCacheMutationGuard ExplicitNested =
				Service->EnterMutation(
					EAngelscriptCacheMutationKind::ModuleSwap,
					&Outer.GetToken());
			ASSERT_THAT(IsTrue(ExplicitNested.IsEntered()));
		}

		{
			FAngelscriptTestFixture Fixture(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Fixture.IsValid()));
			FAngelscriptCacheService* Service =
				Fixture.GetEngine().GetCacheService();
			ASSERT_THAT(IsNotNull(Service));
			ASSERT_THAT(IsFalse(
				Service->GetEphemeralServiceIdentity() == FirstServiceIdentity));
		}
	}

	TEST_METHOD(SuccessfulCompileArtifactsFreezeInsideGateAndOutliveEngine)
	{
		using namespace AngelscriptCacheServiceTests_Private;
		TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe> Frozen;
		FAngelscriptStableModuleKey ExpectedModuleKey;
		FAngelscriptCacheRecordId ExpectedSnapshotRecord;
		int32 ExpectedRecordCount = 0;
		{
			FAngelscriptTestFixture Fixture(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Fixture.IsValid()));
			FAngelscriptCacheService* Service =
				Fixture.GetEngine().GetCacheService();
			ASSERT_THAT(IsNotNull(Service));

			const FAngelscriptCacheCleanCaptureOptions Options =
				MakeCaptureOptions();
			FAngelscriptCacheCleanModuleArtifacts Artifacts;
			ASSERT_THAT(IsTrue(CaptureSimpleModule(
				*TestRunner, Fixture, "ASCacheV2ServiceFreeze",
				Options, Artifacts)));
			ExpectedModuleKey = Artifacts.ModuleKey;
			ExpectedSnapshotRecord = Artifacts.ModuleSnapshot.RecordId;
			ExpectedRecordCount = Artifacts.Records.Num();

			FAngelscriptCacheSuccessfulPublicationInput Input;
			Input.Kind = EAngelscriptCacheSuccessfulCompileKind::Initial;
			Input.Disposition =
				EAngelscriptCachePublicationDisposition::Current;
			Input.Compatibility = Options.Compatibility;
			Input.Context = Options.Context;
			Input.Profile = Options.Profile;
			Input.Modules.Add(MoveTemp(Artifacts));

			const FAngelscriptCacheFreezePublicationResult OutsideGate =
				Service->FreezeSuccessfulCompileArtifacts(
					FAngelscriptCacheMutationToken{},
					FAngelscriptCacheSuccessfulPublicationInput(Input));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheFreezePublicationError::NotMutationOwner,
				OutsideGate.Error));
			ASSERT_THAT(IsFalse(OutsideGate.Publication.IsValid()));

			FAngelscriptCacheMutationGuard Guard = Service->EnterMutation(
				EAngelscriptCacheMutationKind::FreezeSuccessfulCompile);
			ASSERT_THAT(IsTrue(Guard.IsEntered()));
			FAngelscriptCacheSuccessfulPublicationInput Duplicate = Input;
			FAngelscriptCacheCleanModuleArtifacts DuplicateModule =
				Duplicate.Modules[0];
			Duplicate.Modules.Add(MoveTemp(DuplicateModule));
			const FAngelscriptCacheFreezePublicationResult DuplicateResult =
				Service->FreezeSuccessfulCompileArtifacts(
					Guard.GetToken(), MoveTemp(Duplicate));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheFreezePublicationError::DuplicateModule,
				DuplicateResult.Error));
			ASSERT_THAT(IsFalse(DuplicateResult.Publication.IsValid()));
			ASSERT_THAT(IsFalse(
				Service->GetLatestSuccessfulPublication().IsValid()));

			const FAngelscriptCacheFreezePublicationResult Result =
				Service->FreezeSuccessfulCompileArtifacts(
					Guard.GetToken(), MoveTemp(Input));
			TestRunner->AddInfo(FString::Printf(
				TEXT("V6.1 service freeze: Error=%u Transaction=%llu Modules=%d Records=%d"),
				static_cast<uint32>(Result.Error),
				Result.Publication.IsValid()
					? Result.Publication->TransactionOrdinal : 0,
				Result.Publication.IsValid()
					? Result.Publication->Modules.Num() : 0,
				Result.Publication.IsValid()
					? Result.Publication->Modules[0].Records.Num() : 0));
			ASSERT_THAT(IsTrue(Result.IsSuccess()));
			Frozen = Result.Publication;
			ASSERT_THAT(IsTrue(Frozen ==
				Service->GetLatestSuccessfulPublication()));
		}

		ASSERT_THAT(IsTrue(Frozen.IsValid()));
		ASSERT_THAT(AreEqual(
			FAngelscriptCacheSuccessfulPublicationDto::CurrentSchemaVersion,
			Frozen->SchemaVersion));
		ASSERT_THAT(AreEqual(uint64(1), Frozen->TransactionOrdinal));
		ASSERT_THAT(AreEqual(1, Frozen->Modules.Num()));
		ASSERT_THAT(IsTrue(
			Frozen->Modules[0].ModuleKey.Hash == ExpectedModuleKey.Hash));
		ASSERT_THAT(IsTrue(Frozen->Modules[0].ModuleSnapshot.RecordId
			== ExpectedSnapshotRecord));
		ASSERT_THAT(AreEqual(
			ExpectedRecordCount, Frozen->Modules[0].Records.Num()));
	}

	TEST_METHOD(StaleTokenOffThreadAndShutdownCannotEnterRuntimeGate)
	{
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptCacheService* Service =
			Fixture.GetEngine().GetCacheService();
		ASSERT_THAT(IsNotNull(Service));

		FAngelscriptCacheMutationToken StaleToken;
		{
			FAngelscriptCacheMutationGuard First = Service->EnterMutation(
				EAngelscriptCacheMutationKind::InitialCompile);
			ASSERT_THAT(IsTrue(First.IsEntered()));
			StaleToken = First.GetToken();
		}
		{
			FAngelscriptCacheMutationGuard Second = Service->EnterMutation(
				EAngelscriptCacheMutationKind::RuntimeReload);
			ASSERT_THAT(IsTrue(Second.IsEntered()));
			FAngelscriptCacheMutationGuard StaleNested =
				Service->EnterMutation(
					EAngelscriptCacheMutationKind::RouteRefresh,
					&StaleToken);
			ASSERT_THAT(IsFalse(StaleNested.IsEntered()));
		}

		TFuture<bool> OffThread = Async(EAsyncExecution::Thread,
			[Service]()
			{
				FAngelscriptCacheMutationGuard Guard = Service->EnterMutation(
					EAngelscriptCacheMutationKind::RuntimeReload);
				return Guard.IsEntered();
			});
		ASSERT_THAT(IsFalse(OffThread.Get()));

		Service->BeginEngineShutdown();
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheMutationPhase::ShuttingDown,
			Service->GetMutationPhase()));
		FAngelscriptCacheMutationGuard AfterShutdown = Service->EnterMutation(
			EAngelscriptCacheMutationKind::Shutdown);
		ASSERT_THAT(IsFalse(AfterShutdown.IsEntered()));
	}
};

#endif
