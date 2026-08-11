#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheRemainingRecordCodecTests,
	"Angelscript.TestModule.Cache.Archive.RemainingRecordCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < static_cast<int32>(sizeof(Bytes)); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCachedDebugSidecar MakeDebugSidecar()
	{
		FAngelscriptCachedDebugSidecar Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::DebugSidecarPayloadSchemaVersion;
		Value.FunctionKey.Hash = MakeHash(0x10);
		Value.Profile.Hash = MakeHash(0x40);
		Value.VmDebugCodecVersion = 1;
		Value.CanonicalDebugPayload = {0x10, 0x20};
		Value.DebugHash = FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
			{}, Value.CanonicalDebugPayload).Debug;

		FAngelscriptCachedDebugSourceReference Source;
		Source.SourceFileKey.Hash = MakeHash(0x70);
		Source.CanonicalLogicalSection = TEXT("Module.as");
		const FAngelscriptCacheValidationResult KeyResult =
			FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
				Source.SourceFileKey,
				Source.CanonicalLogicalSection,
				Source.LogicalSectionKey);
		check(KeyResult.IsSuccess());
		Value.Sources.Add(MoveTemp(Source));
		return Value;
	}

	static FAngelscriptCacheRecordId MakeRecordId(
		const EAngelscriptCacheRecordKind Kind,
		const uint8 Seed)
	{
		return FAngelscriptCacheRecordId{Kind, MakeHash(Seed)};
	}

	static FAngelscriptCachedModuleSnapshot MakeModuleSnapshot()
	{
		FAngelscriptCachedModuleSnapshot Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
		Value.ModuleKey.Hash = MakeHash(0x11);
		Value.ModuleInterface.ModuleKey = Value.ModuleKey;
		Value.ModuleInterface.RecordId = MakeRecordId(
			EAngelscriptCacheRecordKind::ModuleInterface, 0x31);
		Value.TypeSchemas.Add({
			FAngelscriptStableTypeKey{MakeHash(0x51)},
			MakeRecordId(EAngelscriptCacheRecordKind::TypeSchema, 0x71)});
		Value.ModuleState.ModuleKey = Value.ModuleKey;
		Value.ModuleState.RecordId = MakeRecordId(
			EAngelscriptCacheRecordKind::ModuleState, 0x91);
		Value.FunctionBodies.Add({
			FAngelscriptStableFunctionKey{MakeHash(0xb1)},
			MakeRecordId(EAngelscriptCacheRecordKind::FunctionBody, 0xd1)});
		return Value;
	}

	static FAngelscriptCachedFunctionBody MakeFunctionBody()
	{
		FAngelscriptCachedFunctionBody Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion;
		Value.ModuleKey.Hash = MakeHash(0x12);
		Value.Identity.FunctionKey.Hash = MakeHash(0x32);
		Value.Identity.Profile.Hash = MakeHash(0x52);
		Value.ExpectedDeclarationAbi = MakeHash(0x72);
		Value.FunctionSourceDigest.Hash = MakeHash(0x92);
		Value.FunctionInputDigest.Hash = MakeHash(0xb2);
		Value.InvocationKind = EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
		Value.VmExecutionCodecVersion = 1;
		Value.CanonicalExecutionPayload = {0x01, 0x02, 0x03};
		Value.Identity.Content.Execution =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Value.CanonicalExecutionPayload, {}).Execution;
		Value.Identity.Content.Debug = MakeHash(0xd2);
		Value.DebugSidecar = MakeRecordId(
			EAngelscriptCacheRecordKind::DebugSidecar, 0xf2);
		return Value;
	}

	static FAngelscriptCachedModuleState MakeEmptyModuleState()
	{
		FAngelscriptCachedModuleState Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
		Value.ModuleKey.Hash = MakeHash(0x14);
		Value.Profile.Hash = MakeHash(0x34);
		const FAngelscriptCacheValidationResult HashResult =
			FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
				Value, Value.StateInputHash);
		check(HashResult.IsSuccess());
		return Value;
	}

	static FAngelscriptCacheSemanticDependency MakeFunctionDependency(
		const EAngelscriptCacheSemanticDependencyKind DependencyKind,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const uint8 KeySeed,
		const uint8 AbiSeed,
		const TOptional<FAngelscriptHash256>& ExpectedContent = {})
	{
		FAngelscriptCacheSemanticDependency Value;
		Value.Kind = DependencyKind;
		Value.Target.Kind = ReferenceKind;
		Value.Target.StableKey = MakeHash(KeySeed);
		Value.Target.ExpectedAbi = MakeHash(AbiSeed);
		Value.ExpectedContentOrValue = ExpectedContent;
		return Value;
	}

public:
	TEST_METHOD(EmptyModuleStateRoundTripsThroughSoleFactoryWithExactTopLevelOffsets)
	{
		const FAngelscriptCachedModuleState SourceValue = MakeEmptyModuleState();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			SourceValue, Payload).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(124), Payload.Num(),
			TEXT("empty ModuleState has the exact V1 canonical byte length")));
		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::ModuleState, Payload, RecordId).IsSuccess()));
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId, Payload, FAngelscriptCacheReadLimits{}, Budget, Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const FAngelscriptCachedModuleState* RoundTripped =
			Decoded.GetValue()->TryGetModuleState();
		ASSERT_THAT(IsNotNull(RoundTripped));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleKey == SourceValue.ModuleKey));
		ASSERT_THAT(IsTrue(RoundTripped->Profile.Hash == SourceValue.Profile.Hash));
		ASSERT_THAT(IsTrue(RoundTripped->StateInputHash == SourceValue.StateInputHash));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->OrderedGlobals.Num()));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->HardValues.Num()));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->Initializers.Num()));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->OrderedInitializationActions.Num()));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->OrderedPostInitFunctions.Num()));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->Dependencies.Num()));

		const auto ExactOffset = [&Decoded](
			const EAngelscriptModuleStateCapturedField Field,
			const uint64 Expected)
		{
			const TOptional<uint64> Actual = Decoded.GetValue()->FindCapturedOffset({
				Field, MAX_uint32, MAX_uint32, MAX_uint32});
			return Actual.IsSet() && Actual.GetValue() == Expected;
		};
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::PayloadSchemaVersion, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::ModuleKey, 4)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Profile, 36)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::StateInputHash, 68)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::OrderedGlobals, 100)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValues, 104)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Initializers, 108)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::OrderedInitializationActions, 112)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::OrderedPostInitFunctions, 116)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Dependencies, 120)));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetReferencesAndRelocations()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetDecodedBytes(), Budget.GetResidentDecodedBytes()));
		UE_LOG(LogTemp, Display,
			TEXT("[CacheV2][ModuleState] empty bytes=%d state=%s resident=%llu"),
			Payload.Num(), *RoundTripped->StateInputHash.ToHexString(),
			Budget.GetResidentDecodedBytes());
	}

	TEST_METHOD(DebugSidecarRoundTripsThroughSoleFactoryWithExactOffsets)
	{
		const FAngelscriptCachedDebugSidecar SourceValue = MakeDebugSidecar();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar(
			SourceValue, Payload).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(195), Payload.Num(),
			TEXT("one-source DebugSidecar has the exact V1 canonical byte length")));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::DebugSidecar, Payload, RecordId).IsSuccess()));

		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId,
			Payload,
			FAngelscriptCacheReadLimits{},
			Budget,
			Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		ASSERT_THAT(IsNull(Decoded.GetValue()->TryGetSourceIndex()));
		ASSERT_THAT(IsNull(Decoded.GetValue()->TryGetModuleInterface()));
		ASSERT_THAT(IsNull(Decoded.GetValue()->TryGetTypeSchema()));
		ASSERT_THAT(IsNull(Decoded.GetValue()->TryGetModuleState()));
		ASSERT_THAT(IsNull(Decoded.GetValue()->TryGetFunctionBody()));
		ASSERT_THAT(IsNull(Decoded.GetValue()->TryGetModuleSnapshot()));

		const FAngelscriptCachedDebugSidecar* RoundTripped =
			Decoded.GetValue()->TryGetDebugSidecar();
		ASSERT_THAT(IsNotNull(RoundTripped));
		ASSERT_THAT(IsTrue(RoundTripped->FunctionKey == SourceValue.FunctionKey));
		ASSERT_THAT(IsTrue(RoundTripped->Profile.Hash == SourceValue.Profile.Hash));
		ASSERT_THAT(IsTrue(RoundTripped->DebugHash == SourceValue.DebugHash));
		ASSERT_THAT(AreEqual(uint32(1), RoundTripped->VmDebugCodecVersion));
		ASSERT_THAT(AreEqual(int32(1), RoundTripped->Sources.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Module.as")),
			RoundTripped->Sources[0].CanonicalLogicalSection));
		ASSERT_THAT(IsTrue(RoundTripped->Sources[0].LogicalSectionKey.Hash
			== SourceValue.Sources[0].LogicalSectionKey.Hash));
		ASSERT_THAT(AreEqual(int32(2), RoundTripped->CanonicalDebugPayload.Num()));
		ASSERT_THAT(AreEqual(uint8(0x10), RoundTripped->CanonicalDebugPayload[0]));
		ASSERT_THAT(AreEqual(uint8(0x20), RoundTripped->CanonicalDebugPayload[1]));

		const auto ExactOffset = [&Decoded](
			const EAngelscriptDebugSidecarCapturedField Field,
			const uint64 Expected,
			const uint32 PrimaryIndex = MAX_uint32)
		{
			const TOptional<uint64> Actual = Decoded.GetValue()->FindCapturedOffset({
				Field, PrimaryIndex, MAX_uint32, MAX_uint32});
			return Actual.IsSet() && Actual.GetValue() == Expected;
		};
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::PayloadSchemaVersion, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::FunctionKey, 4)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::Profile, 36)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::DebugHash, 68)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::VmDebugCodecVersion, 100)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::Sources, 104)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::Source, 108, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::SourceFileKey, 108, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::SourceLogicalSectionKey, 140, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::SourceCanonicalLogicalSection, 172, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptDebugSidecarCapturedField::CanonicalDebugPayload, 185)));

		ASSERT_THAT(IsTrue(Budget.GetDecodedBytes() > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetDecodedBytes(), Budget.GetResidentDecodedBytes(),
			TEXT("the sole factory promotes the complete candidate exactly once")));
	}

	TEST_METHOD(ModuleSnapshotRoundTripsThroughSoleFactoryWithExactLinks)
	{
		const FAngelscriptCachedModuleSnapshot SourceValue = MakeModuleSnapshot();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
			SourceValue, Payload).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(304), Payload.Num(),
			TEXT("one-type/one-body ModuleSnapshot has the exact V1 byte length")));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::ModuleSnapshot, Payload, RecordId).IsSuccess()));
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId,
			Payload,
			FAngelscriptCacheReadLimits{},
			Budget,
			Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const FAngelscriptCachedModuleSnapshot* RoundTripped =
			Decoded.GetValue()->TryGetModuleSnapshot();
		ASSERT_THAT(IsNotNull(RoundTripped));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleKey == SourceValue.ModuleKey));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleInterface.ModuleKey == SourceValue.ModuleKey));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleInterface.RecordId
			== SourceValue.ModuleInterface.RecordId));
		ASSERT_THAT(AreEqual(int32(1), RoundTripped->TypeSchemas.Num()));
		ASSERT_THAT(IsTrue(RoundTripped->TypeSchemas[0].TypeKey
			== SourceValue.TypeSchemas[0].TypeKey));
		ASSERT_THAT(IsTrue(RoundTripped->TypeSchemas[0].RecordId
			== SourceValue.TypeSchemas[0].RecordId));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleState.ModuleKey == SourceValue.ModuleKey));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleState.RecordId
			== SourceValue.ModuleState.RecordId));
		ASSERT_THAT(AreEqual(int32(1), RoundTripped->FunctionBodies.Num()));
		ASSERT_THAT(IsTrue(RoundTripped->FunctionBodies[0].FunctionKey
			== SourceValue.FunctionBodies[0].FunctionKey));
		ASSERT_THAT(IsTrue(RoundTripped->FunctionBodies[0].RecordId
			== SourceValue.FunctionBodies[0].RecordId));

		const auto ExactOffset = [&Decoded](
			const EAngelscriptModuleSnapshotCapturedField Field,
			const uint64 Expected,
			const uint32 PrimaryIndex = MAX_uint32)
		{
			const TOptional<uint64> Actual = Decoded.GetValue()->FindCapturedOffset({
				Field, PrimaryIndex, MAX_uint32, MAX_uint32});
			return Actual.IsSet() && Actual.GetValue() == Expected;
		};
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::PayloadSchemaVersion, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleKey, 4)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleInterface, 36)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceModuleKey, 36)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordId, 68)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordIdKind, 68)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordIdContentHash, 69)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::TypeSchemas, 101)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLink, 105, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkTypeKey, 105, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordId, 137, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordIdKind, 137, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordIdContentHash, 138, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleState, 170)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleStateModuleKey, 170)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordId, 202)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordIdKind, 202)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordIdContentHash, 203)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::FunctionBodies, 235)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::FunctionBodyLink, 239, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkFunctionKey, 239, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordId, 271, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdKind, 271, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdContentHash, 272, 0)));
		ASSERT_THAT(IsTrue(Budget.GetDecodedBytes() > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetDecodedBytes(), Budget.GetResidentDecodedBytes()));
	}

	TEST_METHOD(FunctionBodyRoundTripsExecutionIdentityAndOptionalDebugLink)
	{
		const FAngelscriptCachedFunctionBody SourceValue = MakeFunctionBody();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
			SourceValue, Payload).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(314), Payload.Num(),
			TEXT("zero-dependency/present-debug FunctionBody has the exact V1 byte length")));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody, Payload, RecordId).IsSuccess()));
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId,
			Payload,
			FAngelscriptCacheReadLimits{},
			Budget,
			Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const FAngelscriptCachedFunctionBody* RoundTripped =
			Decoded.GetValue()->TryGetFunctionBody();
		ASSERT_THAT(IsNotNull(RoundTripped));
		ASSERT_THAT(IsTrue(RoundTripped->ModuleKey == SourceValue.ModuleKey));
		ASSERT_THAT(IsTrue(RoundTripped->Identity.FunctionKey
			== SourceValue.Identity.FunctionKey));
		ASSERT_THAT(IsTrue(RoundTripped->Identity.Content.Execution
			== SourceValue.Identity.Content.Execution));
		ASSERT_THAT(IsTrue(RoundTripped->Identity.Content.Debug
			== SourceValue.Identity.Content.Debug));
		ASSERT_THAT(IsTrue(RoundTripped->Identity.Profile.Hash
			== SourceValue.Identity.Profile.Hash));
		ASSERT_THAT(IsTrue(RoundTripped->ExpectedDeclarationAbi
			== SourceValue.ExpectedDeclarationAbi));
		ASSERT_THAT(IsTrue(RoundTripped->FunctionSourceDigest.Hash
			== SourceValue.FunctionSourceDigest.Hash));
		ASSERT_THAT(IsTrue(RoundTripped->FunctionInputDigest.Hash
			== SourceValue.FunctionInputDigest.Hash));
		ASSERT_THAT(IsTrue(RoundTripped->InvocationKind
			== EAngelscriptCachedFunctionInvocationKind::GlobalFunction));
		ASSERT_THAT(AreEqual(uint32(1), RoundTripped->VmExecutionCodecVersion));
		ASSERT_THAT(AreEqual(int32(3), RoundTripped->CanonicalExecutionPayload.Num()));
		ASSERT_THAT(AreEqual(int32(0), RoundTripped->ActualDependencies.Num()));
		ASSERT_THAT(IsTrue(RoundTripped->DebugSidecar.IsSet()));
		ASSERT_THAT(IsTrue(RoundTripped->DebugSidecar.GetValue()
			== SourceValue.DebugSidecar.GetValue()));

		const auto ExactOffset = [&Decoded](
			const EAngelscriptFunctionBodyCapturedField Field,
			const uint64 Expected)
		{
			const TOptional<uint64> Actual = Decoded.GetValue()->FindCapturedOffset({
				Field, MAX_uint32, MAX_uint32, MAX_uint32});
			return Actual.IsSet() && Actual.GetValue() == Expected;
		};
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::PayloadSchemaVersion, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ModuleKey, 4)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::Identity, 36)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::IdentityFunctionKey, 36)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::IdentityContent, 68)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::IdentityContentExecution, 68)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::IdentityContentDebug, 100)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::IdentityProfile, 132)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ExpectedDeclarationAbi, 164)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::FunctionSourceDigest, 196)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::FunctionInputDigest, 228)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::InvocationKind, 260)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::VmExecutionCodecVersion, 261)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::CanonicalExecutionPayload, 265)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ActualDependencies, 276)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::DebugSidecarPresence, 280)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::DebugSidecar, 281)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::DebugSidecarKind, 281)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::DebugSidecarContentHash, 282)));
		ASSERT_THAT(IsFalse(Decoded.GetValue()->FindCapturedOffset({
			EAngelscriptFunctionBodyCapturedField::ActualDependency,
			0,
			MAX_uint32,
			MAX_uint32}).IsSet()));
		ASSERT_THAT(IsTrue(Budget.GetDecodedBytes() > 0));
		UE_LOG(LogTemp, Display,
			TEXT("[CacheV2][FunctionBody] bytes=%d dependencies=%d references=%llu resident=%llu"),
			Payload.Num(), RoundTripped->ActualDependencies.Num(),
			Budget.GetReferencesAndRelocations(), Budget.GetResidentDecodedBytes());
		ASSERT_THAT(AreEqual(UINT64_C(1), Budget.GetReferencesAndRelocations(),
			TEXT("the present DebugSidecar RecordId is one bounded graph reference")));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetDecodedBytes(), Budget.GetResidentDecodedBytes()));
	}

	TEST_METHOD(FunctionBodyCanonicalizesDependenciesAndSupportsProfileSpecificDebugAbsence)
	{
		FAngelscriptCachedFunctionBody SourceValue = MakeFunctionBody();
		SourceValue.DebugSidecar.Reset();
		SourceValue.Identity.Content.Debug =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(
				SourceValue.Identity.Profile);
		const FAngelscriptCacheSemanticDependency Declaration = MakeFunctionDependency(
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			EAngelscriptCacheReferenceKind::ScriptFunction,
			0x13,
			0x33);
		const FAngelscriptCacheSemanticDependency HardValue = MakeFunctionDependency(
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			0x53,
			0x73,
			MakeHash(0x93));
		// Deliberately reverse canonical kind order; the producer owns set ordering.
		SourceValue.ActualDependencies = {HardValue, Declaration};

		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
			SourceValue, Payload).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(447), Payload.Num(),
			TEXT("two-dependency/absent-debug FunctionBody has exact V1 length")));
		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody, Payload, RecordId).IsSuccess()));

		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId, Payload, FAngelscriptCacheReadLimits{}, Budget, Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const FAngelscriptCachedFunctionBody* RoundTripped =
			Decoded.GetValue()->TryGetFunctionBody();
		ASSERT_THAT(IsNotNull(RoundTripped));
		ASSERT_THAT(AreEqual(int32(2), RoundTripped->ActualDependencies.Num()));
		ASSERT_THAT(IsTrue(RoundTripped->ActualDependencies[0] == Declaration));
		ASSERT_THAT(IsTrue(RoundTripped->ActualDependencies[1] == HardValue));
		ASSERT_THAT(IsFalse(RoundTripped->DebugSidecar.IsSet()));
		ASSERT_THAT(IsTrue(RoundTripped->Identity.Content.Debug
			== FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(
				RoundTripped->Identity.Profile)));

		const auto ExactOffset = [&Decoded](
			const EAngelscriptFunctionBodyCapturedField Field,
			const uint32 PrimaryIndex,
			const uint64 Expected)
		{
			const TOptional<uint64> Actual = Decoded.GetValue()->FindCapturedOffset({
				Field, PrimaryIndex, MAX_uint32, MAX_uint32});
			return Actual.IsSet() && Actual.GetValue() == Expected;
		};
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ActualDependency, 0, 280)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ActualDependencyTarget, 0, 281)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ActualDependencyTargetStableKey,
			0, 282)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::
				ActualDependencyExpectedContentOrValuePresence,
			0, 346)));
		ASSERT_THAT(IsFalse(Decoded.GetValue()->FindCapturedOffset({
			EAngelscriptFunctionBodyCapturedField::
				ActualDependencyExpectedContentOrValue,
			0, MAX_uint32, MAX_uint32}).IsSet()));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::ActualDependency, 1, 347)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::
				ActualDependencyExpectedContentOrValuePresence,
			1, 413)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::
				ActualDependencyExpectedContentOrValue,
			1, 414)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptFunctionBodyCapturedField::DebugSidecarPresence,
			MAX_uint32, 446)));
		ASSERT_THAT(IsFalse(Decoded.GetValue()->FindCapturedOffset({
			EAngelscriptFunctionBodyCapturedField::DebugSidecar,
			MAX_uint32, MAX_uint32, MAX_uint32}).IsSet()));
		ASSERT_THAT(AreEqual(UINT64_C(2), Budget.GetReferencesAndRelocations()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));

		TArray<uint8> Reserialized;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
			*RoundTripped, Reserialized).IsSuccess()));
		ASSERT_THAT(IsTrue(Payload == Reserialized,
			TEXT("decoded canonical FunctionBody must reproduce identical bytes")));

		FAngelscriptCacheReadLimits TightLimits;
		TightLimits.MaxReferencesAndRelocations = 1;
		FAngelscriptCacheReadBudget TightBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Rejected;
		const FAngelscriptCacheValidationResult TightResult =
			FAngelscriptDecodedCacheRecord::TryDecode(
				RecordId, Payload, TightLimits, TightBudget, Rejected);
		UE_LOG(LogTemp, Display,
			TEXT("[CacheV2][FunctionBody] bounded-reference result=%u offset=%llu references=%llu temporary=%llu"),
			static_cast<uint8>(TightResult.Error), TightResult.ByteOffset,
			TightBudget.GetReferencesAndRelocations(),
			TightBudget.GetTemporaryResidentDecodedBytes());
		ASSERT_THAT(IsTrue(TightResult.Error
			== EAngelscriptCacheValidationError::BudgetExceeded));
		ASSERT_THAT(AreEqual(UINT64_C(348), TightResult.ByteOffset));
		ASSERT_THAT(IsFalse(Rejected.IsSet()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TightBudget.GetTemporaryResidentDecodedBytes()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
