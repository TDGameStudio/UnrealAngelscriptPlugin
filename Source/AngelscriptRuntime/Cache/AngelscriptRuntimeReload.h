#pragma once

#include "CoreMinimal.h"

#include "AngelscriptRuntimeReload.generated.h"

/**
 * Controls whether a non-editor Runtime engine may inspect loose AngelScript
 * sources after startup. Disabled is deliberately the shipping-safe default.
 */
UENUM(BlueprintType)
enum class EAngelscriptPackagedRuntimeReloadMode : uint8
{
	Disabled = 0,
	Manual = 1,
	Automatic = 2,
};

/** Immediate result of asking the Engine to queue a packaged Runtime reload. */
UENUM(BlueprintType)
enum class EAngelscriptRuntimeReloadRequestStatus : uint8
{
	Queued = 0,
	Disabled = 1,
	Busy = 2,
	ShuttingDown = 3,
};

/** Final result produced when a queued request reaches the game-thread safe point. */
UENUM(BlueprintType)
enum class EAngelscriptRuntimeReloadOutcome : uint8
{
	NoChanges = 0,
	AppliedCodeOnly = 1,
	RequiresRestart = 2,
	CompileFailed = 3,
	Cancelled = 4,
};

/**
 * Pointer-free diagnostic result suitable for Blueprint, logs and JSON tools.
 * Stable module keys are emitted as full lowercase hexadecimal strings.
 */
USTRUCT(BlueprintType)
struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeReloadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	EAngelscriptRuntimeReloadOutcome Outcome =
		EAngelscriptRuntimeReloadOutcome::NoChanges;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	TArray<FString> ChangedModuleNames;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	TArray<FString> ChangedModuleKeys;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	int32 CacheHitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	int32 CacheMissCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	int32 RecompiledModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	FString SelectedGenerationId;

	UPROPERTY(BlueprintReadOnly, Category="AngelScript|Runtime Reload")
	FString Diagnostics;
};
