#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheDiagnosticsTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bytes); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2DiagnosticsTests"),
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
enum E%sDiagnosticState
{
	Ready = 1,
}

int Read%sDiagnosticValue()
{
	return %d;
}
)AS"), *ModuleSymbol, *ModuleSymbol, ReturnValue);
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			ModuleName, TCHAR_TO_UTF8(*Source));
		if (ScriptModule == nullptr)
		{
			Test.AddError(TEXT("Diagnostic fixture module did not compile"));
			return false;
		}

		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			Test.AddError(TEXT("Diagnostic fixture module was not active"));
			return false;
		}
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("V6.3 diagnostic capture: Module=%s Error=%u Records=%d Graph=%u"),
			UTF8_TO_TCHAR(ModuleName), static_cast<uint32>(Capture.Error),
			OutArtifacts.Records.Num(), Capture.ValidatedGraphRecordCount));
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

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDiagnosticsTests,
	"Angelscript.TestModule.Cache.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyServiceSnapshotAndJsonHaveStableSchema)
	{
		FAngelscriptCacheService Service;
		const FAngelscriptCacheDiagnosticSnapshot Snapshot =
			Service.CaptureDiagnosticSnapshot();
		ASSERT_THAT(AreEqual(uint32(4), Snapshot.SchemaVersion));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheMutationPhase::InitializingAnyThread,
			Snapshot.MutationPhase));
		ASSERT_THAT(AreEqual(uint64(0), Snapshot.LastTransactionOrdinal));
		ASSERT_THAT(IsFalse(Snapshot.Current.bPresent));
		ASSERT_THAT(IsFalse(Snapshot.PendingColdStart.bPresent));
		ASSERT_THAT(IsFalse(Snapshot.LatestSuccessful.bPresent));

		FString JsonA;
		FString JsonB;
		ASSERT_THAT(IsTrue(SerializeAngelscriptCacheDiagnosticSnapshotJson(
			Snapshot, JsonA)));
		ASSERT_THAT(IsTrue(SerializeAngelscriptCacheDiagnosticSnapshotJson(
			Snapshot, JsonB)));
		ASSERT_THAT(IsTrue(JsonA == JsonB));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"schemaVersion\":4"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"functionReuse\":{\"present\":false}"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"mutationPhase\":1"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"current\":{\"present\":false}"))));
		ASSERT_THAT(IsFalse(JsonA.Contains(TEXT("serviceIdentity"))));
		ASSERT_THAT(IsFalse(JsonA.Contains(TEXT("functionId"))));
	}

	TEST_METHOD(PublishedSlotsProduceDeterministicPointerFreeSummaries)
	{
		using namespace AngelscriptCacheDiagnosticsTests_Private;
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
			*TestRunner, Fixture, "ASCacheV2DiagnosticCurrent", 61,
			Options, CurrentArtifacts)));
		ASSERT_THAT(IsTrue(CaptureModule(
			*TestRunner, Fixture, "ASCacheV2DiagnosticPending", 62,
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
		const FAngelscriptCacheFreezePublicationResult Pending = FreezePublication(
			*Service, Guard.GetToken(), Options,
			EAngelscriptCacheSuccessfulCompileKind::FullReload,
			EAngelscriptCachePublicationDisposition::PendingColdStart,
			MoveTemp(PendingArtifacts));
		ASSERT_THAT(IsTrue(Pending.IsSuccess()));
		Guard = {};

		const FAngelscriptCacheDiagnosticSnapshot Snapshot =
			Service->CaptureDiagnosticSnapshot();
		ASSERT_THAT(AreEqual(uint64(2), Snapshot.LastTransactionOrdinal));
		ASSERT_THAT(IsTrue(Snapshot.Current.bPresent));
		ASSERT_THAT(IsTrue(Snapshot.PendingColdStart.bPresent));
		ASSERT_THAT(IsTrue(Snapshot.LatestSuccessful.bPresent));
		ASSERT_THAT(AreEqual(uint64(1),
			Snapshot.Current.TransactionOrdinal));
		ASSERT_THAT(AreEqual(uint64(2),
			Snapshot.PendingColdStart.TransactionOrdinal));
		ASSERT_THAT(AreEqual(uint64(2),
			Snapshot.LatestSuccessful.TransactionOrdinal));
		ASSERT_THAT(AreEqual(1, Snapshot.Current.Modules.Num()));
		ASSERT_THAT(AreEqual(1, Snapshot.PendingColdStart.Modules.Num()));
		ASSERT_THAT(AreEqual(
			Current.Publication->Modules[0].Records.Num(),
			Snapshot.Current.TotalRecordCount));
		ASSERT_THAT(AreEqual(7,
			Snapshot.Current.Modules[0].RecordKinds.Num()));
		ASSERT_THAT(IsTrue(
			Snapshot.Current.Modules[0].ModuleKey
				== Current.Publication->Modules[0].ModuleKey));
		ASSERT_THAT(AreEqual(
			TEXT("ASCacheV2DiagnosticCurrent"),
			Snapshot.Current.Modules[0].CanonicalModuleName));
		ASSERT_THAT(IsTrue(
			Snapshot.Current.Modules[0].Declarations.Num() >= 2));
		ASSERT_THAT(AreEqual(1,
			Snapshot.Current.Modules[0].Types.Num()));
		ASSERT_THAT(AreEqual(1,
			Snapshot.Current.Modules[0].Functions.Num()));
		ASSERT_THAT(AreEqual(0,
			Snapshot.Current.Modules[0].DecodeFailures.Num()));
		ASSERT_THAT(IsFalse(
			Snapshot.Current.Modules[0].Types[0].Schema.TypeKey.Hash.IsZero()));
		ASSERT_THAT(AreEqual(
			TEXT("EASCacheV2DiagnosticCurrentDiagnosticState"),
			Snapshot.Current.Modules[0].Types[0].Schema.CanonicalName));
		ASSERT_THAT(IsFalse(
			Snapshot.Current.Modules[0].Functions[0]
				.Identity.FunctionKey.Hash.IsZero()));
		ASSERT_THAT(IsFalse(
			Snapshot.Current.Modules[0].Functions[0]
				.FunctionSourceDigest.Hash.IsZero()));

		FString JsonA;
		FString JsonB;
		ASSERT_THAT(IsTrue(SerializeAngelscriptCacheDiagnosticSnapshotJson(
			Snapshot, JsonA)));
		ASSERT_THAT(IsTrue(SerializeAngelscriptCacheDiagnosticSnapshotJson(
			Snapshot, JsonB)));
		ASSERT_THAT(IsTrue(JsonA == JsonB));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			*Snapshot.Current.Modules[0].ModuleKey.Hash.ToHexString())));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			*Snapshot.PendingColdStart.Modules[0].ModuleKey.Hash.ToHexString())));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"canonicalModuleName\":\"ASCacheV2DiagnosticCurrent\""))));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"declarations\":["))));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"types\":["))));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"layoutInputs\":["))));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"kindPayload\":{"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(TEXT("\"functions\":["))));
		ASSERT_THAT(IsFalse(JsonA.Contains(TEXT("serviceIdentity"))));
		ASSERT_THAT(IsFalse(JsonA.Contains(TEXT("functionId"))));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 C++ diagnostic JSON: Bytes=%d CurrentTx=%llu PendingTx=%llu CurrentRecords=%d"),
			JsonA.Len(), Snapshot.Current.TransactionOrdinal,
			Snapshot.PendingColdStart.TransactionOrdinal,
			Snapshot.Current.TotalRecordCount));
	}

	TEST_METHOD(RouteAndValidationDiagnosticsRemainStablePointerFreeValues)
	{
		using namespace AngelscriptCacheDiagnosticsTests_Private;
		FAngelscriptCacheFunctionRouteSnapshot LiveRoutes;
		LiveRoutes.PublicationOrdinal = 17;
		LiveRoutes.VmRouteCount = 1;
		FAngelscriptCacheLiveFunctionRoute& LiveRoute =
			LiveRoutes.FunctionRoutes.AddDefaulted_GetRef();
		LiveRoute.ModuleKey = FAngelscriptStableModuleKey{MakeHash(1)};
		LiveRoute.Identity.FunctionKey =
			FAngelscriptStableFunctionKey{MakeHash(33)};
		LiveRoute.Identity.Content.Execution = MakeHash(65);
		LiveRoute.Identity.Content.Debug = MakeHash(97);
		LiveRoute.Identity.Profile = MakeCaptureOptions().Profile;
		LiveRoute.CanonicalDeclaration = TEXT("int ReadValue()");
		LiveRoute.NumericFunctionId = 9123;
		LiveRoute.bHasVerifiedArtifactIdentity = true;

		FAngelscriptCacheDiagnosticSnapshot Snapshot;
		Snapshot.FunctionRoutes =
			BuildAngelscriptCacheDiagnosticFunctionRoutes(LiveRoutes);
		ASSERT_THAT(IsTrue(Snapshot.FunctionRoutes.bPresent));
		ASSERT_THAT(AreEqual(uint64(17),
			Snapshot.FunctionRoutes.PublicationOrdinal));
		ASSERT_THAT(AreEqual(1, Snapshot.FunctionRoutes.Routes.Num()));
		ASSERT_THAT(AreEqual(
			TEXT("int ReadValue()"),
			Snapshot.FunctionRoutes.Routes[0].CanonicalDeclaration));

		FAngelscriptCacheService Service;
		Service.ConfigureDecisionTrace(true, 4);
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::StartupRestore;
		Event.Outcome = EAngelscriptCacheDecisionOutcome::Rejected;
		Event.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::ExactStartup;
		Event.Validation = FAngelscriptCacheValidationResult::AtStage(
			EAngelscriptCacheValidationError::CurrentContentMismatch,
			EAngelscriptCacheRecordKind::FunctionBody,
			EAngelscriptCacheValidationStage::OpaqueCodec,
			1234567890123ull);
		Event.Detail = FString::ChrN(
			FAngelscriptCacheDecisionEvent::MaxDetailCharacters + 100,
			TEXT('x'));
		Service.RecordDecisionEvent(MoveTemp(Event));
		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service.CaptureDecisionTrace();
		ASSERT_THAT(AreEqual(1, Trace.Events.Num()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(
				FAngelscriptCacheDecisionEvent::MaxDetailCharacters),
			Trace.Events[0].Detail.Len()));
		Snapshot.DecisionTrace = Trace;

		FString Json;
		ASSERT_THAT(IsTrue(
			SerializeAngelscriptCacheDiagnosticSnapshotJson(Snapshot, Json)));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"functionRoutes\":"))));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"selectedRouteName\":\"Vm\""))));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"validation\":{"))));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"errorName\":\"CurrentContentMismatch\""))));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"stageName\":\"OpaqueCodec\""))));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"byteOffset\":\"1234567890123\""))));
		ASSERT_THAT(IsFalse(Json.Contains(TEXT("9123"))));
		ASSERT_THAT(IsFalse(Json.Contains(TEXT("functionId"))));
	}
};

#endif
