#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheModuleStateCodecTests,
	"Angelscript.TestModule.Cache.Archive.ModuleStateCodec",
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

	static FAngelscriptCachedDataType MakeInt32Type()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	static FAngelscriptCacheStableReference MakeReference(
		const EAngelscriptCacheReferenceKind Kind,
		const FAngelscriptHash256& StableKey,
		const uint8 AbiSeed)
	{
		return {Kind, StableKey, MakeHash(AbiSeed)};
	}

	static FAngelscriptCacheSemanticDependency MakeDependency(
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const FAngelscriptCacheStableReference& Target,
		const TOptional<FAngelscriptHash256>& Content = {})
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = Kind;
		Dependency.Target = Target;
		Dependency.ExpectedContentOrValue = Content;
		return Dependency;
	}

	static FAngelscriptCachedModuleState MakeRepresentativeState()
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
		Global.CanonicalName = TEXT("Answer");
		Global.Type = MakeInt32Type();
		Global.GlobalTraitFlags =
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Const);
		Global.InitializationKind = EAngelscriptCachedGlobalInitializationKind::PureConstant;
		Global.CleanupPolicy = EAngelscriptCachedGlobalCleanupPolicy::None;
		check(FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalStorageLayoutFingerprint(
				State.ModuleKey, Global, Global.StorageLayoutFingerprint).IsSuccess());
		State.OrderedGlobals.Add(Global);

		const FAngelscriptCacheStableReference GlobalReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptGlobal, Global.GlobalKey.Hash, 0x70);
		FAngelscriptCachedHardValue HardValue;
		HardValue.HardValueKind = EAngelscriptCachedHardValueKind::GlobalConstant;
		HardValue.Owner = GlobalReference;
		HardValue.Type = Global.Type;
		HardValue.CanonicalValue.Emplace();
		HardValue.CanonicalValue->ValueKind =
			EAngelscriptCachedCanonicalValueKind::SignedInteger;
		HardValue.CanonicalValue->FixedWidthValueBytes = {0x2a, 0x00, 0x00, 0x00};
		check(FAngelscriptCacheRemainingRecordArchive::ComputeGlobalConstantHardValueHash(
			HardValue, HardValue.HardValueHash).IsSuccess());
		State.HardValues.Add(HardValue);

		FAngelscriptCachedInitializerUnit Initializer;
		Initializer.InitializerKind = EAngelscriptCachedInitializerKind::Module;
		Initializer.InitializerKey.Hash = MakeHash(0x90);
		Initializer.VmInitializerCodecVersion = 1;
		Initializer.CanonicalExecutionPayload = {0xa0, 0xb0};
		check(FAngelscriptCacheRemainingRecordArchive::ComputeInitializerExecutionHash(
			State.ModuleKey,
			State.Profile,
			Initializer,
			Initializer.InitializerExecutionHash).IsSuccess());
		State.Initializers.Add(Initializer);

		const FAngelscriptCacheStableReference InitializerReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptFunction,
			Initializer.InitializerKey.Hash,
			0xb0);
		FAngelscriptCachedInitializationAction Action;
		Action.ActionOrdinal = 0;
		Action.ActionKind = EAngelscriptCachedInitializationActionKind::ExecuteInitializer;
		Action.Target = InitializerReference;
		Action.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::GlobalStorage,
			GlobalReference,
			Global.StorageLayoutFingerprint));
		State.OrderedInitializationActions.Add(Action);

		const FAngelscriptCacheStableReference PostInitReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptFunction, MakeHash(0xd0), 0xf0);
		State.OrderedPostInitFunctions.Add({0, PostInitReference});

		const FAngelscriptCacheStableReference ModuleReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptModule, State.ModuleKey.Hash, 0x20);
		const FAngelscriptCacheSemanticDependency DeclarationModule = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Declaration, ModuleReference);
		const FAngelscriptCacheSemanticDependency DeclarationInitializer = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Declaration, InitializerReference);
		const FAngelscriptCacheSemanticDependency DeclarationPostInit = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Declaration, PostInitReference);
		const FAngelscriptCacheSemanticDependency GlobalStorage = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::GlobalStorage,
			GlobalReference,
			Global.StorageLayoutFingerprint);
		const FAngelscriptCacheSemanticDependency GlobalHardValue = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			GlobalReference,
			HardValue.HardValueHash);
		// Deliberately reverse the canonical set. The producer owns set ordering.
		State.Dependencies = {
			GlobalHardValue,
			GlobalStorage,
			DeclarationPostInit,
			DeclarationInitializer,
			DeclarationModule};

		check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
			State, State.StateInputHash).IsSuccess());
		return State;
	}

public:
	TEST_METHOD(RepresentativeNonEmptyStateRoundTripsEveryCollectionAndExactOffsets)
	{
		const FAngelscriptCachedModuleState SourceValue = MakeRepresentativeState();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
			SourceValue, Payload).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(1075), Payload.Num(),
			TEXT("representative non-empty ModuleState has exact V1 wire length")));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::ModuleState, Payload, RecordId).IsSuccess()));
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId, Payload, FAngelscriptCacheReadLimits{}, Budget, Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const FAngelscriptCachedModuleState* State = Decoded.GetValue()->TryGetModuleState();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(int32(1), State->OrderedGlobals.Num()));
		ASSERT_THAT(AreEqual(int32(1), State->HardValues.Num()));
		ASSERT_THAT(AreEqual(int32(1), State->Initializers.Num()));
		ASSERT_THAT(AreEqual(int32(1), State->OrderedInitializationActions.Num()));
		ASSERT_THAT(AreEqual(int32(1), State->OrderedPostInitFunctions.Num()));
		ASSERT_THAT(AreEqual(int32(5), State->Dependencies.Num()));
		ASSERT_THAT(IsTrue(State->StateInputHash == SourceValue.StateInputHash));
		ASSERT_THAT(IsTrue(State->Dependencies[0].Kind
			== EAngelscriptCacheSemanticDependencyKind::Declaration));
		ASSERT_THAT(IsTrue(State->Dependencies[3].Kind
			== EAngelscriptCacheSemanticDependencyKind::GlobalStorage));
		ASSERT_THAT(IsTrue(State->Dependencies[4].Kind
			== EAngelscriptCacheSemanticDependencyKind::HardValue));

		const auto ExactOffset = [&Decoded](
			const EAngelscriptModuleStateCapturedField Field,
			const uint64 Expected,
			const uint32 Primary = MAX_uint32,
			const uint32 Secondary = MAX_uint32)
		{
			const TOptional<uint64> Actual = Decoded.GetValue()->FindCapturedOffset({
				Field, Primary, Secondary, MAX_uint32});
			return Actual.IsSet() && Actual.GetValue() == Expected;
		};
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::OrderedGlobals, 100)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Global, 104, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::GlobalCanonicalNamespace, 140, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 162, 0, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence, 164, 0, 0)));
		ASSERT_THAT(IsFalse(Decoded.GetValue()->FindCapturedOffset({
			EAngelscriptModuleStateCapturedField::GlobalTypeReference,
			0, 0, MAX_uint32}).IsSet()));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::GlobalStorageLayoutFingerprint, 179, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValues, 211)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValue, 215, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValueOwnerStableKey, 217, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 281, 0, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValuePresence, 292, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValue, 293, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValueFixedWidthValueBytes,
			294, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::HardValueHash, 306, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Initializers, 338)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Initializer, 342, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::InitializerOwnerGlobalPresence, 375, 0)));
		ASSERT_THAT(IsFalse(Decoded.GetValue()->FindCapturedOffset({
			EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal,
			0, MAX_uint32, MAX_uint32}).IsSet()));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::InitializerCanonicalExecutionPayload,
			412, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::OrderedInitializationActions, 422)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::InitializationAction, 426, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::InitializationActionDependencies, 496, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::InitializationActionDependency,
			500, 0, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::
				InitializationActionDependencyExpectedContentOrValue,
			567, 0, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::OrderedPostInitFunctions, 599)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::PostInitFunction, 603, 0)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Dependencies, 672)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::Dependency, 676, 0)));
		ASSERT_THAT(IsFalse(Decoded.GetValue()->FindCapturedOffset({
			EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue,
			0, MAX_uint32, MAX_uint32}).IsSet()));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue,
			944, 3)));
		ASSERT_THAT(IsTrue(ExactOffset(
			EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue,
			1043, 4)));

		ASSERT_THAT(AreEqual(UINT64_C(9), Budget.GetReferencesAndRelocations()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetDecodedBytes(), Budget.GetResidentDecodedBytes()));
		UE_LOG(LogTemp, Display,
			TEXT("[CacheV2][ModuleState] representative bytes=%d globals=%d dependencies=%d references=%llu resident=%llu"),
			Payload.Num(), State->OrderedGlobals.Num(), State->Dependencies.Num(),
			Budget.GetReferencesAndRelocations(), Budget.GetResidentDecodedBytes());
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
