#pragma once

#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheService.h"

struct FAngelscriptEngine;

enum class EAngelscriptCacheDiagnosticApiError : uint8
{
	None = 0,
	EngineUnavailable = 1,
	ServiceUnavailable = 2,
	SerializationFailed = 3,
	CacheDisabled = 4,
	PersistenceDisabled = 5,
	RootSelectionFailed = 6,
	InvalidRequest = 7,
	FlushFailed = 8,
	NoMatch = 9,
	StoreUnavailable = 10,
	VerifyFailed = 11,
	CompactionFailed = 12,
	ForceCleanFailed = 13,
	ReportPathInvalid = 14,
	ReportWriteFailed = 15,
};

enum class EAngelscriptCacheDiagnosticGeneration : uint8
{
	Current = 1,
	Previous = 2,
	PendingColdStart = 3,
};

enum class EAngelscriptCacheForceCleanOutcome : uint8
{
	Invalid = 0,
	Applied = 1,
	RequiresFullReload = 2,
	CompileFailed = 3,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticJsonResult final
{
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	FString Json;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& !Json.IsEmpty();
	}
};

// Typed result for the process-level -as-cache-report output boundary. The
// report contains the same pointer-free JSON emitted by the live C++ API and
// consumed by offline Python tooling; it is presentation, never cache authority.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheReportWriteResult final
{
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	FString ResolvedPath;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& !ResolvedPath.IsEmpty();
	}
};

// AND-composed selectors over an already-captured bounded decision journal.
// At least one selector is required. All values are stable or per-journal
// ordinals; process-local pointers and numeric AngelScript FunctionIds are not
// accepted query coordinates.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExplainRequest final
{
	TOptional<uint64> EventOrdinal;
	TOptional<uint64> TransactionOrdinal;
	TOptional<EAngelscriptCacheDecisionStage> Stage;
	TOptional<FAngelscriptStableModuleKey> ModuleKey;
	TOptional<FAngelscriptStableFunctionKey> FunctionKey;
	TOptional<FAngelscriptCacheRecordId> RecordId;

	bool HasSelector() const
	{
		return EventOrdinal.IsSet()
			|| TransactionOrdinal.IsSet()
			|| Stage.IsSet()
			|| ModuleKey.IsSet()
			|| FunctionKey.IsSet()
			|| RecordId.IsSet();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExplainResult final
{
	static constexpr uint32 CurrentSchemaVersion = 1;

	uint32 SchemaVersion = CurrentSchemaVersion;
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	FAngelscriptCacheExplainRequest Request;
	TArray<FAngelscriptCacheDecisionEvent> Events;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& !Events.IsEmpty();
	}
};

// Typed result for an explicit lifecycle flush. This owns only Store/result
// values; no Engine, Service, AS object or UObject pointer crosses the API.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFlushApiResult final
{
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	FAngelscriptCacheStoreResult RootSelection;
	FAngelscriptCacheLifecycleFlushResult Flush;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& Flush.IsSuccess();
	}
};

// Typed result for one persisted Store slot. Shallow verification validates the
// atomic pointer and content-addressed Manifest object; deep verification also
// validates every referenced Pack, record envelope and semantic record graph.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheVerifyApiResult final
{
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	EAngelscriptCacheDiagnosticGeneration Generation =
		EAngelscriptCacheDiagnosticGeneration::Current;
	bool bDeep = false;
	FAngelscriptCacheStoreResult RootSelection;
	FAngelscriptCacheStoreResult Store;
	TOptional<FAngelscriptHash256> GenerationId;
	int32 ManifestModuleCount = 0;
	int32 ManifestRecordCount = 0;
	int32 ReachableRecordCount = 0;
	int32 ReferencedPackCount = 0;
	uint64 StoredBytesRead = 0;
	uint64 DecompressedBytesRead = 0;
	uint64 DecodedBytesRetained = 0;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& Store.IsSuccess()
			&& GenerationId.IsSet();
	}
};

// Typed result for the existing two-phase Store compactor. Authority is derived
// from the Engine's immutable Current publication; callers cannot supply or
// weaken Profile/SourceSnapshot validation.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompactApiResult final
{
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	FAngelscriptCacheStoreResult RootSelection;
	FAngelscriptCacheStoreResult Store;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& Store.IsSuccess()
			&& Store.CommitState ==
				EAngelscriptCacheStoreCommitState::CompactionCommitted;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheForceCleanApiResult final
{
	EAngelscriptCacheDiagnosticApiError Error =
		EAngelscriptCacheDiagnosticApiError::None;
	EAngelscriptCacheForceCleanOutcome Outcome =
		EAngelscriptCacheForceCleanOutcome::Invalid;
	TArray<FString> SelectedModuleNames;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDiagnosticApiError::None
			&& Outcome == EAngelscriptCacheForceCleanOutcome::Applied;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticRecordKindSummary final
{
	EAngelscriptCacheRecordKind Kind =
		static_cast<EAngelscriptCacheRecordKind>(0);
	int32 RecordCount = 0;
	uint64 CanonicalPayloadBytes = 0;
};

// A failed diagnostic decode is observable but never weakens publication. The
// publication itself was already admitted by the production validator; this
// row only prevents the debug frontend from silently omitting an unreadable
// record if a schema/budget regression is introduced later.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticDecodeFailure final
{
	FAngelscriptCacheRecordId RecordId;
	FAngelscriptCacheValidationResult Validation;
};

// FunctionBody without its opaque VM bytes. Stable identity/content/profile,
// source/input digests and semantic dependencies are sufficient to correlate
// persisted functions, current Engine routes and sibling StaticJIT providers.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticFunctionSummary final
{
	FAngelscriptCacheRecordId RecordId;
	FAngelscriptFunctionArtifactIdentity Identity;
	FAngelscriptHash256 ExpectedDeclarationAbi;
	FAngelscriptFunctionSourceDigest FunctionSourceDigest;
	FAngelscriptFunctionInputDigest FunctionInputDigest;
	EAngelscriptCachedFunctionInvocationKind InvocationKind =
		EAngelscriptCachedFunctionInvocationKind::Invalid;
	uint32 VmExecutionCodecVersion = 0;
	uint64 CanonicalExecutionPayloadBytes = 0;
	TArray<FAngelscriptCacheSemanticDependency> ActualDependencies;
	TOptional<FAngelscriptCacheRecordId> DebugSidecar;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticInitializerSummary final
{
	EAngelscriptCachedInitializerKind InitializerKind =
		EAngelscriptCachedInitializerKind::Invalid;
	FAngelscriptStableFunctionKey InitializerKey;
	TOptional<FAngelscriptStableGlobalKey> OwnerGlobal;
	uint32 VmInitializerCodecVersion = 0;
	FAngelscriptHash256 InitializerExecutionHash;
	uint64 CanonicalExecutionPayloadBytes = 0;
};

// Keeps the semantic TypeSchema beside the exact content-addressed record that
// supplied it.  This is what lets an offline dump prove that a live type view
// and one persisted generation refer to the same immutable artifact.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticTypeSummary final
{
	FAngelscriptCacheRecordId RecordId;
	FAngelscriptCachedTypeSchema Schema;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticModuleSummary final
{
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalModuleName;
	FAngelscriptCacheRecordId ModuleSnapshotRecordId;
	TOptional<FAngelscriptCacheRecordId> ModuleInterfaceRecordId;
	TOptional<FAngelscriptCacheRecordId> ModuleStateRecordId;
	FAngelscriptHash256 InterfaceAbi;
	TArray<FAngelscriptCachedDeclaration> Declarations;
	TArray<FAngelscriptCachedImportDeclaration> Imports;
	TArray<FAngelscriptCacheSemanticDependency> InterfaceDependencies;
	TArray<FAngelscriptCacheDiagnosticTypeSummary> Types;
	FAngelscriptArtifactProfileKey StateProfile;
	FAngelscriptHash256 StateInputHash;
	TArray<FAngelscriptCachedGlobalSchema> Globals;
	TArray<FAngelscriptCacheSemanticDependency> StateDependencies;
	TArray<FAngelscriptCacheDiagnosticInitializerSummary> Initializers;
	TArray<FAngelscriptCacheDiagnosticFunctionSummary> Functions;
	TArray<FAngelscriptCacheDiagnosticDecodeFailure> DecodeFailures;
	int32 TotalRecordCount = 0;
	uint64 CanonicalPayloadBytes = 0;
	TArray<FAngelscriptCacheDiagnosticRecordKindSummary> RecordKinds;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticFunctionRoute final
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptFunctionArtifactIdentity Identity;
	FString CanonicalDeclaration;
	EAngelscriptCacheFunctionExecutionRoute SelectedExecutionRoute =
		EAngelscriptCacheFunctionExecutionRoute::Vm;
	bool bHasVerifiedArtifactIdentity = false;
};

struct ANGELSCRIPTRUNTIME_API
	FAngelscriptCacheDiagnosticFunctionRouteSnapshot final
{
	bool bPresent = false;
	uint64 PublicationOrdinal = 0;
	uint32 VmRouteCount = 0;
	uint32 NativeRouteCount = 0;
	TArray<FAngelscriptCacheDiagnosticFunctionRoute> Routes;
};

struct ANGELSCRIPTRUNTIME_API
	FAngelscriptCacheDiagnosticPublicationSummary final
{
	bool bPresent = false;
	uint32 PublicationSchemaVersion = 0;
	uint64 TransactionOrdinal = 0;
	EAngelscriptCacheSuccessfulCompileKind CompileKind =
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
	int32 TotalRecordCount = 0;
	uint64 CanonicalPayloadBytes = 0;
	TArray<FAngelscriptCacheDiagnosticModuleSummary> Modules;
};

// Pointer-free, immutable-by-value diagnostic observation of one Cache service.
// It deliberately omits ephemeral service identity, object addresses and numeric
// AngelScript FunctionIds so the JSON can correlate separate Editor/game runs.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticSnapshot final
{
	static constexpr uint32 CurrentSchemaVersion = 4;

	uint32 SchemaVersion = CurrentSchemaVersion;
	EAngelscriptCacheMutationPhase MutationPhase =
		EAngelscriptCacheMutationPhase::InitializingAnyThread;
	uint64 LastTransactionOrdinal = 0;
	FAngelscriptCacheDiagnosticPublicationSummary Current;
	FAngelscriptCacheDiagnosticPublicationSummary PendingColdStart;
	FAngelscriptCacheDiagnosticPublicationSummary LatestSuccessful;
	FAngelscriptCacheDiagnosticFunctionRouteSnapshot FunctionRoutes;
	FAngelscriptCacheFunctionReuseSummary FunctionReuse;
	FAngelscriptCacheDecisionTraceSnapshot DecisionTrace;
};

// Copies only persistent stable coordinates and immutable route decisions from
// the live Engine publication. AS pointers and numeric FunctionIds are omitted.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticFunctionRouteSnapshot
BuildAngelscriptCacheDiagnosticFunctionRoutes(
	const FAngelscriptCacheFunctionRouteSnapshot& Routes);

// Constructs summaries only from immutable publication DTOs. Callers that own a
// service should prefer FAngelscriptCacheService::CaptureDiagnosticSnapshot(),
// which captures phase, ordinal and all three slots under one lock.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticSnapshot
BuildAngelscriptCacheDiagnosticSnapshot(
	EAngelscriptCacheMutationPhase MutationPhase,
	uint64 LastTransactionOrdinal,
	const FAngelscriptCacheLifecyclePublications& Publications,
	const FAngelscriptCacheDecisionTraceSnapshot& DecisionTrace);

// Emits a deterministic condensed JSON object. All full-width hashes are lower-
// case hexadecimal strings; uint64 ordinals/byte counts are decimal strings so
// JSON consumers cannot lose precision through IEEE-754 number conversion.
ANGELSCRIPTRUNTIME_API bool SerializeAngelscriptCacheDiagnosticSnapshotJson(
	const FAngelscriptCacheDiagnosticSnapshot& Snapshot,
	FString& OutJson);

// Filters and deterministically orders immutable events already present in one
// bounded journal. It never reads source, recompiles, or mutates cache state.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheExplainResult
ExplainAngelscriptCacheDecisions(
	const FAngelscriptCacheDecisionTraceSnapshot& Trace,
	const FAngelscriptCacheExplainRequest& Request);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheExplainResult
ExplainAngelscriptCacheDecisions(
	const FAngelscriptEngine* Engine,
	const FAngelscriptCacheExplainRequest& Request);

ANGELSCRIPTRUNTIME_API bool SerializeAngelscriptCacheExplainResultJson(
	const FAngelscriptCacheExplainResult& Result,
	FString& OutJson);

// Captures the same stable schema used by Blueprint, console and offline Python
// correlation. Passing an explicit Engine keeps multi-engine callers and tests
// deterministic; nullptr is a typed error and never falls back implicitly.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticJsonResult
CaptureAngelscriptCacheDiagnosticJson(const FAngelscriptEngine* Engine);

// Convenience boundary for Editor/game tooling. It resolves the active scoped
// Engine first, then the initialized primary Engine when one exists.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheDiagnosticJsonResult
CaptureCurrentAngelscriptCacheDiagnosticJson();

// Writes one deterministic process-session report to an explicitly requested
// absolute .json path. Directory creation is allowed, but relative paths and
// non-JSON targets are rejected. This does not flush or mutate Cache V2.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheReportWriteResult
WriteAngelscriptCacheDiagnosticJsonReport(
	const FAngelscriptEngine* Engine,
	const FString& RequestedPath);

// Resolves the production Cache V2 base root for one Engine from its explicit
// config override, process command line, or Saved default. Shutdown and public
// controls share this helper so they cannot silently target different stores.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheStoreResult
ResolveAngelscriptCacheRequestedBaseRootForEngine(
	const FAngelscriptEngine& Engine,
	FString& OutRequestedBaseRoot);

// Commits this Engine's already-frozen Current/PendingColdStart publications
// through the same Cache V2 root policy used at shutdown. A zero timeout uses
// UAngelscriptCacheSettings::ShutdownFlushTimeoutSeconds. It never discovers,
// preprocesses or compiles source.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheFlushApiResult
FlushAngelscriptCacheToStore(
	const FAngelscriptEngine* Engine,
	double TimeoutSeconds = 0.0);

// Current-Engine convenience boundary used by console/Blueprint frontends.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheFlushApiResult
FlushCurrentAngelscriptCacheToStore(double TimeoutSeconds = 0.0);

// Verifies one persisted generation slot in the namespace selected by this
// Engine's immutable publication coordinates. A zero timeout uses the same
// bounded setting as shutdown flush.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheVerifyApiResult
VerifyAngelscriptCacheStore(
	const FAngelscriptEngine* Engine,
	EAngelscriptCacheDiagnosticGeneration Generation,
	bool bDeep,
	double TimeoutSeconds = 0.0);

// Compacts the namespace selected by this Engine. The Store implementation
// keeps pinned readers safe, switches rewritten roots before sweeping and never
// silently deletes the only selected last-good generation.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompactApiResult
CompactAngelscriptCacheStoreForEngine(
	const FAngelscriptEngine* Engine,
	double TimeoutSeconds = 0.0);

// Recompiles either every active script module (empty selector) or one module
// selected by canonical name/full StableModuleKey. The normal preprocessor,
// forced-clean compile policy, activation and last-good rollback remain the sole
// authorities; this API never edits persisted records directly.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheForceCleanApiResult
ForceCleanAngelscriptCache(
	FAngelscriptEngine* Engine,
	FString ModuleSelector);
