#pragma once

#include "Cache/AngelscriptCacheManifestPack.h"

class IAngelscriptCacheStorageCodec;

enum class EAngelscriptCacheStoreCommitState : uint8
{
	NotStarted = 0,
	NotCommitted = 1,
	CurrentCommitted = 2,
	PendingCommitted = 3,
	CompactionCommitted = 4,
};

enum class EAngelscriptCacheStoreStage : uint8
{
	None = 0,
	RootValidation = 1,
	LockAcquisition = 2,
	TempCleanup = 3,
	Rebase = 4,
	PackTemp = 5,
	PackFinal = 6,
	ManifestTemp = 7,
	ManifestFinal = 8,
	PreviousPointer = 9,
	CurrentPointer = 10,
	PendingPointer = 11,
	SessionPin = 12,
	CandidateValidation = 13,
	CompactionRewrite = 14,
	CompactionSwitch = 15,
	CompactionSweep = 16,
};

enum class EAngelscriptCacheStoreError : uint8
{
	None = 0,
	InvalidRoot = 1,
	PathEscapesRoot = 2,
	UnsupportedPlatformAtomicity = 3,
	LockTimeout = 4,
	Cancelled = 5,
	OpenFailed = 6,
	ReadFailed = 7,
	WriteFailed = 8,
	FlushFailed = 9,
	RenameFailed = 10,
	AtomicReplaceFailed = 11,
	ImmutableObjectCollisionOrCorruption = 12,
	PointerInvalid = 13,
	ManifestMissing = 14,
	PackMissing = 15,
	NeedsSourceRevalidation = 16,
	RebaseSemanticConflict = 17,
	PointerRemoveFailed = 18,
	DeleteDeferred = 19,
	ContentValidationFailed = 20,
	DirectorySyncFailed = 21,
	FaultInjected = 22,
};

enum class EAngelscriptCacheStorePathCategory : uint8
{
	None = 0,
	Root = 1,
	Pack = 2,
	Manifest = 3,
	CurrentPointer = 4,
	PreviousPointer = 5,
	PendingColdStartPointer = 6,
};

enum class EAngelscriptCachePointerKind : uint8
{
	Invalid = 0,
	Current = 1,
	Previous = 2,
	PendingColdStart = 3,
};

enum class EAngelscriptCachePublicationDisposition : uint8
{
	Invalid = 0,
	Current = 1,
	PendingColdStart = 2,
};

// Exact transaction-local process-stop boundaries. These values are not
// persisted and do not participate in Pack, Manifest, Generation, or pointer
// identity. A normal publication supplies no injector.
enum class EAngelscriptCacheStoreFaultPoint : uint8
{
	Invalid = 0,
	BeforePackTempWrite = 1,
	AfterPackTempFlush = 2,
	AfterPackRename = 3,
	AfterManifestTempFlush = 4,
	AfterManifestRename = 5,
	AfterPointerTempsFlush = 6,
	BeforePreviousReplace = 7,
	AfterPreviousReplace = 8,
	BeforeCurrentReplace = 9,
	AfterCurrentReplace = 10,
	BeforePendingReplace = 11,
	AfterPendingReplace = 12,
};

enum class EAngelscriptCacheRebaseAction : uint8
{
	Invalid = 0,
	ProceedFromObservedBase = 1,
	AlreadySelected = 2,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRebasePlan
{
	EAngelscriptCacheRebaseAction Action = EAngelscriptCacheRebaseAction::Invalid;
	TOptional<FAngelscriptHash256> SelectedGenerationId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachePointerValue
{
	EAngelscriptCachePointerKind Kind = EAngelscriptCachePointerKind::Invalid;
	FAngelscriptHash256 GenerationId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheReadSelection
{
	FAngelscriptCacheCompatibilityKey Compatibility;
	FAngelscriptCacheContextKey Context;
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 SourceSnapshot;
	// Startup does not know the authoritative dependency-expanded snapshot until
	// it has opened a persisted SourceIndex and re-observed its inputs. Candidate
	// mode still requires exact Compatibility/Context/Profile and complete graph
	// validation; the exact-start coordinator remains the sole source authority.
	bool bRequireSourceSnapshotMatch = true;
	bool bAllowPendingColdStart = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompactionAuthority
{
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 SourceSnapshot;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult
{
	EAngelscriptCacheStoreError Error = EAngelscriptCacheStoreError::None;
	EAngelscriptCacheStoreStage Stage = EAngelscriptCacheStoreStage::None;
	EAngelscriptCacheStoreCommitState CommitState = EAngelscriptCacheStoreCommitState::NotStarted;
	TOptional<FAngelscriptCacheValidationResult> ContentValidation;
	TOptional<FAngelscriptHash256> GenerationBefore;
	TOptional<FAngelscriptHash256> GenerationAfter;
	EAngelscriptCacheStorePathCategory PathCategory = EAngelscriptCacheStorePathCategory::None;
	TOptional<int64> PlatformErrorCode;

	static FAngelscriptCacheStoreResult Success();
	static FAngelscriptCacheStoreResult Failure(
		EAngelscriptCacheStoreError Error,
		EAngelscriptCacheStoreStage Stage,
		EAngelscriptCacheStorePathCategory PathCategory = EAngelscriptCacheStorePathCategory::None);

	bool IsSuccess() const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCanonicalCacheRoot
{
	// Stable absolute spelling used for filesystem operations.
	FString AbsolutePath;

	// Canonical alias/case-normalized spelling used for containment and lock identity.
	FString IdentityPath;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRootSelectionInputs
{
	FString ProjectSavedDirectory;
	FString LaunchWorkingDirectory;
	TOptional<FString> Override;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheWriterToken
{
public:
	static TOptional<FAngelscriptCacheWriterToken> TryParse(FStringView Token);

	const FString& ToString() const;

private:
	explicit FAngelscriptCacheWriterToken(FString&& InValue);

	FString Value;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheStorePaths
{
	FString BaseRoot;
	FString BaseRootIdentity;
	FString NamespaceRoot;
	FString NamespaceIdentity;
	FString PacksDirectory;
	FString GenerationsDirectory;
	FString CurrentPointer;
	FString PreviousPointer;
	FString PendingColdStartPointer;

	FString BuildPackPath(const FAngelscriptHash256& PackId) const;
	FString BuildManifestPath(const FAngelscriptHash256& GenerationId) const;
	FString BuildPackTempPath(
		const FAngelscriptHash256& PackId,
		const FAngelscriptCacheWriterToken& WriterToken) const;
	FString BuildManifestTempPath(
		const FAngelscriptHash256& GenerationId,
		const FAngelscriptCacheWriterToken& WriterToken) const;
	FString BuildCurrentPointerTempPath(
		const FAngelscriptCacheWriterToken& WriterToken) const;
	FString BuildPreviousPointerTempPath(
		const FAngelscriptCacheWriterToken& WriterToken) const;
	FString BuildPendingColdStartPointerTempPath(
		const FAngelscriptCacheWriterToken& WriterToken) const;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheNamespaceLockHandle
{
public:
	virtual ~IAngelscriptCacheNamespaceLockHandle() = default;
};

// Optional deterministic crash-invariant seam. Implementations must be owned by
// one publication transaction; Store never retains this pointer or publishes it
// through global settings/state. Returning true stops at the exact named point.
class ANGELSCRIPTRUNTIME_API IAngelscriptCacheStoreFaultInjector
{
public:
	virtual ~IAngelscriptCacheStoreFaultInjector() = default;

	virtual bool ShouldStopAt(EAngelscriptCacheStoreFaultPoint Point) = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheNamespaceLockOps
{
public:
	virtual ~IAngelscriptCacheNamespaceLockOps() = default;

	virtual double MonotonicSeconds() const = 0;
	virtual TUniquePtr<IAngelscriptCacheNamespaceLockHandle> TryAcquire(
		const FString& LockName,
		FTimespan WaitSlice) = 0;
};

// Returns null on targets that do not provide the required cross-process mutex.
ANGELSCRIPTRUNTIME_API TUniquePtr<IAngelscriptCacheNamespaceLockOps>
	CreateAngelscriptCacheNamespaceLockOps();

// One immutable file opened by a read session while the namespace lock is held.
// The handle remains bound to that physical object even if its path is later
// unlinked or reused. Session validation reads through this handle only.
class ANGELSCRIPTRUNTIME_API IAngelscriptCachePinnedFileHandle
{
public:
	virtual ~IAngelscriptCachePinnedFileHandle() = default;

	virtual uint64 GetSize() const = 0;
	virtual FAngelscriptCacheStoreResult ReadAll(TArray<uint8>& OutBytes) = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheAtomicFileOps
{
public:
	virtual ~IAngelscriptCacheAtomicFileOps() = default;

	virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
		const FString& RequestedBaseRoot,
		FAngelscriptCanonicalCacheRoot& OutRoot) = 0;

	// Idempotently creates this exact validated directory tree. Store control
	// flow supplies only Base/Namespace/Packs/Generations paths, never a glob.
	virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
		const FString& DirectoryPath) = 0;

	virtual FAngelscriptCacheStoreResult WriteFlushClose(
		const FString& TempPath,
		TConstArrayView<uint8> Bytes) = 0;

	virtual FAngelscriptCacheStoreResult ReopenReadAll(
		const FString& Path,
		uint64 MaxBytes,
		TArray<uint8>& OutBytes) = 0;

	// Opens and retains the exact immutable object currently selected by Path.
	// Implementations used by Cache V2 read sessions must allow later unlink when
	// the platform supports delete sharing. Existing writer-only test seams may
	// keep the default unsupported result until they exercise read sessions.
	virtual FAngelscriptCacheStoreResult OpenReadPinned(
		const FString& Path,
		uint64 MaxBytes,
		TUniquePtr<IAngelscriptCachePinnedFileHandle>& OutHandle)
	{
		OutHandle.Reset();
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
			EAngelscriptCacheStoreStage::SessionPin);
	}

	// Enumerates only direct non-directory child basenames beneath this exact
	// caller-validated directory. The caller never supplies a glob or recurses.
	virtual FAngelscriptCacheStoreResult EnumerateDirectFileNames(
		const FString& DirectoryPath,
		TArray<FString>& OutFileNames) = 0;

	// Destination must not exist and must never replace an immutable final object.
	virtual FAngelscriptCacheStoreResult RenameNewImmutable(
		const FString& TempPath,
		const FString& FinalPath) = 0;

	// Success guarantees either the complete old pointer or complete new pointer is visible.
	virtual FAngelscriptCacheStoreResult AtomicInstallOrReplacePointer(
		const FString& TempPath,
		const FString& PointerPath) = 0;

	// Success guarantees either the complete old pointer or absence is visible.
	virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
		const FString& PointerPath) = 0;

	// Removes only an exact same-directory temp constructed for this writer attempt.
	virtual FAngelscriptCacheStoreResult RemoveOwnTemp(const FString& TempPath) = 0;

	// Removes only a direct-child final Pack or Manifest whose strict content-
	// addressed basename was already validated by the locked compactor.
	virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
		const FString& FinalPath) = 0;

	virtual FAngelscriptCacheStoreResult SyncDirectory(const FString& DirectoryPath) = 0;
	virtual bool SupportsSharedAtomicCacheStore() const = 0;
};

class FAngelscriptCacheReadSession;
struct FAngelscriptCacheReadSessionFactory;

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult
OpenBestAngelscriptCacheReadSession(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptCacheReadSelection& Selection,
	const FAngelscriptCacheReadLimits& Limits,
	double LockDeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	IAngelscriptCacheAtomicFileOps& FileOps,
	TUniquePtr<FAngelscriptCacheReadSession>& OutSession);

// Explicit startup-external compaction. Authority must contain the current
// nonzero Profile and SourceSnapshot before the function may acquire the
// namespace lock or mutate the Store.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult CompactAngelscriptCacheStore(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptCacheCompactionAuthority& Authority,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCachePackPolicy& PackPolicy,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	double LockDeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	IAngelscriptCacheAtomicFileOps& FileOps);

// One immutable, fully validated generation selected from
// Current -> Previous -> optionally PendingColdStart. The session owns the one
// cumulative attempt Budget plus every physical Manifest/Pack handle pinned
// before the namespace lock was released.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheReadSession final
{
public:
	~FAngelscriptCacheReadSession();

	FAngelscriptCacheReadSession(const FAngelscriptCacheReadSession&) = delete;
	FAngelscriptCacheReadSession& operator=(
		const FAngelscriptCacheReadSession&) = delete;

	EAngelscriptCachePointerKind GetPointerKind() const;
	const FAngelscriptHash256& GetGenerationId() const;
	const FAngelscriptValidatedGeneration& GetGeneration() const;
	const FAngelscriptCacheReadBudget& GetBudget() const;
	int32 GetPinnedPackCount() const;

private:
	friend struct FAngelscriptCacheReadSessionFactory;

	FAngelscriptCacheReadSession(
		EAngelscriptCachePointerKind InPointerKind,
		const FAngelscriptHash256& InGenerationId,
		TUniquePtr<FAngelscriptCacheReadBudget>&& InBudget,
		FAngelscriptValidatedGeneration&& InGeneration,
		TArray<uint8>&& InManifestBytes,
		TUniquePtr<IAngelscriptCachePinnedFileHandle>&& InManifestHandle,
		TArray<TUniquePtr<IAngelscriptCachePinnedFileHandle>>&& InPackHandles);

	EAngelscriptCachePointerKind PointerKind =
		EAngelscriptCachePointerKind::Invalid;
	FAngelscriptHash256 GenerationId;
	TUniquePtr<FAngelscriptCacheReadBudget> Budget;
	FAngelscriptValidatedGeneration Generation;
	TArray<uint8> ManifestBytes;
	TUniquePtr<IAngelscriptCachePinnedFileHandle> ManifestHandle;
	TArray<TUniquePtr<IAngelscriptCachePinnedFileHandle>> PackHandles;
};

// Returns the production platform implementation, or null when this target has
// no implementation that can satisfy the shared atomic-store contract.
ANGELSCRIPTRUNTIME_API TUniquePtr<IAngelscriptCacheAtomicFileOps>
	CreateAngelscriptCacheAtomicFileOps();

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult BuildAngelscriptCacheStorePaths(
	const FString& RequestedBaseRoot,
	const FAngelscriptCacheCompatibilityKey& Compatibility,
	const FAngelscriptCacheContextKey& Context,
	IAngelscriptCacheAtomicFileOps& FileOps,
	FAngelscriptCacheStorePaths& OutPaths);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult ResolveAngelscriptCacheRequestedBaseRoot(
	const FAngelscriptCacheRootSelectionInputs& Inputs,
	FString& OutRequestedBaseRoot);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult EnsureAngelscriptCacheStoreDirectories(
	const FAngelscriptCacheStorePaths& Paths,
	IAngelscriptCacheAtomicFileOps& FileOps);

// The caller must own NamespaceLock. Only exact direct-child Pack, Manifest and
// pointer temp names with a canonical WriterToken are removed. Enumeration is
// completed for all three directories before the first deletion.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult
CleanupAngelscriptCacheStaleTempsUnderLock(
	const FAngelscriptCacheStorePaths& Paths,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult BuildAngelscriptCacheNamespaceLockName(
	const FAngelscriptCacheStorePaths& Paths,
	FString& OutLockName);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult AcquireAngelscriptCacheNamespaceLock(
	const FAngelscriptCacheStorePaths& Paths,
	double DeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	TUniquePtr<IAngelscriptCacheNamespaceLockHandle>& OutLock);

// ActualManifest is present exactly when ActualGenerationId is present and has
// already passed complete manifest/pack validation under the namespace lock.
// Physical Pack locations are deliberately excluded from semantic equality.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult EvaluateAngelscriptCacheRebase(
	const TOptional<FAngelscriptHash256>& ObservedBaseGenerationId,
	const TOptional<FAngelscriptHash256>& ActualGenerationId,
	const FAngelscriptCacheGenerationManifest* ActualManifest,
	const FAngelscriptCacheGenerationManifest& PreparedManifest,
	FAngelscriptCacheRebasePlan& OutPlan);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult EncodeAngelscriptCachePointer(
	const FAngelscriptCachePointerValue& Value,
	TArray<uint8>& OutBytes);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult DecodeAngelscriptCachePointer(
	TConstArrayView<uint8> Bytes,
	EAngelscriptCachePointerKind ExpectedKind,
	FAngelscriptCachePointerValue& OutValue);

// A physically missing slot is successful absence. A present malformed slot is
// PointerInvalid, and OutGenerationId is empty on every non-success result.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult ReadAngelscriptCachePointerSlot(
	const FAngelscriptCacheStorePaths& Paths,
	EAngelscriptCachePointerKind Kind,
	IAngelscriptCacheAtomicFileOps& FileOps,
	TOptional<FAngelscriptHash256>& OutGenerationId);

// The caller must keep NamespaceLock owned on the current thread throughout
// this call. The selected manifest and every distinct referenced Pack are read
// once and passed through the sole complete generation validator. Outputs are
// published only after the entire generation succeeds.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult
ReadAndValidateAngelscriptCacheGenerationUnderLock(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptHash256& GenerationId,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps,
	TOptional<FAngelscriptValidatedGeneration>& OutGeneration);

// Publishes one already prepared generation as a single writer transaction.
// Preparation is immutable before this call. The function acquires the
// namespace lock, rereads and fully validates the target slot, rebases, installs
// only referenced immutable objects, then delegates the sole pointer commit.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult
PublishAngelscriptCacheGeneration(
	const FAngelscriptCacheStorePaths& Paths,
	EAngelscriptCachePublicationDisposition Disposition,
	const TOptional<FAngelscriptHash256>& ObservedGenerationId,
	TConstArrayView<FAngelscriptEncodedPack> NewPacks,
	const FAngelscriptCacheGenerationManifest& PreparedManifest,
	const FAngelscriptEncodedCacheGenerationManifest& EncodedManifest,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	double LockDeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector = nullptr);

// The caller must keep NamespaceLock owned on the current thread throughout
// this call. ValidatedOldCurrent is supplied only after locked generation
// reread/revalidation; a merely decoded old pointer is insufficient.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult PublishAngelscriptCachePointers(
	const FAngelscriptCacheStorePaths& Paths,
	EAngelscriptCachePublicationDisposition Disposition,
	const FAngelscriptHash256& NewGenerationId,
	const TOptional<FAngelscriptHash256>& ValidatedOldCurrent,
	const FAngelscriptCacheWriterToken& WriterToken,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector = nullptr);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult PutAngelscriptCachePackIfAbsent(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptHash256& ExpectedPackId,
	TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector = nullptr);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult PutAngelscriptCacheManifestIfAbsent(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptHash256& ExpectedGenerationId,
	TConstArrayView<uint8> CompleteManifestBytes,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector = nullptr);
