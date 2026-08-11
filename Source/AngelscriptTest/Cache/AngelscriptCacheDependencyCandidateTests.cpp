#include "Cache/AngelscriptCacheSourcePlanner.h"

#include "Algo/Reverse.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDependencyCandidateTests,
	"Angelscript.TestModule.Cache.DependencyCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
		Provider.ProviderKind =
			EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity =
			TEXT("Angelscript.DependencyCandidateDisk.V1");
		Provider.IdentityFingerprint = HashString(
			TEXT("dependency-provider-identity"),
			Provider.CanonicalImplementationIdentity);
		Provider.VersionFingerprint = HashString(
			TEXT("dependency-provider-version"), TEXT("1"));
		Provider.ConfigurationFingerprint = HashString(
			TEXT("dependency-provider-config"), TEXT("Game/Script"));
		Provider.ContentFingerprint = HashString(
			TEXT("dependency-provider-content"), TEXT("inventory-v1"));
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			{Provider.ProviderKind,
				Provider.CanonicalImplementationIdentity,
				Provider.IdentityFingerprint},
			Provider.ProviderKey).IsSuccess());
		return Provider;
	}

	static FAngelscriptCachedSourceProvider MakeGeneratedProvider()
	{
		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::Generated;
		Provider.CanonicalImplementationIdentity =
			TEXT("Angelscript.DependencyCandidateGenerated.V1");
		Provider.IdentityFingerprint = HashString(
			TEXT("dependency-generated-provider-identity"),
			Provider.CanonicalImplementationIdentity);
		Provider.VersionFingerprint = HashString(
			TEXT("dependency-generated-provider-version"), TEXT("1"));
		Provider.ConfigurationFingerprint = HashString(
			TEXT("dependency-generated-provider-config"), TEXT("default"));
		Provider.ContentFingerprint = HashString(
			TEXT("dependency-generated-provider-content"), TEXT("bindings-v1"));
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
			TEXT("dependency-mount-root"), TEXT("Game/Script"));
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Mount.MountKey).IsSuccess());
		return Mount;
	}

	static FAngelscriptCacheDirectSourceInputs MakeDirectInputs()
	{
		FAngelscriptCacheDirectSourceInputs Inputs;
		Inputs.Profile.Hash = HashString(
			TEXT("dependency-profile"), TEXT("EditorDevelopment"));
		Inputs.DiscoveryPolicyVersion = 2;
		Inputs.Options.Add({
			EAngelscriptCacheDirectOptionKind::Preprocessor,
			TEXT("WITH_EDITOR"), TEXT("1")});
		Inputs.Providers.Add(MakeProvider());
		Inputs.Mounts.Add(MakeMount(Inputs.Providers[0].ProviderKey));
		Inputs.Providers.Add(MakeGeneratedProvider());
		FAngelscriptCachedSourceMount GeneratedMount =
			MakeMount(Inputs.Providers[1].ProviderKey);
		GeneratedMount.LogicalMount = TEXT("/Angelscript/Generated");
		GeneratedMount.RootConfigurationFingerprint = HashString(
			TEXT("dependency-generated-mount-root"), TEXT("Generated"));
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{GeneratedMount.SourceKind,
				GeneratedMount.LogicalMount,
				GeneratedMount.ProviderKey},
			GeneratedMount.MountKey).IsSuccess());
		Inputs.Mounts.Add(MoveTemp(GeneratedMount));

		const FAngelscriptStableModuleKey PlayerModule{
			HashString(TEXT("dependency-module"), TEXT("Player"))};
		const FAngelscriptStableModuleKey CommonModule{
			HashString(TEXT("dependency-module"), TEXT("Common"))};
		const FAngelscriptStableModuleKey GeneratedModule{
			HashString(TEXT("dependency-module"), TEXT("Generated"))};

		FAngelscriptCacheDirectSourceFileInput Player;
		Player.MountIndex = 0;
		Player.RelativeLogicalPath = TEXT("Game/Player.as");
		Player.RawSourceBytes = SourceBytes(
			TEXT("#include \"Common.as\"\nint GetHealth() { return 100; }"));
		Player.ModuleKey = PlayerModule;
		Inputs.Files.Add(MoveTemp(Player));

		FAngelscriptCacheDirectSourceFileInput Common;
		Common.MountIndex = 0;
		Common.RelativeLogicalPath = TEXT("Game/Common.as");
		Common.RawSourceBytes = SourceBytes(TEXT("const int MaxHealth = 100;"));
		Common.ModuleKey = CommonModule;
		Inputs.Files.Add(MoveTemp(Common));

		FAngelscriptCacheDirectSourceFileInput Generated;
		Generated.MountIndex = 1;
		Generated.RelativeLogicalPath = TEXT("Generated/Bindings.as");
		Generated.RawSourceBytes = SourceBytes(TEXT("int GeneratedValue = 7;"));
		Generated.GeneratedSourceKey = HashString(
			TEXT("dependency-generated-source"), TEXT("Bindings"));
		Generated.GeneratedConfigurationFingerprint = HashString(
			TEXT("dependency-generated-config"), TEXT("default"));
		Generated.ModuleKey = GeneratedModule;
		Inputs.Files.Add(MoveTemp(Generated));
		return Inputs;
	}

	static const FAngelscriptCachedSourceFile& FindFile(
		const FAngelscriptCacheDirectSourcePlan& Plan,
		const FStringView RelativePath)
	{
		for (const FAngelscriptCachedSourceFile& File :
			Plan.DirectProjection.Files)
		{
			if (File.RelativeLogicalPath == RelativePath)
			{
				return File;
			}
		}
		checkNoEntry();
		return Plan.DirectProjection.Files[0];
	}

	static FAngelscriptCachedPreprocessorInput MakeInput(
		const EAngelscriptCachePreprocessorInputKind Kind,
		const FString& Name,
		const EAngelscriptCachePreprocessorInputTargetKind TargetKind,
		const TOptional<FAngelscriptHash256>& TargetKey,
		const FAngelscriptHash256& EffectiveHash,
		const FAngelscriptStableModuleKey& OwnerModule)
	{
		FAngelscriptCachedPreprocessorInput Input;
		Input.OwnerScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Input.OwnerScopeStableKey = OwnerModule.Hash;
		Input.InputKind = Kind;
		Input.CanonicalName = Name;
		Input.TargetKind = TargetKind;
		Input.TargetStableKey = TargetKey;
		Input.EffectiveValueOrContentHash = EffectiveHash;
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
			{Input.OwnerScopeStableKey,
				Input.InputKind,
				Input.CanonicalName,
				Input.TargetStableKey},
			Input.InputKey).IsSuccess());
		return Input;
	}

	static FAngelscriptCachedSourceEdge MakeEdge(
		const EAngelscriptCachedSourceEdgeKind Kind,
		const FAngelscriptCachedSourceFileKey& FromKey,
		const FAngelscriptHash256& ToKey,
		const FString& Identity,
		const uint32 Ordinal)
	{
		FAngelscriptCachedSourceEdge Edge;
		Edge.EdgeKind = Kind;
		Edge.FromSourceFileKey = FromKey;
		Edge.ToSourceOrGeneratedKey = ToKey;
		Edge.CanonicalIncludeOrGeneratorIdentity = Identity;
		Edge.SemanticOrdinal = Ordinal;
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(
			{Edge.EdgeKind,
				Edge.FromSourceFileKey,
				Edge.ToSourceOrGeneratedKey,
				Edge.CanonicalIncludeOrGeneratorIdentity},
			Edge.EdgeKey).IsSuccess());
		return Edge;
	}

	static void MakeCapturedDependencies(
		const FAngelscriptCacheDirectSourcePlan& Plan,
		TArray<FAngelscriptCachedPreprocessorInput>& OutInputs,
		TArray<FAngelscriptCachedSourceEdge>& OutEdges)
	{
		const FAngelscriptCachedSourceFile& Player =
			FindFile(Plan, TEXT("Game/Player.as"));
		const FAngelscriptCachedSourceFile& Common =
			FindFile(Plan, TEXT("Game/Common.as"));
		const FAngelscriptCachedSourceFile& Generated =
			FindFile(Plan, TEXT("Generated/Bindings.as"));

		OutInputs.Add(MakeInput(
			EAngelscriptCachePreprocessorInputKind::IncludeFile,
			TEXT("Game/Common.as"),
			EAngelscriptCachePreprocessorInputTargetKind::SourceFile,
			Common.SourceFileKey.Hash,
			Common.RawContentHash,
			Player.ModuleKey));
		OutInputs.Add(MakeInput(
			EAngelscriptCachePreprocessorInputKind::ConditionalSymbol,
			TEXT("WITH_EDITOR"),
			EAngelscriptCachePreprocessorInputTargetKind::None,
			{},
			HashString(TEXT("dependency-conditional"), TEXT("1")),
			Player.ModuleKey));
		OutInputs.Add(MakeInput(
			EAngelscriptCachePreprocessorInputKind::GeneratedSource,
			TEXT("Bindings"),
			EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource,
			Generated.GeneratedSourceKey,
			Generated.RawContentHash,
			Player.ModuleKey));

		OutEdges.Add(MakeEdge(
			EAngelscriptCachedSourceEdgeKind::Include,
			Player.SourceFileKey,
			Common.SourceFileKey.Hash,
			TEXT("include:Game/Common.as"),
			0));
		OutEdges.Add(MakeEdge(
			EAngelscriptCachedSourceEdgeKind::GeneratedSource,
			Player.SourceFileKey,
			Generated.GeneratedSourceKey.GetValue(),
			TEXT("generator:Bindings"),
			0));
	}

	static FAngelscriptCacheDirectSourcePlan BuildDirectPlan(
		const FAngelscriptCacheDirectSourceInputs& Inputs)
	{
		FAngelscriptCacheDirectSourcePlan Plan;
		check(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			Inputs, {}, Plan).IsSuccess());
		return Plan;
	}

	static FAngelscriptCachedSourceIndex BuildCandidate(
		const FAngelscriptCacheDirectSourcePlan& Plan,
		const FAngelscriptArtifactProfileKey& Profile)
	{
		TArray<FAngelscriptCachedPreprocessorInput> Inputs;
		TArray<FAngelscriptCachedSourceEdge> Edges;
		MakeCapturedDependencies(Plan, Inputs, Edges);
		FAngelscriptCachedSourceIndex Candidate;
		check(FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
			Plan, Profile, Inputs, Edges, {}, Candidate).IsSuccess());
		return Candidate;
	}

	static TArray<FAngelscriptCacheObservedDependencyInput> MakeObservations(
		const FAngelscriptCachedSourceIndex& Candidate)
	{
		TArray<FAngelscriptCacheObservedDependencyInput> Observations;
		for (const FAngelscriptCachedPreprocessorInput& Input :
			Candidate.PreprocessorInputs)
		{
			Observations.Add({Input.InputKey, Input.EffectiveValueOrContentHash});
		}
		return Observations;
	}

public:
	TEST_METHOD(BuildCanonicalizesBoundedCapturedDependencies)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		TArray<FAngelscriptCachedPreprocessorInput> Inputs;
		TArray<FAngelscriptCachedSourceEdge> Edges;
		MakeCapturedDependencies(Plan, Inputs, Edges);

		FAngelscriptCachedSourceIndex Baseline;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
				Plan, DirectInputs.Profile, Inputs, Edges, {}, Baseline).IsSuccess()));

		Algo::Reverse(Inputs);
		Algo::Reverse(Edges);
		FAngelscriptCachedSourceIndex Reordered;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
				Plan, DirectInputs.Profile, Inputs, Edges, {}, Reordered).IsSuccess()));

		ASSERT_THAT(AreEqual(3, Reordered.PreprocessorInputs.Num()));
		ASSERT_THAT(AreEqual(2, Reordered.Edges.Num()));
		ASSERT_THAT(AreEqual(
			Baseline.SourceSnapshot.ToHexString(),
			Reordered.SourceSnapshot.ToHexString(),
			TEXT("captured dependency order must not change the candidate")));
	}

	TEST_METHOD(ExactObservationsRebuildCurrentCandidateAtomically)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		const FAngelscriptCachedSourceIndex Candidate =
			BuildCandidate(Plan, DirectInputs.Profile);
		TArray<FAngelscriptCacheObservedDependencyInput> Observations =
			MakeObservations(Candidate);
		Algo::Reverse(Observations);

		FAngelscriptCacheDependencyCandidateResult Result;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				Plan,
				DirectInputs.Profile,
				Candidate,
				Observations,
				{},
				Result).IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::Exact,
			Result.Match));
		ASSERT_THAT(AreEqual(3, Result.ComparedDependencyCount));
		ASSERT_THAT(AreEqual(
			Candidate.SourceSnapshot.ToHexString(),
			Result.ExactSourceIndex.SourceSnapshot.ToHexString()));
	}

	TEST_METHOD(DirectMismatchSkipsDependencyComparison)
	{
		const FAngelscriptCacheDirectSourceInputs OriginalInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan OriginalPlan =
			BuildDirectPlan(OriginalInputs);
		const FAngelscriptCachedSourceIndex Candidate =
			BuildCandidate(OriginalPlan, OriginalInputs.Profile);
		const TArray<FAngelscriptCacheObservedDependencyInput> Observations =
			MakeObservations(Candidate);

		FAngelscriptCacheDirectSourceInputs ChangedInputs = OriginalInputs;
		ChangedInputs.Files[0].RawSourceBytes.Add(static_cast<uint8>(' '));
		const FAngelscriptCacheDirectSourcePlan ChangedPlan =
			BuildDirectPlan(ChangedInputs);
		FAngelscriptCacheDependencyCandidateResult Result;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				ChangedPlan,
				ChangedInputs.Profile,
				Candidate,
				Observations,
				{},
				Result).IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::DirectInputMismatch,
			Result.Match));
		ASSERT_THAT(AreEqual(0, Result.ComparedDependencyCount));
		ASSERT_THAT(IsTrue(Result.ExactSourceIndex.SourceSnapshot.IsZero()));
	}

	TEST_METHOD(MissingObservationIsANormalUnavailableMiss)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		const FAngelscriptCachedSourceIndex Candidate =
			BuildCandidate(Plan, DirectInputs.Profile);
		TArray<FAngelscriptCacheObservedDependencyInput> Observations =
			MakeObservations(Candidate);
		const FAngelscriptCachedPreprocessorInputKey MissingKey =
			Observations.Pop(EAllowShrinking::No).InputKey;

		FAngelscriptCacheDependencyCandidateResult Result;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				Plan, DirectInputs.Profile, Candidate, Observations, {}, Result)
				.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::ObservationUnavailable,
			Result.Match));
		ASSERT_THAT(AreEqual(
			MissingKey.Hash.ToHexString(),
			Result.FirstNonMatchingInputKey.Hash.ToHexString()));
		ASSERT_THAT(IsTrue(Result.ExactSourceIndex.SourceSnapshot.IsZero()));
	}

	TEST_METHOD(ChangedObservationIsANormalDependencyMiss)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		const FAngelscriptCachedSourceIndex Candidate =
			BuildCandidate(Plan, DirectInputs.Profile);
		TArray<FAngelscriptCacheObservedDependencyInput> Observations =
			MakeObservations(Candidate);
		Observations[0].EffectiveValueOrContentHash = HashString(
			TEXT("dependency-observation"), TEXT("changed"));

		FAngelscriptCacheDependencyCandidateResult Result;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				Plan, DirectInputs.Profile, Candidate, Observations, {}, Result)
				.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::DependencyMismatch,
			Result.Match));
		ASSERT_THAT(AreEqual(
			Observations[0].InputKey.Hash.ToHexString(),
			Result.FirstNonMatchingInputKey.Hash.ToHexString()));
		ASSERT_THAT(IsTrue(Result.ExactSourceIndex.SourceSnapshot.IsZero()));
	}

	TEST_METHOD(CorruptCandidateAndDuplicateObservationsFailClosed)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		const FAngelscriptCachedSourceIndex Candidate =
			BuildCandidate(Plan, DirectInputs.Profile);
		const TArray<FAngelscriptCacheObservedDependencyInput> Observations =
			MakeObservations(Candidate);

		{
			FAngelscriptCachedSourceIndex Corrupt = Candidate;
			Corrupt.SourceSnapshot = HashString(
				TEXT("dependency-corrupt-snapshot"), TEXT("wrong"));
			FAngelscriptCacheDependencyCandidateResult Result;
			const FAngelscriptCacheValidationResult Validation =
				FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
					Plan,
					DirectInputs.Profile,
					Corrupt,
					Observations,
					{},
					Result);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::DerivedHashMismatch,
				Validation.Error));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheDependencyCandidateMatch::Invalid,
				Result.Match));
			ASSERT_THAT(IsTrue(Result.ExactSourceIndex.SourceSnapshot.IsZero()));
		}

		{
			TArray<FAngelscriptCacheObservedDependencyInput> Duplicate =
				Observations;
			Duplicate.Add(Observations[0]);
			FAngelscriptCacheDependencyCandidateResult Result;
			const FAngelscriptCacheValidationResult Validation =
				FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
					Plan,
					DirectInputs.Profile,
					Candidate,
					Duplicate,
					{},
					Result);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::DuplicateKey,
				Validation.Error));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheDependencyCandidateMatch::Invalid,
				Result.Match));
		}
	}

	TEST_METHOD(CountBudgetsAndMissingEdgeTargetsFailBeforePublication)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		TArray<FAngelscriptCachedPreprocessorInput> Inputs;
		TArray<FAngelscriptCachedSourceEdge> Edges;
		MakeCapturedDependencies(Plan, Inputs, Edges);

		{
			FAngelscriptCacheDependencyCandidateLimits Limits;
			Limits.MaxPreprocessorInputs = 2;
			FAngelscriptCachedSourceIndex Candidate;
			const FAngelscriptCacheValidationResult Validation =
				FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
					Plan,
					DirectInputs.Profile,
					Inputs,
					Edges,
					Limits,
					Candidate);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::BudgetExceeded,
				Validation.Error));
			ASSERT_THAT(IsTrue(Candidate.SourceSnapshot.IsZero()));
		}

		{
			TArray<FAngelscriptCachedSourceEdge> InvalidEdges = Edges;
			InvalidEdges[0].ToSourceOrGeneratedKey = HashString(
				TEXT("dependency-missing-target"), TEXT("missing"));
			ASSERT_THAT(IsTrue(
				FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(
					{InvalidEdges[0].EdgeKind,
						InvalidEdges[0].FromSourceFileKey,
						InvalidEdges[0].ToSourceOrGeneratedKey,
						InvalidEdges[0].CanonicalIncludeOrGeneratorIdentity},
					InvalidEdges[0].EdgeKey).IsSuccess()));
			FAngelscriptCachedSourceIndex Candidate;
			const FAngelscriptCacheValidationResult Validation =
				FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
					Plan,
					DirectInputs.Profile,
					Inputs,
					InvalidEdges,
					{},
					Candidate);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::MissingGraphTarget,
				Validation.Error));
			ASSERT_THAT(IsTrue(Candidate.SourceSnapshot.IsZero()));
		}
	}

	TEST_METHOD(EmptyDependencyCandidateNeedsNoObservations)
	{
		const FAngelscriptCacheDirectSourceInputs DirectInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildDirectPlan(DirectInputs);
		FAngelscriptCachedSourceIndex Candidate;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
				Plan,
				DirectInputs.Profile,
				{},
				{},
				{},
				Candidate).IsSuccess()));

		FAngelscriptCacheDependencyCandidateResult Result;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				Plan,
				DirectInputs.Profile,
				Candidate,
				{},
				{},
				Result).IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::Exact,
			Result.Match));
		ASSERT_THAT(AreEqual(0, Result.ComparedDependencyCount));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
