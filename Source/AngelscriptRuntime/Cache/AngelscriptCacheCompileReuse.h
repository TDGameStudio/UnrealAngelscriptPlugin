#pragma once

#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheStore.h"

struct FAngelscriptModuleDesc;
class FAngelscriptCacheService;

// Pointer-free aggregate for one selected persisted Generation carried through
// an authoritative compile transaction. Counts describe compiler decisions and
// deliberately overlap: a NotCacheable function may also increment
// CompiledMissCount after the maintained compiler is invoked for it.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFunctionReuseSummary final
{
	static constexpr uint32 CurrentSchemaVersion = 1;

	uint32 SchemaVersion = CurrentSchemaVersion;
	bool bPresent = false;
	FAngelscriptHash256 CandidateGenerationId;
	uint32 CandidateModuleCount = 0;
	uint32 RestoredFunctionCount = 0;
	uint32 CompiledMissCount = 0;
	uint32 NotCacheableCount = 0;
	uint32 RejectedCorruptCount = 0;
};

enum class EAngelscriptCacheCompileReusePrepareError : uint8
{
	None = 0,
	InvalidInput = 1,
	SourceSnapshotMismatch = 2,
	CandidateModuleMissing = 3,
	GraphValidationFailed = 4,
	CurrentAuthorityMissing = 5,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompileReusePrepareResult
{
	EAngelscriptCacheCompileReusePrepareError Error =
		EAngelscriptCacheCompileReusePrepareError::None;
	TOptional<FAngelscriptCacheValidationResult> Validation;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheCompileReusePrepareError::None;
	}
};

// Owns one immutable, pinned persisted input candidate for the lifetime of one
// authoritative compile transaction. It never publishes active modules or
// writes the Store; normal compile/swap/capture retains those responsibilities.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompileReuseContext final
	: public IAngelscriptCacheRestoredFunctionDependencySource
{
public:
	~FAngelscriptCacheCompileReuseContext();

	FAngelscriptCacheCompileReuseContext(
		const FAngelscriptCacheCompileReuseContext&) = delete;
	FAngelscriptCacheCompileReuseContext& operator=(
		const FAngelscriptCacheCompileReuseContext&) = delete;

	static TUniquePtr<FAngelscriptCacheCompileReuseContext> Create(
		TUniquePtr<FAngelscriptCacheReadSession>&& Session,
		const FAngelscriptHash256& GenerationId,
		const FAngelscriptCacheCleanCaptureOptions& CurrentOptions);

	bool IsValid() const;
	FAngelscriptCacheFunctionReuseSummary CaptureSummary() const;

	virtual bool TryCopyActualDependencies(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptStableFunctionKey& FunctionKey,
		TArray<FAngelscriptCacheSemanticDependency>& OutDependencies)
		const override;

	// Prepares one candidate against semantic authority frozen from the current
	// staging module. Exact-source candidates may reuse their already validated
	// authority; changed-source candidates must use the shared current producer.
	FAngelscriptCacheCompileReusePrepareResult
	PrepareModule(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptHash256& CurrentSourceSnapshot,
		FAngelscriptCacheService* Service);

private:
	FAngelscriptCacheCompileReuseContext();

	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
