#pragma once

#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheTypeSchema.h"

class asCScriptFunction;
struct FAngelscriptModuleDesc;

// One current function declaration paired with the live staging function that
// owns it. The pointer never leaves the compile transaction; every persisted or
// diagnostic identity is the full-width StableKey in Declaration.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCurrentFunctionAuthority final
{
	asCScriptFunction* Function = nullptr;
	FAngelscriptCachedDeclaration Declaration;
};

// Pointer-free semantic input authority frozen after declarations/layouts exist
// and before function bodies compile. Function entries retain live pointers only
// so the same authority plan can feed post-compile artifact finalization.
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCurrentModuleAuthority final
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptCachedModuleInterface ModuleInterface;
	TArray<FAngelscriptCachedTypeSchema> TypeSchemas;
	FAngelscriptCachedModuleState ModuleState;
	TArray<FAngelscriptCacheCurrentFunctionAuthority> Functions;

	void Reset()
	{
		ModuleKey = {};
		ModuleInterface = {};
		TypeSchemas.Reset();
		ModuleState = {};
		Functions.Reset();
	}
};

// Builds only semantic inputs; it never reads or encodes function execution or
// debug artifacts and never writes the Store. Unsupported current shapes return
// NotCacheable so normal compilation remains authoritative.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
BuildAngelscriptCacheCurrentModuleAuthority(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptStableModuleKey& ModuleKey,
	FAngelscriptCacheCurrentModuleAuthority& OutAuthority);
