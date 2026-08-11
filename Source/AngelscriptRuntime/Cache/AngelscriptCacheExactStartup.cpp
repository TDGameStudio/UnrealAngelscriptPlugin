#include "Cache/AngelscriptCacheExactStartup.h"

#include "Cache/AngelscriptCacheService.h"
#include "Core/AngelscriptEngine.h"

namespace AngelscriptCacheExactStartup_Private
{
	static FAngelscriptCacheExactStartupResult Miss(
		const EAngelscriptCacheExactStartupReason Reason,
		FString Detail,
		const EAngelscriptCacheDependencyCandidateMatch CandidateMatch =
			EAngelscriptCacheDependencyCandidateMatch::Invalid)
	{
		FAngelscriptCacheExactStartupResult Result;
		Result.Disposition = EAngelscriptCacheExactStartupDisposition::Miss;
		Result.Reason = Reason;
		Result.CandidateMatch = CandidateMatch;
		Result.Detail = MoveTemp(Detail);
		return Result;
	}

	static FAngelscriptCacheExactStartupResult Rejected(
		const EAngelscriptCacheExactStartupReason Reason,
		FString Detail,
		const TOptional<FAngelscriptCacheValidationResult>& Validation = {})
	{
		FAngelscriptCacheExactStartupResult Result;
		Result.Disposition =
			EAngelscriptCacheExactStartupDisposition::Rejected;
		Result.Reason = Reason;
		Result.Validation = Validation;
		Result.Detail = MoveTemp(Detail);
		return Result;
	}

	static const FAngelscriptCachedSourceIndex* FindSourceIndex(
		const FAngelscriptValidatedGeneration& Generation)
	{
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (Record->GetRecordId()
				== Generation.Manifest.SourceIndexRecordId)
			{
				return Record->TryGetSourceIndex();
			}
		}
		return nullptr;
	}

	static const FAngelscriptDecodedCacheRecord* FindRecord(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheRecordId& RecordId)
	{
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (Record->GetRecordId() == RecordId)
			{
				return &Record.Get();
			}
		}
		return nullptr;
	}

	static const FAngelscriptCacheModuleSourcePlan* FindCurrentModule(
		const FAngelscriptCacheProductionSourceDiscoveryResult& Discovery,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		return Discovery.Modules.FindByPredicate(
			[&ModuleKey](const FAngelscriptCacheModuleSourcePlan& Module)
			{
				return Module.ModuleKey == ModuleKey;
			});
	}

	static bool IsSupportedRestoredModuleShape(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheModuleSnapshotLink& Link)
	{
		const FAngelscriptDecodedCacheRecord* SnapshotRecord =
			FindRecord(Generation, Link.RecordId);
		const FAngelscriptCachedModuleSnapshot* Snapshot =
			SnapshotRecord != nullptr
				? SnapshotRecord->TryGetModuleSnapshot() : nullptr;
		if (Snapshot == nullptr || Snapshot->ModuleKey != Link.ModuleKey
			|| Snapshot->TypeSchemas.Num() != 1
			|| Snapshot->FunctionBodies.Num() != 1)
		{
			return false;
		}

		const FAngelscriptDecodedCacheRecord* TypeRecord = FindRecord(
			Generation, Snapshot->TypeSchemas[0].RecordId);
		const FAngelscriptCachedTypeSchema* TypeSchema = TypeRecord != nullptr
			? TypeRecord->TryGetTypeSchema() : nullptr;
		return TypeSchema != nullptr
			&& TypeSchema->TypeKind == EAngelscriptCachedTypeKind::Enum;
	}

	static bool AddPreparedRecord(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheRecordId& RecordId,
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		if (Artifacts.Records.ContainsByPredicate(
			[&RecordId](const FAngelscriptPreparedRecord& Existing)
			{
				return Existing.RecordId == RecordId;
			}))
		{
			return true;
		}
		const FAngelscriptDecodedCacheRecord* Record =
			FindRecord(Generation, RecordId);
		if (Record == nullptr || Record->GetCanonicalPayload().IsEmpty())
		{
			return false;
		}
		FAngelscriptPreparedRecord& Prepared =
			Artifacts.Records.AddDefaulted_GetRef();
		Prepared.RecordId = RecordId;
		Prepared.CanonicalPayload.Append(Record->GetCanonicalPayload());
		return true;
	}

	static bool BuildRestoredPublicationInput(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheProductionSourceDiscoveryResult& Discovery,
		const FAngelscriptHash256& GenerationId,
		FAngelscriptCacheSuccessfulPublicationInput& OutInput)
	{
		OutInput = {};
		OutInput.Kind = EAngelscriptCacheSuccessfulCompileKind::Initial;
		OutInput.Disposition = EAngelscriptCachePublicationDisposition::Current;
		OutInput.Compatibility = Generation.Manifest.Compatibility;
		OutInput.Context = Generation.Manifest.Context;
		OutInput.Profile = Generation.Manifest.Profile;
		OutInput.bRestoredFromStore = true;
		OutInput.PersistedGenerationId = GenerationId;
		OutInput.Modules.Reserve(Generation.Manifest.ModuleSnapshots.Num());

		for (const FAngelscriptCacheModuleSnapshotLink& Link
			: Generation.Manifest.ModuleSnapshots)
		{
			const FAngelscriptCacheModuleSourcePlan* CurrentModule =
				FindCurrentModule(Discovery, Link.ModuleKey);
			const FAngelscriptDecodedCacheRecord* SnapshotRecord =
				FindRecord(Generation, Link.RecordId);
			const FAngelscriptCachedModuleSnapshot* Snapshot =
				SnapshotRecord != nullptr
					? SnapshotRecord->TryGetModuleSnapshot() : nullptr;
			if (CurrentModule == nullptr || Snapshot == nullptr)
			{
				return false;
			}

			FAngelscriptCacheCleanModuleArtifacts& Artifacts =
				OutInput.Modules.AddDefaulted_GetRef();
			Artifacts.ModuleKey = Link.ModuleKey;
			Artifacts.CanonicalModuleName =
				CurrentModule->CanonicalModuleName;
			Artifacts.SourceSnapshot = Generation.Manifest.SourceSnapshot;
			Artifacts.SourceIndexRecordId =
				Generation.Manifest.SourceIndexRecordId;
			Artifacts.ModuleSnapshot = Link;
			if (!AddPreparedRecord(
					Generation, Generation.Manifest.SourceIndexRecordId, Artifacts)
				|| !AddPreparedRecord(
					Generation, Snapshot->ModuleInterface.RecordId, Artifacts))
			{
				return false;
			}
			for (const FAngelscriptCachedTypeSchemaLink& Type
				: Snapshot->TypeSchemas)
			{
				if (!AddPreparedRecord(Generation, Type.RecordId, Artifacts))
				{
					return false;
				}
			}
			if (!AddPreparedRecord(
					Generation, Snapshot->ModuleState.RecordId, Artifacts))
			{
				return false;
			}
			for (const FAngelscriptCachedFunctionBodyLink& Function
				: Snapshot->FunctionBodies)
			{
				const FAngelscriptDecodedCacheRecord* BodyRecord =
					FindRecord(Generation, Function.RecordId);
				const FAngelscriptCachedFunctionBody* Body =
					BodyRecord != nullptr
						? BodyRecord->TryGetFunctionBody() : nullptr;
				if (Body == nullptr
					|| !AddPreparedRecord(
						Generation, Function.RecordId, Artifacts))
				{
					return false;
				}
				Artifacts.ValidatedFunctionArtifactIdentities.Add(
					Body->Identity);
				if (Body->DebugSidecar.IsSet()
					&& !AddPreparedRecord(
						Generation,
						Body->DebugSidecar.GetValue(),
						Artifacts))
				{
					return false;
				}
			}
			if (!AddPreparedRecord(Generation, Link.RecordId, Artifacts))
			{
				return false;
			}
		}
		return true;
	}
}

FAngelscriptCacheExactStartupResult RestoreAngelscriptCacheExactStartup(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptArtifactProfileKey& CurrentProfile,
	const FAngelscriptCacheProductionSourceDiscoveryResult& CurrentDiscovery,
	const FAngelscriptCacheDependencyCandidateLimits& DependencyLimits,
	const FAngelscriptCacheReadLimits& ReadLimits,
	const FAngelscriptHash256* PersistedGenerationId,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	using namespace AngelscriptCacheExactStartup_Private;

	if (TargetEngine.GetScriptEngine() == nullptr
		|| CurrentProfile.Hash.IsZero()
		|| CurrentDiscovery.DirectPlan.DirectInputDigest.IsZero())
	{
		return Rejected(
			EAngelscriptCacheExactStartupReason::InvalidInput,
			TEXT("Exact startup requires a normally initialized Engine, current profile and direct source plan"));
	}
	if (Generation.Manifest.Profile.Hash != CurrentProfile.Hash)
	{
		return Miss(
			EAngelscriptCacheExactStartupReason::ProfileMismatch,
			TEXT("The selected generation belongs to a different artifact profile"));
	}

	const FAngelscriptCachedSourceIndex* PersistedSourceIndex =
		FindSourceIndex(Generation);
	if (PersistedSourceIndex == nullptr)
	{
		return Rejected(
			EAngelscriptCacheExactStartupReason::MissingSourceIndex,
			TEXT("The validated generation has no reachable SourceIndex record"));
	}

	FAngelscriptCacheDependencyObservationPlan Observations;
	const FAngelscriptCacheValidationResult ObservationResult =
		FAngelscriptCacheSourceDiscovery::BuildCurrentDependencyObservations(
			CurrentDiscovery.DirectPlan,
			*PersistedSourceIndex,
			Observations);
	if (!ObservationResult.IsSuccess())
	{
		return Rejected(
			EAngelscriptCacheExactStartupReason::SourceCandidateRejected,
			TEXT("Current dependency observation planning rejected the persisted SourceIndex"),
			ObservationResult);
	}

	FAngelscriptCacheDependencyCandidateResult Candidate;
	const FAngelscriptCacheValidationResult CandidateResult =
		FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
			CurrentDiscovery.DirectPlan,
			CurrentProfile,
			*PersistedSourceIndex,
			Observations.Observations,
			DependencyLimits,
			Candidate);
	if (!CandidateResult.IsSuccess())
	{
		return Rejected(
			EAngelscriptCacheExactStartupReason::SourceCandidateRejected,
			TEXT("The persisted SourceIndex candidate failed validation"),
			CandidateResult);
	}

	switch (Candidate.Match)
	{
	case EAngelscriptCacheDependencyCandidateMatch::DirectInputMismatch:
		return Miss(
			EAngelscriptCacheExactStartupReason::DirectInputMismatch,
			TEXT("Current direct source inputs differ from the persisted generation"),
			Candidate.Match);
	case EAngelscriptCacheDependencyCandidateMatch::ObservationUnavailable:
		return Miss(
			EAngelscriptCacheExactStartupReason::ObservationUnavailable,
			TEXT("A persisted preprocessing dependency has no current authority"),
			Candidate.Match);
	case EAngelscriptCacheDependencyCandidateMatch::DependencyMismatch:
		return Miss(
			EAngelscriptCacheExactStartupReason::DependencyMismatch,
			TEXT("A current preprocessing dependency differs from the persisted candidate"),
			Candidate.Match);
	case EAngelscriptCacheDependencyCandidateMatch::Exact:
		break;
	default:
		return Rejected(
			EAngelscriptCacheExactStartupReason::SourceCandidateRejected,
			TEXT("Dependency candidate validation returned no usable disposition"));
	}

	if (Candidate.ExactSourceIndex.SourceSnapshot
			!= Generation.Manifest.SourceSnapshot
		|| PersistedSourceIndex->SourceSnapshot
			!= Generation.Manifest.SourceSnapshot)
	{
		return Rejected(
			EAngelscriptCacheExactStartupReason::SourceSnapshotMismatch,
			TEXT("The exact rebuilt SourceIndex does not match the generation source snapshot"),
			FAngelscriptCacheValidationResult(
				EAngelscriptCacheValidationError::SourceSnapshotMismatch,
				EAngelscriptCacheRecordKind::SourceIndex));
	}

	if (Generation.Manifest.ModuleSnapshots.IsEmpty()
		|| Generation.Manifest.ModuleSnapshots.Num()
			!= CurrentDiscovery.Modules.Num())
	{
		return Miss(
			EAngelscriptCacheExactStartupReason::ModuleSetMismatch,
			TEXT("The current and persisted exact-start module sets differ"));
	}
	TArray<FAngelscriptStableModuleKey> ModuleKeys;
	ModuleKeys.Reserve(Generation.Manifest.ModuleSnapshots.Num());
	for (const FAngelscriptCacheModuleSnapshotLink& Link
		: Generation.Manifest.ModuleSnapshots)
	{
		const FAngelscriptCacheModuleSourcePlan* CurrentModule =
			FindCurrentModule(CurrentDiscovery, Link.ModuleKey);
		if (CurrentModule == nullptr)
		{
			return Miss(
				EAngelscriptCacheExactStartupReason::ModuleSetMismatch,
				TEXT("A persisted exact-start module is absent from current discovery"));
		}
		if (!CurrentModule->bExactFastPathEligible)
		{
			return Miss(
				EAngelscriptCacheExactStartupReason::ModuleIneligible,
				TEXT("A current module has an unstable provider or preprocessing-hook scope"));
		}
		// Fail closed before mutating the target Engine. Capture already admits
		// broader graphs than the first executable restore vertical; a mixed
		// generation must never activate only its early supported modules.
		if (!IsSupportedRestoredModuleShape(Generation, Link))
		{
			return Miss(
				EAngelscriptCacheExactStartupReason::ModuleIneligible,
				TEXT("A persisted module is outside the executable exact-start restore vertical"));
		}
		ModuleKeys.Add(Link.ModuleKey);
	}

	FAngelscriptCacheMutationGuard Mutation;
	if (FAngelscriptCacheService* Service = TargetEngine.GetCacheService())
	{
		Mutation = Service->EnterMutation(
			EAngelscriptCacheMutationKind::StartupRestore);
		if (!Mutation.IsEntered())
		{
			return Rejected(
				EAngelscriptCacheExactStartupReason::RestoreRejected,
				TEXT("The per-Engine startup restore mutation gate is unavailable"));
		}
	}

	FAngelscriptCacheExactStartupResult Result;
	Result.CandidateMatch = Candidate.Match;
	Result.SelectedModuleCount = static_cast<uint32>(
		Generation.Manifest.ModuleSnapshots.Num());
	const FAngelscriptCacheRestoreResult Restore =
		RestoreAngelscriptCacheModules(
			TargetEngine,
			Generation,
			ModuleKeys,
			CurrentDiscovery.CurrentSources,
			ReadLimits,
			FaultInjector);
	if (!Restore.IsSuccess())
	{
		Result.Disposition = Restore.Error
			== EAngelscriptCacheRestoreError::CurrentSourceProjectionMismatch
				? EAngelscriptCacheExactStartupDisposition::Miss
				: EAngelscriptCacheExactStartupDisposition::Rejected;
		Result.Reason = Restore.Error
			== EAngelscriptCacheRestoreError::CurrentSourceProjectionMismatch
				? EAngelscriptCacheExactStartupReason::
					CurrentSourceProjectionMismatch
				: EAngelscriptCacheExactStartupReason::RestoreRejected;
		Result.Validation = Restore.Validation;
		Result.Detail = Restore.Detail;
		return Result;
	}
	Result.RestoredModuleCount = Restore.RestoredModuleCount;
	Result.RestoredTypeCount = Restore.RestoredTypeCount;
	Result.RestoredFunctionCount = Restore.RestoredFunctionCount;

	Result.Disposition = EAngelscriptCacheExactStartupDisposition::Restored;
	Result.Reason = EAngelscriptCacheExactStartupReason::None;
	if (PersistedGenerationId != nullptr)
	{
		FAngelscriptCacheSuccessfulPublicationInput PublicationInput;
		FAngelscriptCacheService* Service = TargetEngine.GetCacheService();
		if (PersistedGenerationId->IsZero()
			|| Service == nullptr
			|| !Mutation.IsEntered()
			|| !BuildRestoredPublicationInput(
				Generation,
				CurrentDiscovery,
				*PersistedGenerationId,
				PublicationInput))
		{
			Result.Disposition =
				EAngelscriptCacheExactStartupDisposition::Rejected;
			Result.Reason =
				EAngelscriptCacheExactStartupReason::RestoreRejected;
			Result.Detail = TEXT(
				"The restored generation could not build its immutable Current diagnostic publication");
			return Result;
		}
		const FAngelscriptCacheFreezePublicationResult Freeze =
			Service->FreezeSuccessfulCompileArtifacts(
				Mutation.GetToken(), MoveTemp(PublicationInput));
		if (!Freeze.IsSuccess())
		{
			Result.Disposition =
				EAngelscriptCacheExactStartupDisposition::Rejected;
			Result.Reason =
				EAngelscriptCacheExactStartupReason::RestoreRejected;
			Result.Detail = FString::Printf(
				TEXT("The restored generation could not adopt Current: FreezeError=%u"),
				static_cast<uint32>(Freeze.Error));
			return Result;
		}
	}
	Result.Detail = FString::Printf(
		TEXT("Exact source candidate restored %u modules without frontend, compiler or Store publication work"),
		Result.RestoredModuleCount);
	return Result;
}
