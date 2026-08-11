#pragma once

#include "Cache/AngelscriptCacheTypes.h"

enum class EAngelscriptCacheDecisionStage : uint8
{
	Invalid = 0,
	StartupSelection = 1,
	StartupRestore = 2,
	FunctionLookup = 3,
	DependencyPropagation = 4,
	SuccessfulPublication = 5,
	LifecycleFlush = 6,
	StableRoute = 7,
	RuntimeReload = 8,
};

enum class EAngelscriptCacheDecisionOutcome : uint8
{
	Invalid = 0,
	Restored = 1,
	Miss = 2,
	Rejected = 3,
	NotCacheable = 4,
	Compiled = 5,
	Reused = 6,
	Published = 7,
	Deferred = 8,
	RolledBack = 9,
	Completed = 10,
};

// ReasonCode is interpreted only within this typed domain. Existing Runtime
// error/reason enum values are copied numerically rather than reformatted into
// an unstable human string authority.
enum class EAngelscriptCacheDecisionReasonDomain : uint8
{
	None = 0,
	ExactStartup = 1,
	Validation = 2,
	FunctionLookup = 3,
	DependencyMiss = 4,
	FreezePublication = 5,
	LifecycleFlush = 6,
	Store = 7,
	StableRoute = 8,
	RuntimeReload = 9,
	CleanCapture = 10,
};

// Pointer-free, immutable-by-value event. All identities remain full stable
// coordinates. Numeric AngelScript FunctionIds and object addresses are
// intentionally absent.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDecisionEvent final
{
	static constexpr uint32 CurrentSchemaVersion = 2;
	static constexpr uint32 MaxDetailCharacters = 1024;

	uint32 SchemaVersion = CurrentSchemaVersion;
	uint64 EventOrdinal = 0;
	uint64 TransactionOrdinal = 0;
	EAngelscriptCacheDecisionStage Stage =
		EAngelscriptCacheDecisionStage::Invalid;
	EAngelscriptCacheDecisionOutcome Outcome =
		EAngelscriptCacheDecisionOutcome::Invalid;
	EAngelscriptCacheDecisionReasonDomain ReasonDomain =
		EAngelscriptCacheDecisionReasonDomain::None;
	uint32 ReasonCode = 0;
	TArray<FAngelscriptStableModuleKey> ModuleKeys;
	TOptional<FAngelscriptStableFunctionKey> FunctionKey;
	TOptional<FAngelscriptCacheRecordId> RecordId;
	TOptional<FAngelscriptHash256> ExpectedCoordinate;
	TOptional<FAngelscriptHash256> CurrentCoordinate;
	TOptional<FAngelscriptCacheValidationResult> Validation;
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 SourceSnapshot;
	uint32 PrimaryCount = 0;
	uint32 SecondaryCount = 0;
	uint64 ElapsedMicroseconds = 0;
	// Human context is deliberately bounded by the journal owner. Typed enums,
	// stable keys and validation coordinates above remain the query authority.
	FString Detail;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDecisionTraceSnapshot final
{
	static constexpr uint32 CurrentSchemaVersion = 1;

	uint32 SchemaVersion = CurrentSchemaVersion;
	bool bEnabled = false;
	uint32 Capacity = 0;
	uint64 NextEventOrdinal = 0;
	uint64 EvictedEventCount = 0;
	TArray<FAngelscriptCacheDecisionEvent> Events;
};
