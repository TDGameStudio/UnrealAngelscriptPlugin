#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheCleanCaptureOpaqueValidationTests,
	"Angelscript.TestModule.Cache.CleanCaptureOpaqueValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	enum class EOpaqueMutation : uint8
	{
		Truncate,
		CorruptMagic,
	};

	struct FDecodedOpaqueRecords
	{
		FAngelscriptCacheRecordId FunctionRecordId;
		FAngelscriptCacheRecordId DebugRecordId;
		FAngelscriptCacheRecordId SnapshotRecordId;
		FAngelscriptCachedFunctionBody FunctionBody;
		FAngelscriptCachedDebugSidecar DebugSidecar;
		FAngelscriptCachedModuleSnapshot Snapshot;
	};

	struct FValidationObservation
	{
		FAngelscriptCacheCleanCaptureResult Result;
		FAngelscriptCacheCleanModuleArtifacts Output;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2CleanCaptureOpaqueValidation"),
			TEXT("VmExecutionCodec=2"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context = FAngelscriptArtifactIdentityBuilder::BuildContextKey(
			Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static bool CaptureCandidate(
		FAutomationTestBase& Test,
		FAngelscriptTestFixture& Fixture,
		const char* ModuleName,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		TSharedPtr<FAngelscriptModuleDesc>& OutModule,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
	{
		OutModule.Reset();
		OutArtifacts.Reset();
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create an isolated full AngelScript engine"));
			return false;
		}

		const FString ScriptSource = ASTEST_AS(R"AS(
			enum ECacheV2OpaqueState
			{
				Ready = 7,
			}

			int Answer()
			{
				return 42;
			}
			)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(ModuleName, ScriptSource);
		if (ScriptModule == nullptr)
		{
			return false;
		}

		OutModule = Fixture.GetEngine().GetModule(ScriptModule);
		if (!OutModule.IsValid())
		{
			Test.AddError(TEXT("The normal compile did not retain its module descriptor"));
			return false;
		}
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				OutModule.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("Opaque validation baseline capture: Module=%s Error=%u Records=%d GraphRecords=%u Detail=%s"),
			*OutModule->ModuleName,
			static_cast<uint32>(Capture.Error),
			OutArtifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			*Capture.Detail));
		return Capture.IsSuccess();
	}

	static const FAngelscriptPreparedRecord* FindRecord(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EAngelscriptCacheRecordKind Kind)
	{
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind == Kind)
			{
				return &Record;
			}
		}
		return nullptr;
	}

	static bool ReplaceRecord(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCacheRecordId& OldRecordId,
		FAngelscriptPreparedRecord&& NewRecord)
	{
		for (FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId == OldRecordId)
			{
				Record = MoveTemp(NewRecord);
				return true;
			}
		}
		return false;
	}

	static bool DecodeOpaqueRecords(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FDecodedOpaqueRecords& OutRecords)
	{
		const FAngelscriptPreparedRecord* FunctionRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::FunctionBody);
		const FAngelscriptPreparedRecord* DebugRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::DebugSidecar);
		const FAngelscriptPreparedRecord* SnapshotRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleSnapshot);
		if (FunctionRecord == nullptr || DebugRecord == nullptr
			|| SnapshotRecord == nullptr)
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedFunction;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedDebug;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedSnapshot;
		if (!FAngelscriptDecodedCacheRecord::TryDecode(
				FunctionRecord->RecordId,
				FunctionRecord->CanonicalPayload,
				Limits,
				Budget,
				DecodedFunction).IsSuccess()
			|| !DecodedFunction.IsSet()
			|| !FAngelscriptDecodedCacheRecord::TryDecode(
				DebugRecord->RecordId,
				DebugRecord->CanonicalPayload,
				Limits,
				Budget,
				DecodedDebug).IsSuccess()
			|| !DecodedDebug.IsSet()
			|| !FAngelscriptDecodedCacheRecord::TryDecode(
				SnapshotRecord->RecordId,
				SnapshotRecord->CanonicalPayload,
				Limits,
				Budget,
				DecodedSnapshot).IsSuccess()
			|| !DecodedSnapshot.IsSet())
		{
			return false;
		}

		const FAngelscriptCachedFunctionBody* FunctionBody =
			DecodedFunction.GetValue()->TryGetFunctionBody();
		const FAngelscriptCachedDebugSidecar* DebugSidecar =
			DecodedDebug.GetValue()->TryGetDebugSidecar();
		const FAngelscriptCachedModuleSnapshot* Snapshot =
			DecodedSnapshot.GetValue()->TryGetModuleSnapshot();
		if (FunctionBody == nullptr || DebugSidecar == nullptr
			|| Snapshot == nullptr)
		{
			return false;
		}

		OutRecords.FunctionRecordId = FunctionRecord->RecordId;
		OutRecords.DebugRecordId = DebugRecord->RecordId;
		OutRecords.SnapshotRecordId = SnapshotRecord->RecordId;
		OutRecords.FunctionBody = *FunctionBody;
		OutRecords.DebugSidecar = *DebugSidecar;
		OutRecords.Snapshot = *Snapshot;
		return true;
	}

	static bool MutateOpaquePayload(
		TArray<uint8>& Payload,
		const EOpaqueMutation Mutation)
	{
		if (Payload.IsEmpty())
		{
			return false;
		}
		if (Mutation == EOpaqueMutation::Truncate)
		{
			Payload.RemoveAt(Payload.Num() - 1, 1, EAllowShrinking::No);
		}
		else
		{
			Payload[0] ^= 0x5au;
		}
		return true;
	}

	static bool BuildPreparedRecord(
		const EAngelscriptCacheRecordKind Kind,
		TArray<uint8>&& Payload,
		FAngelscriptPreparedRecord& OutRecord)
	{
		OutRecord = {};
		OutRecord.CanonicalPayload = MoveTemp(Payload);
		return FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, OutRecord.CanonicalPayload, OutRecord.RecordId).IsSuccess();
	}

	static bool RebuildFunctionAndSnapshot(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FDecodedOpaqueRecords& Records)
	{
		TArray<uint8> FunctionPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
				Records.FunctionBody, FunctionPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord FunctionRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::FunctionBody,
			MoveTemp(FunctionPayload), FunctionRecord))
		{
			return false;
		}
		const FAngelscriptCacheRecordId NewFunctionRecordId =
			FunctionRecord.RecordId;
		if (!ReplaceRecord(Artifacts, Records.FunctionRecordId,
			MoveTemp(FunctionRecord)))
		{
			return false;
		}

		if (Records.Snapshot.FunctionBodies.Num() != 1
			|| !(Records.Snapshot.FunctionBodies[0].FunctionKey
				== Records.FunctionBody.Identity.FunctionKey))
		{
			return false;
		}
		Records.Snapshot.FunctionBodies[0].RecordId = NewFunctionRecordId;
		TArray<uint8> SnapshotPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				Records.Snapshot, SnapshotPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord SnapshotRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::ModuleSnapshot,
			MoveTemp(SnapshotPayload), SnapshotRecord))
		{
			return false;
		}
		Artifacts.ModuleSnapshot.RecordId = SnapshotRecord.RecordId;
		return ReplaceRecord(Artifacts, Records.SnapshotRecordId,
			MoveTemp(SnapshotRecord));
	}

	static bool MutateExecutionArtifact(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EOpaqueMutation Mutation)
	{
		FDecodedOpaqueRecords Records;
		if (!DecodeOpaqueRecords(Artifacts, Records)
			|| !MutateOpaquePayload(
				Records.FunctionBody.CanonicalExecutionPayload, Mutation))
		{
			return false;
		}
		Records.FunctionBody.Identity.Content =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Records.FunctionBody.CanonicalExecutionPayload,
				Records.DebugSidecar.CanonicalDebugPayload);
		return RebuildFunctionAndSnapshot(Artifacts, Records);
	}

	static bool MutateDebugArtifact(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EOpaqueMutation Mutation)
	{
		FDecodedOpaqueRecords Records;
		if (!DecodeOpaqueRecords(Artifacts, Records)
			|| !MutateOpaquePayload(
				Records.DebugSidecar.CanonicalDebugPayload, Mutation))
		{
			return false;
		}

		Records.DebugSidecar.DebugHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				{}, Records.DebugSidecar.CanonicalDebugPayload).Debug;
		TArray<uint8> DebugPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar(
				Records.DebugSidecar, DebugPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord DebugRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::DebugSidecar,
			MoveTemp(DebugPayload), DebugRecord))
		{
			return false;
		}
		const FAngelscriptCacheRecordId NewDebugRecordId = DebugRecord.RecordId;
		if (!ReplaceRecord(Artifacts, Records.DebugRecordId, MoveTemp(DebugRecord)))
		{
			return false;
		}

		Records.FunctionBody.DebugSidecar = NewDebugRecordId;
		Records.FunctionBody.Identity.Content =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Records.FunctionBody.CanonicalExecutionPayload,
				Records.DebugSidecar.CanonicalDebugPayload);
		return RebuildFunctionAndSnapshot(Artifacts, Records);
	}

	static FValidationObservation ValidateCandidate(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheCleanModuleArtifacts Candidate)
	{
		FValidationObservation Observation;
		Observation.Output = Candidate;
		Observation.Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module, Options, MoveTemp(Candidate), Observation.Output);
		return Observation;
	}

	static FString MutationName(const EOpaqueMutation Mutation)
	{
		return Mutation == EOpaqueMutation::Truncate
			? TEXT("Truncate") : TEXT("CorruptMagic");
	}

public:
	TEST_METHOD(ExecutionArtifactCorruptionFailsAtOpaqueGraphBoundary)
	{
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		TSharedPtr<FAngelscriptModuleDesc> Module;
		FAngelscriptCacheCleanModuleArtifacts Baseline;
		ASSERT_THAT(IsTrue(CaptureCandidate(
			*TestRunner,
			Fixture,
			"ASCacheV2OpaqueExecution",
			Options,
			Module,
			Baseline)));

		for (const EOpaqueMutation Mutation : {
			EOpaqueMutation::Truncate,
			EOpaqueMutation::CorruptMagic})
		{
			FAngelscriptCacheCleanModuleArtifacts Candidate = Baseline;
			ASSERT_THAT(IsTrue(MutateExecutionArtifact(Candidate, Mutation),
				TEXT("The execution fixture should remain locally self-consistent after mutation")));
			const FValidationObservation Observation = ValidateCandidate(
				Module.ToSharedRef(), Options, MoveTemp(Candidate));
			TestRunner->AddInfo(FString::Printf(
				TEXT("Execution opaque rejection: Mutation=%s Error=%u GraphRecords=%u OutputRecords=%d Detail=%s"),
				*MutationName(Mutation),
				static_cast<uint32>(Observation.Result.Error),
				Observation.Result.ValidatedGraphRecordCount,
				Observation.Output.Records.Num(),
				*Observation.Result.Detail));

			ASSERT_THAT(AreEqual(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				Observation.Result.Error));
			ASSERT_THAT(AreEqual(uint32(0),
				Observation.Result.ValidatedGraphRecordCount));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(TEXT("Kind=5"))));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(TEXT("Stage=4"))));
			ASSERT_THAT(IsTrue(Observation.Output.Records.IsEmpty()));
			ASSERT_THAT(IsTrue(Observation.Output.ModuleKey.Hash.IsZero()));
			ASSERT_THAT(IsTrue(Observation.Output.SourceSnapshot.IsZero()));
		}
	}

	TEST_METHOD(DebugArtifactCorruptionFailsAtOpaqueGraphBoundary)
	{
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		TSharedPtr<FAngelscriptModuleDesc> Module;
		FAngelscriptCacheCleanModuleArtifacts Baseline;
		ASSERT_THAT(IsTrue(CaptureCandidate(
			*TestRunner,
			Fixture,
			"ASCacheV2OpaqueDebug",
			Options,
			Module,
			Baseline)));

		for (const EOpaqueMutation Mutation : {
			EOpaqueMutation::Truncate,
			EOpaqueMutation::CorruptMagic})
		{
			FAngelscriptCacheCleanModuleArtifacts Candidate = Baseline;
			ASSERT_THAT(IsTrue(MutateDebugArtifact(Candidate, Mutation),
				TEXT("The debug fixture should remain locally self-consistent after mutation")));
			const FValidationObservation Observation = ValidateCandidate(
				Module.ToSharedRef(), Options, MoveTemp(Candidate));
			TestRunner->AddInfo(FString::Printf(
				TEXT("Debug opaque rejection: Mutation=%s Error=%u GraphRecords=%u OutputRecords=%d Detail=%s"),
				*MutationName(Mutation),
				static_cast<uint32>(Observation.Result.Error),
				Observation.Result.ValidatedGraphRecordCount,
				Observation.Output.Records.Num(),
				*Observation.Result.Detail));

			ASSERT_THAT(AreEqual(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				Observation.Result.Error));
			ASSERT_THAT(AreEqual(uint32(0),
				Observation.Result.ValidatedGraphRecordCount));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(TEXT("Kind=6"))));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(TEXT("Stage=4"))));
			ASSERT_THAT(IsTrue(Observation.Output.Records.IsEmpty()));
			ASSERT_THAT(IsTrue(Observation.Output.ModuleKey.Hash.IsZero()));
			ASSERT_THAT(IsTrue(Observation.Output.SourceSnapshot.IsZero()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
