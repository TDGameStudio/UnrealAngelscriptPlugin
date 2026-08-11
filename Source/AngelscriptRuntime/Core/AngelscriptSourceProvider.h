#pragma once

#include "CoreMinimal.h"

#include "AngelscriptSource.h"

class IFileManager;

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceState
{
	FDateTime Timestamp;
	uint64 ContentHash = 0;
	bool bHasContentHash = false;
};

enum class EAngelscriptSourceProviderDescriptorKind : uint8
{
	Unknown = 0,
	BuiltInDisk = 1,
	Memory = 2,
	Generated = 3,
	External = 4,
};

/**
 * Stable, source-scoped provenance exposed by a source provider.
 *
 * Every string is a logical/configuration coordinate and must be independent of
 * process addresses, registration order and host absolute paths. Providers that
 * cannot make that guarantee should return false from QuerySourceDescriptor;
 * callers can still consume their sources while treating the affected scope as
 * unsuitable for an exact persisted fast path.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceProviderDescriptor
{
	EAngelscriptSourceProviderDescriptorKind Kind =
		EAngelscriptSourceProviderDescriptorKind::Unknown;
	FString CanonicalImplementationIdentity;
	TOptional<FString> StableInstanceIdentity;
	TOptional<FString> Version;
	TOptional<FString> Configuration;
	TOptional<FString> GeneratedSourceIdentity;
	TOptional<FString> GeneratedSourceConfiguration;
};

struct ANGELSCRIPTRUNTIME_API IAngelscriptSourceProvider
{
	virtual ~IAngelscriptSourceProvider() = default;

	virtual void FindSources(
		const TArray<FAngelscriptSourceRoot>& ScriptRoots,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts,
		TArray<FAngelscriptSource>& OutSources) = 0;

	virtual bool LoadSourceText(const FAngelscriptSource& Source, FString& OutSourceText) = 0;
	virtual bool QuerySourceState(const FAngelscriptSource& Source, FAngelscriptSourceState& OutState) = 0;

	/**
	 * Load the directly observable source-byte authority. The default preserves
	 * compatibility for text providers by encoding their authoritative FString as
	 * UTF-8. Disk providers should override this to return exact file bytes.
	 */
	virtual bool LoadSourceBytes(
		const FAngelscriptSource& Source,
		TArray<uint8>& OutSourceBytes);

	/** Return stable source-scoped provenance, or false when unavailable. */
	virtual bool QuerySourceDescriptor(
		const FAngelscriptSource& Source,
		FAngelscriptSourceProviderDescriptor& OutDescriptor);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptDiskSourceProvider final : IAngelscriptSourceProvider
{
	virtual void FindSources(
		const TArray<FAngelscriptSourceRoot>& ScriptRoots,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts,
		TArray<FAngelscriptSource>& OutSources) override;

	virtual bool LoadSourceText(const FAngelscriptSource& Source, FString& OutSourceText) override;
	virtual bool QuerySourceState(const FAngelscriptSource& Source, FAngelscriptSourceState& OutState) override;
	virtual bool LoadSourceBytes(
		const FAngelscriptSource& Source,
		TArray<uint8>& OutSourceBytes) override;
	virtual bool QuerySourceDescriptor(
		const FAngelscriptSource& Source,
		FAngelscriptSourceProviderDescriptor& OutDescriptor) override;

private:
	static void FindScriptFiles(
		IFileManager& FileManager,
		const FAngelscriptSourceRoot& Root,
		const FString& RelativeRoot,
		const FString& SearchDirectory,
		const TCHAR* Pattern,
		TArray<FAngelscriptSource>& OutSources,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts);
};
