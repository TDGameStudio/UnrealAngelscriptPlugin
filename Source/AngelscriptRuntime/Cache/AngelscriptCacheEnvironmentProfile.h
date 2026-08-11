#pragma once

#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheSourceDiscovery.h"
#include "Core/AngelscriptSource.h"

struct FAngelscriptEngine;
struct FAngelscriptPreprocessorContext;

enum class EAngelscriptCacheEnvironmentProfileError : uint8
{
	None = 0,
	InvalidEngine = 1,
	InvalidSourceRoot = 2,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheEnvironmentProfileResult
{
	EAngelscriptCacheEnvironmentProfileError Error =
		EAngelscriptCacheEnvironmentProfileError::None;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheEnvironmentProfileError::None;
	}
};

// The sole production construction of Compatibility/Context/Profile and the
// matching compiler/preprocessor option coordinates. Absolute host paths and
// current source bytes belong to SourceIndex and never enter this root profile.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheEnvironmentProfile
{
	FAngelscriptCacheCleanCaptureOptions CaptureOptions;
	FAngelscriptCacheProductionSourceDiscoveryConfig DiscoveryConfig;
};

enum class EAngelscriptCacheCompileCapturePreparationError : uint8
{
	None = 0,
	EnvironmentProfileFailed = 1,
	SourceDiscoveryFailed = 2,
	SourceCandidateFailed = 3,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompileCapturePreparationResult
{
	EAngelscriptCacheCompileCapturePreparationError Error =
		EAngelscriptCacheCompileCapturePreparationError::None;
	EAngelscriptCacheEnvironmentProfileError EnvironmentError =
		EAngelscriptCacheEnvironmentProfileError::None;
	EAngelscriptCacheSourceDiscoveryError SourceDiscoveryError =
		EAngelscriptCacheSourceDiscoveryError::None;
	FAngelscriptCacheValidationResult Validation;
	FString Detail;

	bool IsSuccess() const
	{
		return Error ==
			EAngelscriptCacheCompileCapturePreparationError::None;
	}
};

// Immutable source/profile authority for one real compile transaction. It owns
// no Engine, source-provider or module pointers and may be passed through the
// compiler until final lifecycle publication is decided.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompileCaptureContext
{
	FAngelscriptCacheEnvironmentProfile Environment;
	FAngelscriptCachedSourceIndex AuthoritativeSourceIndex;
	int32 DiscoveredSourceCount = 0;
	int32 LoadedSourceCount = 0;

	void Reset()
	{
		Environment = {};
		AuthoritativeSourceIndex = {};
		DiscoveredSourceCount = 0;
		LoadedSourceCount = 0;
	}
};

ANGELSCRIPTRUNTIME_API FAngelscriptCacheEnvironmentProfileResult
BuildAngelscriptCacheEnvironmentProfile(
	const FAngelscriptEngine& Engine,
	const FAngelscriptPreprocessorContext& PreprocessorContext,
	TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
	FAngelscriptCacheEnvironmentProfile& OutProfile);

// Sole production composition of environment identity, exact direct source
// discovery and the persisted dependency candidate used by lifecycle capture.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompileCapturePreparationResult
PrepareAngelscriptCacheCompileCaptureContext(
	const FAngelscriptEngine& Engine,
	const FAngelscriptPreprocessorContext& PreprocessorContext,
	IAngelscriptSourceProvider& SourceProvider,
	TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
	bool bSkipDevelopmentScripts,
	bool bSkipEditorScripts,
	FAngelscriptCacheCompileCaptureContext& OutContext);
