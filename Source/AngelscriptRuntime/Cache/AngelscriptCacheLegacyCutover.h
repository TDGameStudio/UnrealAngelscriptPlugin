#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

enum class EAngelscriptLegacyCacheArtifactKind : uint8
{
	PrecompiledScript = 0,
	PrecompiledScriptDevelopment,
	PrecompiledScriptTest,
	PrecompiledScriptShipping,
};

struct FAngelscriptRejectedLegacyCacheArtifact
{
	EAngelscriptLegacyCacheArtifactKind Kind =
		EAngelscriptLegacyCacheArtifactKind::PrecompiledScript;
	FString AbsolutePath;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptLegacyCacheInspection
{
	TArray<FAngelscriptRejectedLegacyCacheArtifact> RejectedArtifacts;
	TArray<FString> AcceptedBindCachePaths;

	bool HasRejectedLegacyScriptCache() const
	{
		return !RejectedArtifacts.IsEmpty();
	}

	FString FormatDiagnostic() const;
};

/**
 * Inspect only the fixed legacy filenames beside authoritative Script roots.
 *
 * The supplied predicate is intentionally existence-only. Cache V2 does not open,
 * decode, migrate or hash a legacy payload, and Binds.Cache remains a separate
 * accepted artifact.
 */
ANGELSCRIPTRUNTIME_API FAngelscriptLegacyCacheInspection
InspectAngelscriptLegacyCacheArtifacts(
	TConstArrayView<FString> ScriptRoots,
	TFunctionRef<bool(const FString& AbsoluteCandidate)> FileExists);

ANGELSCRIPTRUNTIME_API FAngelscriptLegacyCacheInspection
InspectAngelscriptLegacyCacheArtifactsFromDisk(
	TConstArrayView<FString> ScriptRoots);
