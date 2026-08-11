#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheLifecyclePublicationTests_Private
{
	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2LifecyclePublicationTests"),
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

	static bool CaptureModule(
		FAutomationTestBase& Test,
		FAngelscriptTestFixture& Fixture,
		const char* ModuleName,
		const int32 ReturnValue,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
	{
		const FString ModuleSymbol = UTF8_TO_TCHAR(ModuleName);
		const FString Source = FString::Printf(TEXT(R"AS(
enum E%sMode
{
	Ready = 1,
}

int Read%sValue()
{
	return %d;
}
)AS"), *ModuleSymbol, *ModuleSymbol, ReturnValue);
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			ModuleName, TCHAR_TO_UTF8(*Source));
		if (ScriptModule == nullptr)
		{
			Test.AddError(TEXT("Lifecycle fixture module did not compile"));
			return false;
		}

		TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			Test.AddError(TEXT("Lifecycle fixture module was not active"));
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("V6.2 lifecycle capture: Module=%s Return=%d Error=%u Records=%d Graph=%u Snapshot=%s Detail=%s"),
			UTF8_TO_TCHAR(ModuleName), ReturnValue,
			static_cast<uint32>(Capture.Error), OutArtifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			*OutArtifacts.SourceSnapshot.ToHexString(), *Capture.Detail));
		return Capture.IsSuccess();
	}

	static FAngelscriptCacheFreezePublicationResult FreezePublication(
		FAngelscriptCacheService& Service,
		const FAngelscriptCacheMutationToken& Token,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const EAngelscriptCacheSuccessfulCompileKind Kind,
		const EAngelscriptCachePublicationDisposition Disposition,
		FAngelscriptCacheCleanModuleArtifacts Artifacts)
	{
		FAngelscriptCacheSuccessfulPublicationInput Input;
		Input.Kind = Kind;
		Input.Disposition = Disposition;
		Input.Compatibility = Options.Compatibility;
		Input.Context = Options.Context;
		Input.Profile = Options.Profile;
		Input.Modules.Add(MoveTemp(Artifacts));
		return Service.FreezeSuccessfulCompileArtifacts(Token, MoveTemp(Input));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheLifecyclePublicationTests,
	"Angelscript.TestModule.Cache.LifecyclePublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CurrentAndPendingColdStartRemainIndependentUntilFullSuccess)
	{
		using namespace AngelscriptCacheLifecyclePublicationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptCacheService* Service =
			Fixture.GetEngine().GetCacheService();
		ASSERT_THAT(IsNotNull(Service));

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts CurrentArtifacts;
		FAngelscriptCacheCleanModuleArtifacts PendingArtifacts;
		ASSERT_THAT(IsTrue(CaptureModule(
			*TestRunner, Fixture, "ASCacheV2LifecycleCurrent", 41,
			Options, CurrentArtifacts)));
		ASSERT_THAT(IsTrue(CaptureModule(
			*TestRunner, Fixture, "ASCacheV2LifecyclePending", 42,
			Options, PendingArtifacts)));

		FAngelscriptCacheMutationGuard Guard = Service->EnterMutation(
			EAngelscriptCacheMutationKind::FreezeSuccessfulCompile);
		ASSERT_THAT(IsTrue(Guard.IsEntered()));
		const FAngelscriptCacheFreezePublicationResult Current = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::Initial,
			EAngelscriptCachePublicationDisposition::Current,
			MoveTemp(CurrentArtifacts));
		ASSERT_THAT(IsTrue(Current.IsSuccess()));

		const FAngelscriptCacheLifecyclePublications AfterCurrent =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(AfterCurrent.Current == Current.Publication));
		ASSERT_THAT(IsFalse(AfterCurrent.PendingColdStart.IsValid()));
		ASSERT_THAT(IsTrue(
			AfterCurrent.LatestSuccessful == Current.Publication));

		const FAngelscriptCacheFreezePublicationResult Pending = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::FullReload,
			EAngelscriptCachePublicationDisposition::PendingColdStart,
			MoveTemp(PendingArtifacts));
		ASSERT_THAT(IsTrue(Pending.IsSuccess()));
		ASSERT_THAT(AreEqual(uint64(1),
			Current.Publication->TransactionOrdinal));
		ASSERT_THAT(AreEqual(uint64(2),
			Pending.Publication->TransactionOrdinal));

		const FAngelscriptCacheLifecyclePublications AfterPending =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(AfterPending.Current == Current.Publication));
		ASSERT_THAT(IsTrue(
			AfterPending.PendingColdStart == Pending.Publication));
		ASSERT_THAT(IsTrue(
			AfterPending.LatestSuccessful == Pending.Publication));

		const FAngelscriptCacheFreezePublicationResult Promoted = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::FullReload,
			EAngelscriptCachePublicationDisposition::Current,
			Pending.Publication->Modules[0]);
		ASSERT_THAT(IsTrue(Promoted.IsSuccess()));
		ASSERT_THAT(AreEqual(uint64(3),
			Promoted.Publication->TransactionOrdinal));

		const FAngelscriptCacheLifecyclePublications AfterPromotion =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.2 lifecycle promotion: CurrentTx=%llu Pending=%d LatestTx=%llu"),
			AfterPromotion.Current.IsValid()
				? AfterPromotion.Current->TransactionOrdinal : 0,
			AfterPromotion.PendingColdStart.IsValid() ? 1 : 0,
			AfterPromotion.LatestSuccessful.IsValid()
				? AfterPromotion.LatestSuccessful->TransactionOrdinal : 0));
		ASSERT_THAT(IsTrue(AfterPromotion.Current == Promoted.Publication));
		ASSERT_THAT(IsFalse(AfterPromotion.PendingColdStart.IsValid()));
		ASSERT_THAT(IsTrue(
			AfterPromotion.LatestSuccessful == Promoted.Publication));
	}

	TEST_METHOD(FailedPublicationPreservesLastGoodSlotsAndOrdinal)
	{
		using namespace AngelscriptCacheLifecyclePublicationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptCacheService* Service =
			Fixture.GetEngine().GetCacheService();
		ASSERT_THAT(IsNotNull(Service));

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts CurrentArtifacts;
		FAngelscriptCacheCleanModuleArtifacts PendingArtifacts;
		ASSERT_THAT(IsTrue(CaptureModule(
			*TestRunner, Fixture, "ASCacheV2LifecycleLastGood", 51,
			Options, CurrentArtifacts)));
		ASSERT_THAT(IsTrue(CaptureModule(
			*TestRunner, Fixture, "ASCacheV2LifecycleCandidate", 52,
			Options, PendingArtifacts)));

		FAngelscriptCacheMutationGuard Guard = Service->EnterMutation(
			EAngelscriptCacheMutationKind::FreezeSuccessfulCompile);
		ASSERT_THAT(IsTrue(Guard.IsEntered()));
		const FAngelscriptCacheFreezePublicationResult Current = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::Initial,
			EAngelscriptCachePublicationDisposition::Current,
			MoveTemp(CurrentArtifacts));
		const FAngelscriptCacheFreezePublicationResult Pending = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::FullReload,
			EAngelscriptCachePublicationDisposition::PendingColdStart,
			MoveTemp(PendingArtifacts));
		ASSERT_THAT(IsTrue(Current.IsSuccess()));
		ASSERT_THAT(IsTrue(Pending.IsSuccess()));

		FAngelscriptCacheSuccessfulPublicationInput Invalid;
		Invalid.Kind = EAngelscriptCacheSuccessfulCompileKind::SoftReload;
		Invalid.Disposition =
			EAngelscriptCachePublicationDisposition::Current;
		Invalid.Compatibility = Options.Compatibility;
		Invalid.Context = Options.Context;
		Invalid.Profile = Options.Profile;
		const FAngelscriptCacheFreezePublicationResult Failed =
			Service->FreezeSuccessfulCompileArtifacts(
				Guard.GetToken(), MoveTemp(Invalid));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFreezePublicationError::InvalidInput,
			Failed.Error));

		const FAngelscriptCacheLifecyclePublications AfterFailure =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(AfterFailure.Current == Current.Publication));
		ASSERT_THAT(IsTrue(
			AfterFailure.PendingColdStart == Pending.Publication));
		ASSERT_THAT(IsTrue(
			AfterFailure.LatestSuccessful == Pending.Publication));

		const FAngelscriptCacheFreezePublicationResult NextSuccess = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::SoftReload,
			EAngelscriptCachePublicationDisposition::Current,
			Current.Publication->Modules[0]);
		ASSERT_THAT(IsTrue(NextSuccess.IsSuccess()));
		ASSERT_THAT(AreEqual(uint64(3),
			NextSuccess.Publication->TransactionOrdinal));

		const FAngelscriptCacheLifecyclePublications AfterSoftCurrent =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.2 last-good preservation: FailedError=%u CurrentTx=%llu PendingTx=%llu NextTx=%llu"),
			static_cast<uint32>(Failed.Error),
			AfterSoftCurrent.Current.IsValid()
				? AfterSoftCurrent.Current->TransactionOrdinal : 0,
			AfterSoftCurrent.PendingColdStart.IsValid()
				? AfterSoftCurrent.PendingColdStart->TransactionOrdinal : 0,
			NextSuccess.Publication->TransactionOrdinal));
		ASSERT_THAT(IsTrue(
			AfterSoftCurrent.Current == NextSuccess.Publication));
		ASSERT_THAT(IsTrue(
			AfterSoftCurrent.PendingColdStart == Pending.Publication));
	}
};

#endif
