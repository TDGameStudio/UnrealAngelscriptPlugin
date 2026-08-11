#pragma once

#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSourceDiscovery.h"

struct FAngelscriptEngine;
class asIScriptFunction;

enum class EAngelscriptCacheRestoreError : uint8
{
	None = 0,
	InvalidInput = 1,
	MissingModuleSnapshot = 2,
	GraphValidationFailed = 3,
	UnsupportedRecordShape = 4,
	VmRestoreFailed = 5,
	ActivationFailed = 6,
	CurrentSourceProjectionMismatch = 7,
};

enum class EAngelscriptCacheRestoreStage : uint8
{
	None = 0,
	SelectModule = 1,
	ValidateGraph = 2,
	ReconstructDescriptor = 3,
	MaterializeTypes = 4,
	RestoreFunctions = 5,
	FinalizeModule = 6,
	ActivateModule = 7,
};

enum class EAngelscriptCacheRestoreFaultPoint : uint8
{
	AfterModulePrepared = 1,
};

// Optional deterministic transaction-local seam used to prove that staging a
// later module cannot publish an earlier module. The restorer never retains this
// pointer and production callers omit it.
class ANGELSCRIPTRUNTIME_API IAngelscriptCacheRestoreFaultInjector
{
public:
	virtual ~IAngelscriptCacheRestoreFaultInjector() = default;

	virtual bool ShouldStopAt(
		EAngelscriptCacheRestoreFaultPoint Point,
		uint32 ModuleOrdinal,
		const FAngelscriptStableModuleKey& ModuleKey) = 0;
};

enum class EAngelscriptCacheFunctionExecutionRoute : uint8
{
	Vm = 1,
	Native = 2,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheLiveFunctionRoute
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptFunctionArtifactIdentity Identity;
	FString CanonicalDeclaration;
	asIScriptFunction* Function = nullptr;
	int32 NumericFunctionId = -1;
	EAngelscriptCacheFunctionExecutionRoute SelectedExecutionRoute =
		EAngelscriptCacheFunctionExecutionRoute::Vm;
	bool bHasVerifiedArtifactIdentity = false;
};

// One Engine-session publication. Entries are immutable after publication and
// sorted by the full StableFunctionKey. Function pointers and numeric ids are
// intentionally transient; only StableFunctionKey/artifact identity may cross
// process or persistence boundaries.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFunctionRouteSnapshot
{
	uint64 PublicationOrdinal = 0;
	uint32 VmRouteCount = 0;
	uint32 NativeRouteCount = 0;
	TArray<FAngelscriptCacheLiveFunctionRoute> FunctionRoutes;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRestoreResult
{
	EAngelscriptCacheRestoreError Error = EAngelscriptCacheRestoreError::None;
	EAngelscriptCacheRestoreStage Stage = EAngelscriptCacheRestoreStage::None;
	TOptional<FAngelscriptCacheValidationResult> Validation;
	uint32 RestoredModuleCount = 0;
	uint32 RestoredTypeCount = 0;
	uint32 RestoredFunctionCount = 0;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheRestoreError::None;
	}
};

// Prepares every selected module off the active Engine maps and commits the
// complete set through one ClassGenerator/SwapIn/route publication. Any failure
// before commit discards all staging modules and reports zero restored modules.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheRestoreResult
RestoreAngelscriptCacheModules(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector = nullptr);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheRestoreResult
RestoreAngelscriptCacheModules(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
	TConstArrayView<FAngelscriptCacheCurrentSourceProjection> CurrentSources,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector = nullptr);

// Restores one complete graph-validated Cache V2 ModuleSnapshot into a staging
// module owned by TargetEngine, then publishes the VM module, plugin descriptor
// indexes and stable-function routes together. V1 initially admits the clean-
// capture enum plus self-contained primitive-global-function vertical.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheRestoreResult
RestoreAngelscriptCacheModule(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector = nullptr);

// Production exact-start overload. CurrentSources are the bounded transient
// projection from the same discovery attempt that validated the generation.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheRestoreResult
RestoreAngelscriptCacheModule(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptStableModuleKey& ModuleKey,
	const TConstArrayView<FAngelscriptCacheCurrentSourceProjection> CurrentSources,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector = nullptr);
