#pragma once

#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompileReuse.h"
#include "Cache/AngelscriptCacheDecisionTrace.h"
#include "Cache/AngelscriptCacheStore.h"
#include "HAL/CriticalSection.h"

enum class EAngelscriptCacheMutationKind : uint8
{
	Invalid = 0,
	InitialCompile = 1,
	StartupRestore = 2,
	ModuleSwap = 3,
	FreezeSuccessfulCompile = 4,
	RuntimeReload = 5,
	RouteRefresh = 6,
	Shutdown = 7,
};

enum class EAngelscriptCacheMutationPhase : uint8
{
	InitializingAnyThread = 1,
	RuntimeGameThread = 2,
	ShuttingDown = 3,
};

// Ephemeral proof that the current thread owns one service's mutation gate.
// It never enters a cache record, diagnostic correlation key or publication DTO.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheMutationToken final
{
private:
	uint64 ServiceIdentity = 0;
	uint64 Epoch = 0;
	uint32 OwnerThreadId = 0;

	friend class FAngelscriptCacheService;
};

class FAngelscriptCacheService;
struct FAngelscriptCacheDiagnosticSnapshot;

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheMutationGuard final
{
public:
	FAngelscriptCacheMutationGuard() = default;
	~FAngelscriptCacheMutationGuard();

	FAngelscriptCacheMutationGuard(
		const FAngelscriptCacheMutationGuard&) = delete;
	FAngelscriptCacheMutationGuard& operator=(
		const FAngelscriptCacheMutationGuard&) = delete;
	FAngelscriptCacheMutationGuard(FAngelscriptCacheMutationGuard&& Other) noexcept;
	FAngelscriptCacheMutationGuard& operator=(
		FAngelscriptCacheMutationGuard&& Other) noexcept;

	bool IsEntered() const
	{
		return Service != nullptr;
	}

	const FAngelscriptCacheMutationToken& GetToken() const
	{
		return Token;
	}

private:
	FAngelscriptCacheMutationGuard(
		FAngelscriptCacheService& InService,
		const FAngelscriptCacheMutationToken& InToken);
	void Reset();

	FAngelscriptCacheService* Service = nullptr;
	FAngelscriptCacheMutationToken Token;

	friend class FAngelscriptCacheService;
};

enum class EAngelscriptCacheSuccessfulCompileKind : uint8
{
	Invalid = 0,
	Initial = 1,
	SoftReload = 2,
	FullReload = 3,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSuccessfulPublicationInput final
{
	EAngelscriptCacheSuccessfulCompileKind Kind =
		EAngelscriptCacheSuccessfulCompileKind::Invalid;
	EAngelscriptCachePublicationDisposition Disposition =
		EAngelscriptCachePublicationDisposition::Invalid;
	FAngelscriptCacheCompatibilityKey Compatibility;
	FAngelscriptCacheContextKey Context;
	FAngelscriptArtifactProfileKey Profile;
	bool bRestoredFromStore = false;
	FAngelscriptHash256 PersistedGenerationId;
	TArray<FAngelscriptCacheCleanModuleArtifacts> Modules;
};

// Immutable after publication through TSharedPtr<const ...>. Every member owns
// pointer-free semantic coordinates or canonical bytes; no AS/UE object pointer,
// numeric FunctionId or mutable module descriptor crosses this boundary.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSuccessfulPublicationDto final
{
	static constexpr uint32 CurrentSchemaVersion = 2;

	uint32 SchemaVersion = CurrentSchemaVersion;
	uint64 TransactionOrdinal = 0;
	EAngelscriptCacheSuccessfulCompileKind Kind =
		EAngelscriptCacheSuccessfulCompileKind::Invalid;
	EAngelscriptCachePublicationDisposition Disposition =
		EAngelscriptCachePublicationDisposition::Invalid;
	FAngelscriptCacheCompatibilityKey Compatibility;
	FAngelscriptCacheContextKey Context;
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 SourceSnapshot;
	FAngelscriptCacheRecordId SourceIndexRecordId;
	bool bRestoredFromStore = false;
	FAngelscriptHash256 PersistedGenerationId;
	TArray<FAngelscriptCacheCleanModuleArtifacts> Modules;
};

// One lock-consistent view of the lifecycle publication slots. Current is the
// last-good generation aligned with the active Engine state. PendingColdStart
// is a separately compiled candidate that must not replace Current until a
// later successful initial/full transaction. LatestSuccessful is diagnostic
// chronology only and is never an activation authority.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheLifecyclePublications final
{
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> Current;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> PendingColdStart;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> LatestSuccessful;
};

enum class EAngelscriptCacheLifecycleFlushError : uint8
{
	None = 0,
	InvalidInput = 1,
	PlatformStoreUnavailable = 2,
	SlotFailure = 3,
	TimedOut = 4,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheLifecycleFlushSlotResult final
{
	bool bAttempted = false;
	uint64 TransactionOrdinal = 0;
	EAngelscriptCachePublicationDisposition Disposition =
		EAngelscriptCachePublicationDisposition::Invalid;
	FAngelscriptCacheCleanCaptureResult Preparation;
	FAngelscriptCacheStoreResult Publication;
	FAngelscriptHash256 GenerationId;

	bool IsSuccess() const
	{
		return bAttempted
			&& Preparation.IsSuccess()
			&& Publication.IsSuccess()
			&& !GenerationId.IsZero();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheLifecycleFlushResult final
{
	EAngelscriptCacheLifecycleFlushError Error =
		EAngelscriptCacheLifecycleFlushError::None;
	FAngelscriptCacheLifecycleFlushSlotResult Current;
	FAngelscriptCacheLifecycleFlushSlotResult PendingColdStart;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheLifecycleFlushError::None
			&& (!Current.bAttempted || Current.IsSuccess())
			&& (!PendingColdStart.bAttempted
				|| PendingColdStart.IsSuccess());
	}
};

enum class EAngelscriptCacheFreezePublicationError : uint8
{
	None = 0,
	NotMutationOwner = 1,
	ShuttingDown = 2,
	InvalidInput = 3,
	DuplicateModule = 4,
	InconsistentSourceSnapshot = 5,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFreezePublicationResult final
{
	EAngelscriptCacheFreezePublicationError Error =
		EAngelscriptCacheFreezePublicationError::None;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> Publication;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheFreezePublicationError::None
			&& Publication.IsValid();
	}
};

// One instance is owned by one FAngelscriptEngine. This class owns lifecycle
// serialization and the immutable handoff boundary; Store workers and later
// diagnostic frontends consume only frozen DTOs.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheService final
{
public:
	FAngelscriptCacheService();
	~FAngelscriptCacheService();

	FAngelscriptCacheMutationGuard EnterMutation(
		EAngelscriptCacheMutationKind Kind,
		const FAngelscriptCacheMutationToken* ParentToken = nullptr);

	void TransitionToRuntimeGameThread();
	void BeginEngineShutdown();

	FAngelscriptCacheFreezePublicationResult FreezeSuccessfulCompileArtifacts(
		const FAngelscriptCacheMutationToken& Token,
		FAngelscriptCacheSuccessfulPublicationInput Input);

	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> GetLatestSuccessfulPublication() const;
	FAngelscriptCacheLifecyclePublications GetLifecyclePublications() const;
	FAngelscriptCacheDiagnosticSnapshot CaptureDiagnosticSnapshot() const;

	// Live diagnostics are opt-in and bounded. Reconfiguration to a different
	// capacity starts a fresh deterministic journal; disabling preserves the
	// captured events for a later Dump until Clear is requested.
	void ConfigureDecisionTrace(bool bEnabled, uint32 Capacity);
	void ClearDecisionTrace();
	FAngelscriptCacheDecisionTraceSnapshot CaptureDecisionTrace() const;
	void RecordDecisionEvent(FAngelscriptCacheDecisionEvent Event);
	void PublishFunctionReuseSummary(
		const FAngelscriptCacheFunctionReuseSummary& Summary);
	void ClearFunctionReuseSummary();
	FAngelscriptCacheFunctionReuseSummary CaptureFunctionReuseSummary() const;

	// Configured during Engine construction, before startup mutation. The copied
	// physical writer policy is captured with frozen publications so detached
	// shutdown workers never read mutable Engine/settings state.
	void ConfigureWriterPolicy(const FAngelscriptCachePackPolicy& Policy);
	FAngelscriptCachePackPolicy CaptureWriterPolicy() const;

	// Writes only already-frozen, pointer-free successful publications. This
	// blocking core does not discover, preprocess or compile source and does not
	// change lifecycle slots. Shutdown may run it from a bounded worker because
	// the captured DTOs own every byte they expose.
	FAngelscriptCacheLifecycleFlushResult FlushLifecyclePublicationsToStore(
		const FString& RequestedBaseRoot,
		double TimeoutSeconds);

	// Atomically prevents later Engine mutations, snapshots the immutable
	// lifecycle publications, executes the Store work without capturing this
	// Service and waits only for TimeoutSeconds. A timed-out worker owns only its
	// frozen DTOs and cancellation state until its current filesystem call exits.
	FAngelscriptCacheLifecycleFlushResult BeginEngineShutdownAndFlushToStore(
		const FString& RequestedBaseRoot,
		double TimeoutSeconds);

	uint64 GetEphemeralServiceIdentity() const
	{
		return ServiceIdentity;
	}

	EAngelscriptCacheMutationPhase GetMutationPhase() const;

private:
	bool IsCurrentOwnerToken(
		const FAngelscriptCacheMutationToken& Token) const;
	void LeaveMutation(const FAngelscriptCacheMutationToken& Token);
	void RecordDecisionLocked(FAngelscriptCacheDecisionEvent Event);
	void RecordLifecycleFlushDecisions(
		const FAngelscriptCacheLifecyclePublications& Publications,
		const FAngelscriptCacheLifecycleFlushResult& Result,
		uint64 ElapsedMicroseconds);
	FAngelscriptCacheDecisionTraceSnapshot CaptureDecisionTraceLocked() const;

	uint64 ServiceIdentity = 0;
	mutable FCriticalSection Gate;
	EAngelscriptCacheMutationPhase Phase =
		EAngelscriptCacheMutationPhase::InitializingAnyThread;
	uint64 CurrentEpoch = 0;
	uint32 OwnerThreadId = 0;
	uint32 ActiveDepth = 0;
	uint64 NextTransactionOrdinal = 0;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> LatestSuccessfulPublication;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> CurrentSuccessfulPublication;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> PendingColdStartPublication;
	bool bDecisionTraceEnabled = false;
	uint32 DecisionTraceCapacity = 1024;
	uint64 NextDecisionEventOrdinal = 0;
	uint64 EvictedDecisionEventCount = 0;
	TArray<FAngelscriptCacheDecisionEvent> DecisionEvents;
	FAngelscriptCacheFunctionReuseSummary FunctionReuseSummary;
	FAngelscriptCachePackPolicy WriterPolicy;

	friend class FAngelscriptCacheMutationGuard;
};
