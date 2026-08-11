#pragma once

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

class asCScriptFunction;
class asCTypeInfo;
struct FAngelscriptModuleDesc;

// Rebuilds stable script-symbol identities exclusively from the current
// Engine's semantic declaration surface. Engine-local pointers and numeric ids
// are inputs only and are never part of the resulting key.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheStableSymbolIdentity final
{
public:
	static bool TryBuildModuleKey(
		const FAngelscriptModuleDesc& Module,
		FAngelscriptStableModuleKey& OutKey,
		FString* OutFailure = nullptr);

	static bool TryBuildLocalTypeKey(
		const FAngelscriptStableModuleKey& ModuleKey,
		const asCTypeInfo& Type,
		FAngelscriptStableTypeKey& OutKey);

	static bool TryBuildFunctionKey(
		const FAngelscriptStableModuleKey& ModuleKey,
		const asCScriptFunction& Function,
		FAngelscriptStableFunctionKey& OutKey,
		FString* OutFailure = nullptr);
};
