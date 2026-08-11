#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheModuleStateValidationTests,
	"Angelscript.TestModule.Cache.Archive.ModuleStateValidation",
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

	static FAngelscriptCacheStableReference MakeReference(
		const EAngelscriptCacheReferenceKind Kind,
		const uint8 KeySeed,
		const uint8 AbiSeed)
	{
		return {Kind, MakeHash(KeySeed), MakeHash(AbiSeed)};
	}

	static FAngelscriptCachedModuleState MakeOneGlobalState()
	{
		FAngelscriptCachedModuleState State;
		State.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
		State.ModuleKey.Hash = MakeHash(0x10);
		State.Profile.Hash = MakeHash(0x30);

		FAngelscriptCachedGlobalSchema Global;
		Global.StorageOrdinal = 0;
		Global.GlobalKey.Hash = MakeHash(0x50);
		Global.CanonicalNamespace = TEXT("Gameplay");
		Global.CanonicalName = TEXT("Counter");
		Global.Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Global.Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		Global.InitializationKind = EAngelscriptCachedGlobalInitializationKind::Default;
		Global.CleanupPolicy = EAngelscriptCachedGlobalCleanupPolicy::None;
		check(FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalStorageLayoutFingerprint(
				State.ModuleKey, Global, Global.StorageLayoutFingerprint).IsSuccess());
		State.OrderedGlobals.Add(Global);
		check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
			State, State.StateInputHash).IsSuccess());
		return State;
	}

	static FAngelscriptCacheValidationResult Decode(
		const TArray<uint8>& Payload,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheRecordId RecordId;
		const FAngelscriptCacheValidationResult IdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::ModuleState, Payload, RecordId);
		if (!IdResult.IsSuccess())
		{
			return IdResult;
		}
		FAngelscriptCacheReadBudget Budget;
		return FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId,
			Payload,
			FAngelscriptCacheReadLimits{},
			Budget,
			OutRecord);
	}

public:
	TEST_METHOD(HashHelpersRejectInvalidSemanticInputs)
	{
		FAngelscriptStableModuleKey ModuleKey{MakeHash(0x10)};
		FAngelscriptArtifactProfileKey Profile{MakeHash(0x20)};
		FAngelscriptHash256 OutHash;

		FAngelscriptCachedGlobalSchema InvalidGlobal;
		InvalidGlobal.GlobalKey.Hash = MakeHash(0x30);
		InvalidGlobal.CanonicalName = TEXT("Invalid");
		InvalidGlobal.InitializationKind =
			EAngelscriptCachedGlobalInitializationKind::Default;
		InvalidGlobal.CleanupPolicy = EAngelscriptCachedGlobalCleanupPolicy::None;
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRemainingRecordArchive::
				ComputeGlobalStorageLayoutFingerprint(
					ModuleKey, InvalidGlobal, OutHash);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::UnknownEnumValue, Result.Error));

		FAngelscriptCachedHardValue InvalidHardValue;
		InvalidHardValue.HardValueKind =
			EAngelscriptCachedHardValueKind::GlobalConstant;
		InvalidHardValue.Owner = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptGlobal, 0x40, 0x50);
		InvalidHardValue.CanonicalValue.Emplace();
		InvalidHardValue.CanonicalValue->ValueKind =
			EAngelscriptCachedCanonicalValueKind::SignedInteger;
		InvalidHardValue.CanonicalValue->FixedWidthValueBytes = {1, 0, 0, 0};
		Result = FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalConstantHardValueHash(InvalidHardValue, OutHash);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::UnknownEnumValue, Result.Error));

		FAngelscriptCachedInitializerUnit InvalidInitializer;
		InvalidInitializer.InitializerKind = EAngelscriptCachedInitializerKind::Global;
		InvalidInitializer.InitializerKey.Hash = MakeHash(0x60);
		Result = FAngelscriptCacheRemainingRecordArchive::ComputeInitializerExecutionHash(
			ModuleKey, Profile, InvalidInitializer, OutHash);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::InvalidPresence, Result.Error));
	}

	TEST_METHOD(CanonicalDependencyInsertionOrderProducesIdenticalPayload)
	{
		FAngelscriptCachedModuleState Forward = MakeOneGlobalState();
		const FAngelscriptCacheSemanticDependency ModuleDependency{
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptModule, 0x70, 0x80),
			{}};
		const FAngelscriptCacheSemanticDependency FunctionDependency{
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x90, 0xa0),
			{}};
		Forward.Dependencies = {ModuleDependency, FunctionDependency};
		check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
			Forward, Forward.StateInputHash).IsSuccess());
		FAngelscriptCachedModuleState Reverse = Forward;
		Reverse.Dependencies = {FunctionDependency, ModuleDependency};
		check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
			Reverse, Reverse.StateInputHash).IsSuccess());

		TArray<uint8> ForwardPayload;
		TArray<uint8> ReversePayload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			Forward, ForwardPayload).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			Reverse, ReversePayload).IsSuccess()));
		ASSERT_THAT(IsTrue(ForwardPayload == ReversePayload));
	}

	TEST_METHOD(NestedHashMutationReportsItsExactCapturedOffset)
	{
		const FAngelscriptCachedModuleState State = MakeOneGlobalState();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			State, Payload).IsSuccess()));
		TOptional<FAngelscriptDecodedCacheRecordHandle> GoodRecord;
		ASSERT_THAT(IsTrue(Decode(Payload, GoodRecord).IsSuccess()));
		ASSERT_THAT(IsTrue(GoodRecord.IsSet()));
		const TOptional<uint64> FingerprintOffset = GoodRecord.GetValue()->FindCapturedOffset({
			EAngelscriptModuleStateCapturedField::GlobalStorageLayoutFingerprint,
			0,
			MAX_uint32,
			MAX_uint32});
		ASSERT_THAT(IsTrue(FingerprintOffset.IsSet()));
		Payload[static_cast<int32>(FingerprintOffset.GetValue())] ^= 0x01;

		TOptional<FAngelscriptDecodedCacheRecordHandle> Rejected;
		const FAngelscriptCacheValidationResult Result = Decode(Payload, Rejected);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::DerivedHashMismatch, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::LocalSemantic, Result.Stage));
		ASSERT_THAT(AreEqual(FingerprintOffset.GetValue(), Result.ByteOffset));
		ASSERT_THAT(IsFalse(Rejected.IsSet()));
	}

	TEST_METHOD(TrailingDataPrecedesNestedDerivedHashMismatch)
	{
		const FAngelscriptCachedModuleState State = MakeOneGlobalState();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			State, Payload).IsSuccess()));
		const int32 OriginalLength = Payload.Num();
		// The sole global fingerprint is the final 32 bytes before the next array count.
		Payload[OriginalLength - 52] ^= 0x01;
		Payload.Add(0x5a);

		TOptional<FAngelscriptDecodedCacheRecordHandle> Rejected;
		const FAngelscriptCacheValidationResult Result = Decode(Payload, Rejected);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::TrailingData, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(static_cast<uint64>(OriginalLength), Result.ByteOffset));
		ASSERT_THAT(IsFalse(Rejected.IsSet()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
