#include "Cache/AngelscriptCacheSourcePlanner.h"

#include "Algo/Reverse.h"
#include "CQTest.h"
#include "HAL/PlatformTime.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheEligibilityBatchTests_Private
{
	static FAngelscriptHash256 HashString(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static TArray<uint8> SourceBytes(const FStringView Value)
	{
		const FTCHARToUTF8 Utf8(Value.GetData(), Value.Len());
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return Bytes;
	}

	static FAngelscriptCachedSourceProvider MakeProvider()
	{
		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity = TEXT("CacheV2.Batch.Provider");
		Provider.IdentityFingerprint = HashString(
			TEXT("batch-provider-identity"), TEXT("provider"));
		Provider.VersionFingerprint = HashString(
			TEXT("batch-provider-version"), TEXT("1"));
		Provider.ConfigurationFingerprint = HashString(
			TEXT("batch-provider-config"), TEXT("game"));
		Provider.ContentFingerprint = HashString(
			TEXT("batch-provider-content"), TEXT("inventory"));
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			{Provider.ProviderKind, Provider.CanonicalImplementationIdentity,
				Provider.IdentityFingerprint}, Provider.ProviderKey).IsSuccess());
		return Provider;
	}

	static FAngelscriptCachedSourceMount MakeMount(
		const FAngelscriptCachedSourceProviderKey& ProviderKey)
	{
		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = EAngelscriptCachedSourceKind::Game;
		Mount.LogicalMount = TEXT("/Angelscript/Game");
		Mount.ProviderKey = ProviderKey;
		Mount.RootConfigurationFingerprint = HashString(
			TEXT("batch-mount-root"), TEXT("Script"));
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Mount.MountKey).IsSuccess());
		return Mount;
	}

	static FAngelscriptCacheDirectSourcePlan MakePlan(
		const int32 ModuleCount,
		TArray<FAngelscriptStableModuleKey>& OutModuleKeys)
	{
		FAngelscriptCacheDirectSourceInputs Inputs;
		Inputs.Profile.Hash = HashString(TEXT("batch-profile"), TEXT("Editor"));
		Inputs.DiscoveryPolicyVersion = 2;
		Inputs.Providers.Add(MakeProvider());
		Inputs.Mounts.Add(MakeMount(Inputs.Providers[0].ProviderKey));
		for (int32 Index = 0; Index < ModuleCount; ++Index)
		{
			const FString Name = FString::Printf(TEXT("Module%03d"), Index);
			const FAngelscriptStableModuleKey ModuleKey{
				HashString(TEXT("batch-module"), Name)};
			OutModuleKeys.Add(ModuleKey);
			FAngelscriptCacheDirectSourceFileInput& File =
				Inputs.Files.AddDefaulted_GetRef();
			File.MountIndex = 0;
			File.RelativeLogicalPath = Name + TEXT(".as");
			File.RawSourceBytes = SourceBytes(FString::Printf(
				TEXT("int Read%s() { return %d; }"), *Name, Index));
			File.ModuleKey = ModuleKey;
		}

		FAngelscriptCacheDirectSourcePlan Plan;
		check(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			Inputs, {}, Plan).IsSuccess());
		for (int32 Index = 0; Index < ModuleCount; Index += 2)
		{
			FAngelscriptCachedFastPathIneligibleScope& Scope =
				Plan.DirectProjection.IneligibleScopes.AddDefaulted_GetRef();
			Scope.ScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
			Scope.ScopeStableKey = OutModuleKeys[Index].Hash;
			Scope.Reason =
				EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior;
			Scope.CanonicalDiagnosticIdentity = FString::Printf(
				TEXT("Module%03d requires clean preprocessing"), Index);
		}
		check(FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(
			Plan.DirectProjection).IsSuccess());
		return Plan;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheEligibilityBatchTests,
	"Angelscript.TestModule.Cache.EligibilityBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BatchPreparesOnceMatchesSingleAuthorityAndFailsAtomicallyOnBudget)
	{
		using namespace AngelscriptCacheEligibilityBatchTests_Private;
		TArray<FAngelscriptStableModuleKey> ModuleKeys;
		const FAngelscriptCacheDirectSourcePlan Plan = MakePlan(3, ModuleKeys);
		Algo::Reverse(ModuleKeys);

		FAngelscriptCacheReadBudget BatchBudget;
		FAngelscriptCacheExactFastPathEligibilityBatch Batch;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::
				QueryCurrentExactFastPathEligibilityBatch(
					Plan.DirectProjection, ModuleKeys, {}, BatchBudget, Batch);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(3, Batch.Entries.Num()));
		ASSERT_THAT(AreEqual(uint32(1), Batch.SourceValidationPasses));
		ASSERT_THAT(AreEqual(uint32(1), Batch.PreparedIndexBuilds));
		ASSERT_THAT(AreEqual(uint32(3), Batch.ModuleQueries));
		for (int32 Index = 1; Index < Batch.Entries.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Batch.Entries[Index - 1].ModuleKey.Hash
				< Batch.Entries[Index].ModuleKey.Hash));
		}
		for (const FAngelscriptCacheExactFastPathEligibilityBatchEntry& Entry
			: Batch.Entries)
		{
			FAngelscriptCacheReadBudget SingleBudget;
			FAngelscriptCacheExactFastPathEligibility Single;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::
				QueryCurrentExactFastPathEligibility(
					Plan.DirectProjection, Entry.ModuleKey, {}, SingleBudget,
					Single).IsSuccess()));
			ASSERT_THAT(AreEqual(
				Single.bExactFastPathEligible,
				Entry.Eligibility.bExactFastPathEligible));
			ASSERT_THAT(AreEqual(
				Single.MatchingScopes.Num(),
				Entry.Eligibility.MatchingScopes.Num()));
		}

		const uint64 RequiredDecodedBytes = BatchBudget.GetDecodedBytes();
		ASSERT_THAT(IsTrue(RequiredDecodedBytes > 0));
		FAngelscriptCacheReadLimits ShortLimits;
		ShortLimits.MaxTotalDecodedBytes = RequiredDecodedBytes - 1;
		FAngelscriptCacheReadBudget ShortBudget;
		FAngelscriptCacheExactFastPathEligibilityBatch Short;
		Short.Entries.Add({});
		const FAngelscriptCacheValidationResult ShortResult =
			FAngelscriptCacheSemanticArchive::
				QueryCurrentExactFastPathEligibilityBatch(
					Plan.DirectProjection, ModuleKeys, ShortLimits,
					ShortBudget, Short);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			ShortResult.Error));
		ASSERT_THAT(IsTrue(Short.Entries.IsEmpty()));
		ASSERT_THAT(AreEqual(uint32(0), Short.SourceValidationPasses));
		ASSERT_THAT(AreEqual(uint64(0),
			ShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			ShortBudget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(ManyModuleBatchKeepsOnePreparationAndDeterministicOrder)
	{
		using namespace AngelscriptCacheEligibilityBatchTests_Private;
		TArray<FAngelscriptStableModuleKey> ModuleKeys;
		const FAngelscriptCacheDirectSourcePlan Plan = MakePlan(64, ModuleKeys);
		Algo::Reverse(ModuleKeys);
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptCacheExactFastPathEligibilityBatch Batch;
		const double Start = FPlatformTime::Seconds();
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::
				QueryCurrentExactFastPathEligibilityBatch(
					Plan.DirectProjection, ModuleKeys, {}, Budget, Batch);
		const double ElapsedMs =
			(FPlatformTime::Seconds() - Start) * 1000.0;
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.1 eligibility batch: Modules=%d Prepare=%u IndexBuilds=%u Queries=%u DecodedBytes=%llu PeakResident=%llu ElapsedMs=%.3f"),
			Batch.Entries.Num(), Batch.SourceValidationPasses,
			Batch.PreparedIndexBuilds, Batch.ModuleQueries,
			Budget.GetDecodedBytes(), Budget.GetPeakLiveResidentDecodedBytes(),
			ElapsedMs));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(64, Batch.Entries.Num()));
		ASSERT_THAT(AreEqual(uint32(1), Batch.SourceValidationPasses));
		ASSERT_THAT(AreEqual(uint32(1), Batch.PreparedIndexBuilds));
		ASSERT_THAT(AreEqual(uint32(64), Batch.ModuleQueries));
		for (int32 Index = 1; Index < Batch.Entries.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Batch.Entries[Index - 1].ModuleKey.Hash
				< Batch.Entries[Index].ModuleKey.Hash));
		}
	}
};

#endif
