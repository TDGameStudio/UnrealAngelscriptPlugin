#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

class asCScriptEngine;
class asCScriptFunction;
class asCTypeInfo;
class UClass;

// Canonical identity for application-registered AngelScript types. The
// persisted reference contains only canonical AngelScript surface data and ABI
// hashes; current Engine pointers and numeric TypeIds never cross this boundary.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheEnvironmentIdentity final
{
public:
	static bool TryBuildTypeReference(
		const asCTypeInfo& Type,
		FAngelscriptCacheStableReference& OutReference);

	static bool TryBuildFunctionReference(
		const asCScriptFunction& Function,
		FAngelscriptCacheStableReference& OutReference);

	static bool TryBuildCodeRootReference(
		const UClass& CodeRoot,
		FAngelscriptCacheStableReference& OutReference);
};

// Resolves a persisted EnvironmentSymbol against the application-registered
// types of one current AngelScript Engine. Ambiguous identities fail closed.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheEngineEnvironmentResolver final
	: public IAngelscriptCacheCurrentSymbolResolver
{
public:
	explicit FAngelscriptCacheEngineEnvironmentResolver(
		const asCScriptEngine& InEngine);

	virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
		EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey) const override;

	// Materialization-time companions to Resolve(). These return current-Engine
	// objects only after both the stable key and expected ABI match exactly.
	// Ambiguous references fail closed just like the validation-only resolver.
	asCTypeInfo* ResolveTypeReference(
		const FAngelscriptCacheStableReference& Expected) const;
	asCScriptFunction* ResolveFunctionReference(
		const FAngelscriptCacheStableReference& Expected) const;
	UClass* ResolveCodeRootReference(
		const FAngelscriptCacheStableReference& Expected) const;

private:
	const asCScriptEngine* Engine = nullptr;
};

// Resolves the storage facts that are external to a selected script module:
// registered AngelScript value/handle layouts and the reflected C++ code-root
// boundary used by UE-derived script classes.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheEngineLayoutResolver final
	: public IAngelscriptCacheCurrentLayoutResolver
{
public:
	explicit FAngelscriptCacheEngineLayoutResolver(
		const asCScriptEngine& InEngine);

	virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout>
	ResolveDataTypeLayout(
		const FAngelscriptCachedDataType& DataType,
		const IAngelscriptCacheProspectiveTypeLayoutView& LocalLayouts)
		const override;

	virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
	ResolveTypeLayoutInput(
		EAngelscriptCachedTypeLayoutInputKind InputKind,
		EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey) const override;

private:
	const asCScriptEngine* Engine = nullptr;
};
