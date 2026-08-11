#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheRestore.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFreshEngineRollbackTests_Private
{
	class FPreparedPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPreparedPackSource(
			const TArray<FAngelscriptEncodedPack>& InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			for (const FAngelscriptEncodedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			OutBytes = {};
			return false;
		}

	private:
		const TArray<FAngelscriptEncodedPack>& Packs;
	};

	struct FOpaqueRecords
	{
		FAngelscriptCacheRecordId FunctionRecordId;
		FAngelscriptCacheRecordId DebugRecordId;
		FAngelscriptCacheRecordId SnapshotRecordId;
		FAngelscriptCachedFunctionBody FunctionBody;
		FAngelscriptCachedDebugSidecar DebugSidecar;
		FAngelscriptCachedModuleSnapshot Snapshot;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2FreshEngineRollbackTest"),
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

	class FStopAfterPrepared final
		: public IAngelscriptCacheRestoreFaultInjector
	{
	public:
		virtual bool ShouldStopAt(
			const EAngelscriptCacheRestoreFaultPoint Point,
			const uint32 ModuleOrdinal,
			const FAngelscriptStableModuleKey& ModuleKey) override
		{
			if (Point != EAngelscriptCacheRestoreFaultPoint::AfterModulePrepared
				|| bStopped)
			{
				return false;
			}
			bStopped = true;
			ObservedOrdinal = ModuleOrdinal;
			ObservedModuleKey = ModuleKey;
			return true;
		}

		FAngelscriptStableModuleKey ObservedModuleKey;
		uint32 ObservedOrdinal = MAX_uint32;
		bool bStopped = false;
	};

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

	static bool DecodeOpaqueRecords(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FOpaqueRecords& OutRecords)
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

	static bool ReadUInt32(
		const TConstArrayView<uint8> Bytes,
		uint64& Offset,
		uint32& OutValue)
	{
		if (Offset > static_cast<uint64>(Bytes.Num())
			|| sizeof(uint32) > static_cast<uint64>(Bytes.Num()) - Offset)
		{
			return false;
		}
		const uint8* Data = Bytes.GetData() + Offset;
		OutValue = static_cast<uint32>(Data[0])
			| (static_cast<uint32>(Data[1]) << 8)
			| (static_cast<uint32>(Data[2]) << 16)
			| (static_cast<uint32>(Data[3]) << 24);
		Offset += sizeof(uint32);
		return true;
	}

	static bool SkipBytes(
		const TConstArrayView<uint8> Bytes,
		uint64& Offset,
		const uint64 Count)
	{
		if (Offset > static_cast<uint64>(Bytes.Num())
			|| Count > static_cast<uint64>(Bytes.Num()) - Offset)
		{
			return false;
		}
		Offset += Count;
		return true;
	}

	static bool LocateDebugParameterCount(
		const TConstArrayView<uint8> Bytes,
		uint64& OutOffset,
		uint32& OutCount)
	{
		uint64 Offset = sizeof(uint64) + sizeof(uint32);
		uint32 SourceCount = 0;
		if (!ReadUInt32(Bytes, Offset, SourceCount))
		{
			return false;
		}
		for (uint32 Index = 0; Index < SourceCount; ++Index)
		{
			uint32 StringBytes = 0;
			if (!SkipBytes(Bytes, Offset, 64)
				|| !ReadUInt32(Bytes, Offset, StringBytes)
				|| !SkipBytes(Bytes, Offset, StringBytes))
			{
				return false;
			}
		}

		uint32 Count = 0;
		if (!SkipBytes(Bytes, Offset, sizeof(uint32))
			|| !ReadUInt32(Bytes, Offset, Count)
			|| !SkipBytes(Bytes, Offset, static_cast<uint64>(Count) * 4u)
			|| !ReadUInt32(Bytes, Offset, Count)
			|| !SkipBytes(Bytes, Offset, static_cast<uint64>(Count) * 8u))
		{
			return false;
		}
		OutOffset = Offset;
		return ReadUInt32(Bytes, Offset, OutCount);
	}

	static void WriteUInt32(
		TArray<uint8>& Bytes,
		const uint64 Offset,
		const uint32 Value)
	{
		Bytes[Offset + 0] = static_cast<uint8>(Value);
		Bytes[Offset + 1] = static_cast<uint8>(Value >> 8);
		Bytes[Offset + 2] = static_cast<uint8>(Value >> 16);
		Bytes[Offset + 3] = static_cast<uint8>(Value >> 24);
	}

	static bool AddImpossibleDebugParameter(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		FOpaqueRecords Records;
		if (!DecodeOpaqueRecords(Artifacts, Records))
		{
			return false;
		}

		uint64 ParameterCountOffset = 0;
		uint32 ParameterCount = 0;
		if (!LocateDebugParameterCount(
				Records.DebugSidecar.CanonicalDebugPayload,
				ParameterCountOffset,
				ParameterCount)
			|| ParameterCount != 0
			|| ParameterCountOffset > static_cast<uint64>(MAX_int32 - 4))
		{
			return false;
		}

		TArray<uint8>& DebugBytes =
			Records.DebugSidecar.CanonicalDebugPayload;
		DebugBytes.InsertZeroed(
			static_cast<int32>(ParameterCountOffset + sizeof(uint32)),
			sizeof(uint32));
		WriteUInt32(DebugBytes, ParameterCountOffset, 1);
		Records.DebugSidecar.DebugHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				{}, DebugBytes).Debug;

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
		if (!ReplaceRecord(
			Artifacts, Records.DebugRecordId, MoveTemp(DebugRecord)))
		{
			return false;
		}

		Records.FunctionBody.DebugSidecar = NewDebugRecordId;
		Records.FunctionBody.Identity.Content =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Records.FunctionBody.CanonicalExecutionPayload, DebugBytes);
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
		if (!ReplaceRecord(
			Artifacts, Records.FunctionRecordId, MoveTemp(FunctionRecord))
			|| Records.Snapshot.FunctionBodies.Num() != 1)
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
		return ReplaceRecord(
			Artifacts, Records.SnapshotRecordId, MoveTemp(SnapshotRecord));
	}

	static bool CaptureCleanGeneration(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCachePreparedColdGeneration& OutPrepared,
		FAngelscriptStableModuleKey& OutModuleKey,
		FAngelscriptStableFunctionKey& OutFunctionKey)
	{
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				Test, ETestEngineMode::IsolatedFull);
			if (!Producer.IsValid())
			{
				return false;
			}
			const FString Source = ASTEST_AS(R"AS(
				enum ECacheV2RollbackState
				{
					Ready = 7,
				}

				int Answer()
				{
					return 42;
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2FreshEngineRollback", Source);
			if (ScriptModule == nullptr)
			{
				return false;
			}
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			if (!Module.IsValid()
				|| !CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts).IsSuccess())
			{
				return false;
			}
			OutModuleKey = Artifacts.ModuleKey;
		}
		if (Artifacts.ValidatedFunctionArtifactIdentities.Num() != 1)
		{
			return false;
		}
		OutFunctionKey =
			Artifacts.ValidatedFunctionArtifactIdentities[0].FunctionKey;

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheCleanCaptureResult Prepare =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts, Options, PackPolicy, Codec, OutPrepared);
		Test.AddInfo(FString::Printf(
			TEXT("Rollback candidate: PrepareError=%u Records=%d Packs=%d Generation=%s Detail=%s"),
			static_cast<uint32>(Prepare.Error),
			Artifacts.Records.Num(),
			OutPrepared.Packs.Num(),
			*OutPrepared.EncodedManifest.ComputedGenerationId.ToHexString(),
			*Prepare.Detail));
		return Prepare.IsSuccess();
	}

	static bool ReopenPreparedGeneration(
		const FAngelscriptCachePreparedColdGeneration& Prepared,
		FAngelscriptValidatedGeneration& OutGeneration)
	{
		FPreparedPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TOptional<FAngelscriptValidatedGeneration> Reopened;
		const FAngelscriptCacheValidationResult Result =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				Packs,
				Limits,
				Budget,
				Codec,
				Reopened);
		if (!Result.IsSuccess() || !Reopened.IsSet())
		{
			return false;
		}
		OutGeneration = MoveTemp(Reopened.GetValue());
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFreshEngineRollbackTests,
	"Angelscript.TestModule.Cache.FreshEngineRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LateArtifactFailureLeavesFreshEngineUnchanged)
	{
		using namespace AngelscriptCacheFreshEngineRollbackTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCachePreparedColdGeneration Prepared;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableFunctionKey FunctionKey;
		ASSERT_THAT(IsTrue(CaptureCleanGeneration(
			*TestRunner, Options, Prepared, ModuleKey, FunctionKey)));

		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(ReopenPreparedGeneration(Prepared, Generation)));
		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		const int32 ActiveModulesBefore =
			Consumer.GetEngine().GetActiveModules().Num();
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesBefore =
			Consumer.GetEngine().GetFunctionRouteSnapshot();

		FAngelscriptCacheReadLimits Limits;
		FStopAfterPrepared Injector;
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(), Generation, ModuleKey, Limits, &Injector);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Rollback injected late failure: Error=%u Stage=%u Stopped=%d Ordinal=%u ActiveBefore=%d ActiveAfter=%d Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Injector.bStopped ? 1 : 0,
			Injector.ObservedOrdinal,
			ActiveModulesBefore,
			Consumer.GetEngine().GetActiveModules().Num(),
			*Restore.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRestoreError::ActivationFailed, Restore.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRestoreStage::FinalizeModule, Restore.Stage));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredModuleCount));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredTypeCount));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredFunctionCount));
		ASSERT_THAT(IsTrue(Injector.bStopped));
		ASSERT_THAT(AreEqual(uint32(0), Injector.ObservedOrdinal));
		ASSERT_THAT(IsTrue(Injector.ObservedModuleKey == ModuleKey));
		ASSERT_THAT(AreEqual(
			ActiveModulesBefore,
			Consumer.GetEngine().GetActiveModules().Num()));
		ASSERT_THAT(IsTrue(
			Consumer.GetEngine().GetFunctionRouteSnapshot().Get()
				== RoutesBefore.Get()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(
			TEXT("ASCacheV2FreshEngineRollback")).IsValid()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetEnum(
			TEXT("ECacheV2RollbackState")).IsValid()));
		FAngelscriptCacheLiveFunctionRoute RejectedRoute;
		ASSERT_THAT(IsFalse(Consumer.GetEngine().ResolveCacheFunctionRoute(
			FunctionKey, RejectedRoute)));

		const FString AuthoritativeSource = ASTEST_AS(R"AS(
			enum ECacheV2RollbackState
			{
				Ready = 7,
			}

			int Answer()
			{
				return 42;
			}
		)AS");
		asIScriptModule* Compiled = Consumer.BuildModule(
			"ASCacheV2FreshEngineRollback", AuthoritativeSource);
		ASSERT_THAT(IsNotNull(Compiled));
		asIScriptFunction* Answer = Compiled->GetFunctionByDecl("int Answer()");
		ASSERT_THAT(IsNotNull(Answer));
		int32 AnswerValue = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Answer, AnswerValue)));
		ASSERT_THAT(AreEqual(42, AnswerValue));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Rollback authoritative fallback: Module=%s Result=%d"),
			TEXT("ASCacheV2FreshEngineRollback"),
			AnswerValue));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
