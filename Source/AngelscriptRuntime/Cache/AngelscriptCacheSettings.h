#pragma once

#include "CoreMinimal.h"
#include "Cache/AngelscriptRuntimeReload.h"
#include "Engine/DeveloperSettings.h"

#include "AngelscriptCacheSettings.generated.h"

UCLASS(Config=Engine, DefaultConfig,
	meta=(DisplayName="AngelScript Incremental Cache"))
class ANGELSCRIPTRUNTIME_API UAngelscriptCacheSettings final
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }

	/** Generate and consume Cache V2 in Editor and packaged Runtime. */
	UPROPERTY(EditAnywhere, Config, Category="Cache V2")
	bool bEnableCacheV2 = true;

	/** Packaged builds never watch/reload loose script unless explicitly enabled. */
	UPROPERTY(EditAnywhere, Config, Category="Runtime Reload")
	EAngelscriptPackagedRuntimeReloadMode PackagedRuntimeReloadMode =
		EAngelscriptPackagedRuntimeReloadMode::Disabled;

	/** Poll interval used only by Automatic packaged Runtime reload. */
	UPROPERTY(EditAnywhere, Config, Category="Runtime Reload",
		meta=(ClampMin="0.1", UIMin="0.1", Units="s"))
	float RuntimeReloadScanIntervalSeconds = 1.0f;

	/** Maximum time Engine shutdown waits for its frozen Cache V2 publication. */
	UPROPERTY(EditAnywhere, Config, Category="Cache V2",
		meta=(ClampMin="0.1", UIMin="0.1", Units="s"))
	float ShutdownFlushTimeoutSeconds = 5.0f;

	/** Canonical raw bytes targeted per immutable Pack; physical writer policy. */
	UPROPERTY(EditAnywhere, Config, Category="Cache V2",
		meta=(ClampMin="1", ClampMax="256", UIMin="4", UIMax="64", Units="MiB"))
	uint32 PackTargetMiB = 64;

	/** Prepare immutable record compression and independent Packs on workers. */
	UPROPERTY(EditAnywhere, Config, Category="Cache V2")
	bool bEnableParallelPreparation = true;

	/** Upper bound for Cache-only immutable preparation workers. */
	UPROPERTY(EditAnywhere, Config, Category="Cache V2",
		meta=(ClampMin="1", ClampMax="64", UIMin="1", UIMax="16"))
	uint32 MaxPreparationWorkerCount = 4;

	/** Opt-in bounded live decision tracing; aggregate status remains available. */
	UPROPERTY(EditAnywhere, Config, Category="Diagnostics")
	bool bEnableDecisionTrace = false;

	/** Maximum retained trace events; oldest events are evicted first. */
	UPROPERTY(EditAnywhere, Config, Category="Diagnostics",
		meta=(ClampMin="1", ClampMax="65536", UIMin="1", UIMax="65536"))
	uint32 DecisionTraceCapacity = 1024;
};
