#include "Cache/AngelscriptCacheService.h"

#include "Cache/AngelscriptCacheDiagnostics.h"

#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"

namespace AngelscriptCacheService_Private
{
	FThreadSafeCounter64 GNextServiceIdentity;

	static bool IsValidRecordId(const FAngelscriptCacheRecordId& RecordId)
	{
		return static_cast<uint8>(RecordId.Kind)
			>= static_cast<uint8>(EAngelscriptCacheRecordKind::SourceIndex)
			&& static_cast<uint8>(RecordId.Kind)
			<= static_cast<uint8>(EAngelscriptCacheRecordKind::ModuleSnapshot)
			&& !RecordId.ContentHash.IsZero();
	}

	static bool ContainsRecord(
		const TConstArrayView<FAngelscriptPreparedRecord> Records,
		const FAngelscriptCacheRecordId& RecordId)
	{
		return Records.ContainsByPredicate(
			[&RecordId](const FAngelscriptPreparedRecord& Record)
			{
				return Record.RecordId == RecordId;
			});
	}

	static bool IsValidModuleArtifacts(
		const FAngelscriptCacheCleanModuleArtifacts& Module)
	{
		if (Module.ModuleKey.Hash.IsZero()
			|| Module.SourceSnapshot.IsZero()
			|| !IsValidRecordId(Module.SourceIndexRecordId)
			|| Module.SourceIndexRecordId.Kind
				!= EAngelscriptCacheRecordKind::SourceIndex
			|| Module.ModuleSnapshot.ModuleKey != Module.ModuleKey
			|| !IsValidRecordId(Module.ModuleSnapshot.RecordId)
			|| Module.ModuleSnapshot.RecordId.Kind
				!= EAngelscriptCacheRecordKind::ModuleSnapshot
			|| Module.Records.IsEmpty()
			|| !ContainsRecord(Module.Records, Module.SourceIndexRecordId)
			|| !ContainsRecord(Module.Records, Module.ModuleSnapshot.RecordId))
		{
			return false;
		}

		for (const FAngelscriptPreparedRecord& Record : Module.Records)
		{
			if (!IsValidRecordId(Record.RecordId)
				|| Record.CanonicalPayload.IsEmpty())
			{
				return false;
			}
		}
		return true;
	}

	static TOptional<FAngelscriptCacheWriterToken> MakeWriterToken()
	{
		const FString Nonce = FGuid::NewGuid()
			.ToString(EGuidFormats::Digits)
			.ToLower();
		return FAngelscriptCacheWriterToken::TryParse(FString::Printf(
			TEXT("%u-%s"),
			FPlatformProcess::GetCurrentProcessId(),
			*Nonce));
	}

	static void FlushPublicationToStore(
		const TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe>& Publication,
		const FAngelscriptCachePackPolicy& PackPolicy,
		const FString& RequestedBaseRoot,
		const FAngelscriptCacheWriterToken& WriterToken,
		const double DeadlineSeconds,
		TFunctionRef<bool()> IsCancellationRequested,
		IAngelscriptCacheStorageCodec& Codec,
		IAngelscriptCacheNamespaceLockOps& LockOps,
		IAngelscriptCacheAtomicFileOps& FileOps,
		FAngelscriptCacheLifecycleFlushSlotResult& OutResult)
	{
		if (!Publication.IsValid())
		{
			return;
		}
		// A restored Current already names the immutable generation selected from
		// this Store. It remains visible to status/report tools, but flushing it
		// would only rebuild and republish identical content.
		if (Publication->bRestoredFromStore)
		{
			return;
		}

		OutResult.bAttempted = true;
		OutResult.TransactionOrdinal = Publication->TransactionOrdinal;
		OutResult.Disposition = Publication->Disposition;
		if (IsCancellationRequested())
		{
			OutResult.Publication = FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::Cancelled,
				EAngelscriptCacheStoreStage::CandidateValidation);
			return;
		}
		FAngelscriptCacheCleanCaptureOptions CaptureOptions;
		CaptureOptions.Compatibility = Publication->Compatibility;
		CaptureOptions.Context = Publication->Context;
		CaptureOptions.Profile = Publication->Profile;
		FAngelscriptCachePreparedColdGeneration Prepared;
		OutResult.Preparation = PrepareAngelscriptCacheColdGeneration(
			Publication->Modules,
			CaptureOptions,
			PackPolicy,
			Codec,
			Prepared);
		if (!OutResult.Preparation.IsSuccess())
		{
			return;
		}
		OutResult.GenerationId =
			Prepared.EncodedManifest.ComputedGenerationId;

		FAngelscriptCacheStorePaths Paths;
		OutResult.Publication = BuildAngelscriptCacheStorePaths(
			RequestedBaseRoot,
			Publication->Compatibility,
			Publication->Context,
			FileOps,
			Paths);
		if (!OutResult.Publication.IsSuccess())
		{
			return;
		}

		const EAngelscriptCachePointerKind PointerKind =
			Publication->Disposition
				== EAngelscriptCachePublicationDisposition::Current
			? EAngelscriptCachePointerKind::Current
			: EAngelscriptCachePointerKind::PendingColdStart;
		TOptional<FAngelscriptHash256> ObservedGenerationId;
		OutResult.Publication = ReadAngelscriptCachePointerSlot(
			Paths, PointerKind, FileOps, ObservedGenerationId);
		if (!OutResult.Publication.IsSuccess())
		{
			return;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		OutResult.Publication = PublishAngelscriptCacheGeneration(
			Paths,
			Publication->Disposition,
			ObservedGenerationId,
			Prepared.Packs,
			Prepared.Manifest,
			Prepared.EncodedManifest,
			WriterToken,
			Limits,
			Budget,
			DeadlineSeconds,
			IsCancellationRequested,
			Codec,
			LockOps,
			FileOps);
	}

	static FAngelscriptCacheLifecycleFlushResult FlushPublicationsToStore(
		const FAngelscriptCacheLifecyclePublications& Publications,
		const FAngelscriptCachePackPolicy& PackPolicy,
		const FString& RequestedBaseRoot,
		const double DeadlineSeconds,
		TFunctionRef<bool()> IsExternalCancellationRequested)
	{
		FAngelscriptCacheLifecycleFlushResult Result;
		if (RequestedBaseRoot.IsEmpty()
			|| !FMath::IsFinite(DeadlineSeconds))
		{
			Result.Error = EAngelscriptCacheLifecycleFlushError::InvalidInput;
			Result.Detail = TEXT("Cache lifecycle flush requires a root and a finite deadline");
			return Result;
		}
		if (!Publications.Current.IsValid()
			&& !Publications.PendingColdStart.IsValid())
		{
			Result.Detail = TEXT("Cache lifecycle flush had no frozen publication to write");
			return Result;
		}

		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		const TOptional<FAngelscriptCacheWriterToken> WriterToken =
			MakeWriterToken();
		if (!FileOps.IsValid() || !LockOps.IsValid() || !WriterToken.IsSet())
		{
			Result.Error =
				EAngelscriptCacheLifecycleFlushError::PlatformStoreUnavailable;
			Result.Detail = TEXT("Cache lifecycle flush platform Store dependencies are unavailable");
			return Result;
		}

		auto IsCancellationRequested = [
			DeadlineSeconds,
			&IsExternalCancellationRequested]()
		{
			return IsExternalCancellationRequested()
				|| FPlatformTime::Seconds() >= DeadlineSeconds;
		};
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FlushPublicationToStore(
			Publications.Current,
			PackPolicy,
			RequestedBaseRoot,
			WriterToken.GetValue(),
			DeadlineSeconds,
			IsCancellationRequested,
			Codec,
			*LockOps,
			*FileOps,
			Result.Current);
		FlushPublicationToStore(
			Publications.PendingColdStart,
			PackPolicy,
			RequestedBaseRoot,
			WriterToken.GetValue(),
			DeadlineSeconds,
			IsCancellationRequested,
			Codec,
			*LockOps,
			*FileOps,
			Result.PendingColdStart);

		const bool bCurrentFailed = Result.Current.bAttempted
			&& !Result.Current.IsSuccess();
		const bool bPendingFailed = Result.PendingColdStart.bAttempted
			&& !Result.PendingColdStart.IsSuccess();
		if (bCurrentFailed || bPendingFailed)
		{
			Result.Error = EAngelscriptCacheLifecycleFlushError::SlotFailure;
			Result.Detail = FString::Printf(
				TEXT("Cache lifecycle flush slot failure: CurrentPrepare=%u CurrentStore=%u PendingPrepare=%u PendingStore=%u"),
				static_cast<uint32>(Result.Current.Preparation.Error),
				static_cast<uint32>(Result.Current.Publication.Error),
				static_cast<uint32>(Result.PendingColdStart.Preparation.Error),
				static_cast<uint32>(Result.PendingColdStart.Publication.Error));
			return Result;
		}

		Result.Detail = FString::Printf(
			TEXT("Cache lifecycle flush succeeded: Current=%d PendingColdStart=%d"),
			Result.Current.bAttempted ? 1 : 0,
			Result.PendingColdStart.bAttempted ? 1 : 0);
		return Result;
	}

	struct FBoundedShutdownFlushState final
	{
		FBoundedShutdownFlushState()
			: Completion(FPlatformProcess::GetSynchEventFromPool(true))
		{
		}

		~FBoundedShutdownFlushState()
		{
			if (Completion != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(Completion);
			}
		}

		FEvent* Completion = nullptr;
		TAtomic<bool> bCancellationRequested{false};
		FCriticalSection ResultGate;
		FAngelscriptCacheLifecycleFlushResult Result;
	};
}

FAngelscriptCacheMutationGuard::FAngelscriptCacheMutationGuard(
	FAngelscriptCacheService& InService,
	const FAngelscriptCacheMutationToken& InToken)
	: Service(&InService)
	, Token(InToken)
{
}

FAngelscriptCacheMutationGuard::~FAngelscriptCacheMutationGuard()
{
	Reset();
}

FAngelscriptCacheMutationGuard::FAngelscriptCacheMutationGuard(
	FAngelscriptCacheMutationGuard&& Other) noexcept
	: Service(Other.Service)
	, Token(Other.Token)
{
	Other.Service = nullptr;
	Other.Token = {};
}

FAngelscriptCacheMutationGuard& FAngelscriptCacheMutationGuard::operator=(
	FAngelscriptCacheMutationGuard&& Other) noexcept
{
	if (this != &Other)
	{
		Reset();
		Service = Other.Service;
		Token = Other.Token;
		Other.Service = nullptr;
		Other.Token = {};
	}
	return *this;
}

void FAngelscriptCacheMutationGuard::Reset()
{
	if (Service != nullptr)
	{
		FAngelscriptCacheService* OwnedService = Service;
		Service = nullptr;
		OwnedService->LeaveMutation(Token);
		Token = {};
	}
}

FAngelscriptCacheService::FAngelscriptCacheService()
	: ServiceIdentity(static_cast<uint64>(
		AngelscriptCacheService_Private::GNextServiceIdentity.Increment()))
{
	check(ServiceIdentity != 0);
}

FAngelscriptCacheService::~FAngelscriptCacheService()
{
	BeginEngineShutdown();
}

FAngelscriptCacheMutationGuard FAngelscriptCacheService::EnterMutation(
	const EAngelscriptCacheMutationKind Kind,
	const FAngelscriptCacheMutationToken* ParentToken)
{
	if (Kind == EAngelscriptCacheMutationKind::Invalid)
	{
		return {};
	}

	Gate.Lock();
	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
	if (Phase == EAngelscriptCacheMutationPhase::ShuttingDown
		|| (Phase == EAngelscriptCacheMutationPhase::RuntimeGameThread
			&& !IsInGameThread()))
	{
		Gate.Unlock();
		return {};
	}

	if (ActiveDepth == 0)
	{
		OwnerThreadId = CurrentThreadId;
		++CurrentEpoch;
		if (CurrentEpoch == 0)
		{
			++CurrentEpoch;
		}
		ActiveDepth = 1;
	}
	else if (OwnerThreadId == CurrentThreadId
		&& ParentToken != nullptr
		&& IsCurrentOwnerToken(*ParentToken))
	{
		++ActiveDepth;
	}
	else
	{
		Gate.Unlock();
		return {};
	}

	FAngelscriptCacheMutationToken Token;
	Token.ServiceIdentity = ServiceIdentity;
	Token.Epoch = CurrentEpoch;
	Token.OwnerThreadId = OwnerThreadId;
	return FAngelscriptCacheMutationGuard(*this, Token);
}

void FAngelscriptCacheService::TransitionToRuntimeGameThread()
{
	check(IsInGameThread());
	FScopeLock Lock(&Gate);
	check(ActiveDepth == 0);
	if (Phase != EAngelscriptCacheMutationPhase::ShuttingDown)
	{
		Phase = EAngelscriptCacheMutationPhase::RuntimeGameThread;
	}
}

void FAngelscriptCacheService::BeginEngineShutdown()
{
	FScopeLock Lock(&Gate);
	Phase = EAngelscriptCacheMutationPhase::ShuttingDown;
}

bool FAngelscriptCacheService::IsCurrentOwnerToken(
	const FAngelscriptCacheMutationToken& Token) const
{
	return ActiveDepth != 0
		&& Token.ServiceIdentity == ServiceIdentity
		&& Token.Epoch == CurrentEpoch
		&& Token.OwnerThreadId == OwnerThreadId
		&& Token.OwnerThreadId == FPlatformTLS::GetCurrentThreadId();
}

void FAngelscriptCacheService::LeaveMutation(
	const FAngelscriptCacheMutationToken& Token)
{
	check(IsCurrentOwnerToken(Token));
	check(ActiveDepth != 0);
	--ActiveDepth;
	if (ActiveDepth == 0)
	{
		OwnerThreadId = 0;
	}
	Gate.Unlock();
}

FAngelscriptCacheFreezePublicationResult
FAngelscriptCacheService::FreezeSuccessfulCompileArtifacts(
	const FAngelscriptCacheMutationToken& Token,
	FAngelscriptCacheSuccessfulPublicationInput Input)
{
	using namespace AngelscriptCacheService_Private;
	FScopeLock Lock(&Gate);
	FAngelscriptCacheFreezePublicationResult Result;
	if (!IsCurrentOwnerToken(Token))
	{
		Result.Error =
			EAngelscriptCacheFreezePublicationError::NotMutationOwner;
		return Result;
	}
	if (Phase == EAngelscriptCacheMutationPhase::ShuttingDown)
	{
		Result.Error = EAngelscriptCacheFreezePublicationError::ShuttingDown;
		return Result;
	}
	if (Input.Kind == EAngelscriptCacheSuccessfulCompileKind::Invalid
		|| (Input.Disposition
			!= EAngelscriptCachePublicationDisposition::Current
			&& Input.Disposition
				!= EAngelscriptCachePublicationDisposition::PendingColdStart)
		|| Input.Compatibility.Hash.IsZero()
		|| Input.Context.Hash.IsZero()
		|| Input.Profile.Hash.IsZero()
		|| (Input.bRestoredFromStore
			!= !Input.PersistedGenerationId.IsZero())
		|| Input.Modules.IsEmpty())
	{
		Result.Error = EAngelscriptCacheFreezePublicationError::InvalidInput;
		return Result;
	}

	Input.Modules.Sort([](
		const FAngelscriptCacheCleanModuleArtifacts& Left,
		const FAngelscriptCacheCleanModuleArtifacts& Right)
	{
		return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
	});
	for (int32 Index = 0; Index < Input.Modules.Num(); ++Index)
	{
		const FAngelscriptCacheCleanModuleArtifacts& Module =
			Input.Modules[Index];
		if (!IsValidModuleArtifacts(Module))
		{
			Result.Error =
				EAngelscriptCacheFreezePublicationError::InvalidInput;
			return Result;
		}
		if (Index != 0
			&& Input.Modules[Index - 1].ModuleKey == Module.ModuleKey)
		{
			Result.Error =
				EAngelscriptCacheFreezePublicationError::DuplicateModule;
			return Result;
		}
		if (Index != 0
			&& (!(Input.Modules[0].SourceSnapshot == Module.SourceSnapshot)
				|| !(Input.Modules[0].SourceIndexRecordId
					== Module.SourceIndexRecordId)))
		{
			Result.Error = EAngelscriptCacheFreezePublicationError::
				InconsistentSourceSnapshot;
			return Result;
		}
	}

	TSharedRef<FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> Publication =
		MakeShared<FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe>();
	Publication->TransactionOrdinal = ++NextTransactionOrdinal;
	Publication->Kind = Input.Kind;
	Publication->Disposition = Input.Disposition;
	Publication->Compatibility = Input.Compatibility;
	Publication->Context = Input.Context;
	Publication->Profile = Input.Profile;
	Publication->SourceSnapshot = Input.Modules[0].SourceSnapshot;
	Publication->SourceIndexRecordId = Input.Modules[0].SourceIndexRecordId;
	Publication->bRestoredFromStore = Input.bRestoredFromStore;
	Publication->PersistedGenerationId = Input.PersistedGenerationId;
	Publication->Modules = MoveTemp(Input.Modules);

	LatestSuccessfulPublication = Publication;
	if (Publication->Disposition
		== EAngelscriptCachePublicationDisposition::Current)
	{
		CurrentSuccessfulPublication = Publication;
		if (Publication->Kind
			== EAngelscriptCacheSuccessfulCompileKind::Initial
			|| Publication->Kind
				== EAngelscriptCacheSuccessfulCompileKind::FullReload)
		{
			PendingColdStartPublication.Reset();
		}
	}
	else
	{
		PendingColdStartPublication = Publication;
	}

	FAngelscriptCacheDecisionEvent Decision;
	Decision.TransactionOrdinal = Publication->TransactionOrdinal;
	// StartupRestore records the actual validation/activation result. This
	// separate event records adoption of that active generation into Current.
	Decision.Stage = EAngelscriptCacheDecisionStage::SuccessfulPublication;
	Decision.Outcome = Publication->bRestoredFromStore
		? EAngelscriptCacheDecisionOutcome::Reused
		: EAngelscriptCacheDecisionOutcome::Published;
	Decision.ReasonDomain = Publication->bRestoredFromStore
		? EAngelscriptCacheDecisionReasonDomain::ExactStartup
		: EAngelscriptCacheDecisionReasonDomain::FreezePublication;
	Decision.ReasonCode = 0;
	if (Publication->bRestoredFromStore)
	{
		Decision.ExpectedCoordinate = Publication->PersistedGenerationId;
	}
	Decision.Profile = Publication->Profile;
	Decision.SourceSnapshot = Publication->SourceSnapshot;
	Decision.PrimaryCount = static_cast<uint32>(Publication->Modules.Num());
	Decision.ModuleKeys.Reserve(Publication->Modules.Num());
	for (const FAngelscriptCacheCleanModuleArtifacts& Module
		: Publication->Modules)
	{
		Decision.ModuleKeys.Add(Module.ModuleKey);
	}
	RecordDecisionLocked(MoveTemp(Decision));

	Result.Publication = MoveTemp(Publication);
	return Result;
}

TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
	ESPMode::ThreadSafe>
FAngelscriptCacheService::GetLatestSuccessfulPublication() const
{
	FScopeLock Lock(&Gate);
	return LatestSuccessfulPublication;
}

FAngelscriptCacheLifecyclePublications
FAngelscriptCacheService::GetLifecyclePublications() const
{
	FScopeLock Lock(&Gate);
	FAngelscriptCacheLifecyclePublications Result;
	Result.Current = CurrentSuccessfulPublication;
	Result.PendingColdStart = PendingColdStartPublication;
	Result.LatestSuccessful = LatestSuccessfulPublication;
	return Result;
}

FAngelscriptCacheDiagnosticSnapshot
FAngelscriptCacheService::CaptureDiagnosticSnapshot() const
{
	FAngelscriptCacheLifecyclePublications Publications;
	FAngelscriptCacheDecisionTraceSnapshot DecisionTrace;
	FAngelscriptCacheFunctionReuseSummary FunctionReuse;
	EAngelscriptCacheMutationPhase CapturedPhase;
	uint64 CapturedLastTransactionOrdinal = 0;
	{
		FScopeLock Lock(&Gate);
		CapturedPhase = Phase;
		CapturedLastTransactionOrdinal = NextTransactionOrdinal;
		Publications.Current = CurrentSuccessfulPublication;
		Publications.PendingColdStart = PendingColdStartPublication;
		Publications.LatestSuccessful = LatestSuccessfulPublication;
		DecisionTrace = CaptureDecisionTraceLocked();
		FunctionReuse = FunctionReuseSummary;
	}
	FAngelscriptCacheDiagnosticSnapshot Snapshot =
		BuildAngelscriptCacheDiagnosticSnapshot(
		CapturedPhase, CapturedLastTransactionOrdinal, Publications,
		DecisionTrace);
	Snapshot.FunctionReuse = FunctionReuse;
	return Snapshot;
}

void FAngelscriptCacheService::PublishFunctionReuseSummary(
	const FAngelscriptCacheFunctionReuseSummary& Summary)
{
	FScopeLock Lock(&Gate);
	FunctionReuseSummary = Summary;
	FunctionReuseSummary.SchemaVersion =
		FAngelscriptCacheFunctionReuseSummary::CurrentSchemaVersion;
}

void FAngelscriptCacheService::ClearFunctionReuseSummary()
{
	FScopeLock Lock(&Gate);
	FunctionReuseSummary = {};
}

FAngelscriptCacheFunctionReuseSummary
FAngelscriptCacheService::CaptureFunctionReuseSummary() const
{
	FScopeLock Lock(&Gate);
	return FunctionReuseSummary;
}

void FAngelscriptCacheService::ConfigureWriterPolicy(
	const FAngelscriptCachePackPolicy& Policy)
{
	FScopeLock Lock(&Gate);
	WriterPolicy = Policy;
}

FAngelscriptCachePackPolicy
FAngelscriptCacheService::CaptureWriterPolicy() const
{
	FScopeLock Lock(&Gate);
	return WriterPolicy;
}

void FAngelscriptCacheService::ConfigureDecisionTrace(
	const bool bEnabled,
	const uint32 Capacity)
{
	FScopeLock Lock(&Gate);
	const uint32 BoundedCapacity = FMath::Clamp<uint32>(Capacity, 1, 65536);
	if (DecisionTraceCapacity != BoundedCapacity)
	{
		DecisionTraceCapacity = BoundedCapacity;
		DecisionEvents.Reset();
		NextDecisionEventOrdinal = 0;
		EvictedDecisionEventCount = 0;
	}
	bDecisionTraceEnabled = bEnabled;
}

void FAngelscriptCacheService::ClearDecisionTrace()
{
	FScopeLock Lock(&Gate);
	DecisionEvents.Reset();
	NextDecisionEventOrdinal = 0;
	EvictedDecisionEventCount = 0;
}

FAngelscriptCacheDecisionTraceSnapshot
FAngelscriptCacheService::CaptureDecisionTrace() const
{
	FScopeLock Lock(&Gate);
	return CaptureDecisionTraceLocked();
}

void FAngelscriptCacheService::RecordDecisionEvent(
	FAngelscriptCacheDecisionEvent Event)
{
	FScopeLock Lock(&Gate);
	RecordDecisionLocked(MoveTemp(Event));
}

void FAngelscriptCacheService::RecordDecisionLocked(
	FAngelscriptCacheDecisionEvent Event)
{
	if (!bDecisionTraceEnabled || DecisionTraceCapacity == 0)
	{
		return;
	}
	Event.SchemaVersion = FAngelscriptCacheDecisionEvent::CurrentSchemaVersion;
	Event.EventOrdinal = ++NextDecisionEventOrdinal;
	if (Event.Detail.Len() > static_cast<int32>(
			FAngelscriptCacheDecisionEvent::MaxDetailCharacters))
	{
		Event.Detail.LeftInline(
			static_cast<int32>(
				FAngelscriptCacheDecisionEvent::MaxDetailCharacters),
			EAllowShrinking::No);
	}
	Event.ModuleKeys.Sort([](
		const FAngelscriptStableModuleKey& Left,
		const FAngelscriptStableModuleKey& Right)
	{
		return Left.Hash < Right.Hash;
	});
	if (DecisionEvents.Num() == static_cast<int32>(DecisionTraceCapacity))
	{
		DecisionEvents.RemoveAt(0, 1, EAllowShrinking::No);
		++EvictedDecisionEventCount;
	}
	DecisionEvents.Add(MoveTemp(Event));
}

FAngelscriptCacheDecisionTraceSnapshot
FAngelscriptCacheService::CaptureDecisionTraceLocked() const
{
	FAngelscriptCacheDecisionTraceSnapshot Snapshot;
	Snapshot.bEnabled = bDecisionTraceEnabled;
	Snapshot.Capacity = DecisionTraceCapacity;
	Snapshot.NextEventOrdinal = NextDecisionEventOrdinal;
	Snapshot.EvictedEventCount = EvictedDecisionEventCount;
	Snapshot.Events = DecisionEvents;
	return Snapshot;
}

void FAngelscriptCacheService::RecordLifecycleFlushDecisions(
	const FAngelscriptCacheLifecyclePublications& Publications,
	const FAngelscriptCacheLifecycleFlushResult& Result,
	const uint64 ElapsedMicroseconds)
{
	FScopeLock Lock(&Gate);
	auto RecordSlot = [this, ElapsedMicroseconds](
		const TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe>& Publication,
		const FAngelscriptCacheLifecycleFlushSlotResult& Slot)
	{
		if (!Slot.bAttempted || !Publication.IsValid())
		{
			return;
		}

		FAngelscriptCacheDecisionEvent Event;
		Event.TransactionOrdinal = Slot.TransactionOrdinal;
		Event.Stage = EAngelscriptCacheDecisionStage::LifecycleFlush;
		Event.Outcome = Slot.IsSuccess()
			? EAngelscriptCacheDecisionOutcome::Completed
			: EAngelscriptCacheDecisionOutcome::Rejected;
		if (!Slot.Preparation.IsSuccess())
		{
			Event.ReasonDomain =
				EAngelscriptCacheDecisionReasonDomain::CleanCapture;
			Event.ReasonCode = static_cast<uint32>(Slot.Preparation.Error);
		}
		else if (!Slot.Publication.IsSuccess())
		{
			Event.ReasonDomain = EAngelscriptCacheDecisionReasonDomain::Store;
			Event.ReasonCode = static_cast<uint32>(Slot.Publication.Error);
		}
		else
		{
			Event.ReasonDomain =
				EAngelscriptCacheDecisionReasonDomain::LifecycleFlush;
			Event.ReasonCode = static_cast<uint32>(
				EAngelscriptCacheLifecycleFlushError::None);
		}
		Event.Profile = Publication->Profile;
		Event.SourceSnapshot = Publication->SourceSnapshot;
		if (!Slot.GenerationId.IsZero())
		{
			Event.CurrentCoordinate = Slot.GenerationId;
		}
		Event.PrimaryCount = 1;
		Event.SecondaryCount = Slot.IsSuccess() ? 1 : 0;
		Event.ElapsedMicroseconds = ElapsedMicroseconds;
		Event.ModuleKeys.Reserve(Publication->Modules.Num());
		for (const FAngelscriptCacheCleanModuleArtifacts& Module
			: Publication->Modules)
		{
			Event.ModuleKeys.Add(Module.ModuleKey);
		}
		RecordDecisionLocked(MoveTemp(Event));
	};

	const uint64 EventOrdinalBefore = NextDecisionEventOrdinal;
	RecordSlot(Publications.Current, Result.Current);
	RecordSlot(Publications.PendingColdStart, Result.PendingColdStart);
	if (NextDecisionEventOrdinal != EventOrdinalBefore)
	{
		return;
	}

	// Invalid/no-publication/timeout outcomes still need one bounded operation
	// event so a failed Flush never disappears merely because no slot committed.
	FAngelscriptCacheDecisionEvent Event;
	Event.Stage = EAngelscriptCacheDecisionStage::LifecycleFlush;
	Event.Outcome = Result.IsSuccess()
		? EAngelscriptCacheDecisionOutcome::Completed
		: Result.Error == EAngelscriptCacheLifecycleFlushError::TimedOut
			? EAngelscriptCacheDecisionOutcome::Deferred
			: EAngelscriptCacheDecisionOutcome::Rejected;
	Event.ReasonDomain = EAngelscriptCacheDecisionReasonDomain::LifecycleFlush;
	Event.ReasonCode = static_cast<uint32>(Result.Error);
	Event.ElapsedMicroseconds = ElapsedMicroseconds;
	RecordDecisionLocked(MoveTemp(Event));
}

FAngelscriptCacheLifecycleFlushResult
FAngelscriptCacheService::FlushLifecyclePublicationsToStore(
	const FString& RequestedBaseRoot,
	const double TimeoutSeconds)
{
	using namespace AngelscriptCacheService_Private;
	const double StartedSeconds = FPlatformTime::Seconds();
	FAngelscriptCacheLifecycleFlushResult Result;
	if (RequestedBaseRoot.IsEmpty()
		|| !FMath::IsFinite(TimeoutSeconds)
		|| TimeoutSeconds <= 0.0)
	{
		Result.Error = EAngelscriptCacheLifecycleFlushError::InvalidInput;
		Result.Detail = TEXT("Cache lifecycle flush requires a root and a positive finite timeout");
		RecordLifecycleFlushDecisions({}, Result,
			static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
				* 1000000.0));
		return Result;
	}

	const FAngelscriptCacheLifecyclePublications Publications =
		GetLifecyclePublications();
	const FAngelscriptCachePackPolicy PackPolicy = CaptureWriterPolicy();
	const double DeadlineSeconds =
		FPlatformTime::Seconds() + TimeoutSeconds;
	Result = FlushPublicationsToStore(
		Publications,
		PackPolicy,
		RequestedBaseRoot,
		DeadlineSeconds,
		[]() { return false; });
	RecordLifecycleFlushDecisions(Publications, Result,
		static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
			* 1000000.0));
	return Result;
}

FAngelscriptCacheLifecycleFlushResult
FAngelscriptCacheService::BeginEngineShutdownAndFlushToStore(
	const FString& RequestedBaseRoot,
	const double TimeoutSeconds)
{
	using namespace AngelscriptCacheService_Private;
	const double StartedSeconds = FPlatformTime::Seconds();
	FAngelscriptCacheLifecyclePublications Publications;
	FAngelscriptCachePackPolicy PackPolicy;
	{
		FScopeLock Lock(&Gate);
		Phase = EAngelscriptCacheMutationPhase::ShuttingDown;
		Publications.Current = CurrentSuccessfulPublication;
		Publications.PendingColdStart = PendingColdStartPublication;
		Publications.LatestSuccessful = LatestSuccessfulPublication;
		PackPolicy = WriterPolicy;
	}

	FAngelscriptCacheLifecycleFlushResult ImmediateResult;
	if (RequestedBaseRoot.IsEmpty()
		|| !FMath::IsFinite(TimeoutSeconds)
		|| TimeoutSeconds <= 0.0)
	{
		ImmediateResult.Error =
			EAngelscriptCacheLifecycleFlushError::InvalidInput;
		ImmediateResult.Detail = TEXT("Bounded shutdown flush requires a root and a positive finite timeout");
		RecordLifecycleFlushDecisions(Publications, ImmediateResult,
			static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
				* 1000000.0));
		return ImmediateResult;
	}
	if (!Publications.Current.IsValid()
		&& !Publications.PendingColdStart.IsValid())
	{
		ImmediateResult.Detail =
			TEXT("Bounded shutdown flush had no frozen publication to write");
		RecordLifecycleFlushDecisions(Publications, ImmediateResult,
			static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
				* 1000000.0));
		return ImmediateResult;
	}

	TSharedRef<FBoundedShutdownFlushState, ESPMode::ThreadSafe> State =
		MakeShared<FBoundedShutdownFlushState, ESPMode::ThreadSafe>();
	if (State->Completion == nullptr)
	{
		ImmediateResult.Error = EAngelscriptCacheLifecycleFlushError::
			PlatformStoreUnavailable;
		ImmediateResult.Detail =
			TEXT("Bounded shutdown flush could not allocate a completion event");
		RecordLifecycleFlushDecisions(Publications, ImmediateResult,
			static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
				* 1000000.0));
		return ImmediateResult;
	}

	const double DeadlineSeconds =
		FPlatformTime::Seconds() + TimeoutSeconds;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[State, Publications, PackPolicy, RequestedBaseRoot, DeadlineSeconds]()
		{
			FAngelscriptCacheLifecycleFlushResult WorkerResult =
				FlushPublicationsToStore(
					Publications,
					PackPolicy,
					RequestedBaseRoot,
					DeadlineSeconds,
					[State]()
					{
						return State->bCancellationRequested.Load();
					});
			{
				FScopeLock Lock(&State->ResultGate);
				State->Result = MoveTemp(WorkerResult);
			}
			State->Completion->Trigger();
		});

	const int64 WaitMilliseconds64 = FMath::Clamp<int64>(
		FMath::CeilToInt64(TimeoutSeconds * 1000.0),
		1,
		static_cast<int64>(MAX_uint32));
	if (State->Completion->Wait(static_cast<uint32>(WaitMilliseconds64)))
	{
		FAngelscriptCacheLifecycleFlushResult CompletedResult;
		{
			FScopeLock Lock(&State->ResultGate);
			CompletedResult = State->Result;
		}
		RecordLifecycleFlushDecisions(Publications, CompletedResult,
			static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
				* 1000000.0));
		return CompletedResult;
	}

	State->bCancellationRequested.Store(true);
	if (State->Completion->Wait(0))
	{
		FAngelscriptCacheLifecycleFlushResult CompletedResult;
		{
			FScopeLock Lock(&State->ResultGate);
			CompletedResult = State->Result;
		}
		RecordLifecycleFlushDecisions(Publications, CompletedResult,
			static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
				* 1000000.0));
		return CompletedResult;
	}

	ImmediateResult.Error = EAngelscriptCacheLifecycleFlushError::TimedOut;
	ImmediateResult.Detail = FString::Printf(
		TEXT("Bounded shutdown flush exceeded %.3f seconds; cancellation was requested and the detached worker retains only frozen cache DTOs"),
		TimeoutSeconds);
	RecordLifecycleFlushDecisions(Publications, ImmediateResult,
		static_cast<uint64>((FPlatformTime::Seconds() - StartedSeconds)
			* 1000000.0));
	return ImmediateResult;
}

EAngelscriptCacheMutationPhase
FAngelscriptCacheService::GetMutationPhase() const
{
	FScopeLock Lock(&Gate);
	return Phase;
}
