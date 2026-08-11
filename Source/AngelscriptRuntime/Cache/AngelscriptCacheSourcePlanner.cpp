#include "Cache/AngelscriptCacheSourcePlanner.h"

#include "Hash/Blake3.h"

namespace AngelscriptCacheSourcePlanner_Private
{
	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error)
	{
		return FAngelscriptCacheValidationResult(
			Error, EAngelscriptCacheRecordKind::SourceIndex);
	}

	static bool Exceeds(const int32 Count, const uint64 Limit)
	{
		return static_cast<uint64>(Count) > Limit;
	}

	static bool ExceedsString(
		const FString& Value,
		const FAngelscriptCacheDirectSourceLimits& Limits)
	{
		return static_cast<uint64>(Value.Len())
			> Limits.MaxCanonicalStringCharacters;
	}

	static bool ContainsEmbeddedNul(const FString& Value)
	{
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			if (Value[Index] == TEXT('\0'))
			{
				return true;
			}
		}
		return false;
	}

	static FAngelscriptHash256 HashDirectOptionValue(
		const EAngelscriptCacheDirectOptionKind Kind,
		const FString& CanonicalValue)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-direct-option-value-v1"));
		Writer.WriteUInt8(static_cast<uint8>(Kind));
		Writer.WriteString(CanonicalValue);
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 HashRawSource(
		const TConstArrayView<uint8> Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static void AddIneligibleScope(
		FAngelscriptCachedSourceIndex& Projection,
		const EAngelscriptCachedFastPathScopeKind ScopeKind,
		const FAngelscriptHash256& ScopeKey,
		const EAngelscriptCachedFastPathIneligibleReason Reason,
		const FString& DiagnosticIdentity)
	{
		FAngelscriptCachedFastPathIneligibleScope& Scope =
			Projection.IneligibleScopes.AddDefaulted_GetRef();
		Scope.ScopeKind = ScopeKind;
		Scope.ScopeStableKey = ScopeKey;
		Scope.Reason = Reason;
		Scope.CanonicalDiagnosticIdentity = DiagnosticIdentity;
	}

	static void AddMissingProviderCapabilities(
		const FAngelscriptCachedSourceProvider& Provider,
		FAngelscriptCachedSourceIndex& Projection)
	{
		struct FMissingCapability
		{
			EAngelscriptCachedFingerprintCapabilityFlags Flag;
			EAngelscriptCachedFastPathIneligibleReason Reason;
			const TCHAR* Name;
		};
		const FMissingCapability Capabilities[] = {
			{EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity,
				EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity,
				TEXT("stable-identity")},
			{EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint,
				TEXT("version")},
			{EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint,
				TEXT("configuration")},
			{EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource,
				TEXT("content")},
		};
		for (const FMissingCapability& Capability : Capabilities)
		{
			if ((Provider.CapabilityFlags
				& static_cast<uint32>(Capability.Flag)) == 0)
			{
				AddIneligibleScope(
					Projection,
					EAngelscriptCachedFastPathScopeKind::Provider,
					Provider.ProviderKey.Hash,
					Capability.Reason,
					FString::Printf(
						TEXT("provider:%s:%s"),
						*Provider.CanonicalImplementationIdentity,
						Capability.Name));
			}
		}
	}

	static void AddMissingHookCapabilities(
		const FAngelscriptCachedPreprocessHook& Hook,
		FAngelscriptCachedSourceIndex& Projection)
	{
		struct FMissingCapability
		{
			EAngelscriptCachedFingerprintCapabilityFlags Flag;
			EAngelscriptCachedFastPathIneligibleReason Reason;
			const TCHAR* Name;
		};
		const FMissingCapability Capabilities[] = {
			{EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity,
				EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity,
				TEXT("stable-identity")},
			{EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint,
				TEXT("version")},
			{EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint,
				TEXT("configuration")},
			{EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
				TEXT("content")},
		};
		for (const FMissingCapability& Capability : Capabilities)
		{
			if ((Hook.CapabilityFlags
				& static_cast<uint32>(Capability.Flag)) == 0)
			{
				AddIneligibleScope(
					Projection,
					EAngelscriptCachedFastPathScopeKind::Hook,
					Hook.HookKey.Hash,
					Capability.Reason,
					FString::Printf(
						TEXT("hook:%s:%s"),
						*Hook.CanonicalImplementationIdentity,
						Capability.Name));
			}
		}
	}

	static FAngelscriptCacheValidationResult ValidateInputBounds(
		const FAngelscriptCacheDirectSourceInputs& Inputs,
		const FAngelscriptCacheDirectSourceLimits& Limits)
	{
		if (Exceeds(Inputs.Providers.Num(), Limits.MaxProviders)
			|| Exceeds(Inputs.Mounts.Num(), Limits.MaxMounts)
			|| Exceeds(
				Inputs.PreprocessHooks.Num(), Limits.MaxPreprocessHooks)
			|| Exceeds(Inputs.Files.Num(), Limits.MaxFiles))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
		}
		uint64 TotalOptions = static_cast<uint64>(Inputs.Options.Num());
		if (TotalOptions > Limits.MaxOptions)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
		}
		for (const FAngelscriptCachedSourceMount& Mount : Inputs.Mounts)
		{
			const uint64 MountOptions = static_cast<uint64>(Mount.Options.Num());
			if (TotalOptions > Limits.MaxOptions
				|| MountOptions > Limits.MaxOptions - TotalOptions)
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
			TotalOptions += MountOptions;
		}

		uint64 TotalRawSourceBytes = 0;
		for (const FAngelscriptCacheDirectSourceFileInput& File : Inputs.Files)
		{
			const uint64 RawBytes = static_cast<uint64>(File.RawSourceBytes.Num());
			if (RawBytes > Limits.MaxSingleRawSourceBytes
				|| TotalRawSourceBytes > Limits.MaxTotalRawSourceBytes
				|| RawBytes
					> Limits.MaxTotalRawSourceBytes - TotalRawSourceBytes)
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
			TotalRawSourceBytes += RawBytes;
			if (ExceedsString(File.RelativeLogicalPath, Limits))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
		}
		for (const FAngelscriptCacheDirectOptionInput& Option : Inputs.Options)
		{
			if (Option.CanonicalKey.IsEmpty())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			if (ExceedsString(Option.CanonicalKey, Limits)
				|| ExceedsString(Option.CanonicalValue, Limits))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
			if (ContainsEmbeddedNul(Option.CanonicalKey)
				|| ContainsEmbeddedNul(Option.CanonicalValue))
			{
				return Failure(EAngelscriptCacheValidationError::EmbeddedNul);
			}
		}
		for (const FAngelscriptCachedSourceProvider& Provider : Inputs.Providers)
		{
			if (ExceedsString(Provider.CanonicalImplementationIdentity, Limits))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
		}
		for (const FAngelscriptCachedSourceMount& Mount : Inputs.Mounts)
		{
			if (ExceedsString(Mount.LogicalMount, Limits))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
			for (const FAngelscriptCachedCanonicalOption& Option : Mount.Options)
			{
				if (ExceedsString(Option.CanonicalKey, Limits))
				{
					return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
				}
			}
		}
		for (const FAngelscriptCachedPreprocessHook& Hook :
			Inputs.PreprocessHooks)
		{
			if (ExceedsString(Hook.CanonicalImplementationIdentity, Limits))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateDependencyBounds(
		const TConstArrayView<FAngelscriptCachedPreprocessorInput> Inputs,
		const TConstArrayView<FAngelscriptCachedSourceEdge> Edges,
		const uint64 ObservationCount,
		const FAngelscriptCacheDependencyCandidateLimits& Limits)
	{
		if (static_cast<uint64>(Inputs.Num()) > Limits.MaxPreprocessorInputs
			|| static_cast<uint64>(Edges.Num()) > Limits.MaxEdges
			|| ObservationCount > Limits.MaxObservations)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
		}

		uint64 TotalCharacters = 0;
		const auto AddCharacters = [&](const FString& Value)
		{
			const uint64 Characters = static_cast<uint64>(Value.Len());
			if (TotalCharacters > Limits.MaxCanonicalStringCharacters
				|| Characters
					> Limits.MaxCanonicalStringCharacters - TotalCharacters)
			{
				return false;
			}
			TotalCharacters += Characters;
			return true;
		};
		for (const FAngelscriptCachedPreprocessorInput& Input : Inputs)
		{
			if (!AddCharacters(Input.CanonicalName))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
		}
		for (const FAngelscriptCachedSourceEdge& Edge : Edges)
		{
			if (!AddCharacters(Edge.CanonicalIncludeOrGeneratorIdentity))
			{
				return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateDirectPlan(
		const FAngelscriptCacheDirectSourcePlan& Plan,
		const FAngelscriptArtifactProfileKey& Profile)
	{
		if (Plan.DirectInputDigest.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		if (!Plan.DirectProjection.PreprocessorInputs.IsEmpty()
			|| !Plan.DirectProjection.Edges.IsEmpty())
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		FAngelscriptHash256 ComputedDigest;
		const FAngelscriptCacheValidationResult DigestResult =
			FAngelscriptCacheSemanticArchive::ComputeDirectSourceInputDigest(
				Plan.DirectProjection, Profile, ComputedDigest);
		if (!DigestResult.IsSuccess())
		{
			return DigestResult;
		}
		return ComputedDigest == Plan.DirectInputDigest
			? FAngelscriptCacheValidationResult{}
			: Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
	}

	static FAngelscriptCacheValidationResult BuildObservationIndex(
		const TConstArrayView<FAngelscriptCacheObservedDependencyInput> Observations,
		TArray<int32>& OutIndex)
	{
		OutIndex.Reset();
		OutIndex.Reserve(Observations.Num());
		for (int32 Index = 0; Index < Observations.Num(); ++Index)
		{
			const FAngelscriptCacheObservedDependencyInput& Observation =
				Observations[Index];
			if (Observation.InputKey.Hash.IsZero()
				|| Observation.EffectiveValueOrContentHash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			OutIndex.Add(Index);
		}
		OutIndex.Sort([&](const int32 AIndex, const int32 BIndex)
		{
			const FAngelscriptHash256& A = Observations[AIndex].InputKey.Hash;
			const FAngelscriptHash256& B = Observations[BIndex].InputKey.Hash;
			return A == B ? AIndex < BIndex : A < B;
		});
		for (int32 Index = 1; Index < OutIndex.Num(); ++Index)
		{
			const FAngelscriptCacheObservedDependencyInput& Previous =
				Observations[OutIndex[Index - 1]];
			const FAngelscriptCacheObservedDependencyInput& Current =
				Observations[OutIndex[Index]];
			if (Previous.InputKey.Hash == Current.InputKey.Hash)
			{
				return Failure(
					Previous.EffectiveValueOrContentHash
						== Current.EffectiveValueOrContentHash
						? EAngelscriptCacheValidationError::DuplicateKey
						: EAngelscriptCacheValidationError::ConflictingKey);
			}
		}
		return {};
	}

	static int32 FindObservationIndex(
		const TConstArrayView<FAngelscriptCacheObservedDependencyInput> Observations,
		const TConstArrayView<int32> SortedIndex,
		const FAngelscriptHash256& InputKey)
	{
		int32 Lower = 0;
		int32 Upper = SortedIndex.Num();
		while (Lower < Upper)
		{
			const int32 Middle = Lower + (Upper - Lower) / 2;
			const FAngelscriptHash256& Candidate =
				Observations[SortedIndex[Middle]].InputKey.Hash;
			if (Candidate < InputKey)
			{
				Lower = Middle + 1;
			}
			else
			{
				Upper = Middle;
			}
		}
		return Lower < SortedIndex.Num()
			&& Observations[SortedIndex[Lower]].InputKey.Hash == InputKey
			? SortedIndex[Lower]
			: INDEX_NONE;
	}
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
	const FAngelscriptCacheDirectSourceInputs& Inputs,
	const FAngelscriptCacheDirectSourceLimits& Limits,
	FAngelscriptCacheDirectSourcePlan& OutPlan)
{
	using namespace AngelscriptCacheSourcePlanner_Private;
	OutPlan.Reset();
	const FAngelscriptCacheValidationResult BoundsResult =
		ValidateInputBounds(Inputs, Limits);
	if (!BoundsResult.IsSuccess())
	{
		return BoundsResult;
	}
	if (Inputs.Profile.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}

	FAngelscriptCacheDirectSourcePlan Candidate;
	FAngelscriptCachedSourceIndex& Projection = Candidate.DirectProjection;
	Projection.PayloadSchemaVersion =
		FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
	Projection.DiscoveryPolicy.PolicyVersion = Inputs.DiscoveryPolicyVersion;
	Projection.DiscoveryPolicy.FilterFlags = Inputs.DiscoveryFilterFlags;

	for (const FAngelscriptCacheDirectOptionInput& InputOption : Inputs.Options)
	{
		const TCHAR* KindPrefix = nullptr;
		switch (InputOption.Kind)
		{
		case EAngelscriptCacheDirectOptionKind::Preprocessor:
			KindPrefix = TEXT("preprocessor");
			break;
		case EAngelscriptCacheDirectOptionKind::Compiler:
			KindPrefix = TEXT("compiler");
			break;
		default:
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		FAngelscriptCachedCanonicalOption& Option =
			Projection.DiscoveryPolicy.Options.AddDefaulted_GetRef();
		Option.CanonicalKey = FString::Printf(
			TEXT("%s:%s"), KindPrefix, *InputOption.CanonicalKey);
		Option.ValueFingerprint = HashDirectOptionValue(
			InputOption.Kind, InputOption.CanonicalValue);
	}

	Projection.Providers = Inputs.Providers;
	Projection.Mounts = Inputs.Mounts;
	Projection.PreprocessHooks = Inputs.PreprocessHooks;
	for (const FAngelscriptCachedSourceProvider& Provider : Projection.Providers)
	{
		AddMissingProviderCapabilities(Provider, Projection);
	}
	for (const FAngelscriptCachedPreprocessHook& Hook :
		Projection.PreprocessHooks)
	{
		AddMissingHookCapabilities(Hook, Projection);
	}

	for (const FAngelscriptCacheDirectSourceFileInput& InputFile : Inputs.Files)
	{
		if (!Projection.Mounts.IsValidIndex(InputFile.MountIndex))
		{
			return Failure(EAngelscriptCacheValidationError::MissingGraphTarget);
		}
		const FAngelscriptCachedSourceMount& Mount =
			Projection.Mounts[InputFile.MountIndex];
		FAngelscriptCachedSourceFile& File =
			Projection.Files.AddDefaulted_GetRef();
		File.SourceKind = Mount.SourceKind;
		File.MountKey = Mount.MountKey;
		File.ProviderKey = Mount.ProviderKey;
		File.RelativeLogicalPath = InputFile.RelativeLogicalPath;
		File.RawContentHash = HashRawSource(InputFile.RawSourceBytes);
		File.GeneratedSourceKey = InputFile.GeneratedSourceKey;
		File.GeneratedConfigurationFingerprint =
			InputFile.GeneratedConfigurationFingerprint;
		File.ModuleKey = InputFile.ModuleKey;
		const FAngelscriptCacheValidationResult FileKeyResult =
			FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
				{File.SourceKind,
					File.MountKey,
					File.ProviderKey,
					File.RelativeLogicalPath,
					File.GeneratedSourceKey},
				File.SourceFileKey);
		if (!FileKeyResult.IsSuccess())
		{
			return FileKeyResult;
		}
	}

	const FAngelscriptCacheValidationResult CanonicalResult =
		FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(Projection);
	if (!CanonicalResult.IsSuccess())
	{
		return CanonicalResult;
	}
	const FAngelscriptCacheValidationResult DigestResult =
		FAngelscriptCacheSemanticArchive::ComputeDirectSourceInputDigest(
			Projection, Inputs.Profile, Candidate.DirectInputDigest);
	if (!DigestResult.IsSuccess())
	{
		return DigestResult;
	}
	OutPlan = MoveTemp(Candidate);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSourcePlanner::ComputePersistedDirectInputDigest(
	const FAngelscriptCachedSourceIndex& PersistedSourceIndex,
	const FAngelscriptArtifactProfileKey& Profile,
	FAngelscriptHash256& OutDigest)
{
	return FAngelscriptCacheSemanticArchive::ComputeDirectSourceInputDigest(
		PersistedSourceIndex, Profile, OutDigest);
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
	const FAngelscriptCacheDirectSourcePlan& DirectPlan,
	const FAngelscriptArtifactProfileKey& Profile,
	const TConstArrayView<FAngelscriptCachedPreprocessorInput> CapturedInputs,
	const TConstArrayView<FAngelscriptCachedSourceEdge> CapturedEdges,
	const FAngelscriptCacheDependencyCandidateLimits& Limits,
	FAngelscriptCachedSourceIndex& OutCandidate)
{
	using namespace AngelscriptCacheSourcePlanner_Private;
	OutCandidate = {};
	const FAngelscriptCacheValidationResult BoundsResult =
		ValidateDependencyBounds(CapturedInputs, CapturedEdges, 0, Limits);
	if (!BoundsResult.IsSuccess())
	{
		return BoundsResult;
	}
	const FAngelscriptCacheValidationResult DirectResult =
		ValidateDirectPlan(DirectPlan, Profile);
	if (!DirectResult.IsSuccess())
	{
		return DirectResult;
	}

	FAngelscriptCachedSourceIndex Candidate = DirectPlan.DirectProjection;
	Candidate.PreprocessorInputs.Append(CapturedInputs);
	Candidate.Edges.Append(CapturedEdges);
	const FAngelscriptCacheValidationResult CandidateResult =
		FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(Candidate);
	if (!CandidateResult.IsSuccess())
	{
		return CandidateResult;
	}
	OutCandidate = MoveTemp(Candidate);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
	const FAngelscriptCacheDirectSourcePlan& CurrentDirectPlan,
	const FAngelscriptArtifactProfileKey& Profile,
	const FAngelscriptCachedSourceIndex& PersistedCandidate,
	const TConstArrayView<FAngelscriptCacheObservedDependencyInput> Observations,
	const FAngelscriptCacheDependencyCandidateLimits& Limits,
	FAngelscriptCacheDependencyCandidateResult& OutResult)
{
	using namespace AngelscriptCacheSourcePlanner_Private;
	OutResult.Reset();
	const FAngelscriptCacheValidationResult BoundsResult =
		ValidateDependencyBounds(
			PersistedCandidate.PreprocessorInputs,
			PersistedCandidate.Edges,
			static_cast<uint64>(Observations.Num()),
			Limits);
	if (!BoundsResult.IsSuccess())
	{
		return BoundsResult;
	}
	const FAngelscriptCacheValidationResult DirectPlanResult =
		ValidateDirectPlan(CurrentDirectPlan, Profile);
	if (!DirectPlanResult.IsSuccess())
	{
		return DirectPlanResult;
	}

	FAngelscriptHash256 PersistedDirectDigest;
	const FAngelscriptCacheValidationResult PersistedResult =
		ComputePersistedDirectInputDigest(
			PersistedCandidate, Profile, PersistedDirectDigest);
	if (!PersistedResult.IsSuccess())
	{
		return PersistedResult;
	}
	if (!(PersistedDirectDigest == CurrentDirectPlan.DirectInputDigest))
	{
		OutResult.Match =
			EAngelscriptCacheDependencyCandidateMatch::DirectInputMismatch;
		return {};
	}

	TArray<int32> ObservationIndex;
	const FAngelscriptCacheValidationResult ObservationResult =
		BuildObservationIndex(Observations, ObservationIndex);
	if (!ObservationResult.IsSuccess())
	{
		return ObservationResult;
	}

	FAngelscriptCachedSourceIndex CanonicalCandidate = PersistedCandidate;
	const FAngelscriptCacheValidationResult CanonicalResult =
		FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(
			CanonicalCandidate);
	if (!CanonicalResult.IsSuccess())
	{
		return CanonicalResult;
	}
	for (const FAngelscriptCachedPreprocessorInput& Input :
		CanonicalCandidate.PreprocessorInputs)
	{
		const int32 ObservationValueIndex = FindObservationIndex(
			Observations, ObservationIndex, Input.InputKey.Hash);
		if (ObservationValueIndex == INDEX_NONE)
		{
			OutResult.Match =
				EAngelscriptCacheDependencyCandidateMatch::ObservationUnavailable;
			OutResult.FirstNonMatchingInputKey = Input.InputKey;
			return {};
		}
		++OutResult.ComparedDependencyCount;
		if (!(Observations[ObservationValueIndex].EffectiveValueOrContentHash
			== Input.EffectiveValueOrContentHash))
		{
			OutResult.Match =
				EAngelscriptCacheDependencyCandidateMatch::DependencyMismatch;
			OutResult.FirstNonMatchingInputKey = Input.InputKey;
			return {};
		}
	}
	if (Observations.Num() != CanonicalCandidate.PreprocessorInputs.Num())
	{
		OutResult.Reset();
		return Failure(EAngelscriptCacheValidationError::UnexpectedRecord);
	}

	FAngelscriptCachedSourceIndex Rebuilt = CurrentDirectPlan.DirectProjection;
	Rebuilt.PreprocessorInputs = CanonicalCandidate.PreprocessorInputs;
	Rebuilt.Edges = CanonicalCandidate.Edges;
	const FAngelscriptCacheValidationResult RebuildResult =
		FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(Rebuilt);
	if (!RebuildResult.IsSuccess())
	{
		OutResult.Reset();
		return RebuildResult;
	}
	if (!(Rebuilt.SourceSnapshot == CanonicalCandidate.SourceSnapshot))
	{
		OutResult.Reset();
		return Failure(EAngelscriptCacheValidationError::SourceSnapshotMismatch);
	}

	OutResult.Match = EAngelscriptCacheDependencyCandidateMatch::Exact;
	OutResult.ExactSourceIndex = MoveTemp(Rebuilt);
	return {};
}
