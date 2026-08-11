#include "Cache/AngelscriptCacheSourcePlanner.h"

#include "Algo/Reverse.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheDirectSourcePlannerTests_Private
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
		Bytes.Append(
			reinterpret_cast<const uint8*>(Utf8.Get()),
			Utf8.Length());
		return Bytes;
	}

	static FAngelscriptCachedSourceProvider MakeProvider()
	{
		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity =
			TEXT("Angelscript.BuiltInDiskSource.V2");
		Provider.IdentityFingerprint = HashString(
			TEXT("direct-provider-identity"),
			Provider.CanonicalImplementationIdentity);
		Provider.VersionFingerprint = HashString(
			TEXT("direct-provider-version"), TEXT("2"));
		Provider.ConfigurationFingerprint = HashString(
			TEXT("direct-provider-config"), TEXT("/Angelscript/Game"));
		Provider.ContentFingerprint = HashString(
			TEXT("direct-provider-content"), TEXT("file-inventory-v1"));
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			{Provider.ProviderKind,
				Provider.CanonicalImplementationIdentity,
				Provider.IdentityFingerprint},
			Provider.ProviderKey).IsSuccess());
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
			TEXT("direct-mount-root"), TEXT("Game/Script"));
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Mount.MountKey).IsSuccess());
		return Mount;
	}

	static FAngelscriptCachedPreprocessHook MakeHook(
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::ProcessChunks;
		Hook.CanonicalImplementationIdentity =
			TEXT("Angelscript.DefaultPreprocessHook.V2");
		Hook.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Hook.AffectedScopeStableKey = ModuleKey.Hash;
		Hook.IdentityFingerprint = HashString(
			TEXT("direct-hook-identity"),
			Hook.CanonicalImplementationIdentity);
		Hook.VersionFingerprint = HashString(
			TEXT("direct-hook-version"), TEXT("2"));
		Hook.ConfigurationFingerprint = HashString(
			TEXT("direct-hook-config"), TEXT("default"));
		Hook.ContentFingerprint = HashString(
			TEXT("direct-hook-content"), TEXT("process-chunks-v2"));
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
			{Hook.Phase,
				Hook.CanonicalImplementationIdentity,
				Hook.AffectedScopeKind,
				Hook.AffectedScopeStableKey},
			Hook.HookKey).IsSuccess());
		return Hook;
	}

	static FAngelscriptCacheDirectSourceFileInput MakeFile(
		const FString& RelativePath,
		const FStringView Source,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCacheDirectSourceFileInput File;
		File.MountIndex = 0;
		File.RelativeLogicalPath = RelativePath;
		File.RawSourceBytes = SourceBytes(Source);
		File.ModuleKey = ModuleKey;
		return File;
	}

	static FAngelscriptCacheDirectSourceInputs MakeInputs()
	{
		FAngelscriptCacheDirectSourceInputs Inputs;
		Inputs.Profile.Hash = HashString(
			TEXT("direct-profile"), TEXT("EditorDevelopment"));
		Inputs.DiscoveryPolicyVersion = 2;
		Inputs.DiscoveryFilterFlags = static_cast<uint32>(
			EAngelscriptCachedSourceDiscoveryFilterFlags::SkipDevelopment);

		Inputs.Options.Add({
			EAngelscriptCacheDirectOptionKind::Compiler,
			TEXT("optimization"), TEXT("debug")});
		Inputs.Options.Add({
			EAngelscriptCacheDirectOptionKind::Preprocessor,
			TEXT("WITH_EDITOR"), TEXT("1")});

		Inputs.Providers.Add(MakeProvider());
		Inputs.Mounts.Add(MakeMount(Inputs.Providers[0].ProviderKey));

		const FAngelscriptStableModuleKey PlayerModule{
			HashString(TEXT("direct-module"), TEXT("Player"))};
		const FAngelscriptStableModuleKey InventoryModule{
			HashString(TEXT("direct-module"), TEXT("Inventory"))};
		Inputs.PreprocessHooks.Add(MakeHook(PlayerModule));
		Inputs.Files.Add(MakeFile(
			TEXT("Game/Player.as"),
			TEXT("int GetHealth() { return 100; }"),
			PlayerModule));
		Inputs.Files.Add(MakeFile(
			TEXT("Game/Inventory.as"),
			TEXT("int GetSlots() { return 8; }"),
			InventoryModule));
		return Inputs;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDirectSourcePlannerTests,
	"Angelscript.TestModule.Cache.DirectSourcePlanner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CanonicalDigestIgnoresCollectionOrderAndPathSpelling)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		const FAngelscriptCacheDirectSourceInputs BaselineInputs = MakeInputs();
		FAngelscriptCacheDirectSourcePlan Baseline;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				BaselineInputs, {}, Baseline).IsSuccess(),
			TEXT("baseline direct-source inputs should be accepted")));

		FAngelscriptCacheDirectSourceInputs ReorderedInputs = BaselineInputs;
		Algo::Reverse(ReorderedInputs.Options);
		Algo::Reverse(ReorderedInputs.Files);
		for (FAngelscriptCacheDirectSourceFileInput& File : ReorderedInputs.Files)
		{
			if (File.RelativeLogicalPath == TEXT("Game/Player.as"))
			{
				File.RelativeLogicalPath = TEXT("Game\\./Characters/../Player.as");
			}
		}
		FAngelscriptCacheDirectSourcePlan Reordered;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				ReorderedInputs, {}, Reordered).IsSuccess(),
			TEXT("equivalent reordered direct-source inputs should be accepted")));

		ASSERT_THAT(AreEqual(
			Baseline.DirectInputDigest.ToHexString(),
			Reordered.DirectInputDigest.ToHexString(),
			TEXT("canonical direct-input digest must ignore input order and equivalent path spelling")));
		ASSERT_THAT(AreEqual(2, Reordered.DirectProjection.Files.Num()));
		ASSERT_THAT(IsTrue(Reordered.DirectProjection.PreprocessorInputs.IsEmpty()));
		ASSERT_THAT(IsTrue(Reordered.DirectProjection.Edges.IsEmpty()));
		ASSERT_THAT(IsTrue(Reordered.DirectProjection.SourceSnapshot.IsZero() == false));

		bool bFoundCanonicalPlayerPath = false;
		for (const FAngelscriptCachedSourceFile& File : Reordered.DirectProjection.Files)
		{
			bFoundCanonicalPlayerPath |= File.RelativeLogicalPath == TEXT("Game/Player.as");
		}
		ASSERT_THAT(IsTrue(
			bFoundCanonicalPlayerPath,
			TEXT("the direct projection should expose only normalized logical paths")));
	}

	TEST_METHOD(EveryDirectAuthorityChangesTheDigest)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		const FAngelscriptCacheDirectSourceInputs BaselineInputs = MakeInputs();
		FAngelscriptCacheDirectSourcePlan Baseline;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			BaselineInputs, {}, Baseline).IsSuccess()));

		TArray<FAngelscriptHash256> ChangedDigests;
		{
			FAngelscriptCacheDirectSourceInputs Changed = BaselineInputs;
			Changed.Files[0].RawSourceBytes.Add(static_cast<uint8>(' '));
			FAngelscriptCacheDirectSourcePlan Plan;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Changed, {}, Plan).IsSuccess()));
			ChangedDigests.Add(Plan.DirectInputDigest);
		}
		{
			FAngelscriptCacheDirectSourceInputs Changed = BaselineInputs;
			Changed.Profile.Hash = HashString(
				TEXT("direct-profile"), TEXT("Shipping"));
			FAngelscriptCacheDirectSourcePlan Plan;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Changed, {}, Plan).IsSuccess()));
			ChangedDigests.Add(Plan.DirectInputDigest);
		}
		{
			FAngelscriptCacheDirectSourceInputs Changed = BaselineInputs;
			Changed.Options[0].CanonicalValue = TEXT("release");
			FAngelscriptCacheDirectSourcePlan Plan;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Changed, {}, Plan).IsSuccess()));
			ChangedDigests.Add(Plan.DirectInputDigest);
		}
		{
			FAngelscriptCacheDirectSourceInputs Changed = BaselineInputs;
			Changed.Providers[0].VersionFingerprint = HashString(
				TEXT("direct-provider-version"), TEXT("3"));
			FAngelscriptCacheDirectSourcePlan Plan;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Changed, {}, Plan).IsSuccess()));
			ChangedDigests.Add(Plan.DirectInputDigest);
		}
		{
			FAngelscriptCacheDirectSourceInputs Changed = BaselineInputs;
			Changed.PreprocessHooks[0].VersionFingerprint = HashString(
				TEXT("direct-hook-version"), TEXT("3"));
			FAngelscriptCacheDirectSourcePlan Plan;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Changed, {}, Plan).IsSuccess()));
			ChangedDigests.Add(Plan.DirectInputDigest);
		}
		{
			FAngelscriptCacheDirectSourceInputs Changed = BaselineInputs;
			Changed.Files[0].ModuleKey.Hash = HashString(
				TEXT("direct-module"), TEXT("RenamedPlayer"));
			Changed.PreprocessHooks[0] = MakeHook(Changed.Files[0].ModuleKey);
			FAngelscriptCacheDirectSourcePlan Plan;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Changed, {}, Plan).IsSuccess()));
			ChangedDigests.Add(Plan.DirectInputDigest);
		}

		ASSERT_THAT(AreEqual(6, ChangedDigests.Num()));
		for (const FAngelscriptHash256& ChangedDigest : ChangedDigests)
		{
			ASSERT_THAT(AreNotEqual(
				Baseline.DirectInputDigest.ToHexString(),
				ChangedDigest.ToHexString(),
				TEXT("each direct authority must independently invalidate the exact candidate")));
		}
	}

	TEST_METHOD(PreprocessDerivedDependenciesDoNotChangeTheDirectDigest)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		const FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
		FAngelscriptCacheDirectSourcePlan Plan;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			Inputs, {}, Plan).IsSuccess()));

		FAngelscriptCachedSourceIndex PersistedCandidate = Plan.DirectProjection;
		FAngelscriptCachedPreprocessorInput Dependency;
		Dependency.OwnerScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Dependency.OwnerScopeStableKey = Inputs.Files[0].ModuleKey.Hash;
		Dependency.InputKind = EAngelscriptCachePreprocessorInputKind::ConditionalSymbol;
		Dependency.CanonicalName = TEXT("WITH_EDITOR");
		Dependency.TargetKind = EAngelscriptCachePreprocessorInputTargetKind::None;
		Dependency.EffectiveValueOrContentHash = HashString(
			TEXT("direct-preprocess-value"), TEXT("1"));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
				{Dependency.OwnerScopeStableKey,
					Dependency.InputKind,
					Dependency.CanonicalName,
					Dependency.TargetStableKey},
				Dependency.InputKey).IsSuccess()));
		PersistedCandidate.PreprocessorInputs.Add(Dependency);
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			PersistedCandidate, PersistedCandidate.SourceSnapshot).IsSuccess()));

		FAngelscriptHash256 PersistedDirectDigest;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ComputePersistedDirectInputDigest(
				PersistedCandidate,
				Inputs.Profile,
				PersistedDirectDigest).IsSuccess()));
		ASSERT_THAT(AreEqual(
			Plan.DirectInputDigest.ToHexString(),
			PersistedDirectDigest.ToHexString(),
			TEXT("preprocess-derived candidate rows must be validated only after direct lookup")));

		PersistedCandidate.PreprocessorInputs[0].EffectiveValueOrContentHash = HashString(
			TEXT("direct-preprocess-value"), TEXT("0"));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			PersistedCandidate, PersistedCandidate.SourceSnapshot).IsSuccess()));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ComputePersistedDirectInputDigest(
				PersistedCandidate,
				Inputs.Profile,
				PersistedDirectDigest).IsSuccess()));
		ASSERT_THAT(AreEqual(
			Plan.DirectInputDigest.ToHexString(),
			PersistedDirectDigest.ToHexString(),
			TEXT("dependency-value drift is a second-stage miss, not a direct-input lookup miss")));
	}

	TEST_METHOD(MissingFingerprintCapabilitiesMarkOnlyTheirScopesIneligible)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
		Inputs.Providers[0].VersionFingerprint.Reset();
		Inputs.Providers[0].CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		Inputs.PreprocessHooks[0].ContentFingerprint.Reset();
		Inputs.PreprocessHooks[0].CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint);

		FAngelscriptCacheDirectSourcePlan Plan;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			Inputs, {}, Plan).IsSuccess()));
		ASSERT_THAT(AreEqual(2, Plan.DirectProjection.IneligibleScopes.Num()));

		bool bProviderReason = false;
		bool bHookReason = false;
		for (const FAngelscriptCachedFastPathIneligibleScope& Scope :
			Plan.DirectProjection.IneligibleScopes)
		{
			bProviderReason |= Scope.ScopeKind
					== EAngelscriptCachedFastPathScopeKind::Provider
				&& Scope.Reason
					== EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint;
			bHookReason |= Scope.ScopeKind
					== EAngelscriptCachedFastPathScopeKind::Hook
				&& Scope.Reason
					== EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior;
		}
		ASSERT_THAT(IsTrue(bProviderReason));
		ASSERT_THAT(IsTrue(bHookReason));
	}

	TEST_METHOD(CollisionsInvalidReferencesAndRawByteLimitsFailClosed)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		{
			FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
			Inputs.Files[0].RelativeLogicalPath = TEXT("Game/Hero.as");
			Inputs.Files[1].RelativeLogicalPath = TEXT("game/hero.as");
			FAngelscriptCacheDirectSourcePlan Plan;
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
					Inputs, {}, Plan);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::CaseCollision,
				Result.Error));
		}
		{
			FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
			Inputs.Files[0].MountIndex = 7;
			FAngelscriptCacheDirectSourcePlan Plan;
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
					Inputs, {}, Plan);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::MissingGraphTarget,
				Result.Error));
		}
		{
			const FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
			FAngelscriptCacheDirectSourceLimits Limits;
			Limits.MaxTotalRawSourceBytes = 1;
			FAngelscriptCacheDirectSourcePlan Plan;
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
					Inputs, Limits, Plan);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::BudgetExceeded,
				Result.Error));
		}
	}

	TEST_METHOD(EmptyOptionKeyIsRejectedBeforeNamespacing)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
		Inputs.Options[0].CanonicalKey.Reset();
		FAngelscriptCacheDirectSourcePlan Plan;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Inputs, {}, Plan);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::InvalidPresence,
			Result.Error,
			TEXT("an option-kind prefix must not hide an empty source key")));
	}

	TEST_METHOD(EmbeddedNulInOptionValueFailsClosed)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
		Inputs.Options[0].CanonicalValue = TEXT("debug");
		Inputs.Options[0].CanonicalValue.GetCharArray().Insert(TEXT('\0'), 2);
		ASSERT_THAT(AreEqual(6, Inputs.Options[0].CanonicalValue.Len()));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(TEXT('\0')),
			static_cast<int32>(Inputs.Options[0].CanonicalValue[2])));
		FAngelscriptCacheDirectSourcePlan Plan;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Inputs, {}, Plan);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::EmbeddedNul,
			Result.Error,
			TEXT("a true embedded NUL must still be rejected")));
	}

	TEST_METHOD(AllDirectAuthorityStringsShareTheInputBudget)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		const FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
		FAngelscriptCacheDirectSourceLimits Limits;
		Limits.MaxCanonicalStringCharacters = 30;
		FAngelscriptCacheDirectSourcePlan Plan;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Inputs, Limits, Plan);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error,
			TEXT("provider, mount and hook strings share the direct-input budget")));
	}

	TEST_METHOD(MountOptionsShareTheAggregateOptionBudget)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		FAngelscriptCacheDirectSourceInputs Inputs = MakeInputs();
		Inputs.Mounts[0].Options.Add({
			TEXT("follow-symlinks"),
			HashString(TEXT("direct-mount-option"), TEXT("false"))});
		FAngelscriptCacheDirectSourceLimits Limits;
		Limits.MaxOptions = 2;
		FAngelscriptCacheDirectSourcePlan Plan;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
				Inputs, Limits, Plan);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error,
			TEXT("mount options and direct compile/preprocess options share one bound")));
	}

	TEST_METHOD(CanonicalDigestHasFrozenV1Identity)
	{
		using namespace AngelscriptCacheDirectSourcePlannerTests_Private;
		FAngelscriptCacheDirectSourcePlan Plan;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			MakeInputs(), {}, Plan).IsSuccess()));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"a0344dacb06ef4c92ec9a8a413d1bbcca7a4814d80ad2d1f1b2bac4157202197")),
			Plan.DirectInputDigest.ToHexString(),
			*FString::Printf(
				TEXT("review and freeze the V1 direct-input identity; actual=%s"),
				*Plan.DirectInputDigest.ToHexString())));
	}
};

#endif
