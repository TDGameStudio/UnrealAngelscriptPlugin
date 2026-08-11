#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheModuleGraphHardValueTests_Private
{
	enum class EGraphMutation : uint8
	{
		ExpectedDeclarationAbi,
		HardValueType,
		SurplusUnownedValue,
	};

	struct FDecodedStateRecords
	{
		FAngelscriptCacheRecordId StateRecordId;
		FAngelscriptCacheRecordId SnapshotRecordId;
		FAngelscriptCachedModuleState State;
		FAngelscriptCachedModuleSnapshot Snapshot;
	};

	struct FValidationObservation
	{
		FAngelscriptCacheCleanCaptureResult Result;
		FAngelscriptCacheCleanModuleArtifacts Output;
	};

	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte);
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2ModuleGraphHardValueTest"),
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
			Test.AddError(TEXT("Failed to create the HardValue isolated Engine"));
			return false;
		}

		const FString Source = ASTEST_AS(R"AS(
			class FCachePayload
			{
				int Count;
			}

			const int GCacheAnswer = 41;

			int GetCacheAnswer()
			{
				return 7;
			}
			)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(ModuleName, Source);
		if (ScriptModule == nullptr)
		{
			return false;
		}
		OutModule = Fixture.GetEngine().GetModule(ScriptModule);
		if (!OutModule.IsValid())
		{
			Test.AddError(TEXT("The HardValue compile lost its module descriptor"));
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				OutModule.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("HardValue baseline capture: Module=%s Error=%u Records=%d Graph=%u Detail=%s"),
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
		return Artifacts.Records.FindByPredicate(
			[Kind](const FAngelscriptPreparedRecord& Record)
			{
				return Record.RecordId.Kind == Kind;
			});
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

	static bool DecodeStateRecords(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FDecodedStateRecords& OutRecords)
	{
		const FAngelscriptPreparedRecord* StateRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleState);
		const FAngelscriptPreparedRecord* SnapshotRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleSnapshot);
		if (StateRecord == nullptr || SnapshotRecord == nullptr)
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptDecodedCacheRecordBatch Batch(Budget, Limits);
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedState;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedSnapshot;
		if (!Batch.TryDecode(StateRecord->RecordId,
				StateRecord->CanonicalPayload, DecodedState).IsSuccess()
			|| !DecodedState.IsSet()
			|| !Batch.TryDecode(SnapshotRecord->RecordId,
				SnapshotRecord->CanonicalPayload, DecodedSnapshot).IsSuccess()
			|| !DecodedSnapshot.IsSet())
		{
			return false;
		}
		const FAngelscriptCachedModuleState* State =
			DecodedState.GetValue()->TryGetModuleState();
		const FAngelscriptCachedModuleSnapshot* Snapshot =
			DecodedSnapshot.GetValue()->TryGetModuleSnapshot();
		if (State == nullptr || Snapshot == nullptr)
		{
			return false;
		}

		OutRecords.StateRecordId = StateRecord->RecordId;
		OutRecords.SnapshotRecordId = SnapshotRecord->RecordId;
		OutRecords.State = *State;
		OutRecords.Snapshot = *Snapshot;
		return true;
	}

	static bool RefreshHardValue(
		FAngelscriptCachedHardValue& HardValue)
	{
		return FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalConstantHardValueHash(
				HardValue, HardValue.HardValueHash).IsSuccess();
	}

	static bool MutateState(
		FDecodedStateRecords& Records,
		const EGraphMutation Mutation,
		EAngelscriptModuleStateCapturedField& OutExpectedField,
		uint32& OutExpectedPrimary,
		uint32& OutExpectedSecondary,
		EAngelscriptCacheValidationError& OutExpectedError)
	{
		if (Records.State.OrderedGlobals.Num() != 1
			|| Records.State.HardValues.Num() != 1)
		{
			return false;
		}
		FAngelscriptCachedHardValue& HardValue = Records.State.HardValues[0];
		switch (Mutation)
		{
		case EGraphMutation::ExpectedDeclarationAbi:
			HardValue.Owner.ExpectedAbi = RepeatedByteHash(0xa1);
			OutExpectedField = EAngelscriptModuleStateCapturedField::
				HardValueOwnerExpectedAbi;
			OutExpectedPrimary = 0;
			OutExpectedSecondary = MAX_uint32;
			OutExpectedError =
				EAngelscriptCacheValidationError::GraphAbiMismatch;
			return RefreshHardValue(HardValue);

		case EGraphMutation::HardValueType:
			HardValue.Type.Primitive = EAngelscriptCachedPrimitiveType::Int64;
			HardValue.CanonicalValue->FixedWidthValueBytes.SetNumZeroed(8);
			HardValue.CanonicalValue->FixedWidthValueBytes[0] = 41;
			OutExpectedField =
				EAngelscriptModuleStateCapturedField::HardValueTypeNode;
			OutExpectedPrimary = 0;
			OutExpectedSecondary = 0;
			OutExpectedError =
				EAngelscriptCacheValidationError::GlobalCoverageMismatch;
			return RefreshHardValue(HardValue);

		case EGraphMutation::SurplusUnownedValue:
		{
			FAngelscriptCachedHardValue Extra = HardValue;
			Extra.Owner.StableKey = RepeatedByteHash(0xff);
			Extra.Owner.ExpectedAbi = RepeatedByteHash(0xf1);
			if (!RefreshHardValue(Extra))
			{
				return false;
			}
			Records.State.HardValues.Add(MoveTemp(Extra));
			OutExpectedField = EAngelscriptModuleStateCapturedField::HardValue;
			OutExpectedPrimary = 1;
			OutExpectedSecondary = MAX_uint32;
			OutExpectedError =
				EAngelscriptCacheValidationError::GlobalCoverageMismatch;
			return true;
		}

		default:
			return false;
		}
	}

	static bool RebuildStateAndSnapshot(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FDecodedStateRecords& Records,
		const EAngelscriptModuleStateCapturedField ExpectedField,
		const uint32 ExpectedPrimary,
		const uint32 ExpectedSecondary,
		uint64& OutExpectedOffset)
	{
		if (!FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
			Records.State, Records.State.StateInputHash).IsSuccess())
		{
			return false;
		}
		TArray<uint8> StatePayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			Records.State, StatePayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord StateRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::ModuleState,
			MoveTemp(StatePayload), StateRecord))
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedState;
		if (!FAngelscriptDecodedCacheRecord::TryDecode(
				StateRecord.RecordId,
				StateRecord.CanonicalPayload,
				Limits,
				Budget,
				DecodedState).IsSuccess()
			|| !DecodedState.IsSet())
		{
			return false;
		}
		const TOptional<uint64> ExpectedOffset =
			DecodedState.GetValue()->FindCapturedOffset({
				ExpectedField,
				ExpectedPrimary,
				ExpectedSecondary,
				MAX_uint32});
		if (!ExpectedOffset.IsSet())
		{
			return false;
		}
		OutExpectedOffset = ExpectedOffset.GetValue();

		const FAngelscriptCacheRecordId NewStateRecordId = StateRecord.RecordId;
		if (!ReplaceRecord(
			Artifacts, Records.StateRecordId, MoveTemp(StateRecord)))
		{
			return false;
		}
		Records.Snapshot.ModuleState.RecordId = NewStateRecordId;
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
		return ReplaceRecord(
			Artifacts, Records.SnapshotRecordId, MoveTemp(SnapshotRecord));
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

	static FString MutationName(const EGraphMutation Mutation)
	{
		switch (Mutation)
		{
		case EGraphMutation::ExpectedDeclarationAbi:
			return TEXT("ExpectedDeclarationAbi");
		case EGraphMutation::HardValueType:
			return TEXT("HardValueType");
		case EGraphMutation::SurplusUnownedValue:
			return TEXT("SurplusUnownedValue");
		default:
			return TEXT("Unknown");
		}
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheModuleGraphHardValueTests,
	"Angelscript.TestModule.Cache.ModuleGraphHardValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ChangedOwnerKeyIsRejectedByLocalCoverageBeforeGraphAdmission)
	{
		using namespace AngelscriptCacheModuleGraphHardValueTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		TSharedPtr<FAngelscriptModuleDesc> Module;
		FAngelscriptCacheCleanModuleArtifacts Baseline;
		ASSERT_THAT(IsTrue(CaptureCandidate(
			*TestRunner,
			Fixture,
			"ASCacheV2HardValueOwnerLocal",
			Options,
			Module,
			Baseline)));

		FDecodedStateRecords Records;
		ASSERT_THAT(IsTrue(DecodeStateRecords(Baseline, Records)));
		ASSERT_THAT(AreEqual(1, Records.State.OrderedGlobals.Num()));
		ASSERT_THAT(AreEqual(1, Records.State.HardValues.Num()));
		Records.State.HardValues[0].Owner.StableKey = RepeatedByteHash(0xfe);
		ASSERT_THAT(IsTrue(RefreshHardValue(Records.State.HardValues[0])));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
				Records.State, Records.State.StateInputHash).IsSuccess()));

		TArray<uint8> RejectedPayload;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
				Records.State, RejectedPayload);
		TestRunner->AddInfo(FString::Printf(
			TEXT("HardValue local owner rejection: Error=%u Class=%u Kind=%u Stage=%u Offset=%llu Payload=%d"),
			static_cast<uint32>(Result.Error),
			static_cast<uint32>(Result.Class),
			static_cast<uint32>(Result.RecordKind),
			static_cast<uint32>(Result.Stage),
			Result.ByteOffset,
			RejectedPayload.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::LocalSemantic,
			Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRecordKind::ModuleState,
			Result.RecordKind));
		ASSERT_THAT(IsTrue(RejectedPayload.IsEmpty()));
	}

	TEST_METHOD(CrossRecordHardValueMismatchesFailClosedAtExactStateCoordinates)
	{
		using namespace AngelscriptCacheModuleGraphHardValueTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		TSharedPtr<FAngelscriptModuleDesc> Module;
		FAngelscriptCacheCleanModuleArtifacts Baseline;
		ASSERT_THAT(IsTrue(CaptureCandidate(
			*TestRunner,
			Fixture,
			"ASCacheV2HardValueGraph",
			Options,
			Module,
			Baseline)));

		for (const EGraphMutation Mutation : {
			EGraphMutation::ExpectedDeclarationAbi,
			EGraphMutation::HardValueType,
			EGraphMutation::SurplusUnownedValue})
		{
			FAngelscriptCacheCleanModuleArtifacts Candidate = Baseline;
			FDecodedStateRecords Records;
			ASSERT_THAT(IsTrue(DecodeStateRecords(Candidate, Records)));
			EAngelscriptModuleStateCapturedField ExpectedField =
				EAngelscriptModuleStateCapturedField::Invalid;
			uint32 ExpectedPrimary = MAX_uint32;
			uint32 ExpectedSecondary = MAX_uint32;
			EAngelscriptCacheValidationError ExpectedError =
				EAngelscriptCacheValidationError::None;
			ASSERT_THAT(IsTrue(MutateState(
				Records,
				Mutation,
				ExpectedField,
				ExpectedPrimary,
				ExpectedSecondary,
				ExpectedError)));
			uint64 ExpectedOffset = 0;
			ASSERT_THAT(IsTrue(RebuildStateAndSnapshot(
				Candidate,
				Records,
				ExpectedField,
				ExpectedPrimary,
				ExpectedSecondary,
				ExpectedOffset)));

			const FValidationObservation Observation = ValidateCandidate(
				Module.ToSharedRef(), Options, MoveTemp(Candidate));
			TestRunner->AddInfo(FString::Printf(
				TEXT("HardValue graph rejection: Mutation=%s ExpectedError=%u ExpectedField=%u ExpectedOffset=%llu Result=%u GraphRecords=%u OutputRecords=%d Detail=%s"),
				*MutationName(Mutation),
				static_cast<uint32>(ExpectedError),
				static_cast<uint32>(ExpectedField),
				ExpectedOffset,
				static_cast<uint32>(Observation.Result.Error),
				Observation.Result.ValidatedGraphRecordCount,
				Observation.Output.Records.Num(),
				*Observation.Result.Detail));

			ASSERT_THAT(AreEqual(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				Observation.Result.Error));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(
				FString::Printf(TEXT("Error=%u"),
					static_cast<uint32>(ExpectedError)))));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(
				FString::Printf(TEXT("Kind=%u"), static_cast<uint32>(
					EAngelscriptCacheRecordKind::ModuleSnapshot)))));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(
				FString::Printf(TEXT("Stage=%u"), static_cast<uint32>(
					EAngelscriptCacheValidationStage::ModuleGraph)))));
			ASSERT_THAT(IsTrue(Observation.Result.Detail.Contains(
				FString::Printf(TEXT("Offset=%llu"), ExpectedOffset))));
			ASSERT_THAT(AreEqual(
				uint32(0), Observation.Result.ValidatedGraphRecordCount));
			ASSERT_THAT(IsTrue(Observation.Output.Records.IsEmpty()));
			ASSERT_THAT(IsTrue(Observation.Output.ModuleKey.Hash.IsZero()));
			ASSERT_THAT(IsTrue(Observation.Output.SourceSnapshot.IsZero()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
