#include "Cache/AngelscriptCacheSourceDiscovery.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDependencyObservationTests,
	"Angelscript.TestModule.Cache.DependencyObservation",
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

	static FAngelscriptCachedSourceProvider MakeProvider(
		const EAngelscriptCachedSourceProviderKind Kind,
		const FString& Identity)
	{
		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = Kind;
		Provider.CanonicalImplementationIdentity = Identity;
		Provider.IdentityFingerprint = HashString(
			TEXT("observation-provider-identity"), Identity);
		Provider.VersionFingerprint = HashString(
			TEXT("observation-provider-version"), TEXT("1"));
		Provider.ConfigurationFingerprint = HashString(
			TEXT("observation-provider-config"), Identity);
		Provider.ContentFingerprint = HashString(
			TEXT("observation-provider-content"), Identity);
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
		const EAngelscriptCachedSourceKind SourceKind,
		const FString& LogicalMount,
		const FAngelscriptCachedSourceProviderKey& ProviderKey)
	{
		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = SourceKind;
		Mount.LogicalMount = LogicalMount;
		Mount.ProviderKey = ProviderKey;
		Mount.RootConfigurationFingerprint = HashString(
			TEXT("observation-mount-config"), LogicalMount);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Mount.MountKey).IsSuccess());
		return Mount;
	}

	static FAngelscriptCacheDirectSourceInputs MakeDirectInputs(
		const bool bIncludeCommon = true,
		const bool bIncludeOption = true)
	{
		FAngelscriptCacheDirectSourceInputs Inputs;
		Inputs.Profile.Hash = HashString(
			TEXT("observation-profile"), TEXT("EditorDevelopment"));
		Inputs.DiscoveryPolicyVersion = 2;
		if (bIncludeOption)
		{
			Inputs.Options.Add({
				EAngelscriptCacheDirectOptionKind::Preprocessor,
				TEXT("WITH_EDITOR"), TEXT("1")});
		}

		Inputs.Providers.Add(MakeProvider(
			EAngelscriptCachedSourceProviderKind::BuiltInDisk,
			TEXT("Angelscript.Observation.Disk.V1")));
		Inputs.Providers.Add(MakeProvider(
			EAngelscriptCachedSourceProviderKind::Generated,
			TEXT("Angelscript.Observation.Generated.V1")));
		Inputs.Mounts.Add(MakeMount(
			EAngelscriptCachedSourceKind::Game,
			TEXT("/Angelscript/Game"),
			Inputs.Providers[0].ProviderKey));
		Inputs.Mounts.Add(MakeMount(
			EAngelscriptCachedSourceKind::Memory,
			TEXT("/Angelscript/Memory/Generated"),
			Inputs.Providers[1].ProviderKey));

		const FAngelscriptStableModuleKey MainModule{
			HashString(TEXT("observation-module"), TEXT("Main"))};
		const FAngelscriptStableModuleKey CommonModule{
			HashString(TEXT("observation-module"), TEXT("Common"))};
		const FAngelscriptStableModuleKey GeneratedModule{
			HashString(TEXT("observation-module"), TEXT("Generated"))};

		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::ProcessChunks;
		Hook.CanonicalImplementationIdentity =
			TEXT("Angelscript.Observation.Hook.V1");
		Hook.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Hook.AffectedScopeStableKey = MainModule.Hash;
		Hook.IdentityFingerprint = HashString(
			TEXT("observation-hook-identity"),
			Hook.CanonicalImplementationIdentity);
		Hook.VersionFingerprint = HashString(
			TEXT("observation-hook-version"), TEXT("1"));
		Hook.ConfigurationFingerprint = HashString(
			TEXT("observation-hook-config"), TEXT("default"));
		Hook.ContentFingerprint = HashString(
			TEXT("observation-hook-content"), TEXT("v1"));
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
			{Hook.Phase,
				Hook.CanonicalImplementationIdentity,
				Hook.AffectedScopeKind,
				Hook.AffectedScopeStableKey},
			Hook.HookKey).IsSuccess());
		Inputs.PreprocessHooks.Add(Hook);

		FAngelscriptCacheDirectSourceFileInput Main;
		Main.MountIndex = 0;
		Main.RelativeLogicalPath = TEXT("Main.as");
		Main.RawSourceBytes = SourceBytes(
			TEXT("#include \"Common.as\"\nint Main() { return VALUE; }"));
		Main.ModuleKey = MainModule;
		Inputs.Files.Add(MoveTemp(Main));

		if (bIncludeCommon)
		{
			FAngelscriptCacheDirectSourceFileInput Common;
			Common.MountIndex = 0;
			Common.RelativeLogicalPath = TEXT("Common.as");
			Common.RawSourceBytes = SourceBytes(TEXT("const int VALUE = 7;"));
			Common.ModuleKey = CommonModule;
			Inputs.Files.Add(MoveTemp(Common));
		}

		FAngelscriptCacheDirectSourceFileInput Generated;
		Generated.MountIndex = 1;
		Generated.RelativeLogicalPath = TEXT("Bindings.as");
		Generated.RawSourceBytes = SourceBytes(TEXT("int GeneratedValue = 9;"));
		Generated.GeneratedSourceKey = HashString(
			TEXT("observation-generated-source"), TEXT("Bindings"));
		Generated.GeneratedConfigurationFingerprint = HashString(
			TEXT("observation-generated-config"), TEXT("default"));
		Generated.ModuleKey = GeneratedModule;
		Inputs.Files.Add(MoveTemp(Generated));
		return Inputs;
	}

	static FAngelscriptCacheDirectSourcePlan BuildPlan(
		const FAngelscriptCacheDirectSourceInputs& Inputs)
	{
		FAngelscriptCacheDirectSourcePlan Plan;
		check(FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			Inputs, {}, Plan).IsSuccess());
		return Plan;
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
		const FAngelscriptStableModuleKey& OwnerModule,
		const EAngelscriptCachePreprocessorInputKind Kind,
		const FString& Name,
		const EAngelscriptCachePreprocessorInputTargetKind TargetKind,
		const TOptional<FAngelscriptHash256>& TargetKey)
	{
		FAngelscriptCachedPreprocessorInput Input;
		Input.OwnerScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Input.OwnerScopeStableKey = OwnerModule.Hash;
		Input.InputKind = Kind;
		Input.CanonicalName = Name;
		Input.TargetKind = TargetKind;
		Input.TargetStableKey = TargetKey;
		Input.EffectiveValueOrContentHash = HashString(
			TEXT("observation-placeholder"), Name);
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
			{Input.OwnerScopeStableKey,
				Input.InputKind,
				Input.CanonicalName,
				Input.TargetStableKey},
			Input.InputKey).IsSuccess());
		return Input;
	}

	static TArray<FAngelscriptCachedPreprocessorInput> MakeAllTargetInputs(
		const FAngelscriptCacheDirectSourcePlan& Plan)
	{
		const FAngelscriptCachedSourceFile& Main = FindFile(Plan, TEXT("Main.as"));
		const FAngelscriptCachedSourceFile& Common = FindFile(Plan, TEXT("Common.as"));
		const FAngelscriptCachedSourceFile& Generated =
			FindFile(Plan, TEXT("Bindings.as"));
		const FAngelscriptCachedSourceProvider& Provider =
			Plan.DirectProjection.Providers[0];
		const FAngelscriptCachedPreprocessHook& Hook =
			Plan.DirectProjection.PreprocessHooks[0];

		TArray<FAngelscriptCachedPreprocessorInput> Inputs;
		Inputs.Add(MakeInput(Main.ModuleKey,
			EAngelscriptCachePreprocessorInputKind::IncludeFile,
			TEXT("Common.as"),
			EAngelscriptCachePreprocessorInputTargetKind::SourceFile,
			Common.SourceFileKey.Hash));
		Inputs.Add(MakeInput(Main.ModuleKey,
			EAngelscriptCachePreprocessorInputKind::GeneratedSource,
			TEXT("Bindings"),
			EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource,
			Generated.GeneratedSourceKey));
		Inputs.Add(MakeInput(Main.ModuleKey,
			EAngelscriptCachePreprocessorInputKind::ConditionalSymbol,
			TEXT("WITH_EDITOR"),
			EAngelscriptCachePreprocessorInputTargetKind::None,
			{}));
		Inputs.Add(MakeInput(Main.ModuleKey,
			EAngelscriptCachePreprocessorInputKind::Define,
			TEXT("provider-state"),
			EAngelscriptCachePreprocessorInputTargetKind::Provider,
			Provider.ProviderKey.Hash));
		Inputs.Add(MakeInput(Main.ModuleKey,
			EAngelscriptCachePreprocessorInputKind::Define,
			TEXT("hook-state"),
			EAngelscriptCachePreprocessorInputTargetKind::Hook,
			Hook.HookKey.Hash));
		Inputs.Add(MakeInput(Main.ModuleKey,
			EAngelscriptCachePreprocessorInputKind::Define,
			TEXT("module-state"),
			EAngelscriptCachePreprocessorInputTargetKind::Module,
			Main.ModuleKey.Hash));
		return Inputs;
	}

	static FAngelscriptCachedSourceIndex BuildCandidate(
		const FAngelscriptCacheDirectSourcePlan& Plan,
		const FAngelscriptArtifactProfileKey& Profile,
		TArray<FAngelscriptCachedPreprocessorInput> Inputs)
	{
		FAngelscriptCachedSourceIndex Candidate;
		check(FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
			Plan, Profile, Inputs, {}, {}, Candidate).IsSuccess());
		return Candidate;
	}

public:
	TEST_METHOD(AllCurrentTargetKindsProduceAnExactCandidateObservation)
	{
		const FAngelscriptCacheDirectSourceInputs Inputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Plan = BuildPlan(Inputs);
		TArray<FAngelscriptCachedPreprocessorInput> Captured =
			MakeAllTargetInputs(Plan);
		FAngelscriptCachedSourceIndex Candidate =
			BuildCandidate(Plan, Inputs.Profile, Captured);

		FAngelscriptCacheDependencyObservationPlan ObservationPlan;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourceDiscovery::BuildCurrentDependencyObservations(
				Plan, Candidate, ObservationPlan).IsSuccess()));
		ASSERT_THAT(AreEqual(6, ObservationPlan.Observations.Num()));
		ASSERT_THAT(IsTrue(ObservationPlan.UnavailableInputKeys.IsEmpty()));

		for (FAngelscriptCachedPreprocessorInput& Input : Captured)
		{
			bool bFound = false;
			for (const FAngelscriptCacheObservedDependencyInput& Observation :
				ObservationPlan.Observations)
			{
				if (Observation.InputKey.Hash == Input.InputKey.Hash)
				{
					Input.EffectiveValueOrContentHash =
						Observation.EffectiveValueOrContentHash;
					bFound = true;
					break;
				}
			}
			ASSERT_THAT(IsTrue(bFound));
		}
		Candidate = BuildCandidate(Plan, Inputs.Profile, Captured);

		FAngelscriptCacheDependencyCandidateResult Match;
		const FAngelscriptCacheValidationResult Validation =
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				Plan,
				Inputs.Profile,
				Candidate,
				ObservationPlan.Observations,
				{},
				Match);
		ASSERT_THAT(IsTrue(Validation.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::Exact,
			Match.Match));
	}

	TEST_METHOD(DeletedTargetAndMissingOptionBecomeNormalUnavailableObservations)
	{
		const FAngelscriptCacheDirectSourceInputs BaselineInputs = MakeDirectInputs();
		const FAngelscriptCacheDirectSourcePlan Baseline = BuildPlan(BaselineInputs);
		const FAngelscriptCachedSourceIndex Candidate = BuildCandidate(
			Baseline,
			BaselineInputs.Profile,
			MakeAllTargetInputs(Baseline));

		const FAngelscriptCacheDirectSourceInputs CurrentInputs =
			MakeDirectInputs(false, false);
		const FAngelscriptCacheDirectSourcePlan Current = BuildPlan(CurrentInputs);
		FAngelscriptCacheDependencyObservationPlan ObservationPlan;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourceDiscovery::BuildCurrentDependencyObservations(
				Current, Candidate, ObservationPlan).IsSuccess()));
		ASSERT_THAT(AreEqual(4, ObservationPlan.Observations.Num()));
		ASSERT_THAT(AreEqual(2, ObservationPlan.UnavailableInputKeys.Num()));

		FAngelscriptCacheDependencyCandidateResult Match;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
				Current,
				CurrentInputs.Profile,
				Candidate,
				ObservationPlan.Observations,
				{},
				Match).IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyCandidateMatch::DirectInputMismatch,
			Match.Match,
			TEXT("direct inventory mismatch must still short-circuit observations")));
	}
};

#endif
