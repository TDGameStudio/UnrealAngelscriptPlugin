#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheSourceInterfaceFactoryTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Fill)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Fill, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptStableModuleKey MakeModuleKey()
	{
		const TOptional<FAngelscriptStableModuleKey> Key =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("Actors/Hero.as"), TEXT("Hero"));
		check(Key.IsSet());
		return Key.GetValue();
	}

	static FAngelscriptCachedSourceIndex MakeMinimalSourceIndex()
	{
		FAngelscriptCachedSourceIndex Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Value.DiscoveryPolicy.PolicyVersion = 1;
		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Value, Value.SourceSnapshot).IsSuccess());
		return Value;
	}

	static FAngelscriptCachedSourceIndex MakeMinimalEligibleSourceIndex()
	{
		FAngelscriptCachedSourceIndex Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Value.DiscoveryPolicy.PolicyVersion = 1;

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity = TEXT("Runtime.DiskSourceProvider");
		Provider.IdentityFingerprint = MakeHash(0x10);
		Provider.VersionFingerprint = MakeHash(0x11);
		Provider.ConfigurationFingerprint = MakeHash(0x12);
		Provider.ContentFingerprint = MakeHash(0x13);
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			{Provider.ProviderKind, Provider.CanonicalImplementationIdentity,
				Provider.IdentityFingerprint}, Provider.ProviderKey).IsSuccess());
		Value.Providers.Add(Provider);

		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = EAngelscriptCachedSourceKind::Game;
		Mount.LogicalMount = TEXT("Game");
		Mount.ProviderKey = Provider.ProviderKey;
		Mount.RootConfigurationFingerprint = MakeHash(0x20);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Mount.MountKey).IsSuccess());
		Value.Mounts.Add(Mount);

		FAngelscriptCachedSourceFile File;
		File.SourceKind = EAngelscriptCachedSourceKind::Game;
		File.MountKey = Mount.MountKey;
		File.ProviderKey = Provider.ProviderKey;
		File.RelativeLogicalPath = TEXT("Actors/Hero.as");
		File.RawContentHash = MakeHash(0x30);
		File.ModuleKey = MakeModuleKey();
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
			{File.SourceKind, File.MountKey, File.ProviderKey,
				File.RelativeLogicalPath, File.GeneratedSourceKey},
			File.SourceFileKey).IsSuccess());
		Value.Files.Add(File);

		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Value, Value.SourceSnapshot).IsSuccess());
		return Value;
	}

	static FAngelscriptCachedModuleInterface MakeMinimalModuleInterface()
	{
		FAngelscriptCachedModuleInterface Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		Value.ModuleKey = MakeModuleKey();
		Value.CanonicalModuleName = TEXT("Hero");
		check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Value, Value.InterfaceAbi).IsSuccess());
		return Value;
	}

	struct FDecodeOutcome final
	{
		FAngelscriptCacheValidationResult Result;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
	};

	template <typename ValueType, typename SerializeFunctionType>
	static FDecodeOutcome Decode(
		const EAngelscriptCacheRecordKind Kind,
		const ValueType& Value,
		SerializeFunctionType&& Serialize,
		FAngelscriptCacheReadBudget& Budget)
	{
		TArray<uint8> Payload;
		check(Serialize(Value, Payload).IsSuccess());
		FAngelscriptCacheRecordId RecordId;
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, Payload, RecordId).IsSuccess());
		FDecodeOutcome Outcome;
		Outcome.Result = FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId,
			Payload,
			FAngelscriptCacheReadLimits{},
			Budget,
			Outcome.Output);
		return Outcome;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheSourceInterfaceFactoryTests,
	"Angelscript.TestModule.Cache.Archive.SourceInterfaceFactory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MinimalSourceIndexAndModuleInterfaceUseTheSoleImmutableFactory)
	{
		using namespace AngelscriptCacheSourceInterfaceFactoryTests_Private;

		FAngelscriptCacheReadBudget SourceBudget;
		const FDecodeOutcome SourceOutcome = Decode(
			EAngelscriptCacheRecordKind::SourceIndex,
			MakeMinimalSourceIndex(),
			&FAngelscriptCacheSemanticArchive::SerializeSourceIndex,
			SourceBudget);
		ASSERT_THAT(IsTrue(SourceOutcome.Result.IsSuccess()));
		ASSERT_THAT(IsTrue(SourceOutcome.Output.IsSet()));
		const FAngelscriptDecodedCacheRecordHandle& Source =
			SourceOutcome.Output.GetValue();
		ASSERT_THAT(IsNotNull(Source->TryGetSourceIndex()));
		ASSERT_THAT(IsNull(Source->TryGetModuleInterface()));
		ASSERT_THAT(AreEqual(uint64(0), Source->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::PayloadSchemaVersion}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(4), Source->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::SourceSnapshot}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(36), Source->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::DiscoveryPolicyVersion}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(48), Source->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::Mounts}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(0),
			SourceBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(SourceBudget.GetResidentDecodedBytes() > 0));

		FAngelscriptCacheReadBudget InterfaceBudget;
		const FDecodeOutcome InterfaceOutcome = Decode(
			EAngelscriptCacheRecordKind::ModuleInterface,
			MakeMinimalModuleInterface(),
			&FAngelscriptCacheSemanticArchive::SerializeModuleInterface,
			InterfaceBudget);
		ASSERT_THAT(IsTrue(InterfaceOutcome.Result.IsSuccess()));
		ASSERT_THAT(IsTrue(InterfaceOutcome.Output.IsSet()));
		const FAngelscriptDecodedCacheRecordHandle& Interface =
			InterfaceOutcome.Output.GetValue();
		ASSERT_THAT(IsNotNull(Interface->TryGetModuleInterface()));
		ASSERT_THAT(IsNull(Interface->TryGetSourceIndex()));
		ASSERT_THAT(AreEqual(uint64(0), Interface->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::PayloadSchemaVersion}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(4), Interface->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::ModuleKey}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(36), Interface->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::CanonicalModuleName}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(44), Interface->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::InterfaceAbi}).GetValue()));
		ASSERT_THAT(AreEqual(uint64(76), Interface->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::CanonicalNamespaces}).GetValue()));
		ASSERT_THAT(IsFalse(Interface->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::CanonicalNamespace, 0}).IsSet()));
		ASSERT_THAT(AreEqual(uint64(0),
			InterfaceBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(InterfaceBudget.GetResidentDecodedBytes() > 0));
	}

	TEST_METHOD(ExactFastPathQueryRejectsTheActualWrongFactoryKindWithoutBudgetMutation)
	{
		using namespace AngelscriptCacheSourceInterfaceFactoryTests_Private;

		FAngelscriptCacheReadBudget DecodeBudget;
		const FDecodeOutcome InterfaceOutcome = Decode(
			EAngelscriptCacheRecordKind::ModuleInterface,
			MakeMinimalModuleInterface(),
			&FAngelscriptCacheSemanticArchive::SerializeModuleInterface,
			DecodeBudget);
		ASSERT_THAT(IsTrue(InterfaceOutcome.Result.IsSuccess()));
		ASSERT_THAT(IsTrue(InterfaceOutcome.Output.IsSet()));

		FAngelscriptCacheReadBudget QueryBudget;
		FAngelscriptCacheExactFastPathEligibility Eligibility;
		Eligibility.bExactFastPathEligible = true;
		Eligibility.MatchingScopes.SetNum(1);
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::QueryExactFastPathEligibility(
				*InterfaceOutcome.Output.GetValue(),
				FAngelscriptStableModuleKey{},
				FAngelscriptCacheReadLimits{},
				QueryBudget,
				Eligibility);

		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongRecordKind,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::GraphOrOwnership,
			Result.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ModuleGraph,
			Result.Stage));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface,
			Result.RecordKind));
		ASSERT_THAT(AreEqual(uint64(0), Result.ByteOffset));
		ASSERT_THAT(IsFalse(Eligibility.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(0, Eligibility.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(uint64(0), QueryBudget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(uint64(0), QueryBudget.GetDecompressedBytes()));
		ASSERT_THAT(AreEqual(uint64(0), QueryBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0), QueryBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			QueryBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			QueryBudget.GetReferencesAndRelocations()));
	}

	TEST_METHOD(ExactFastPathQueryUsesTheCommonSourceTokenAfterTheFactoryHandleIsCopied)
	{
		using namespace AngelscriptCacheSourceInterfaceFactoryTests_Private;

		const FAngelscriptCachedSourceIndex SourceValue =
			MakeMinimalEligibleSourceIndex();
		FAngelscriptCacheReadBudget DecodeBudget;
		FDecodeOutcome SourceOutcome = Decode(
			EAngelscriptCacheRecordKind::SourceIndex,
			SourceValue,
			&FAngelscriptCacheSemanticArchive::SerializeSourceIndex,
			DecodeBudget);
		ASSERT_THAT(IsTrue(SourceOutcome.Result.IsSuccess()));
		ASSERT_THAT(IsTrue(SourceOutcome.Output.IsSet()));
		const FAngelscriptDecodedCacheRecordHandle CopiedHandle =
			SourceOutcome.Output.GetValue();
		SourceOutcome.Output.Reset();

		FAngelscriptCacheReadBudget QueryBudget;
		FAngelscriptCacheExactFastPathEligibility Eligibility;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::QueryExactFastPathEligibility(
				*CopiedHandle,
				SourceValue.Files[0].ModuleKey,
				FAngelscriptCacheReadLimits{},
				QueryBudget,
				Eligibility);

		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(IsTrue(Eligibility.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(0, Eligibility.MatchingScopes.Num()));
		ASSERT_THAT(IsTrue(QueryBudget.GetDecodedBytes() > 0));
		ASSERT_THAT(IsTrue(QueryBudget.GetPeakLiveResidentDecodedBytes() > 0));
		ASSERT_THAT(AreEqual(uint64(0), QueryBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			QueryBudget.GetTemporaryResidentDecodedBytes()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
