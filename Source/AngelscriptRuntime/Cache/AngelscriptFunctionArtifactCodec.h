#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"

class asCModule;
class asCScriptEngine;
class asCScriptFunction;

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionDebugTransition
{
	uint32 ProgramPosition = 0;
	uint32 SourceOrdinal = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionDebugTemporary
{
	uint32 StackOffset = 0;
	uint32 Token = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionDebugLocal
{
	FString Name;
	uint32 DeclaredAtProgramPosition = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionDebugArtifact
{
	TArray<FAngelscriptCachedDebugSourceReference> Sources;
	uint32 DeclaredAt = 0;
	TArray<uint32> LineNumbers;
	TArray<FAngelscriptFunctionDebugTransition> SectionTransitions;
	TArray<FString> ParameterNames;
	TArray<FAngelscriptFunctionDebugTemporary> TemporaryVariables;
	TArray<FAngelscriptFunctionDebugLocal> LocalVariables;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptFunctionArtifactCodec final
	: public IAngelscriptCacheOpaquePayloadValidator
{
public:
	static constexpr uint32 ExecutionCodecVersion = 5;
	static constexpr uint32 DebugCodecVersion = 2;

	FAngelscriptFunctionArtifactCodec(
		asCModule& InModule,
		asCScriptEngine& InEngine);

	static FAngelscriptCacheValidationResult EncodeDebugArtifact(
		const FAngelscriptFunctionDebugArtifact& Artifact,
		TArray<uint8>& OutPayload);

	// Wraps the maintained-fork VM artifact with a pointer-free stable
	// relocation manifest. The returned content hash intentionally covers the
	// inner VM artifact, while the enclosing FunctionBody record hash protects
	// the complete envelope. This keeps function-content identities acyclic
	// when relocation dependencies themselves carry function-content hashes.
	FAngelscriptCacheValidationResult EncodeExecutionArtifact(
		TConstArrayView<uint8> RawVmPayload,
		const FAngelscriptStableModuleKey& ModuleKey,
		TConstArrayView<FAngelscriptCacheSemanticDependency>
			DeclaredDependencies,
		TArray<uint8>& OutPayload,
		FAngelscriptHash256& OutExecutionContentHash) const;

	// Rebuilds and attaches one complete self-contained global function to the
	// codec's staging module. The caller must have graph-validated the owning
	// FunctionBody/DebugSidecar pair and must discard the staging module if a
	// later module-level step fails.
	FAngelscriptCacheValidationResult RestoreGlobalFunction(
		TConstArrayView<uint8> CanonicalExecutionPayload,
		TConstArrayView<uint8> CanonicalDebugPayload,
		const FAngelscriptStableModuleKey& ModuleKey,
		TConstArrayView<FAngelscriptCacheSemanticDependency>
			DeclaredDependencies,
		const FAngelscriptHash256& ExpectedExecutionContentHash,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		asCScriptFunction*& OutFunction) const;

	// Restores into an unpublished donor, applies the complete debug artifact to
	// that donor, then atomically commits the private VM state to the exact
	// already-declared builder target. Any failure leaves TargetFunction's
	// executable state untouched.
	FAngelscriptCacheValidationResult RestoreFunctionIntoExisting(
		TConstArrayView<uint8> CanonicalExecutionPayload,
		TConstArrayView<uint8> CanonicalDebugPayload,
		const FAngelscriptStableModuleKey& ModuleKey,
		TConstArrayView<FAngelscriptCacheSemanticDependency>
			DeclaredDependencies,
		const FAngelscriptHash256& ExpectedExecutionContentHash,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		asCScriptFunction& TargetFunction) const;

	virtual FAngelscriptCacheValidationResult Validate(
		const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		IAngelscriptCacheCandidateChargeSink& GraphCandidate,
		FAngelscriptCacheOpaquePayloadSummary& OutSummary) const override;

	const FString& GetLastExecutionFailureDetail() const;

private:
	FAngelscriptCacheValidationResult ValidateRawExecutionArtifact(
		const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		IAngelscriptCacheCandidateChargeSink& GraphCandidate,
		FAngelscriptCacheOpaquePayloadSummary& OutSummary) const;

	asCModule* Module = nullptr;
	asCScriptEngine* Engine = nullptr;
	mutable FString LastExecutionFailureDetail;
};
